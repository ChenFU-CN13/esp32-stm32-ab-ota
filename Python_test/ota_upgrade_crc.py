import time
import zlib
import serial

from ota_pack import (
    FW_PATH, VERSION, CHUNK_SIZE, MAX_FW_SIZE,
    make_info, make_data,
)

PORT = "COM6"
BAUDRATE = 115200

# 从零开始计数，seq=10 对应第十一包；本次只破坏这一包一次。
INJECT_SEQ = 10

# OTA 应答与普通业务应答是两种不同的帧。
OTA_ACK = bytes.fromhex("66 02 BB")
BUSINESS_ACK = bytes.fromhex("55 7E 00 00 00 00 00 BB")


class ProtocolSyncError(RuntimeError):
    """发送新命令前发现旧应答，不能继续猜测应答属于哪一步。"""


def _take_frame(rx):
    """@brief 从缓冲区取完整帧，丢弃非法帧头和类型，保留未收完的帧。"""
    while rx:
        if rx[0] not in (0x55, 0x66):
            del rx[0]
            continue
        if len(rx) < 2:
            return None
        if rx[0] == 0x55:
            # STM32 上行仅有 0x01～0x08，不能让孤立 55 堵住后面的 OTA ACK。
            length = 8 if 1 <= rx[1] <= 8 else None
        else:
            length = {0x02: 3, 0x0A: 4}.get(rx[1])
        if length is None:
            del rx[0]
            continue
        if len(rx) < length:
            return None
        if rx[length - 1] != 0xBB:
            del rx[0]
            continue
        frame = bytes(rx[:length])
        del rx[:length]
        return frame
    return None


def receive_frame(uart, rx, timeout):
    """
    @brief 从累计接收的数据中取出一帧。
    @param uart 串口对象。
    @param rx 跨函数调用保留的接收缓冲区。
    @param timeout 本次最多等待的秒数。
    @return 完整帧；超时返回 None。
    """
    deadline = time.monotonic() + timeout
    last_byte = time.monotonic()
    while True:
        # 先解析最后一次 read 的结果，再判断超时，避免完整应答留在缓冲区。
        frame = _take_frame(rx)
        if frame is not None:
            return frame
        now = time.monotonic()
        if rx and now - last_byte >= 0.1:
            # 这是主机重同步策略。短应答断流 100 ms 后不再无限等待残帧。
            print(f"[重同步] 丢弃不完整帧前缀：{rx[0]:02X}")
            del rx[0]
            continue
        if now >= deadline:
            return None
        data = uart.read(64)
        if data:
            rx.extend(data)
            last_byte = time.monotonic()


def check_pending(uart, rx, name):
    """@brief 发新命令前检查已到达的旧应答，禁止把旧 ACK 算给下一阶段。"""
    waiting = uart.in_waiting
    if waiting:
        rx.extend(uart.read(waiting))
    deadline = time.monotonic() + 0.15
    while True:
        frame = _take_frame(rx)
        if frame is None and rx and time.monotonic() < deadline:
            # 串口可能刚到半帧业务数据，先短暂等它收完，不能立即误报旧会话。
            frame = receive_frame(uart, rx, deadline - time.monotonic())
        if frame is None:
            break
        if frame[0] == 0x66:
            raise ProtocolSyncError(f"{name}：发送前已有旧应答 {frame.hex(' ').upper()}，停止本轮")
        print(f"[旧业务帧] {frame.hex(' ').upper()}")
    if rx:
        raise ProtocolSyncError(f"{name}：发送前存在残帧 {rx.hex(' ').upper()}，停止本轮")


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
    check_pending(uart, rx, name)
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

    raise TimeoutError(f"{name}：等待预期应答超时，未完成帧={rx.hex(' ').upper() or '无'}")


def get_version(uart, rx, timeout):
    """
    @brief 查询 APP 版本，并为有效业务帧回复 ACK。
    @param uart 串口对象。
    @param rx 累计接收缓冲区。
    @param timeout 最多等待的秒数。
    @return 主、次、修订版本组成的元组。
    @note 返回值仅证明收到了版本上报；下行查询是否到达需结合调试口 VERSION-REQ。
    """
    write_frame(uart, bytes.fromhex("66 09 BB"))
    deadline = time.monotonic() + timeout

    while time.monotonic() < deadline:
        frame = receive_frame(
            uart, rx, deadline - time.monotonic()
        )

        if frame is None:
            break

        if frame[:2] == b"\x66\x0A":
            raise RuntimeError(f"版本查询收到 NACK 0x{frame[2]:02X}，设备不接受当前查询；请核对启动状态")
        if frame == OTA_ACK:
            raise ProtocolSyncError("版本查询收到 OTA ACK，可能存在旧会话，停止本轮")
        if frame[0] == 0x55 and 1 <= frame[1] <= 8:
            if frame[1] == 0x05 and frame[5:7] != b"\x00\x00":
                raise RuntimeError(f"版本帧保留字段异常：{frame.hex(' ').upper()}")
            write_frame(uart, BUSINESS_ACK)

            if frame[1] == 0x05:
                uart.flush()
                return tuple(frame[2:5])

    raise TimeoutError("没有收到 APP 版本帧")


def handshake(uart, rx, timeout=3.5, interval=0.5):
    """
    @brief 在同一个握手阶段有限重发 0x03，防止命令早于 Bootloader 初始化而丢失。
    @param timeout 首次发送后的总等待时间，秒。
    @param interval 两次尝试的间隔，秒。
    @note 当前 Bootloader 在 WAIT_START 忽略重复 0x03；ACK 丢失仍可能失败。
          不重发升级通知，不把超时当成功，不跨阶段复用 ACK。
    """
    check_pending(uart, rx, "Bootloader 握手")
    deadline = time.monotonic() + timeout
    next_send = 0
    attempts = 0
    while time.monotonic() < deadline:
        now = time.monotonic()
        if now >= next_send and not rx:
            attempts += 1
            print(f"[握手] 第 {attempts} 次发送 66 03 BB")
            write_frame(uart, bytes.fromhex("66 03 BB"))
            next_send = now + interval
        response = receive_frame(uart, rx, min(interval, deadline - time.monotonic()))
        if response == OTA_ACK:
            return
        if response and response[:2] == b"\x66\x0A":
            raise RuntimeError(f"Bootloader 握手：NACK 0x{response[2]:02X}")
        if response and response[0] == 0x55:
            print("[握手] 仍收到 APP 业务帧，继续等待；这不算握手成功")
    raise TimeoutError(f"Bootloader 握手：{attempts} 次尝试后未收到 ACK")


def main():
    """
    @brief 注入一次错误数据 CRC，正确重发后完成升级与版本确认。
    @return 无；失败时抛出异常，由入口打印原因。
    @note 只有预期 NACK、正确重发 ACK 和最终版本确认均通过才报告成功。
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
    if not 0 <= INJECT_SEQ < total:
        raise ValueError("异常注入序号必须落在本次固件的数据包范围内")

    print(f"固件：{FW_PATH}")
    print(
        f"大小：{size}，CRC32：0x{fw_crc:08X}，"
        f"目标版本：{VERSION}"
    )
    print(f"测试：seq={INJECT_SEQ} 注入一次 CRC 错误，预期 NACK 0x01")

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
            handshake(uart, rx)

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
                    # 正确帧保持不变，另建副本用于异常注入。
                    bad_frame = bytearray(frame)

                    # 下标八是数据区首字节。翻转一个数据位，但不更新 CRC，
                    # 保留序号、长度和帧尾，使本次只测试数据 CRC 错误。
                    bad_frame[8] ^= 0x01
                    print(f"[注入] seq={seq}：修改数据，保留原 CRC")

                    send_wait_ack(
                        uart, rx, bytes(bad_frame),
                        f"CRC 错误包 seq={seq}", 8,
                        expected_nack=0x01
                    )
                    print(f"[恢复] 重发正确的 seq={seq}")

                # 注入包先被拒收，再发送正确版本；只有正确包收到 ACK 才推进。
                send_wait_ack(
                    uart, rx, frame, f"数据包 seq={seq}", 8
                )

                if seq == INJECT_SEQ:
                    print("[符合预期] 正确重发获得 ACK")

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

            print("通过：CRC 错误拒收、正确重发、最终提交及 APP 版本确认均完成")

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
