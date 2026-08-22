#ifndef _OTA_SESSION_H_
#define _OTA_SESSION_H_

#include "esp_err.h"
#include "driver/uart.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 设置固件信息（在传输前调用）
 * @param firmware_size 固件大小（字节）
 * @param version 固件版本号字符串
 * @param fw_crc 整个固件文件的CRC32校验值（端到端校验用）
 */
void set_firmware_info(int firmware_size, const char* version, uint32_t fw_crc);

/**
 * @brief 通过串口发送固件到STM32
 * @param uart_num 串口号
 * @param firmware_path 固件文件路径（SPIFFS文件系统中的路径）
 * @param stm32_in_bootloader 是否在STM32引导模式下（true/false）
 * @note 用于判断是否需要发送ABORT命令对齐状态或直接放弃上报
 * @note 传输前会先设置固件信息，传输后会发送END命令
 * @return ESP_OK成功，ESP_FAIL失败
 * @note 完整的固件传输流程，包括握手、数据传输、结束确认
 */
esp_err_t serial_send_firmware(uart_port_t uart_num, const char* firmware_path, bool stm32_in_bootloader);

#endif
