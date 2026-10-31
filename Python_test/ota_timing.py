"""记录主机观察到的 OTA 阶段耗时，不改变协议收发。"""

import time


class OtaTiming:
    """用单调时钟计时；APP 终点为版本确认，并非 MCU 跳转指令时刻。"""

    def __init__(self, clock=None):
        self.clock = time.perf_counter if clock is None else clock
        self.points = {}
        self.mode = None
        self.size = 0
        self.baudrate = 0

    def configure(self, mode, size, baudrate):
        self.mode, self.size, self.baudrate = mode, size, baudrate
        self.points.clear()

    def mark(self, name):
        self.points[name] = self.clock()

    def summary(self):
        """只计算已结束的阶段，失败时不把部分传输当作整包速率。"""
        pairs = {
            "notify_s": ("notify_start", "notify_done"),
            "handshake_s": ("handshake_start", "handshake_done"),
            "backup_erase_s": ("prepare_start", "prepare_done"),
            "info_s": ("info_start", "info_done"),
            "transfer_s": ("data_start", "data_done"),
            "final_check_s": ("end_start", "end_done"),
            "abort_ack_s": ("abort_start", "abort_done"),
            "app_wait_s": ("app_wait_start", "app_confirmed"),
            "handshake_to_app_s": ("handshake_start", "app_confirmed"),
            "notify_to_app_s": ("notify_start", "app_confirmed"),
            "timeout_recovery_wait_s": ("silence_start", "recovery_notice"),
        }
        result = {"mode": self.mode, "firmware_bytes": self.size,
                  "baudrate": self.baudrate, "complete": "app_confirmed" in self.points,
                  "endpoint": "主机确认 APP 版本；包含 APP 初始化及主机等待时间"}
        for name, (start, end) in pairs.items():
            if start in self.points and end in self.points:
                result[name] = self.points[end] - self.points[start]
        duration = result.get("transfer_s", 0)
        if duration > 0:
            # 使用 bin 有效字节，不计末包填充、帧头、CRC 和 ACK。
            result["payload_bytes_per_s"] = self.size / duration
            result["payload_kib_per_s"] = self.size / duration / 1024
        return result

    def report(self):
        result = self.summary()
        mode = {"normal": "正常升级", "crc_rollback": "整体 CRC 故障回滚测试",
                "timeout_recovery": "数据阶段超时回滚测试",
                "abort_recovery": "主动ABORT回滚测试"}.get(self.mode, "OTA测试")
        print(f"[计时] {mode}；终点为主机确认 APP 版本")
        labels = {"handshake_s": "Bootloader 握手", "backup_erase_s": "备份与擦除",
                  "info_s": "固件信息", "transfer_s": "数据传输（首包提交前至末包 ACK）",
                  "final_check_s": "整体校验与结束应答", "abort_ack_s": "ABORT 应答",
                  "app_wait_s": "等待 APP 版本确认", "handshake_to_app_s": "握手开始至 APP 版本确认",
                  "notify_to_app_s": "升级通知至 APP 版本确认",
                  "timeout_recovery_wait_s": "停止发送至恢复通知"}
        for key, label in labels.items():
            if key in result:
                print(f"[计时] {label}：{result[key]:.3f} s")
        if "payload_bytes_per_s" in result:
            print(f"[速率] 有效固件 {self.size} 字节："
                  f"{result['payload_bytes_per_s']:.1f} B/s，"
                  f"{result['payload_kib_per_s']:.2f} KiB/s")
        if self.mode == "normal":
            print("[计时说明] APP 确认包含原脚本固定等待 2 秒及版本查询，不等同于准确跳转时刻")
        else:
            print("[计时说明] 本轮包含回滚恢复，不能作为正常升级总耗时")
