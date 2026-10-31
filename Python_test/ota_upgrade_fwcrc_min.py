"""沿用已通过实机验证的普通升级脚本，最小化测试整体 CRC 错误拒收和恢复。"""

import time
import zlib
import serial
from ota_timing import OtaTiming

from ota_pack import FW_PATH, CHUNK_SIZE, MAX_FW_SIZE, make_info, make_data
from ota_upgrade import (
    PORT, BAUDRATE, BUSINESS_ACK,
    get_version, send_wait_ack, receive_frame, write_frame,
    abort_and_wait_restore,
)

# 保留早期用例默认值；通过采集入口测试新固件时显式传入 --version。
TEST_VERSION = "2.0.6"


def wait_restored_version(uart, rx, timeout=30):
    """
    @brief 等待恢复后的 APP 主动上报版本，并回复业务 ACK。
    @param uart 已打开的协议串口。
    @param rx 跨步骤保留的接收缓冲区。
    @param timeout 最长等待秒数。
    @return 收到的主、次、修订版本号。
    @note ABORT 的 ACK 不表示恢复拷贝已完成；等待期间不发送查询打扰回滚。
    """
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        frame = receive_frame(uart, rx, deadline - time.monotonic())
        if frame is None:
            break
        if frame[:2] == b"\x66\x0A":
            raise RuntimeError(f"等待原 APP 时收到 NACK 0x{frame[2]:02X}")
        if frame[0] == 0x55 and 1 <= frame[1] <= 8:
            write_frame(uart, BUSINESS_ACK)
            if frame[1] == 0x05:
                uart.flush()
                return tuple(frame[2:5])
    raise TimeoutError(f"{timeout} 秒内未收到恢复后的 APP 版本")


def main(serial_factory=None, timing=None, version=None, expected_current=None,
         cleanup_result=None):
    """
    @brief 保持普通脚本的升级前流程，只注入整体 CRC 错误并检查恢复应答。
    @param serial_factory 可选的串口创建函数；默认直接使用 pyserial。
    @param timing 可选的阶段计时对象。
    @param version 本次候选固件版本；不提供时使用早期默认值。
    @param expected_current 可选的测试前版本字符串，不匹配时不发送升级通知。
    @param cleanup_result 可选的字典，独立保存ABORT应答和原APP版本恢复结果。
    @return 无；失败时抛出异常，由入口报告并返回非零退出码。
    @note 只打开 COM6，不打开调试口，不启动线程，不使用增强版收帧和握手。
          调试串口由串口助手记录，恢复完整性需核对备份 CRC 与恢复 CRC。
    """
    firmware = FW_PATH.read_bytes()
    cleanup_result = {} if cleanup_result is None else cleanup_result
    size = len(firmware)
    if not 1 <= size <= MAX_FW_SIZE:
        raise ValueError("固件为空或超过当前工程容量")

    real_crc = zlib.crc32(firmware)
    wrong_crc = real_crc ^ 1
    version = TEST_VERSION if version is None else version
    # 只改整体 CRC 声明，make_info 会重新计算信息帧自身的正确 CRC。
    info = make_info(size, version, wrong_crc)
    total = (size + CHUNK_SIZE - 1) // CHUNK_SIZE
    timing = OtaTiming() if timing is None else timing
    timing.configure("crc_rollback", size, BAUDRATE)
    print(f"固件：{FW_PATH}")
    print(f"大小：{size}，真实 CRC32：0x{real_crc:08X}")
    print(f"错误声明：0x{wrong_crc:08X}，信息帧版本：{version}")
    print("最小用例：查询、通知、单次握手均沿用 ota_upgrade.py")
    print("本流程只打开协议口 " + PORT + "；调试口由采集入口记录，单独运行时可用串口助手")

    # 打开参数、清空接收缓冲、查询时限及握手等待均与成功脚本一致。
    open_port = serial.Serial if serial_factory is None else serial_factory
    with open_port(PORT, BAUDRATE, timeout=0.02, write_timeout=2) as uart:
        rx = bytearray()
        uart.reset_input_buffer()
        started = False
        end_sent = False
        data_started = False
        try:
            original = get_version(uart, rx, 5)
            print("测试前上报版本：" + ".".join(map(str, original)))
            if expected_current is not None:
                if original != tuple(map(int, expected_current.split("."))):
                    raise RuntimeError(f"测试前版本不匹配：要求 {expected_current}，收到 {original}")
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

            print("[4] 发送固件信息，仅整体 CRC 声明错误")
            timing.mark("info_start")
            send_wait_ack(uart, rx, info, "固件信息", 8)
            timing.mark("info_done")

            print("[5] 逐包传输，每包数据及 CRC 均正确")
            timing.mark("data_start")
            data_started = True
            for seq in range(total):
                offset = seq * CHUNK_SIZE
                frame = make_data(seq, firmware[offset:offset + CHUNK_SIZE])
                send_wait_ack(uart, rx, frame, f"数据包 seq={seq}", 8)
                done = seq + 1
                if done % 10 == 0 or done == total:
                    print(f"进度：{done}/{total}，{done / total:.1%}")
            timing.mark("data_done")

            print("[6] 结束传输，要求整体 CRC 错误 NACK 0x05")
            end_sent = True
            timing.mark("end_start")
            send_wait_ack(uart, rx, bytes.fromhex("66 07 BB"),
                          "整体 CRC 拒收", 8, expected_nack=0x05)
            timing.mark("end_done")

            # 只有明确收到 0x05 才请求立即回滚，超时或其他应答不得走此分支。
            print("[7] CRC 错误已确认，发送 ABORT 请求立即回滚")
            if not abort_and_wait_restore(uart, rx, original, cleanup_result,
                                          timing=timing):
                raise RuntimeError("整体CRC已拒收，但ABORT或旧APP恢复未完成确认")
            timing.report()
            print("协议检查通过：整体 CRC 拒收、ABORT 应答、原 APP 版本恢复均完成")
            print("请核对 STM32 日志：备份 CRC 与恢复 CRC 一致，才确认镜像完整恢复")

        except (Exception, KeyboardInterrupt):
            if started and not end_sent:
                print("本轮失败，发送一次ABORT并继续采集恢复日志")
                abort_and_wait_restore(uart, rx, original, cleanup_result,
                                       allow_stale_data=data_started)
            elif end_sent:
                print("结束命令已发出，本轮未完成全部确认，不再自动补发 ABORT")
            else:
                print("尚未发送升级通知，本轮未进入升级流程")
            raise


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, KeyboardInterrupt) as error:
        print(f"本轮未完成全部确认：{error or '用户中断'}")
        raise SystemExit(1)
