"""数据阶段停止发送，检查 Bootloader 超时恢复与旧会话收尾。"""

import time
import zlib
import serial
from ota_timing import OtaTiming

from ota_pack import (
    FW_PATH, VERSION, CHUNK_SIZE, MAX_FW_SIZE,
    make_info, make_data,
)
from ota_upgrade import (
    PORT, BAUDRATE, receive_frame, write_frame, send_wait_ack, get_version,
    abort_and_wait_restore,
)

# 确认前十包后停止，必须留下未发送数据，才能停在 WAIT_DATA。
STOP_AFTER_PACKETS = 10

# 与当前 ota_proto.h 的 OTA_WAIT_DATA_TIMEOUT_MS=15000 对应。
# 若固件修改该时限，需要同步调整；静默期间不发查询、ABORT 或数据。
DATA_TIMEOUT_SECONDS = 15
SILENT_SECONDS = DATA_TIMEOUT_SECONDS + 1
RECOVERY_WAIT_SECONDS = 25


def wait_recovery_notice(uart, rx, timeout, probe):
    """
    @brief 静默结束后，探测 Bootloader 是否已经恢复旧镜像。
    @param uart 串口对象。
    @param rx 跨调用保留的接收缓冲区。
    @param timeout 等待恢复通知的总秒数。
    @param probe 本轮已确认的信息帧，用于数据阶段停止发送后的探测。
    @return 收到 NACK 0x08 时返回；应答异常或超时时抛出异常。
    @note 前10包已确认，此时处于WAIT_DATA。重复信息帧只会得到状态错误，
          不写固件；恢复通知挂起后应回复NACK 0x08。
          当前取帧函数不接收Bootloader阶段的0x09查询，不能用它探测。
          不把“没有应答”或“仅看到 APP 版本”作为超时回滚已验证。
    """
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        write_frame(uart, probe)
        # 回滚期间主循环可能暂不处理串口；等这一条请求应答，不重复堆积探测帧。
        frame = receive_frame(uart, rx, deadline - time.monotonic())
        if frame is None:
            break
        if frame[:2] == b"\x66\x0A":
            error = frame[2]
            if error == 0x08:
                print("[符合预期] 收到 NACK 0x08：旧镜像恢复通知已挂起")
                return
            if error == 0x02:
                print(f"[等待] NACK 0x{error:02X}，尚未观察到恢复通知")
                remaining = deadline - time.monotonic()
                if remaining > 0:
                    time.sleep(min(0.5, remaining))
                continue
            raise RuntimeError(f"恢复探测收到非预期 NACK 0x{error:02X}")
        if frame[0] == 0x55:
            raise RuntimeError("已看到APP业务帧，但未观察到恢复通知，不能判定本用例通过")
        raise RuntimeError(f"恢复探测收到非预期应答：{frame.hex(' ').upper()}")

    raise TimeoutError("未收到 NACK 0x08，请查看 STM32 超时和回滚日志")


def main(serial_factory=None, timing=None, version=None, expected_current=None,
         cleanup_result=None, test_result=None):
    """
    @brief 发送部分固件后静默，确认恢复通知、收尾和原 APP 版本。
    @param serial_factory 可选的协议串口创建函数，用于统一采集原始收发。
    @param timing 可选的阶段计时对象。
    @param version 固件信息帧的版本字段，采集入口应显式指定。
    @param expected_current 可选的测试前APP版本，不匹配时不通知升级。
    @param cleanup_result 独立保存ABORT和恢复版本结果的字典。
    @param test_result 保存已确认包数、静默时间和恢复通知结果的字典。
    @return 无；任一步骤失败时抛出异常。
    @note 不发送升级结束命令，不以重新启动新固件为目标。
          版本相同不能单独证明镜像恢复，必须同时取得恢复通知和收尾 ACK；
          原始 CRC 及超时原因仍应结合 STM32 调试日志核对。
    """
    firmware = FW_PATH.read_bytes()
    cleanup_result = {} if cleanup_result is None else cleanup_result
    test_result = {} if test_result is None else test_result
    test_result.update(packets_confirmed=0, silence_s=SILENT_SECONDS,
                       silence_actual_s=None, recovery_notice=False)
    size = len(firmware)
    if not 1 <= size <= MAX_FW_SIZE:
        raise ValueError("固件为空或超过当前工程容量")

    total = (size + CHUNK_SIZE - 1) // CHUNK_SIZE
    if not 1 <= STOP_AFTER_PACKETS < total:
        raise ValueError("停止位置必须至少确认一包，并且不能收满整个固件")
    if DATA_TIMEOUT_SECONDS <= 0 or SILENT_SECONDS <= DATA_TIMEOUT_SECONDS:
        raise ValueError("静默时间必须大于固件配置的数据阶段超时")
    if RECOVERY_WAIT_SECONDS <= 0:
        raise ValueError("恢复通知的等待时间必须大于零")

    fw_crc = zlib.crc32(firmware)
    info = make_info(size, VERSION if version is None else version, fw_crc)
    timing = OtaTiming() if timing is None else timing
    timing.configure("timeout_recovery", size, BAUDRATE)
    print(f"固件：{FW_PATH}")
    print(f"大小：{size}，CRC32：0x{fw_crc:08X}")
    print(f"测试：确认前 {STOP_AFTER_PACKETS}/{total} 包后静默 {SILENT_SECONDS} 秒")

    open_port = serial.Serial if serial_factory is None else serial_factory
    with open_port(PORT, BAUDRATE, timeout=0.02, write_timeout=2) as uart:
        rx = bytearray()
        uart.reset_input_buffer()
        started = False
        data_started = False

        try:
            original_version = get_version(uart, rx, 5)
            print("测试前 APP 版本：" + ".".join(map(str, original_version)))
            if expected_current is not None and original_version != tuple(
                    map(int, expected_current.split("."))):
                raise RuntimeError(f"测试前版本不匹配：要求{expected_current}，收到{original_version}")

            started = True
            print("[1] 通知 APP 重启进入 Bootloader")
            timing.mark("notify_start")
            send_wait_ack(uart, rx, bytes.fromhex("66 01 BB"), "升级通知", 8)
            timing.mark("notify_done")
            time.sleep(0.5)

            print("[2] Bootloader 握手")
            timing.mark("handshake_start")
            send_wait_ack(uart, rx, bytes.fromhex("66 03 BB"), "握手", 1)
            timing.mark("handshake_done")

            print("[3] 等待备份与擦除")
            timing.mark("prepare_start")
            send_wait_ack(uart, rx, bytes.fromhex("66 04 BB"), "备份与擦除", 20)
            timing.mark("prepare_done")

            print("[4] 发送固件信息及部分数据")
            timing.mark("info_start")
            send_wait_ack(uart, rx, info, "固件信息", 8)
            timing.mark("info_done")
            data_started = True
            for seq in range(STOP_AFTER_PACKETS):
                offset = seq * CHUNK_SIZE
                chunk = firmware[offset:offset + CHUNK_SIZE]
                send_wait_ack(uart, rx, make_data(seq, chunk), f"数据包 seq={seq}", 8)
                test_result["packets_confirmed"] = seq + 1

            print(f"[停止] 已确认 {STOP_AFTER_PACKETS}/{total} 包，剩余固件不再发送")
            print(f"[5] 静默 {SILENT_SECONDS} 秒，不发送任何协议帧")
            stopped_at = time.monotonic()
            timing.mark("silence_start")
            if hasattr(uart, "record"):
                uart.record("DATA_SILENCE", after_packets=STOP_AFTER_PACKETS,
                            duration_s=SILENT_SECONDS)
            time.sleep(SILENT_SECONDS)
            test_result["silence_actual_s"] = time.monotonic() - stopped_at
            if hasattr(uart, "record"):
                uart.record("DATA_SILENCE_END", actual_s=test_result["silence_actual_s"])
            print(f"[等待结束] 已静默 {test_result['silence_actual_s']:.1f} 秒")

            print(f"[6] 等待恢复通知，最长 {RECOVERY_WAIT_SECONDS} 秒")
            wait_recovery_notice(uart, rx, RECOVERY_WAIT_SECONDS, info)
            test_result["recovery_notice"] = True
            timing.mark("recovery_notice")

            print("[7] 发送 ABORT，结束旧会话")
            if not abort_and_wait_restore(uart, rx, original_version, cleanup_result,
                                          timing=timing, allow_recovery_notice=True):
                raise RuntimeError("已收到恢复通知，但ABORT或原APP版本未完成确认")

            timing.report()
            print("通过：数据阶段超时恢复通知、ABORT 收尾及原 APP 版本确认均完成")
            print("请同时核对 STM32 的传输超时、恢复成功及恢复 CRC 日志")

        except (Exception, KeyboardInterrupt):
            if started and not cleanup_result.get("attempted"):
                print("本轮未通过，尝试 ABORT 收尾；收尾成功也不改变本轮失败结果")
                abort_and_wait_restore(uart, rx, original_version, cleanup_result,
                                       allow_stale_data=data_started,
                                       allow_recovery_notice=test_result["recovery_notice"])
            raise


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, KeyboardInterrupt) as error:
        print(f"本轮未完成全部确认：{error or '用户中断'}")
        raise SystemExit(1)
