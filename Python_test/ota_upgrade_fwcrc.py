"""声明错误的固件整体 CRC，检查拒绝提交、回滚日志和原 APP 恢复。"""

import argparse
from contextlib import redirect_stdout
from datetime import datetime
import json
from pathlib import Path
import re
import sys
import threading
import time
import zlib

import serial

from ota_pack import FW_PATH, VERSION, CHUNK_SIZE, MAX_FW_SIZE, make_info, make_data
from ota_upgrade_crc import (
    BUSINESS_ACK, receive_frame, write_frame, send_wait_ack, get_version,
    handshake, ProtocolSyncError,
)

PORT = "COM6"
BAUDRATE = 115200


class DebugCapture:
    """独占调试串口，只接收日志，不向 STM32 调试口发送数据。"""

    def __init__(self, port, path):
        """@brief 记录调试串口和原始日志路径，初始化时不开串口。"""
        self.port = port
        self.path = path
        self.stop_event = threading.Event()
        self.error = None
        self.version_request = threading.Event()
        self.received_bytes = 0

    def start(self):
        """@brief 打开日志串口并启动接收，失败时不开始升级。"""
        self.uart = serial.Serial(self.port, BAUDRATE, timeout=0.1)
        try:
            self.output = self.path.open("wb")
        except Exception:
            self.uart.close()
            raise
        self.thread = threading.Thread(target=self._receive, daemon=True)
        self.thread.start()

    def _receive(self):
        """@brief 后台保存原始字节，接收错误留给主流程判断。"""
        marker = b"[UART2][RX][VERSION-REQ]"
        tail = b""
        try:
            while not self.stop_event.is_set():
                data = self.uart.read(512)
                if data:
                    self.output.write(data)
                    self.output.flush()
                    self.received_bytes += len(data)
                    combined = tail + data
                    if marker in combined:
                        self.version_request.set()
                    tail = combined[-(len(marker) - 1):]
        except Exception as error:
            self.error = str(error)

    def stop(self):
        """@brief 等待接收退出，再关闭串口和日志文件。"""
        self.stop_event.set()
        self.thread.join(timeout=2)
        if self.thread.is_alive():
            self.uart.cancel_read()
            self.thread.join(timeout=2)
        self.uart.close()
        self.output.close()
        if self.thread.is_alive():
            self.error = "调试日志接收线程未能及时结束"


class SerialTrace:
    """记录协议口实际收发字节；串口由外层负责关闭，不改变线路控制设置。"""

    def __init__(self, uart, output, capture=None):
        """@brief 绑定协议口和 JSONL 文件，初始化收发计数。"""
        self.uart = uart
        self.output = output
        self.capture = capture
        self.rx_bytes = 0
        self.tx_bytes = 0
        self.stage = "版本查询"

    def _record(self, direction, data, **extra):
        """@brief 保存时间、阶段及原始 HEX，立即落盘，异常后也能检查。"""
        row = {"time": datetime.now().astimezone().isoformat(),
               "monotonic": time.monotonic(), "stage": self.stage,
               "direction": direction, "hex": data.hex(" ").upper(), **extra}
        self.output.write(json.dumps(row, ensure_ascii=False) + "\n")
        self.output.flush()

    def _check_capture(self):
        if self.capture and self.capture.error:
            raise RuntimeError(f"调试日志采集失败：{self.capture.error}")

    @property
    def in_waiting(self):
        self._check_capture()
        return self.uart.in_waiting

    def read(self, size):
        """@brief 记录串口实际返回的数据，不把解析失败误报成完全无数据。"""
        self._check_capture()
        data = self.uart.read(size)
        if data:
            self.rx_bytes += len(data)
            self._record("RX", data)
        return data

    def write(self, data):
        """@brief 记录请求发送内容及提交数量，串口异常同样留下记录。"""
        self._check_capture()
        try:
            written = self.uart.write(data)
        except Exception as error:
            self._record("TX_ERROR", data, error=str(error))
            raise
        self.tx_bytes += written or 0
        self._record("TX", data, written=written)
        return written

    def flush(self):
        self.uart.flush()


class ConsoleLog:
    """让同一份测试输出同时显示在终端并保存到文件。"""

    def __init__(self, terminal, output):
        """@brief 保存输出目标，不修改终端设置。"""
        self.terminal = terminal
        self.output = output

    def write(self, text):
        """@brief 同步写入终端和 UTF-8 日志。"""
        self.terminal.write(text)
        self.output.write(text)
        self.flush()

    def flush(self):
        """@brief 及时保存输出，异常退出也能保留前面的过程。"""
        self.terminal.flush()
        self.output.flush()


def wait_app_version(uart, rx, timeout=30):
    """
    @brief 被动等待 APP 启动上报版本，并回复普通业务 ACK。
    @param uart 协议串口。
    @param rx 累计接收缓冲区。
    @param timeout 最多等待秒数。
    @return APP 上报的三个版本数字。
    @note 回滚拷贝期间不发版本查询，避免命令积压；APP 启动会主动报版本。
    """
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        frame = receive_frame(uart, rx, deadline - time.monotonic())
        if frame is None:
            break
        if frame[0] == 0x55 and 1 <= frame[1] <= 8:
            write_frame(uart, BUSINESS_ACK)
            if frame[1] == 0x05:
                uart.flush()
                return tuple(frame[2:5])
        elif frame[:2] == b"\x66\x0A":
            raise RuntimeError(f"等待 APP 时收到 NACK 0x{frame[2]:02X}")
    raise TimeoutError("三十秒内未确认原 APP 启动版本")


def run_protocol(port, result, trace_output, capture=None):
    """
    @brief 只改声明的整体 CRC，逐包传输后检查整体拒收和原 APP 恢复。
    @param port 协议串口号。
    @param result 保存关键结果的字典，即使失败也保留已完成步骤。
    @param trace_output 协议原始收发记录文件。
    @param capture 调试采集对象；提供时必须观察到本轮查询的接收日志。
    @return 无；任一步骤不符时抛出异常。
    @note 信息帧自身 CRC 和全部数据帧 CRC 保持正确。结束命令只有明确收到
          NACK 0x05 才走预期 ABORT 路径；超时或错误 ACK 不得当作拒收成功。
    """
    firmware = FW_PATH.read_bytes()
    size = len(firmware)
    if not 1 <= size <= MAX_FW_SIZE:
        raise ValueError("固件为空或超过当前工程容量")
    real_crc = zlib.crc32(firmware)
    wrong_crc = real_crc ^ 1
    info = make_info(size, VERSION, wrong_crc)
    total = (size + CHUNK_SIZE - 1) // CHUNK_SIZE
    result.update(size=size, real_crc=f"0x{real_crc:08X}",
                  declared_crc=f"0x{wrong_crc:08X}", total_packets=total)
    print(f"固件：{FW_PATH}")
    print(f"真实整体 CRC：0x{real_crc:08X}，故意声明：0x{wrong_crc:08X}")
    print("信息帧自身和全部数据包的 CRC 均正确，结束时预期 NACK 0x05")

    with serial.Serial(port, BAUDRATE, timeout=0.02, write_timeout=2) as port_uart:
        uart = SerialTrace(port_uart, trace_output, capture)
        rx = bytearray()
        # 不盲目清空串口。旧应答也保存到 protocol.jsonl，供失败后判断状态。
        started = False
        end_sent = False
        try:
            if capture:
                capture.version_request.clear()
            original = get_version(uart, rx, 5)
            result["original_version"] = list(original)
            print("收到 APP 版本：" + ".".join(map(str, original)))
            if capture:
                if not capture.version_request.wait(timeout=1):
                    uart._check_capture()
                    raise RuntimeError("收到版本上报，但调试口未观察到本轮 VERSION-REQ；尚不能确认下行查询，停止升级")
                result["query_seen_in_debug"] = True
                print("[确认] 调试口记录了 APP 收到版本查询")
            else:
                print("[提示] 未采集调试口，版本上报不能单独证明下行查询已到达")
            started = True
            uart.stage = "升级通知"
            print("[1] 通知 APP 进入 Bootloader")
            send_wait_ack(uart, rx, bytes.fromhex("66 01 BB"), "升级通知", 8)
            time.sleep(0.5)
            uart.stage = "Bootloader 握手"
            print("[2] Bootloader 握手")
            handshake(uart, rx)
            uart.stage = "备份与擦除"
            print("[3] 备份与擦除")
            send_wait_ack(uart, rx, bytes.fromhex("66 04 BB"), "备份与擦除", 20)
            uart.stage = "固件信息"
            print("[4] 发送整体 CRC 声明错误的信息帧")
            send_wait_ack(uart, rx, info, "固件信息", 8)
            print("[5] 正常发送全部数据包")
            for seq in range(total):
                uart.stage = f"数据包 seq={seq}"
                offset = seq * CHUNK_SIZE
                frame = make_data(seq, firmware[offset:offset + CHUNK_SIZE])
                send_wait_ack(uart, rx, frame, f"数据包 seq={seq}", 8)
                result["acked_packets"] = seq + 1
                if (seq + 1) % 10 == 0 or seq + 1 == total:
                    print(f"进度：{seq + 1}/{total}，{(seq + 1) / total:.1%}")

            print("[6] 发送结束命令，要求 NACK 0x05")
            uart.stage = "整体 CRC 检验"
            end_sent = True
            send_wait_ack(uart, rx, bytes.fromhex("66 07 BB"),
                          "整体 CRC 错误拒收", 8, expected_nack=0x05)
            result["firmware_crc_rejected"] = True

            # 该分支只安排约两秒后回滚，尽快 ABORT，不等待超时用例的 NACK 0x08。
            print("[7] 发送 ABORT，请求立即回滚")
            uart.stage = "ABORT 回滚"
            send_wait_ack(uart, rx, bytes.fromhex("66 08 BB"), "ABORT", 2)
            result["abort_ack"] = True
            print("[符合预期] ABORT 获得 ACK；继续等待实际恢复完成")

            print("[8] 被动等待原 APP 启动")
            uart.stage = "等待原 APP"
            actual = wait_app_version(uart, rx, 30)
            result["restored_version"] = list(actual)
            print("恢复后版本：" + ".".join(map(str, actual)))
            if actual != original:
                raise RuntimeError(f"原版本 {original}，恢复后版本却为 {actual}")
            result["protocol_pass"] = True
            print("[协议通过] 整体 CRC 被拒收，ABORT 已确认，原 APP 版本恢复")
        except (Exception, KeyboardInterrupt) as failure:
            result["failed_stage"] = uart.stage
            if isinstance(failure, ProtocolSyncError):
                print("发现旧应答或残帧，停止自动发送，保留现场供核对")
            elif not started:
                print("尚未发送升级通知，本轮未进入升级流程")
            elif not end_sent:
                uart.stage = "清理 ABORT"
                print("结束命令尚未发出，尝试 ABORT 清理未完成会话")
                try:
                    send_wait_ack(uart, rx, bytes.fromhex("66 08 BB"), "清理 ABORT", 2)
                except (Exception, KeyboardInterrupt) as error:
                    print(f"清理未确认：{error}")
            else:
                print("本轮结果未确认，不盲目重复结束命令或 ABORT；请保留 STM32 日志")
            raise
        finally:
            result["protocol_rx_bytes"] = uart.rx_bytes
            result["protocol_tx_bytes"] = uart.tx_bytes
            print(f"[串口统计] RX={uart.rx_bytes} 字节，TX={uart.tx_bytes} 字节；原文见 protocol.jsonl")


def check_debug_evidence(raw, real_crc, declared_crc):
    """
    @brief 核对调试日志中的校验失败、备份和回滚 CRC。
    @param raw 调试口收到的原始字节。
    @param real_crc 本次发送文件的真实 CRC 文本。
    @param declared_crc 故意声明的错误 CRC 文本。
    @return 解码后的日志和证据字典；证据不足时抛出异常。
    @note 先查备份，再查校验失败、恢复开始、恢复成功，不能只凭版本相同判回滚。
    """
    candidates = [raw.decode(encoding, errors="replace") for encoding in ("utf-8", "gb18030")]
    markers = ("A区备份完成", "计算CRC32", "期望CRC32", "开始恢复旧固件", "恢复成功")
    text = max(candidates, key=lambda value: sum(value.count(word) for word in markers))
    pattern = re.compile(
        r"A区备份完成，CRC32=0x([0-9a-fA-F]{8}).*?"
        r"计算CRC32:\s*(0x[0-9a-fA-F]{8}).*?"
        r"期望CRC32:\s*(0x[0-9a-fA-F]{8}).*?"
        r"开始恢复旧固件.*?恢复成功，CRC32=0x([0-9a-fA-F]{8})",
        re.S,
    )
    match = pattern.search(text)
    if not match:
        raise RuntimeError("调试日志缺少按顺序出现的备份、整体 CRC 比较或恢复成功证据")
    backup, calculated, expected, restored = match.groups()
    if calculated.lower() != real_crc.lower() or expected.lower() != declared_crc.lower():
        raise RuntimeError("调试日志的计算 CRC 或声明 CRC 与本轮测试不一致")
    if int(backup, 16) != int(restored, 16):
        raise RuntimeError("回滚后 CRC 与擦除前的备份 CRC 不一致")
    return text, {"backup_crc": f"0x{backup.upper()}",
                  "restored_crc": f"0x{restored.upper()}", "debug_pass": True}


def main():
    """@brief 执行测试并保存控制台、调试原文和结构化结果。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default=PORT, help="协议串口，默认 COM6")
    parser.add_argument("--debug-port", help="可选调试串口，例如 COM26，需先释放占用")
    args = parser.parse_args()
    if args.debug_port and args.debug_port.upper() == args.port.upper():
        raise ValueError("协议口和调试口不能使用同一个串口")

    folder = Path(__file__).resolve().parent / "runs" / datetime.now().strftime("fwcrc_%Y%m%d_%H%M%S_%f")
    folder.mkdir(parents=True)
    result = {"test": "firmware_crc_rollback", "started_at": datetime.now().astimezone().isoformat(),
              "port": args.port, "debug_port": args.debug_port, "status": "failed"}
    capture = None
    success = False
    with (folder / "console.log").open("w", encoding="utf-8") as console:
        with redirect_stdout(ConsoleLog(sys.stdout, console)):
            print(f"本轮记录目录：{folder}")
            try:
                if args.debug_port:
                    capture = DebugCapture(args.debug_port, folder / "stm32.raw")
                    capture.start()
                    print(f"已开始记录 {args.debug_port}，随后才启动协议传输")
                try:
                    with (folder / "protocol.jsonl").open("w", encoding="utf-8") as trace:
                        run_protocol(args.port, result, trace, capture)
                finally:
                    if capture and hasattr(capture, "thread"):
                        time.sleep(0.3)
                        capture.stop()

                if capture:
                    if capture.error:
                        raise RuntimeError(f"调试日志采集失败：{capture.error}")
                    raw = (folder / "stm32.raw").read_bytes()
                    text, evidence = check_debug_evidence(raw, result["real_crc"], result["declared_crc"])
                    (folder / "stm32.log").write_text(text, encoding="utf-8")
                    result.update(evidence)
                    result["status"] = "passed"
                    print("通过：整体 CRC 错误拒收、回滚 CRC 校验及原 APP 版本恢复均完成")
                else:
                    result["status"] = "protocol_pass_debug_unverified"
                    print("协议检查通过；没有采集调试口，回滚 CRC 仍需人工核对")
                success = True
            except (Exception, KeyboardInterrupt) as error:
                result["error"] = str(error) or "用户中断"
                print(f"本轮未完成全部确认：{result['error']}")
            finally:
                # 协议失败时也保存采集异常，不能只在 run_protocol 成功后检查。
                if capture:
                    result["debug_rx_bytes"] = capture.received_bytes
                    if capture.error:
                        result["debug_error"] = capture.error
                        print(f"[调试采集异常] {capture.error}")
                # 即使证据检查失败，也保留解码文本，便于定位格式或缺失内容。
                raw_path = folder / "stm32.raw"
                if raw_path.exists() and not (folder / "stm32.log").exists():
                    (folder / "stm32.log").write_text(raw_path.read_bytes().decode("utf-8", errors="replace"), encoding="utf-8")
                result["finished_at"] = datetime.now().astimezone().isoformat()
                (folder / "result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
                print(f"结果：{folder / 'result.json'}")
    return 0 if success else 1


if __name__ == "__main__":
    raise SystemExit(main())
