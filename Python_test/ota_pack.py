from pathlib import Path
import struct
import zlib


# ota_pack.py 放在工程的 Python_test 目录中。
ROOT = Path(__file__).resolve().parent.parent

FW_PATH = ROOT / (
    "STM32-OTA/A区智能哨兵环境监测系统/"
    "MDK-ARM/LVGL_RTOS_DOME5/LVGL_RTOS_DOME5.bin"
)

# 本次准备发送的版本字段，并不是从 bin 文件中自动解析的。
VERSION = "2.0.5"

CHUNK_SIZE = 1024
MAX_FW_SIZE = 960 * 1024


def make_info(size, version, fw_crc):
    """
    @brief 生成二十五字节固件信息帧。
    @param size 原始固件字节数。
    @param version 本次声明的版本字符串，最多八个 ASCII 字节。
    @param fw_crc 原始固件整体 CRC32。
    @return 完整信息帧。
    """
    ver = version.encode("ascii")

    if not 1 <= len(ver) <= 8:
        raise ValueError("版本字符串必须占一到八个 ASCII 字节")

    # body 从命令字开始，正好是信息帧 CRC 的计算范围。
    body = b"\x05"
    body += struct.pack(">I", size)
    body += b"//"
    body += ver.ljust(8, b"\x00")
    body += struct.pack(">I", fw_crc)

    frame_crc = zlib.crc32(body)

    return (
        b"\x66"
        + body
        + struct.pack(">I", frame_crc)
        + b"\xBB"
    )


def make_data(seq, chunk, pad=True):
    """
    @brief 把一段固件数据打包成数据帧，默认补齐到一千零三十七字节。
    @param seq 包序号，从零开始。
    @param chunk 原始数据，一到一千零二十四字节。
    @param pad 是否将数据补齐到1024字节；默认补0xFF，关闭时保留实际长度。
    @return 完整数据帧。
    """
    if not 0 <= seq <= 65535:
        raise ValueError("包序号超出两字节范围")

    if not 1 <= len(chunk) <= CHUNK_SIZE:
        raise ValueError("本包原始数据长度必须为一到一千零二十四")

    # 默认与ESP32发送方式一致；粘帧用例可用实际长度构造末包。
    payload = chunk.ljust(CHUNK_SIZE, b"\xFF") if pad else bytes(chunk)

    # 数据帧CRC只覆盖本帧实际携带的数据区。
    data_crc = zlib.crc32(payload)

    return (
        b"\x66\x06"
        + struct.pack(">HI", seq, len(payload))
        + payload
        + struct.pack(">I", data_crc)
        + b"\xBB"
    )


def main():
    """
    @brief 读取固件，检查基本大小并展示组包结果。
    @return 无。
    """
    firmware = FW_PATH.read_bytes()
    size = len(firmware)

    if not 1 <= size <= MAX_FW_SIZE:
        raise ValueError("固件为空，或者超过当前工程的 960 KiB 上限")

    # 整体 CRC 只计算原始文件，不包含尾包补齐的 0xFF。
    fw_crc = zlib.crc32(firmware)

    packet_count = (size + CHUNK_SIZE - 1) // CHUNK_SIZE
    last_offset = (packet_count - 1) * CHUNK_SIZE
    last_chunk = firmware[last_offset:]

    info_frame = make_info(size, VERSION, fw_crc)
    first_frame = make_data(0, firmware[:CHUNK_SIZE])
    last_frame = make_data(packet_count - 1, last_chunk)

    print(f"固件路径：{FW_PATH}")
    print(f"声明版本：{VERSION}")
    print(f"固件大小：{size} 字节")
    print(f"固件 CRC32：0x{fw_crc:08X}")
    print(f"数据包数：{packet_count}")
    print(f"最后一包有效数据：{len(last_chunk)} 字节")

    print(f"\n信息帧长度：{len(info_frame)} 字节")
    print("信息帧：", info_frame.hex(" ").upper())

    print(f"\n第一包帧长度：{len(first_frame)} 字节")
    print(f"最后一包帧长度：{len(last_frame)} 字节")
    print("最后一包帧头：", last_frame[:8].hex(" ").upper())


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        print(f"准备失败：{error}")
        raise SystemExit(1)
