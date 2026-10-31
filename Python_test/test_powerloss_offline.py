"""离线检查断电测试入口，不打开真实串口，不要求人工拔插。"""

import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zlib

import ota_powerloss_capture as target


class Clock:
    def __init__(self):
        self.now = 0.0

    def monotonic(self):
        return self.now

    def sleep(self, value):
        self.now += value


class PowerlossTests(unittest.TestCase):
    def run_case(self, fault=None):
        firmware = b"test" * 108330  # 与当前固件相同大小，424包，中点212包。
        crc = f"{zlib.crc32(firmware):08X}"
        clock = Clock()
        writes, seqs, readers = [], [], []
        ports_opened = []

        class Uart:
            def __init__(self, *args, **kwargs):
                self.phase = len(ports_opened)
                ports_opened.append(self)
                self.rx = bytearray()
                if self.phase == 1 and fault != "no_version":
                    version = b"\x02\x00\x08" if fault == "bad_version" else b"\x02\x00\x07"
                    self.rx.extend(b"\x55\x07\x00\x00\x00\x00\x00\xBB")
                    self.rx.extend(b"\x55\x05" + version + b"\x00\x00\xBB")

            def __enter__(self):
                return self

            def __exit__(self, *args):
                pass

            def reset_input_buffer(self):
                self.rx.clear()

            def flush(self):
                pass

            def write(self, data):
                writes.append((self.phase, bytes(data)))
                if data == b"\x66\x09\xBB":
                    version = b"\x02\x00\x06" if fault == "bad_initial" else b"\x02\x00\x07"
                    self.rx.extend(b"\x55\x05" + version + b"\x00\x00\xBB")
                elif data[:1] == b"\x66":
                    if data[:2] == b"\x66\x06":
                        seq = int.from_bytes(data[2:4], "big")
                        length = int.from_bytes(data[4:8], "big")
                        self_test.assertEqual(length, 1024)
                        self_test.assertEqual(zlib.crc32(data[8:8 + length]), int.from_bytes(data[-5:-1], "big"))
                        seqs.append(seq)
                    self.rx.extend(b"\x66\x02\xBB")
                return len(data)

            def read(self, size):
                if self.rx:
                    data = bytes(self.rx[:size])
                    del self.rx[:size]
                    return data
                clock.sleep(0.02)
                return b""

        class Debug:
            def __enter__(self):
                return self

            def __exit__(self, *args):
                pass

        class Reader(Debug):
            def __init__(self, uart, raw, text):
                self.error = "模拟拔USB导致设备消失" if not readers else None
                self.count = 0
                self.text, self.phase = text, len(readers)
                readers.append(self)

            def __enter__(self):
                if self.phase == 0:
                    log = f"[回滚] A区备份完成，CRC32=0x{crc}\n"
                else:
                    log = "" if fault == "no_startup" else "[回滚] 检测到未完成升级，进入启动恢复\n"
                    restored = "00000000" if fault == "bad_crc" else crc
                    log += f"[回滚] 恢复成功，CRC32=0x{restored}\n"
                    if fault == "debug_error":
                        self.error = "模拟上电后采集失败"
                self.text.write(log)
                self.text.flush()
                self.count = len(log.encode())
                return self

        def removal(*args):
            if fault == "no_cut":
                raise TimeoutError("8秒内未检测到调试口消失")
            return 2.0

        self_test = self
        answers = ["", "wrong" if fault == "off_unconfirmed" else "OFF",
                   "wrong" if fault == "reset_unconfirmed" else "READY"]
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fw = root / "firmware.bin"
            fw.write_bytes(firmware)
            with contextlib.ExitStack() as stack:
                for name, value in (
                    ("FW_PATH", fw), ("__file__", str(root / "entry.py")),
                    ("open_debug_port", lambda port: Debug()), ("DebugReader", Reader),
                    ("wait_usb_removal", removal), ("wait_ports", lambda ports: None),
                ):
                    stack.enter_context(patch.object(target, name, value))
                stack.enter_context(patch.object(target.serial, "Serial", Uart))
                stack.enter_context(patch("time.monotonic", clock.monotonic))
                stack.enter_context(patch("time.sleep", clock.sleep))
                stack.enter_context(patch("builtins.input", side_effect=answers))
                stack.enter_context(patch("sys.argv", ["entry", "--version", "2.0.7", "--expect-current-version", "2.0.7"]))
                stack.enter_context(contextlib.redirect_stdout(io.StringIO()))
                if fault:
                    with self.assertRaises((RuntimeError, TimeoutError)):
                        target.main()
                else:
                    target.main()
            result = json.loads(next(root.glob("runs/*/result.json")).read_text(encoding="utf-8"))
            self.assertEqual(result["test_pass"], fault is None)
            self.assertFalse(any(data in (b"\x66\x07\xBB", b"\x66\x08\xBB") for _, data in writes))
            if fault != "bad_initial":
                self.assertEqual(seqs, list(range(212)))
            if fault is None:
                self.assertEqual(result["packets_confirmed"], 212)
                self.assertEqual(result["restored_version"], [2, 0, 7])
                self.assertTrue(result["recovery_crc"]["match"])
                self.assertTrue(result["startup_recovery_seen"])
                self.assertEqual(len(ports_opened), 2)
                self.assertTrue(all(data == target.BUSINESS_ACK for phase, data in writes if phase == 1))

    def test_success(self):
        self.run_case()

    def test_failures_are_not_passes(self):
        for fault in ("no_cut", "off_unconfirmed", "reset_unconfirmed", "no_startup",
                      "bad_crc", "no_version", "bad_version", "bad_initial", "debug_error"):
            with self.subTest(fault=fault):
                self.run_case(fault)

    def test_usb_deadline(self):
        for removed in (True, False):
            clock = Clock()
            with patch("time.monotonic", clock.monotonic), patch("time.sleep", clock.sleep), \
                    patch.object(target, "ports_present", side_effect=lambda: set() if removed and clock.now >= 2 else {"COM26"}):
                if removed:
                    self.assertLess(target.wait_usb_removal("COM26"), 3)
                else:
                    with self.assertRaises(TimeoutError):
                        target.wait_usb_removal("COM26")


if __name__ == "__main__":
    unittest.main()
