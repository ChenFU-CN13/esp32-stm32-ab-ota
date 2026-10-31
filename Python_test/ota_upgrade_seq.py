import time
import zlib
import serial

from ota_pack import (
    FW_PATH, VERSION, CHUNK_SIZE, MAX_FW_SIZE,
    make_info, make_data,
)

PORT = "COM6"
BAUDRATE = 115200

# 包序号从零开始；等待 seq=10 时提前发送 seq=11，本次只注入一次。
INJECT_SEQ = 10

# OTA 应答与普通业务应答是两种不同的帧。
OTA_ACK = bytes.fromhex("66 02 BB")
BUSINESS_ACK = bytes.fromhex("55 7E 00 00 00 00 00 BB")


def receive_frame(uart, rx, timeout):
    """
    @brief 从累计接收的数据中取出一帧。
    @param uart 串口对象。
    @param rx 跨函数调用保留的接收缓冲区。
    @param timeout 本次最多等待的秒数。
    @return 完整帧；超时返回 None。
    """
    deadline = time.monotonic() + timeout

    while time.monotonic() < deadline:
        if rx:
            if rx[0] not in (0x55, 0x66):
                del rx[0]
                continue

            if len(rx) >= 2:
                # APP 业务帧八字节；OTA ACK 三字节，NACK 四字节。
                length = 8 if rx[0] == 0x55 else {
                    0x02: 3, 0x0A: 4
                }.get(rx[1])

                if length is None:
                    del rx[0]
                    continue

                if len(rx) >= length:
                    if rx[length - 1] != 0xBB:
                        del rx[0]
                        continue

                    frame = bytes(rx[:length])
                    del rx[:length]
                    return frame

        rx.extend(uart.read(64))

    return None


def write_frame(uart, frame):
    """
    @brief 将完整帧一次提交给串口。
    @param uart 串口对象。
    @param frame 要发送的字节数据。
    @return 无；提交长度不足时抛出异常。
    """
    if uart.write(frame) != len(frame):
        raise RuntimeError("串口没有完整提交本帧")


def send_wait_ack(uart, rx, frame, name, timeout, expected_nack=None):
    """
    @brief 发送一帧，并检查实际应答是否符合本次预期。
    @param uart 串口对象。
    @param rx 累计接收缓冲区。
    @param frame 待发送的完整帧。
    @param name 当前步骤名称，用于错误定位。
    @param timeout 本步骤的等待秒数。
    @param expected_nack None 表示要求 ACK；整数表示要求对应 NACK。
    @return 应答符合预期时返回；超时或应答不符时抛出异常。
    @note 不自动重发，错误帧后的正确重发由测试步骤明确执行。
    """
    write_frame(uart, frame)
    deadline = time.monotonic() + timeout

    while time.monotonic() < deadline:
        response = receive_frame(
            uart, rx, deadline - time.monotonic()
        )

        if response is None:
            break

        if response == OTA_ACK:
            if expected_nack is not None:
                raise RuntimeError(
                    f"{name}：应收到 NACK 0x{expected_nack:02X}，"
                    "却收到 ACK"
                )
            return

        if response[:2] == b"\x66\x0A":
            error = response[2]

            if expected_nack is not None and error == expected_nack:
                print(f"[符合预期] {name}：NACK 0x{error:02X}")
                return

            raise RuntimeError(
                f"{name}：收到非预期 NACK 0x{error:02X}"
            )

        # 升级通知前后的残留业务帧不作为 OTA 应答。

    raise TimeoutError(f"{name}：等待预期应答超时")


def get_version(uart, rx, timeout):
    """
    @brief 查询 APP 版本，并为有效业务帧回复 ACK。
    @param uart 串口对象。
    @param rx 累计接收缓冲区。
    @param timeout 最多等待的秒数。
    @return 主、次、修订版本组成的元组。
    @note 只在正常 APP 阶段调用，不在固件传输期间发送查询。
    """
    write_frame(uart, bytes.fromhex("66 09 BB"))
    deadline = time.monotonic() + timeout

    while time.monotonic() < deadline:
        frame = receive_frame(
            uart, rx, deadline - time.monotonic()
        )

        if frame is None:
            break

        if frame[0] == 0x55 and 1 <= frame[1] <= 8:
            write_frame(uart, BUSINESS_ACK)

            if frame[1] == 0x05:
                uart.flush()
                return tuple(frame[2:5])

    raise TimeoutError("没有收到 APP 版本帧")


def main():
    """
    @brief 提前发送下一包，检查错序拒收后恢复升级与版本确认。
    @return 无；失败时抛出异常，由入口打印原因。
    @note 错序包本身的长度和 CRC 正确，只在预期 NACK 后恢复正常顺序。
    """
    firmware = FW_PATH.read_bytes()
    size = len(firmware)

    if not 1 <= size <= MAX_FW_SIZE:
        raise ValueError("固件为空或超过当前工程容量")

    expected = tuple(
        int(x) for x in VERSION.removeprefix("v").split(".")
    )

    if len(expected) != 3 or any(
        not 0 <= x <= 255 for x in expected
    ):
        raise ValueError(
            "版本必须是三个零到二百五十五的整数，例如 2.0.5"
        )

    fw_crc = zlib.crc32(firmware)
    info = make_info(size, VERSION, fw_crc)
    total = (size + CHUNK_SIZE - 1) // CHUNK_SIZE

    # 在通知升级、擦除 APP 之前检查，避免没有注入异常也误报通过。
    if not 0 <= INJECT_SEQ < total - 1:
        raise ValueError("注入位置必须有效，并且后面还要有下一包可提前发送")

    print(f"固件：{FW_PATH}")
    print(
        f"大小：{size}，CRC32：0x{fw_crc:08X}，"
        f"目标版本：{VERSION}"
    )
    print(
        f"测试：等待 seq={INJECT_SEQ} 时提前发送 seq={INJECT_SEQ + 1}，"
        "预期 NACK 0x07"
    )

    with serial.Serial(
        PORT, BAUDRATE, timeout=0.02, write_timeout=2
    ) as uart:
        rx = bytearray()

        # 仅在会话开始前清理一次，传输中不清空应答。
        uart.reset_input_buffer()

        started = False
        end_sent = False

        try:
            current = get_version(uart, rx, 5)
            print("当前版本：" + ".".join(map(str, current)))

            started = True

            print("[1] 通知 APP 重启进入 Bootloader")
            send_wait_ack(
                uart, rx, bytes.fromhex("66 01 BB"),
                "升级通知", 8
            )
            time.sleep(0.5)

            print("[2] Bootloader 握手")
            send_wait_ack(
                uart, rx, bytes.fromhex("66 03 BB"),
                "握手", 1
            )

            print("[3] 等待备份与擦除")
            send_wait_ack(
                uart, rx, bytes.fromhex("66 04 BB"),
                "备份与擦除", 20
            )

            print("[4] 发送固件信息")
            send_wait_ack(uart, rx, info, "固件信息", 8)

            print("[5] 逐包传输")

            for seq in range(total):
                offset = seq * CHUNK_SIZE
                chunk = firmware[offset:offset + CHUNK_SIZE]
                frame = make_data(seq, chunk)

                if seq == INJECT_SEQ:
                    # 用下一包真实的数据和序号组帧，长度、数据 CRC 都保持正确，
                    # 让本次拒收只针对顺序错误，而不是数据损坏。
                    ahead_seq = seq + 1
                    ahead_offset = ahead_seq * CHUNK_SIZE
                    ahead_chunk = firmware[ahead_offset:ahead_offset + CHUNK_SIZE]
                    ahead_frame = make_data(ahead_seq, ahead_chunk)

                    print(
                        f"[注入] STM32 应等待 seq={seq}，"
                        f"提前发送 seq={ahead_seq}，长度和 CRC 均正确"
                    )

                    send_wait_ack(
                        uart, rx, ahead_frame,
                        f"错序包 seq={ahead_seq}", 8,
                        expected_nack=0x07
                    )
                    print(f"[恢复] 按顺序发送正确的 seq={seq}")

                # 错序包被拒收后仍发送当前包；下一轮再正常发送被提前过的下一包。
                # 只有当前正确包收到 ACK 才推进有效进度。
                send_wait_ack(
                    uart, rx, frame, f"数据包 seq={seq}", 8
                )

                if seq == INJECT_SEQ:
                    print("[符合预期] 恢复顺序后获得 ACK")

                done = seq + 1

                if done % 10 == 0 or done == total:
                    print(
                        f"进度：{done}/{total}，"
                        f"{done / total:.1%}"
                    )

            print("[6] 结束传输，等待整体 CRC 校验及元数据提交")

            # 发出结束命令后，ACK 丢失也可能已经完成提交，
            # 不能贸然发送 ABORT。
            end_sent = True

            send_wait_ack(
                uart, rx, bytes.fromhex("66 07 BB"),
                "升级结束", 8
            )

            print("Bootloader 已确认校验和提交成功，等待 APP 启动")

            time.sleep(2)
            actual = get_version(uart, rx, 15)

            print("升级后版本：" + ".".join(map(str, actual)))

            if actual != expected:
                raise RuntimeError(
                    f"版本不匹配：期望 {expected}，收到 {actual}"
                )

            print("通过：错序包拒收、顺序恢复、最终提交及 APP 版本确认均完成")

        except (Exception, KeyboardInterrupt):
            if started and not end_sent:
                print("发送 ABORT 收尾；回滚结果请查看 STM32 日志")

                try:
                    send_wait_ack(
                        uart, rx, bytes.fromhex("66 08 BB"),
                        "ABORT", 1
                    )
                    print("收到应答，不代表回滚已经完成")

                except (Exception, KeyboardInterrupt) as error:
                    print(f"ABORT 未确认：{error}")

            elif end_sent:
                print(
                    "结束命令已发出，不自动 ABORT；"
                    "请核对 STM32 日志和运行版本"
                )

            raise


if __name__ == "__main__":
    try:
        main()
    except (
        OSError, ValueError, RuntimeError, KeyboardInterrupt
    ) as error:
        print(f"本轮未完成全部确认：{error or '用户中断'}")
        raise SystemExit(1)
