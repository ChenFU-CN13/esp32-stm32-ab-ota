import time
import zlib
import serial
from ota_timing import OtaTiming

# 从零开始计数，seq=10 对应第十一包。
INJECT_SEQ = 10

from ota_pack import (
    FW_PATH, VERSION, CHUNK_SIZE, MAX_FW_SIZE,
    make_info, make_data,
)

PORT = "COM6"
BAUDRATE = 115200
DUP_SEQ = 10


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
    @param name 当前步骤名称。
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


def abort_and_wait_restore(uart, rx, original, result, allow_stale_data=False,
                           timing=None, allow_recovery_notice=False):
    """只发一次ABORT，限时等待应答和原APP版本；收尾结果不覆盖原始错误。"""
    result.update(attempted=True, abort_sent=False, abort_ack=False,
                  restored_version=None, version_match=False, success=False,
                  stale_nacks=[], errors=[])
    ack_expired = False
    try:
        if timing is not None:
            timing.mark("abort_start")
        write_frame(uart, bytes.fromhex("66 08 BB"))
        result["abort_sent"] = True
        ack_deadline = time.monotonic() + 2
        restore_deadline = time.monotonic() + 30
        while time.monotonic() < restore_deadline:
            now = time.monotonic()
            if not result["abort_ack"] and not ack_expired and now >= ack_deadline:
                result["errors"].append("2秒内未确认ABORT ACK")
                ack_expired = True
            deadline = restore_deadline
            if not result["abort_ack"] and not ack_expired:
                deadline = min(deadline, ack_deadline)
            frame = receive_frame(uart, rx, deadline - time.monotonic())
            if frame is None:
                continue
            if frame == OTA_ACK:
                if not result["abort_ack"] and time.monotonic() <= ack_deadline:
                    result["abort_ack"] = True
                    if timing is not None:
                        timing.mark("abort_done")
                        timing.mark("app_wait_start")
                    print("[收尾] 收到ABORT ACK，继续等待实际恢复和APP版本")
            elif frame[:2] == b"\x66\x0A":
                code = frame[2]
                # 协议没有请求编号，只有限处理明确允许的旧数据应答或恢复通知。
                # 0x08只在调用方已确认恢复通知时允许，存储和整体CRC错误不能忽略。
                stale_code = ((allow_stale_data and code in (0x01, 0x03, 0x07)) or
                              (allow_recovery_notice and code == 0x08))
                if (stale_code and not result["abort_ack"] and not ack_expired and
                        len(result["stale_nacks"]) < 4):
                    result["stale_nacks"].append(code)
                    print(f"[收尾] 记录可能残留的NACK 0x{code:02X}，继续等ACK")
                elif not result["errors"]:
                    result["errors"].append(f"收尾期间收到非预期NACK 0x{code:02X}")
            elif frame[0] == 0x55 and 1 <= frame[1] <= 8:
                # 回滚期间不主动查询，只为APP主动发来的业务帧应答。
                write_frame(uart, BUSINESS_ACK)
                if frame[1] == 0x05:
                    uart.flush()
                    actual = tuple(frame[2:5])
                    result["restored_version"] = list(actual)
                    result["version_match"] = actual == original
                    print("[收尾] APP恢复版本：" + ".".join(map(str, actual)))
                    if actual != original:
                        result["errors"].append(f"恢复版本不匹配：要求{original}，收到{actual}")
                    elif timing is not None:
                        timing.mark("app_confirmed")
                    break
            elif not result["errors"]:
                result["errors"].append(f"收尾期间收到未知应答：{frame.hex(' ')}")
        if not result["abort_ack"] and not ack_expired:
            result["errors"].append("未确认ABORT ACK")
        if result["restored_version"] is None:
            result["errors"].append("30秒内未收到恢复后的APP版本")
    except (Exception, KeyboardInterrupt) as error:
        result["errors"].append(str(error) or "收尾被用户中断")
    result["success"] = (result["abort_ack"] and result["version_match"] and
                         not result["errors"])
    print("[收尾] 协议恢复确认成功" if result["success"] else
          "[收尾] 未完成全部确认：" + "；".join(result["errors"]))
    return result["success"]


def send_glued_data(uart, rx, frames, result):
    """一次提交两个相邻数据帧，要求收到两次ACK，保留已完成的步骤。"""
    sizes = [len(frame) for frame in frames]
    seqs = [int.from_bytes(frame[2:4], "big") for frame in frames]
    print(f"[粘帧] seq={seqs}，各帧字节数 {sizes}，"
          f"一次提交 {sum(sizes)} 字节，等待两次ACK")
    if hasattr(uart, "record"):
        uart.record("GLUE_DATA", seqs=seqs, sizes=sizes)
    write_frame(uart, b"".join(frames))
    result["sent"] = True
    deadline = time.monotonic() + 8
    for index in range(1, 3):
        response = receive_frame(uart, rx, deadline - time.monotonic())
        if response is None:
            raise TimeoutError(f"粘帧：等待第 {index} 次ACK超时")
        if response != OTA_ACK:
            raise RuntimeError(f"粘帧：第 {index} 次应答不是ACK：{response.hex(' ')}")
        result["ack_count"] = index
        print(f"[粘帧] 已收到第 {index}/2 次ACK")
        if hasattr(uart, "record"):
            uart.record("GLUE_ACK", index=index)


def main(serial_factory=None, version=None, timing=None, expected_current=None,
         glue_last_data=False, glue_result=None, cleanup_result=None,
         abort_after_packets=None):
    """
    @brief 从APP发起升级，或在指定位置主动中止并检查旧APP恢复。
    @param serial_factory 可选的串口创建函数。
    @param version 可选的目标版本；默认沿用 ota_pack.VERSION。
    @param timing 可选的阶段计时对象。
    @param expected_current 可选的升级前版本字符串，不匹配时不发送升级通知。
    @param glue_last_data 是否将最后两个数据帧合并提交，末包使用实际数据长度。
    @param glue_result 可选的字典，保存粘帧是否提交及已收到的ACK数量。
    @param cleanup_result 可选的字典，独立保存异常收尾结果。
    @param abort_after_packets 可选的主动中止位置，确认指定包数后发送ABORT并检查恢复。
    @return 无；失败时抛出异常，由入口打印原因。
    """
    firmware = FW_PATH.read_bytes()
    cleanup_result = {} if cleanup_result is None else cleanup_result
    size = len(firmware)

    if not 1 <= size <= MAX_FW_SIZE:
        raise ValueError("固件为空或超过当前工程容量")

    version = VERSION if version is None else version
    expected = tuple(
        int(x) for x in version.removeprefix("v").split(".")
    )

    if len(expected) != 3 or any(
        not 0 <= x <= 255 for x in expected
    ):
        raise ValueError(
            "版本必须是三个零到二百五十五的整数，例如 2.0.5"
        )

    fw_crc = zlib.crc32(firmware)
    info = make_info(size, version, fw_crc)
    total = (size + CHUNK_SIZE - 1) // CHUNK_SIZE
    if abort_after_packets is not None:
        if not 1 <= abort_after_packets < total:
            raise ValueError("主动中止位置必须至少确认一包，并且不能收满固件")
        if glue_last_data:
            raise ValueError("主动中止测试不能同时选择粘帧")
    glue_result = {} if glue_result is None else glue_result
    glue_result.update(sent=False, ack_count=0)
    glue_frames = None
    if glue_last_data:
        if total < 2:
            raise ValueError("粘帧测试至少需要两个数据包")
        offset = (total - 2) * CHUNK_SIZE
        glue_frames = (
            make_data(total - 2, firmware[offset:offset + CHUNK_SIZE]),
            make_data(total - 1, firmware[offset + CHUNK_SIZE:], pad=False),
        )
        # 当前STM32的DMA临时区为2048字节，先限制本次连续提交的长度。
        if sum(map(len, glue_frames)) > 2048:
            raise ValueError("最后两帧合计超过2048字节，不能使用当前粘帧用例")
    timing = OtaTiming() if timing is None else timing
    mode = "abort_recovery" if abort_after_packets is not None else "normal"
    timing.configure(mode, size, BAUDRATE)

    # 本轮测试选择中间包，避开末包接收后的状态切换。
    # 必须在打开串口、通知升级之前检查。
    if not 0 <= DUP_SEQ < total - 1:
        raise ValueError("重复包序号必须有效，并且不能选择最后一包")

    print(f"固件：{FW_PATH}")
    print(
        f"大小：{size}，CRC32：0x{fw_crc:08X}，"
        f"目标版本：{version}"
    )

    open_port = serial.Serial if serial_factory is None else serial_factory
    with open_port(
        PORT, BAUDRATE, timeout=0.02, write_timeout=2
    ) as uart:
        rx = bytearray()

        # 仅在会话开始前清理一次，传输中不清空应答。
        uart.reset_input_buffer()

        started = False
        end_sent = False
        data_started = False

        try:
            current = get_version(uart, rx, 5)
            print("当前版本：" + ".".join(map(str, current)))
            if expected_current is not None:
                if current != tuple(map(int, expected_current.split("."))):
                    raise RuntimeError(f"升级前版本不匹配：要求 {expected_current}，收到 {current}")

            started = True

            print("[1] 通知 APP 重启进入 Bootloader")
            timing.mark("notify_start")
            send_wait_ack(
                uart, rx, bytes.fromhex("66 01 BB"),
                "升级通知", 8
            )
            timing.mark("notify_done")
            time.sleep(0.5)

            print("[2] Bootloader 握手")
            timing.mark("handshake_start")
            send_wait_ack(
                uart, rx, bytes.fromhex("66 03 BB"),
                "握手", 1
            )
            timing.mark("handshake_done")

            print("[3] 等待备份与擦除")
            timing.mark("prepare_start")
            send_wait_ack(
                uart, rx, bytes.fromhex("66 04 BB"),
                "备份与擦除", 20
            )
            timing.mark("prepare_done")

            print("[4] 发送固件信息")
            timing.mark("info_start")
            send_wait_ack(uart, rx, info, "固件信息", 8)
            timing.mark("info_done")

            print("[5] 逐包传输")
            timing.mark("data_start")
            data_started = True

            for seq in range(total):
                if glue_last_data and seq == total - 1:
                    # 末包已经与倒数第二包一起提交并确认。
                    continue
                offset = seq * CHUNK_SIZE
                chunk = firmware[offset:offset + CHUNK_SIZE]
                if glue_last_data and seq == total - 2:
                    send_glued_data(uart, rx, glue_frames, glue_result)
                    done = seq + 2
                else:
                    frame = make_data(seq, chunk)
                    send_wait_ack(
                        uart, rx, frame, f"数据包 seq={seq}", 8
                    )
                    done = seq + 1

                if done % 10 == 0 or done == total:
                    print(
                        f"进度：{done}/{total}，"
                        f"{done / total:.1%}"
                    )

                if abort_after_packets is not None and done == abort_after_packets:
                    print(f"[主动中止] 已确认{done}/{total}包，立即发送ABORT")
                    cleanup_result["packets_confirmed"] = done
                    if hasattr(uart, "record"):
                        uart.record("ACTIVE_ABORT", after_packets=done)
                    if not abort_and_wait_restore(uart, rx, current, cleanup_result,
                                                  timing=timing):
                        raise RuntimeError("主动ABORT后未完成应答或旧APP恢复确认")
                    # 本轮只传输部分固件，不标记整份数据发送完成，也不计算整包速率。
                    timing.report()
                    print("协议检查通过：主动ABORT应答和原APP版本恢复均完成")
                    return

            timing.mark("data_done")
            print("[6] 结束传输，等待整体 CRC 校验及元数据提交")

            # 发出结束命令后，ACK 丢失也可能已经完成提交，
            # 不能贸然发送 ABORT。
            end_sent = True
            timing.mark("end_start")

            send_wait_ack(
                uart, rx, bytes.fromhex("66 07 BB"),
                "升级结束", 8
            )
            timing.mark("end_done")
            timing.mark("app_wait_start")

            print("Bootloader 已确认校验和提交成功，等待 APP 启动")

            time.sleep(2)
            actual = get_version(uart, rx, 15)

            print("升级后版本：" + ".".join(map(str, actual)))

            if actual != expected:
                raise RuntimeError(
                    f"版本不匹配：期望 {expected}，收到 {actual}"
                )
            timing.mark("app_confirmed")
            timing.report()

            print("通过：逐包应答、最终提交及 APP 版本确认均完成")

        except (Exception, KeyboardInterrupt):
            if started and not end_sent and not cleanup_result.get("attempted"):
                print("本轮失败，发送一次ABORT并继续采集恢复日志")
                abort_and_wait_restore(uart, rx, current, cleanup_result,
                                       allow_stale_data=data_started)

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
