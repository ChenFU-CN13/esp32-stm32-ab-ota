"""在已验证的最小 CRC 用例上，仅增加协议串口的原始收发记录。"""

import json
import time
from datetime import datetime
from pathlib import Path

import serial
import ota_upgrade_fwcrc_min as baseline


class TraceSerial:
    """透传串口操作并记录字节，不增加读取、清空、重试或协议应答。"""

    def __init__(self, uart, output):
        self.uart = uart
        self.output = output
        self.start = time.monotonic()

    def record(self, event, **fields):
        """记录实际收发块；一条 RX 记录不一定是一整帧。"""
        row = {
            "time": datetime.now().isoformat(timespec="milliseconds"),
            "elapsed_s": round(time.monotonic() - self.start, 6),
            "event": event,
            **fields,
        }
        self.output.write(json.dumps(row, ensure_ascii=False) + "\n")
        self.output.flush()

    def __enter__(self):
        self.uart.__enter__()
        return self

    def __exit__(self, *args):
        return self.uart.__exit__(*args)

    def __getattr__(self, name):
        return getattr(self.uart, name)

    def read(self, size=1):
        data = self.uart.read(size)
        if data:
            self.record("RX", count=len(data), hex=data.hex(" ").upper())
        return data

    def write(self, data):
        # 先调用真实写入，记录实际返回值；短写仍交给原脚本判断。
        try:
            count = self.uart.write(data)
        except Exception as error:
            self.record("TX_ERROR", hex=bytes(data).hex(" ").upper(),
                        error=str(error))
            raise
        self.record("TX", requested=len(data), count=count,
                    hex=bytes(data).hex(" ").upper())
        return count

    def reset_input_buffer(self):
        self.uart.reset_input_buffer()
        self.record("RESET_INPUT_BUFFER")

    def flush(self):
        self.uart.flush()
        self.record("FLUSH")


def main():
    """创建本轮日志，只打开最小脚本指定的协议口，不打开调试口。"""
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    folder = Path(__file__).resolve().parent / "runs" / f"fwcrc_trace_{stamp}"
    folder.mkdir(parents=True, exist_ok=False)
    path = folder / "protocol.jsonl"
    print(f"协议原始记录：{path}")
    print("本轮仅增加同步收发记录；COM26 请继续用串口助手记录 ASCII 日志")
    try:
        with path.open("w", encoding="utf-8") as output:
            def open_port(*args, **kwargs):
                return TraceSerial(serial.Serial(*args, **kwargs), output)

            baseline.main(serial_factory=open_port)
    finally:
        print(f"协议原始记录：{path}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, KeyboardInterrupt) as error:
        print(f"本轮未完成全部确认：{error or '用户中断'}")
        raise SystemExit(1)
