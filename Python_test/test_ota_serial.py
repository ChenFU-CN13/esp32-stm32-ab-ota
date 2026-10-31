"""离线串口回归测试：只使用内存中的模拟设备，禁止访问开发板。"""

import io
import json
import types
import unittest
from contextlib import redirect_stdout
from unittest.mock import patch

import ota_upgrade_crc as link
import ota_upgrade_fwcrc as upgrade


VERSION = bytes.fromhex("55 05 02 00 05 00 00 BB")
BUSINESS = bytes.fromhex("55 06 00 02 00 00 00 BB")


class Clock:
    """虚拟时钟，测试不需要等待实际的超时时间。"""

    def __init__(self):
        self.now = 0

    def monotonic(self):
        return self.now

    def sleep(self, seconds):
        self.now += max(0, seconds)


class Port:
    """模拟串口的字节队列，read 每次推进虚拟时间。"""

    def __init__(self, clock, chunks=()):
        self.clock = clock
        self.chunks = list(chunks)
        self.sent = []

    @property
    def in_waiting(self):
        return sum(len(chunk) for chunk in self.chunks)

    def read(self, size):
        self.clock.sleep(0.02)
        if not self.chunks:
            return b""
        chunk = self.chunks.pop(0)
        if len(chunk) > size:
            self.chunks.insert(0, chunk[size:])
        return chunk[:size]

    def write(self, data):
        self.sent.append(bytes(data))
        return len(data)

    def flush(self):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *args):
        pass


class Device(Port):
    """模拟当前固件：握手就绪前丢弃命令，进入 WAIT_START 后忽略重复握手。"""

    def __init__(self, clock, capture=None, end_reply=b"\x66\x0a\x05\xbb"):
        super().__init__(clock)
        self.capture = capture
        self.end_reply = end_reply
        self.ready_at = float("inf")
        self.confirmed = False
        self.lost_handshake_ack = False

    def write(self, data):
        super().write(data)
        if data == link.BUSINESS_ACK:
            return len(data)
        cmd = data[1]
        if cmd == 9:
            self.chunks.append(VERSION)
            if self.capture:
                self.capture.version_request.set()
        elif cmd == 1:
            self.chunks.append(link.OTA_ACK)
            self.ready_at = self.clock.now + 0.8
        elif cmd == 3:
            if self.clock.now >= self.ready_at and not self.confirmed:
                self.confirmed = True
                if not self.lost_handshake_ack:
                    self.chunks.append(link.OTA_ACK)
        elif cmd in (4, 5, 6):
            if not self.confirmed:
                raise AssertionError("未握手就试图开始升级")
            self.chunks.append(link.OTA_ACK)
        elif cmd == 7:
            if self.end_reply:
                self.chunks.append(self.end_reply)
        elif cmd == 8:
            self.chunks.append(link.OTA_ACK + VERSION)
        return len(data)


class Event:
    def __init__(self):
        self.value = False

    def clear(self):
        self.value = False

    def set(self):
        self.value = True

    def wait(self, timeout):
        return self.value


class SerialTests(unittest.TestCase):
    def setUp(self):
        self.clock = Clock()
        self.patches = [patch.object(link, "time", self.clock),
                        patch.object(upgrade, "time", self.clock),
                        patch.object(link.serial, "Serial", side_effect=AssertionError("禁止真实串口"))]
        for item in self.patches:
            item.start()
            self.addCleanup(item.stop)
        self.stdout = redirect_stdout(io.StringIO())
        self.stdout.__enter__()
        self.addCleanup(self.stdout.__exit__, None, None, None)

    def test_fragmented_version(self):
        port = Port(self.clock, [VERSION[:2], VERSION[2:5], VERSION[5:]])
        self.assertEqual(link.get_version(port, bytearray(), 5), (2, 0, 5))
        self.assertEqual(port.sent, [b"\x66\x09\xbb", link.BUSINESS_ACK])

    def test_joined_business_and_version(self):
        port = Port(self.clock, [BUSINESS + VERSION])
        self.assertEqual(link.get_version(port, bytearray(), 5), (2, 0, 5))
        self.assertEqual(port.sent.count(link.BUSINESS_ACK), 2)

    def test_orphan_header_before_ack(self):
        port = Port(self.clock, [b"\x55" + link.OTA_ACK])
        self.assertEqual(link.receive_frame(port, bytearray(), 1), link.OTA_ACK)

    def test_incomplete_valid_header_resync(self):
        port = Port(self.clock, [b"\x55\x05" + link.OTA_ACK])
        self.assertEqual(link.receive_frame(port, bytearray(), 1), link.OTA_ACK)

    def test_last_read_is_parsed(self):
        port = Port(self.clock, [VERSION])
        self.assertEqual(link.receive_frame(port, bytearray(), 0.01), VERSION)

    def test_version_nack_reported(self):
        port = Port(self.clock, [bytes.fromhex("66 0A 02 BB")])
        with self.assertRaisesRegex(RuntimeError, "NACK 0x02"):
            link.get_version(port, bytearray(), 5)
        self.assertLess(self.clock.now, 1)

    def test_bad_version_reserved_bytes(self):
        port = Port(self.clock, [bytes.fromhex("55 05 02 00 05 01 00 BB")])
        with self.assertRaisesRegex(RuntimeError, "保留字段"):
            link.get_version(port, bytearray(), 5)

    def test_buffered_old_ack_rejected(self):
        port = Port(self.clock)
        with self.assertRaises(link.ProtocolSyncError):
            link.send_wait_ack(port, bytearray(link.OTA_ACK), b"\x66\x03\xbb", "握手", 1)
        self.assertEqual(port.sent, [])

    def test_driver_old_ack_rejected(self):
        port = Port(self.clock, [link.OTA_ACK])
        with self.assertRaises(link.ProtocolSyncError):
            link.send_wait_ack(port, bytearray(), b"\x66\x03\xbb", "握手", 1)
        self.assertEqual(port.sent, [])

    def test_partial_business_before_command(self):
        # 模拟 in_waiting 查询时只有前两字节，后续字节稍后才到达。
        class PartialPort(Port):
            @property
            def in_waiting(self):
                return 2
        port = PartialPort(self.clock, [BUSINESS[:2], BUSINESS[2:]])
        rx = bytearray()
        link.check_pending(port, rx, "升级通知")
        self.assertEqual(rx, bytearray())

    def test_partial_old_ack_still_rejected(self):
        class PartialPort(Port):
            @property
            def in_waiting(self):
                return 1
        port = PartialPort(self.clock, [link.OTA_ACK[:1], link.OTA_ACK[1:]])
        with self.assertRaises(link.ProtocolSyncError):
            link.check_pending(port, bytearray(), "升级通知")

    def test_delayed_boot_handshake(self):
        port = Device(self.clock)
        port.ready_at = 0.8
        link.handshake(port, bytearray())
        self.assertTrue(port.confirmed)
        self.assertGreater(port.sent.count(b"\x66\x03\xbb"), 1)

    def test_lost_handshake_ack_fails(self):
        port = Device(self.clock)
        port.ready_at = 0
        port.lost_handshake_ack = True
        with self.assertRaises(TimeoutError):
            link.handshake(port, bytearray())

    def test_capture_error_not_silent(self):
        capture = types.SimpleNamespace(error="模拟读串口失败")
        trace = upgrade.SerialTrace(Port(self.clock), io.StringIO(), capture)
        with self.assertRaisesRegex(RuntimeError, "模拟读串口失败"):
            trace.read(64)

    def test_raw_garbage_is_recorded(self):
        output = io.StringIO()
        trace = upgrade.SerialTrace(Port(self.clock, [b"\x7f\xaf"]), output)
        self.assertEqual(trace.read(64), b"\x7f\xaf")
        self.assertEqual(json.loads(output.getvalue())["hex"], "7F AF")
        self.assertEqual(trace.rx_bytes, 2)

    def run_upgrade(self, device, capture=None):
        result, output = {}, io.StringIO()
        firmware = types.SimpleNamespace(read_bytes=lambda: bytes(range(256)) * 5)
        with patch.object(upgrade.serial, "Serial", return_value=device), patch.object(upgrade, "FW_PATH", firmware):
            try:
                upgrade.run_protocol("模拟串口", result, output, capture)
            finally:
                self.result, self.trace = result, output.getvalue()
        return result

    def test_full_fwcrc_protocol(self):
        capture = types.SimpleNamespace(error=None, version_request=Event())
        device = Device(self.clock, capture)
        result = self.run_upgrade(device, capture)
        self.assertTrue(result["protocol_pass"])
        self.assertTrue(result["query_seen_in_debug"])
        self.assertEqual(result["acked_packets"], 2)
        self.assertEqual(result["restored_version"], [2, 0, 5])
        self.assertGreater(result["protocol_rx_bytes"], 0)
        records = [json.loads(line) for line in self.trace.splitlines()]
        self.assertTrue(any(row["direction"] == "TX" and row["hex"] == "66 07 BB" for row in records))

    def test_unsolicited_version_without_query_log_stops(self):
        capture = types.SimpleNamespace(error=None, version_request=Event())
        device = Device(self.clock)
        with self.assertRaisesRegex(RuntimeError, "VERSION-REQ"):
            self.run_upgrade(device, capture)
        self.assertNotIn(b"\x66\x01\xbb", device.sent)
        self.assertNotIn(b"\x66\x08\xbb", device.sent)

    def test_wrong_end_ack_no_abort(self):
        device = Device(self.clock, end_reply=link.OTA_ACK)
        with self.assertRaisesRegex(RuntimeError, "却收到 ACK"):
            self.run_upgrade(device)
        self.assertNotIn(b"\x66\x08\xbb", device.sent)

    def test_end_timeout_no_abort(self):
        device = Device(self.clock, end_reply=b"")
        with self.assertRaises(TimeoutError):
            self.run_upgrade(device)
        self.assertNotIn(b"\x66\x08\xbb", device.sent)

    def test_debug_crc_evidence(self):
        text = "A区备份完成，CRC32=0x12345678\n计算CRC32: 0x1798AF7C\n期望CRC32: 0x1798AF7D\n开始恢复旧固件\n恢复成功，CRC32=0x12345678"
        for encoding in ("utf-8", "gb18030"):
            _, evidence = upgrade.check_debug_evidence(text.encode(encoding), "0x1798AF7C", "0x1798AF7D")
            self.assertTrue(evidence["debug_pass"])
        with self.assertRaises(RuntimeError):
            upgrade.check_debug_evidence(text.replace("恢复成功", "恢复失败").encode(), "0x1798AF7C", "0x1798AF7D")


if __name__ == "__main__":
    unittest.main()
