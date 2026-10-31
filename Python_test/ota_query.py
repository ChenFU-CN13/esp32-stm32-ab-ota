import time
import serial


PORT = "COM6"
BAUDRATE = 115200

# 查询版本命令：不会启动升级。
VERSION_QUERY = bytes.fromhex("66 09 BB")

# 普通业务帧的成功应答，注意与 OTA 的三字节 ACK 不同。
BUSINESS_ACK = bytes.fromhex("55 7E 00 00 00 00 00 BB")


def query_version():
    """
    @brief 查询 STM32 当前 APP 版本，并回复业务 ACK。
    @return 收到版本返回 True；等待超时返回 False。
    @note 接收数据先累计到缓冲区，不能假设一次读取就是一帧。
    """
    rx_buffer = bytearray()

    with serial.Serial(
        port=PORT,
        baudrate=BAUDRATE,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=0.05,
        write_timeout=1,
    ) as uart:
        print(f"已打开 {PORT}，波特率 {BAUDRATE}")

        uart.write(VERSION_QUERY)
        print("[发送]", VERSION_QUERY.hex(" ").upper())

        # 使用单调时钟计时，不受电脑系统时间调整影响。
        deadline = time.monotonic() + 5

        while time.monotonic() < deadline:
            # 一次读取可能是半帧，也可能包含多帧。
            rx_buffer.extend(uart.read(64))

            while rx_buffer:
                # 丢弃帧头之前的无效字节。
                head = rx_buffer.find(b"\x55")
                if head < 0:
                    rx_buffer.clear()
                    break

                del rx_buffer[:head]

                # 当前 APP 业务帧固定为八字节，不足就继续接收。
                if len(rx_buffer) < 8:
                    break

                # 包尾不匹配时，只前进一个字节，重新寻找帧头。
                if rx_buffer[7] != 0xBB:
                    del rx_buffer[0]
                    continue

                frame = bytes(rx_buffer[:8])
                del rx_buffer[:8]

                # 当前工程上行业务类型为 0x01～0x08。
                if not 0x01 <= frame[1] <= 0x08:
                    continue

                print("[接收]", frame.hex(" ").upper())

                # 版本帧前面可能有其他业务帧，也需要应答，
                # 让 STM32 结束等待并继续发送下一帧。
                uart.write(BUSINESS_ACK)
                print("[应答]", BUSINESS_ACK.hex(" ").upper())

                if frame[1] == 0x05:
                    major, minor, patch = frame[2:5]

                    # 等待应答发送完，再关闭串口。
                    uart.flush()

                    print(f"查询成功，APP 版本：{major}.{minor}.{patch}")
                    return True

        print("查询超时：五秒内没有收到完整版本帧")
        return False


if __name__ == "__main__":
    try:
        success = query_version()
    except serial.SerialException as error:
        print(f"串口操作失败：{error}")
        success = False

    raise SystemExit(0 if success else 1)