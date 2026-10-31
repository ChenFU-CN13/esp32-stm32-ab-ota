"""传输半份固件后人工拔整板USB，再记录重新上电的旧镜像恢复。"""

import argparse
import hashlib
import json
import time
import zlib
from contextlib import ExitStack
from datetime import datetime
from pathlib import Path

import serial
from serial.tools import list_ports

from ota_pack import FW_PATH, CHUNK_SIZE, MAX_FW_SIZE, make_info, make_data
from ota_upgrade import (
    PORT, BAUDRATE, BUSINESS_ACK, get_version, receive_frame,
    send_wait_ack, write_frame,
)
from ota_upgrade_fwcrc_capture import (
    DebugReader, open_debug_port, read_recovery_crc, version_value,
)
from ota_upgrade_fwcrc_trace import TraceSerial


def ports_present():
    """返回当前枚举出的串口名，不打开设备。"""
    return {item.device.upper() for item in list_ports.comports()}


def wait_usb_removal(port, timeout=8):
    """限时等待板载调试口消失，避免把数据阶段超时当作断电恢复。"""
    started = time.monotonic()
    while time.monotonic() - started < timeout:
        if port.upper() not in ports_present():
            return time.monotonic() - started
        time.sleep(0.05)
    raise TimeoutError("8秒内未检测到调试口消失，本轮不能算断电测试")


def wait_ports(ports, timeout=10):
    """等待重新插入USB后的串口枚举完成。"""
    deadline = time.monotonic() + timeout
    required = {port.upper() for port in ports}
    while time.monotonic() < deadline:
        if required <= ports_present():
            return
        time.sleep(0.1)
    raise TimeoutError("重新上电后串口未就绪，请核对端口号及USB连接")


def wait_app(uart, original, timeout=60):
    """只接收并确认APP业务帧，不在启动恢复期间发送升级命令或查询。"""
    rx = bytearray()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        frame = receive_frame(uart, rx, deadline - time.monotonic())
        if frame is None:
            break
        if frame[0] == 0x55 and 1 <= frame[1] <= 8:
            write_frame(uart, BUSINESS_ACK)
            if frame[1] == 5:
                uart.flush()
                actual = tuple(frame[2:5])
                if actual != original:
                    raise RuntimeError(f"恢复版本错误：原版本{original}，收到{actual}")
                return actual
    raise TimeoutError("未确认旧APP版本；检查RESET是否已松开及启动恢复日志")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--debug-port", default="COM26")
    parser.add_argument("--version", required=True, type=version_value)
    parser.add_argument("--expect-current-version", required=True, type=version_value)
    args = parser.parse_args()
    if args.debug_port.upper() == PORT.upper():
        parser.error("协议口和调试口不能相同")

    firmware = FW_PATH.read_bytes()
    total = (len(firmware) + CHUNK_SIZE - 1) // CHUNK_SIZE
    if not 1 <= len(firmware) <= MAX_FW_SIZE or total < 2:
        parser.error("固件大小不合法，或不足两个数据包")
    stop_after = total // 2
    fw_crc = zlib.crc32(firmware)
    info = make_info(len(firmware), args.version, fw_crc)
    folder = Path(__file__).resolve().parent / "runs" / (
        "powerloss_capture_" + datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    )
    folder.mkdir(parents=True, exist_ok=False)
    (folder / "candidate.bin").write_bytes(firmware)
    result = {
        "test_mode": "powerloss_recovery", "test_pass": False,
        "candidate_version": args.version,
        "expected_current_version": args.expect_current_version,
        "firmware_bytes": len(firmware), "firmware_crc": f"{fw_crc:08X}",
        "firmware_sha256": hashlib.sha256(firmware).hexdigest(),
        "stop_after_packets": stop_after, "packets_confirmed": 0,
        "usb_removed": False, "power_off_confirmed_by_user": False,
        "reset_held_confirmed_by_user": False, "startup_recovery_seen": False,
        "restored_version": None, "debug_errors": [],
    }
    readers = []
    print(f"本轮记录目录：{folder}")
    print(f"固件：{len(firmware)}字节，CRC32=0x{fw_crc:08X}")
    print(f"确认{stop_after}/{total}包后提示拔USB；从提示起8秒内操作。")
    print("先确认拔USB确实能让整板断电，其他USB、下载器或串口模块没有继续供电。")
    print("重新插USB前按住RESET，等调试口重开后再松开；不要按BOOT键。")
    print("本脚本不发送结束命令或ABORT，不自动重新升级。")
    try:
        input("准备好后按回车开始；看到拔USB提示再断电：")
        with ExitStack() as files:
            output = files.enter_context((folder / "protocol.jsonl").open("w", encoding="utf-8"))
            raw = files.enter_context((folder / "stm32.bin").open("wb"))
            text = files.enter_context((folder / "stm32.log").open("w", encoding="utf-8"))
            with ExitStack() as before:
                debug = before.enter_context(open_debug_port(args.debug_port))
                reader = before.enter_context(DebugReader(debug, raw, text))
                readers.append(reader)
                uart = before.enter_context(TraceSerial(
                    serial.Serial(PORT, BAUDRATE, timeout=0.02, write_timeout=2), output))
                uart.reset_input_buffer()
                rx = bytearray()
                original = get_version(uart, rx, 5)
                result["original_version"] = list(original)
                expected = tuple(map(int, args.expect_current_version.split(".")))
                if original != expected:
                    raise RuntimeError(f"测试前版本不匹配：要求{expected}，收到{original}")
                print("[1] 通知APP进入Bootloader")
                send_wait_ack(uart, rx, bytes.fromhex("66 01 BB"), "升级通知", 8)
                time.sleep(0.5)
                send_wait_ack(uart, rx, bytes.fromhex("66 03 BB"), "握手", 1)
                print("[2] 等待备份与擦除")
                send_wait_ack(uart, rx, bytes.fromhex("66 04 BB"), "备份与擦除", 20)
                send_wait_ack(uart, rx, info, "固件信息", 8)
                print("[3] 传输前半份固件")
                for seq in range(stop_after):
                    offset = seq * CHUNK_SIZE
                    send_wait_ack(uart, rx, make_data(seq, firmware[offset:offset + CHUNK_SIZE]),
                                  f"数据包seq={seq}", 8)
                    result["packets_confirmed"] = seq + 1
                    if (seq + 1) % 10 == 0:
                        print(f"进度：{seq + 1}/{total}")
                uart.record("POWER_CUT_READY", packets_confirmed=stop_after)
                print(f"\n[现在拔USB] 已确认{stop_after}/{total}包，停止发送！请在8秒内拔整板USB。", flush=True)
                result["usb_removal_wait_s"] = round(wait_usb_removal(args.debug_port), 3)
                result["usb_removed"] = True
                uart.record("DEBUG_USB_REMOVED", wait_s=result["usb_removal_wait_s"])
            # 旧串口及读取线程已经关闭，重新上电后创建新的串口对象。
            if input("已检测到USB消失。确认整板已断电、供电灯熄灭后输入 OFF 并回车：").strip().upper() != "OFF":
                raise RuntimeError("未确认整板断电")
            result["power_off_confirmed_by_user"] = True
            if input("按住RESET重新插USB，保持RESET按住，串口出现后输入 READY 并回车：").strip().upper() != "READY":
                raise RuntimeError("未确认上电期间按住RESET")
            result["reset_held_confirmed_by_user"] = True
            wait_ports((args.debug_port, PORT))
            # 分隔标记用于只在重新打开调试口后的日志中查启动恢复证据。
            text.write("\n--- POWER_ON_CAPTURE ---\n")
            text.flush()
            with ExitStack() as after:
                debug = after.enter_context(open_debug_port(args.debug_port))
                reader = after.enter_context(DebugReader(debug, raw, text))
                readers.append(reader)
                uart = after.enter_context(TraceSerial(
                    serial.Serial(PORT, BAUDRATE, timeout=0.02, write_timeout=2), output))
                uart.record("RESET_RELEASE_REQUESTED")
                print("[现在松开RESET] 调试口和协议口已重开，松开RESET；等待旧APP恢复，最长60秒。", flush=True)
                result["restored_version"] = list(wait_app(uart, original))
                # 留出少量时间，让调试线程收齐版本确认附近的输出。
                time.sleep(0.3)
        log = (folder / "stm32.log").read_text(encoding="utf-8", errors="replace")
        startup = log.split("--- POWER_ON_CAPTURE ---\n", 1)[-1]
        result["startup_recovery_seen"] = "检测到未完成升级，进入启动恢复" in startup
        result["recovery_crc"] = read_recovery_crc(folder / "stm32.log")
        if not result["startup_recovery_seen"]:
            raise RuntimeError("缺少重新上电后检测到未完成升级的日志，不能判通过")
        if result["recovery_crc"]["match"] is not True:
            raise RuntimeError("备份与恢复CRC缺失或不一致，不能判通过")
        if readers[-1].error:
            raise RuntimeError(f"重新上电后的调试采集失败：{readers[-1].error}")
        result["test_pass"] = True
        print("通过：USB断开、人工确认断电、启动恢复、CRC一致及旧APP版本确认均完成。")
    except (Exception, KeyboardInterrupt) as error:
        result["error"] = str(error) or "用户中断"
        print("未通过。本脚本没有自动ABORT；请先核对设备状态，不要直接重复拔插。")
        raise
    finally:
        result["debug_bytes"] = sum(reader.count for reader in readers)
        result["debug_errors"] = [reader.error for reader in readers if reader.error]
        result["recovery_crc"] = read_recovery_crc(folder / "stm32.log")
        (folder / "result.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
        print(f"恢复CRC证据：{result['recovery_crc']}")
        print(f"记录已保存：{folder}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, KeyboardInterrupt, EOFError) as error:
        print(f"本轮未完成全部确认：{error or '用户中断'}")
        raise SystemExit(1)
