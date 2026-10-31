"""执行正常升级、整体CRC故障、超时或主动ABORT恢复测试，保存原始日志与计时。"""

import argparse
import codecs
import json
import math
import re
import threading
import time
from contextlib import ExitStack
from datetime import datetime
from pathlib import Path

import serial
import ota_upgrade_fwcrc_min as baseline
import ota_upgrade as normal_upgrade
import ota_upgrade_timeout as timeout_test
from ota_timing import OtaTiming
from ota_upgrade_fwcrc_trace import TraceSerial


def open_debug_port(port):
    """先撤销 DTR/RTS，再打开板载 CH340 调试口，避免持续驱动下载电路。"""
    # port=None 时不会打开设备。关闭流控不等于撤销 DTR/RTS，必须显式设置。
    uart = serial.Serial(port=None, baudrate=115200, timeout=0.1,
                         rtscts=False, dsrdtr=False)
    try:
        uart.dtr = False
        uart.rts = False
        uart.port = port
        uart.open()
    except BaseException:
        uart.close()
        raise
    return uart


class DebugReader:
    """只读取调试口，不发送、不清空，不参与升级状态判断。"""

    def __init__(self, uart, raw, text):
        self.uart, self.raw, self.text = uart, raw, text
        self.stop = threading.Event()
        self.error = None
        self.count = 0
        self.thread = threading.Thread(target=self.run, name="stm32-log", daemon=True)

    def run(self):
        decoder = codecs.getincrementaldecoder("utf-8")(errors="replace")
        try:
            while not self.stop.is_set():
                data = self.uart.read(4096)
                if data:
                    self.raw.write(data)
                    self.raw.flush()
                    self.text.write(decoder.decode(data))
                    self.text.flush()
                    self.count += len(data)
            self.text.write(decoder.decode(b"", final=True))
            self.text.flush()
        except Exception as error:
            self.error = str(error)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, *args):
        self.stop.set()
        # 串口读取有 0.1 秒超时；先退出读取线程，再由外层关闭串口与文件。
        self.thread.join()


class SplitDataSerial:
    """按指定位置拆开一个数据包，字节内容和顺序保持一致。"""

    def __init__(self, uart, seq, cuts=(8,), gap_s=0.05):
        self.uart = uart
        self.seq = seq
        self.cuts = cuts
        self.gap_s = gap_s
        self.injected = False

    def __getattr__(self, name):
        return getattr(self.uart, name)

    def __enter__(self):
        self.uart.__enter__()
        return self

    def __exit__(self, *args):
        return self.uart.__exit__(*args)

    def write(self, data):
        if (self.injected or len(data) != 1037 or data[:2] != b"\x66\x06" or
                int.from_bytes(data[2:4], "big") != self.seq):
            return self.uart.write(data)
        points = (0, *self.cuts, len(data))
        sizes = [end - start for start, end in zip(points, points[1:])]
        print(f"[拆帧] seq={self.seq}：各段字节数 {sizes}，"
              f"段间等待 {self.gap_s * 1000:g} ms")
        self.uart.record("SPLIT_DATA", seq=self.seq, cuts=list(self.cuts),
                         sizes=sizes, gap_s=self.gap_s)
        for index, (start, end) in enumerate(zip(points, points[1:]), 1):
            part = data[start:end]
            if self.uart.write(part) != len(part):
                raise RuntimeError(f"拆帧测试：第 {index} 段未完整提交")
            if index < len(sizes):
                self.uart.flush()
                time.sleep(self.gap_s)
        self.injected = True
        return len(data)


def parse_cuts(value):
    """解析累计字节位置，例如8表示收完前8字节后暂停。"""
    try:
        cuts = tuple(int(part.strip()) for part in value.split(","))
    except ValueError as error:
        raise argparse.ArgumentTypeError("拆分位置须为逗号分隔的整数") from error
    if (any(not 1 <= cut <= 1036 for cut in cuts) or
            any(left >= right for left, right in zip(cuts, cuts[1:]))):
        raise argparse.ArgumentTypeError("拆分位置须在1～1036之间，严格递增且不重复")
    return cuts


def version_value(value):
    """检查三段版本号，返回协议中使用的纯数字版本字符串。"""
    value = value.removeprefix("v")
    parts = value.split(".")
    if (len(value) > 8 or len(parts) != 3 or
            any(not part.isascii() or not part.isdigit() or int(part) > 255 for part in parts)):
        raise argparse.ArgumentTypeError("版本须为三段 0～255 的整数，且最多 8 字符，例如 2.0.7")
    return value


def read_recovery_crc(path):
    """从本轮已关闭的调试日志中提取最近一次备份和恢复CRC，缺证据时返回None。"""
    result = {"backup": None, "restored": None, "match": None}
    if not path.exists():
        return result
    try:
        text = path.read_bytes().decode("utf-8", errors="replace")
    except OSError as error:
        result["read_error"] = str(error)
        return result
    for kind, crc in re.findall(
            r"\[回滚\] (A区备份完成|恢复成功)，CRC32=0x([0-9A-Fa-f]{8})", text):
        if kind == "A区备份完成":
            result.update(backup=crc.upper(), restored=None, match=None)
        else:
            result["restored"] = crc.upper()
            if result["backup"] is not None:
                result["match"] = result["backup"] == result["restored"]
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--debug-port", default="COM26", help="调试串口，默认 COM26")
    parser.add_argument("--debug-open-only", action="store_true",
                        help="对照模式：只打开调试口，不创建读取线程")
    parser.add_argument("--normal-upgrade", action="store_true",
                        help="执行正常固件升级，不注入整体 CRC 错误")
    parser.add_argument("--version", type=version_value,
                        help="候选固件版本，正常升级和 CRC 故障测试均可指定")
    parser.add_argument("--expect-current-version", type=version_value,
                        help="要求测试前 APP 为此版本，否则停止并且不通知升级")
    parser.add_argument("--split-data-seq", type=int,
                        help="拆开发送指定序号的数据帧，默认8+1029字节；仅用于正常升级模式")
    parser.add_argument("--split-data-cuts", type=parse_cuts,
                        help="按累计字节位置拆帧，例如1,2,8,520,1032,1036；默认8")
    parser.add_argument("--split-gap-ms", type=float,
                        help="相邻两段发送之间的等待毫秒数，默认50")
    parser.add_argument("--glue-last-data", action="store_true",
                        help="一次提交最后两个数据帧，末包使用实际长度，合计须不超过2048字节")
    parser.add_argument("--timeout-recovery", action="store_true",
                        help="确认前10包后静默16秒，检查数据阶段超时回滚和旧会话收尾")
    parser.add_argument("--abort-after-packets", type=int,
                        help="确认指定包数后立即发送ABORT，检查旧镜像恢复")
    args = parser.parse_args()
    if args.abort_after_packets is not None:
        if (args.normal_upgrade or args.timeout_recovery or args.glue_last_data or
                args.split_data_seq is not None):
            parser.error("主动ABORT测试不能同时选择正常升级、超时、粘帧或拆帧")
        if not args.version:
            parser.error("主动ABORT测试须显式指定 --version")
        total = (normal_upgrade.FW_PATH.stat().st_size + normal_upgrade.CHUNK_SIZE - 1) // normal_upgrade.CHUNK_SIZE
        if not 1 <= args.abort_after_packets < total:
            parser.error("主动中止位置必须至少确认一包，并且不能收满固件")
    if args.timeout_recovery:
        if args.normal_upgrade or args.glue_last_data or args.split_data_seq is not None:
            parser.error("数据超时测试不能同时选择正常升级、粘帧或拆帧")
        if not args.version:
            parser.error("数据超时测试须显式指定 --version")
    if args.glue_last_data and (not args.normal_upgrade or args.split_data_seq is not None):
        parser.error("粘帧测试需要 --normal-upgrade，且本轮不能同时选择拆帧测试")
    if args.split_data_seq is None and (args.split_data_cuts is not None or
                                       args.split_gap_ms is not None):
        parser.error("拆分位置和间隔参数需要同时指定 --split-data-seq")
    if args.split_data_cuts is None:
        args.split_data_cuts = (8,)
    if args.split_gap_ms is None:
        args.split_gap_ms = 50.0
    if not math.isfinite(args.split_gap_ms) or args.split_gap_ms <= 0:
        parser.error("拆帧等待时间必须是大于0的有限毫秒数")
    if args.normal_upgrade and not args.version:
        parser.error("正常升级须显式指定 --version，例如 --version 2.0.6")
    if args.split_data_seq is not None:
        if not args.normal_upgrade:
            parser.error("拆帧测试需要 --normal-upgrade，单独验证拆帧行为")
        count = (normal_upgrade.FW_PATH.stat().st_size + normal_upgrade.CHUNK_SIZE - 1) // normal_upgrade.CHUNK_SIZE
        if not 0 <= args.split_data_seq < count:
            parser.error("拆帧序号不在本次固件的数据包范围内")
    if args.debug_port.upper() == baseline.PORT.upper():
        raise ValueError("调试口不能与协议口相同")
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    if args.timeout_recovery:
        mode, prefix = "timeout_recovery", "timeout_capture"
    elif args.abort_after_packets is not None:
        mode, prefix = "abort_recovery", "abort_capture"
    elif args.normal_upgrade:
        mode, prefix = "normal", "upgrade_capture"
    else:
        mode, prefix = "crc_rollback", "fwcrc_capture"
    folder = Path(__file__).resolve().parent / "runs" / f"{prefix}_{stamp}"
    folder.mkdir(parents=True, exist_ok=False)
    print(f"本轮记录目录：{folder}")
    print(f"Python 接管调试口 {args.debug_port}，请先关闭串口助手对该端口的占用")
    print("调试口打开前设置 DTR=False、RTS=False，撤销板载一键下载控制信号")
    print("调试采集不作为协议条件；协议流程沿用已验证脚本")
    if args.timeout_recovery:
        print("本轮执行数据阶段超时恢复：确认前10包后静默16秒，再检查恢复通知和旧APP")
    elif args.abort_after_packets is not None:
        print(f"本轮执行主动ABORT恢复：确认前{args.abort_after_packets}包后立即中止，检查旧APP恢复")
    elif args.normal_upgrade:
        print(f"本轮执行正常升级，声明目标版本 {args.version}；不注入错误、不主动回滚")
    else:
        print(f"本轮执行整体 CRC 错误回滚，候选版本 {args.version or baseline.TEST_VERSION}；"
              "恢复后应为测试前版本")
    if args.expect_current_version:
        print(f"测试前版本必须为 {args.expect_current_version}，不匹配则停止")
    if args.debug_open_only:
        print("对照模式：调试口仅保持打开，不读取、不发送、不清空；调试日志为空属于预期")
    result = {"protocol_pass": False, "debug_bytes": 0, "debug_error": None,
              "test_mode": mode,
              "candidate_version": args.version or baseline.TEST_VERSION,
              "expected_current_version": args.expect_current_version,
              "split_data_seq": args.split_data_seq, "split_executed": False,
              "split_data_cuts": list(args.split_data_cuts) if args.split_data_seq is not None else None,
              "split_gap_ms": args.split_gap_ms if args.split_data_seq is not None else None,
              "glue_last_data": args.glue_last_data,
              "glue_executed": False, "glue_ack_count": 0,
              "abort_after_packets": args.abort_after_packets,
              "debug_dtr": False, "debug_rts": False,
              "debug_mode": "open_only" if args.debug_open_only else "capture"}
    reader = None
    splitter = None
    glue_result = {}
    cleanup_result = {}
    timeout_result = {}
    timing = OtaTiming()
    try:
        with ExitStack() as stack:
            output = stack.enter_context((folder / "protocol.jsonl").open("w", encoding="utf-8"))
            raw = stack.enter_context((folder / "stm32.bin").open("wb"))
            text = stack.enter_context((folder / "stm32.log").open("w", encoding="utf-8"))
            debug = stack.enter_context(open_debug_port(args.debug_port))
            if not args.debug_open_only:
                reader = stack.enter_context(DebugReader(debug, raw, text))

            def open_port(*port_args, **port_kwargs):
                nonlocal splitter
                uart = TraceSerial(serial.Serial(*port_args, **port_kwargs), output)
                if args.split_data_seq is not None:
                    splitter = SplitDataSerial(uart, args.split_data_seq,
                                               cuts=args.split_data_cuts,
                                               gap_s=args.split_gap_ms / 1000.0)
                    return splitter
                return uart

            if args.timeout_recovery:
                timeout_test.main(serial_factory=open_port, timing=timing, version=args.version,
                                  expected_current=args.expect_current_version,
                                  cleanup_result=cleanup_result, test_result=timeout_result)
                if not timeout_result.get("recovery_notice"):
                    raise RuntimeError("未观察到超时恢复通知，不能判定本用例通过")
            elif args.normal_upgrade or args.abort_after_packets is not None:
                normal_upgrade.main(serial_factory=open_port, version=args.version, timing=timing,
                                    expected_current=args.expect_current_version,
                                    glue_last_data=args.glue_last_data, glue_result=glue_result,
                                    cleanup_result=cleanup_result,
                                    abort_after_packets=args.abort_after_packets)
            else:
                baseline.main(serial_factory=open_port, timing=timing, version=args.version,
                              expected_current=args.expect_current_version,
                              cleanup_result=cleanup_result)
            if args.split_data_seq is not None and (splitter is None or not splitter.injected):
                raise RuntimeError("本轮未完成指定包的拆帧发送，不能判定拆帧测试通过")
            if args.glue_last_data and (not glue_result.get("sent") or
                                       glue_result.get("ack_count") != 2):
                raise RuntimeError("本轮没有完成粘帧提交和两次ACK确认，不能判定粘帧通过")
            if args.abort_after_packets is not None and (
                    not cleanup_result.get("success") or
                    cleanup_result.get("packets_confirmed") != args.abort_after_packets):
                raise RuntimeError("未在指定位置完成主动ABORT和旧APP恢复，不能判定本用例通过")
            result["protocol_pass"] = True
    except (Exception, KeyboardInterrupt) as error:
        result["error"] = str(error) or "用户中断"
        raise
    finally:
        if reader is not None:
            result["debug_bytes"] = reader.count
            result["debug_error"] = reader.error
        if splitter is not None:
            result["split_executed"] = splitter.injected
        result["glue_executed"] = glue_result.get("sent", False)
        result["glue_ack_count"] = glue_result.get("ack_count", 0)
        result["cleanup"] = cleanup_result
        result["timeout_test"] = timeout_result
        result["recovery_crc"] = read_recovery_crc(folder / "stm32.log")
        result["timing"] = timing.summary()
        (folder / "result.json").write_text(
            json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
        print(f"调试口采集：{result['debug_bytes']} 字节")
        if result["debug_error"]:
            print(f"调试采集失败：{result['debug_error']}")
        elif args.debug_open_only:
            print("本轮仅打开调试口，未读取日志；0 字节属于预期")
        elif not result["debug_bytes"]:
            print("未采集到调试日志，不能据此确认恢复 CRC")
        print(f"记录已保存：{folder}")
        if not args.debug_open_only and not args.normal_upgrade:
            print(f"恢复CRC证据：{result['recovery_crc']}")
            print("match为True表示本轮日志的备份和恢复CRC一致；None表示证据不足")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, KeyboardInterrupt) as error:
        print(f"本轮未完成全部确认：{error or '用户中断'}")
        raise SystemExit(1)
