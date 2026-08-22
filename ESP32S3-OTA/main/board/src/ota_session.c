#include "myuart.h"
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "driver/uart.h"
#include "esp_crc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/projdefs.h"
#include <inttypes.h>

/*============================================================================
 *                           全局变量定义
 *============================================================================*/

static const char *TAG = "ota_session";

/* 固件传输相关全局变量 */
static int g_firmware_size = 0;               /* 固件大小（字节） */
static char g_firmware_version[16] = {0};     /* 固件版本号字符串 */
static uint32_t g_firmware_crc32 = 0;        /* 整个固件文件的CRC32（端到端校验用，由下载时流式计算得到） */


/**
 * @brief 通过串口发送固件到STM32（主函数）
 * @param uart_num 串口号
 * @param firmware_path 固件文件路径（SPIFFS文件系统中的路径）
 * @param stm32_in_bootloader 是否在STM32引导模式下（true/false）
 * @note 用于判断是否需要发送ABORT命令对齐状态或直接放弃上报
 * @note 传输前会先设置固件信息，传输后会发送END命令
 * @return ESP_OK成功，ESP_FAIL失败
 * 
 * @note 完整的固件传输流程：
 *       
 *       阶段1：握手阶段
 *       -----------------
 *       Step 1: ESP32发送升级通知 -> STM32返回ACK
 *       Step 2: ESP32发送握手确认 -> STM32返回ACK
 *       Step 3: ESP32发送开始传输命令 -> STM32返回ACK
 *       
 *       阶段2：信息交换
 *       -----------------
 *       Step 4: ESP32发送固件信息（大小+版本） -> STM32返回ACK
 *       
 *       阶段3：数据传输
 *       -----------------
 *       Step 5: 循环发送固件数据包
 *               - 每包1024字节
 *               - 每包都需要STM32返回ACK
 *               - 显示传输进度
 *       
 *       阶段4：结束阶段
 *       -----------------
 *       Step 6: ESP32发送升级结束 -> STM32返回ACK并重启
 */
esp_err_t serial_send_firmware(uart_port_t uart_num, const char* firmware_path, bool stm32_in_bootloader)
{
    ESP_LOGI(TAG, "Starting firmware transmission via serial");

    /*===== 旧方案（已废弃，注释保留对比）=====
    vTaskSuspend(recive_buff1_handle);
    ESP_LOGI(TAG, "Suspended UART receive task");
     * 死因：挂起时接收任务可能正持有UART驱动内部的rx_mux互斥锁
     * （阻塞在uart_read_bytes里消化积压事件），被冻结后锁永不释放，
     * 本任务后面调uart_flush_input()要无限期等这把锁 → Step 2永久卡死。
     * vTaskSuspend的致命缺陷：你无法控制任务被冻结在代码的哪一行，
     * 冻在持锁区间就是死锁。
     *=======================================*/

    /* 新方案：标志门控。接收任务继续活着跑事件循环，但OTA期间
     * 只消费事件不碰ringbuffer，既不抢数据也不抢锁 */
    g_ota_uart_busy = true;
    ESP_LOGI(TAG, "UART locked by OTA task (flag gate)");
    
    esp_err_t ret = ESP_OK;
    int fd = -1;                    /* 文件描述符，用于读取固件文件 */
    uint8_t* data_buffer = NULL;    /* 数据缓冲区，用于存储每次读取的1KB数据 */
    
    /*==================== 打开固件文件 ====================*/
    /* 以只读方式打开固件文件 */
    fd = open(firmware_path, O_RDONLY);
    if (fd < 0)
    {
        ESP_LOGE(TAG, "Failed to open firmware file: %s", firmware_path);
        ret = ESP_FAIL;
        goto cleanup;
    }
    
    /* 获取文件大小：lseek移动到文件末尾，返回偏移量即为文件大小 */
    off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);  /* 移回文件开头，准备读取 */
    
    if (file_size <= 0)
    {
        ESP_LOGE(TAG, "Invalid firmware file size: %d", (int)file_size);
        ret = ESP_FAIL;
        goto cleanup;
    }
    
    ESP_LOGI(TAG, "Firmware file size: %d bytes", (int)file_size);
    
    /* 分配数据缓冲区，用于每次读取1KB固件数据 */
    data_buffer = malloc(SERIAL_DATA_CHUNK_SIZE);
    if (data_buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate data buffer");
        ret = ESP_FAIL;
        goto cleanup;
    }
    
    /*==================== 阶段1：握手阶段 ====================*/
    
    /* Step 1: 发送升级通知
     * 目的：通知STM32即将进行固件升级，让STM32进入升级准备状态
     * 数据包：0x66 0x01 0xBB
     */
    if (!stm32_in_bootloader)
    {
        ESP_LOGI(TAG, "Step 1: Sending upgrade notification...");
        ret = send_handshake_notify(uart_num);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Handshake notify failed");
            goto cleanup;
        }
        ESP_LOGI(TAG, "Step 1: STM32 acknowledged upgrade notification");
    }
    else
    {
        ESP_LOGI(TAG, "Skip Step 1: STM32 already in bootloader (retry)");
    }

    
    /* Step 2: 发送握手确认
     * 目的：确认握手成功，建立通信链路
     * 数据包：0x66 0x03 0xBB
     */
    ESP_LOGI(TAG, "Step 2: Sending handshake confirm...");
    ret = send_handshake_confirm(uart_num);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Handshake confirm failed");
        goto cleanup;
    }
    ESP_LOGI(TAG, "Step 2: Handshake confirmed");
    
    /* Step 3: 发送开始传输命令
     * 目的：通知STM32即将开始发送固件数据
     *       STM32收到此命令后会先擦除Flash，擦除完成后才返回ACK
     * 数据包：0x66 0x04 0xBB
     * 注意：由于Flash擦除需要较长时间，此处超时设置为8秒
     */
    ESP_LOGI(TAG, "Step 3: Sending start transmission command...");
    ret = send_start_transmission(uart_num);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Start transmission failed");
        goto cleanup;
    }
    ESP_LOGI(TAG, "Step 3: STM32 ready for firmware transmission");
    
    /*==================== 阶段2：信息交换 ====================*/
    
    /* Step 4: 发送固件信息（大小+版本号）
     * 目的：告知STM32即将接收的固件大小和版本号
     *       STM32根据固件大小分配Flash空间
     * 数据包格式：包头(1) + 命令(1) + 固件大小(4) + 分隔符(2) + 版本号(4) + CRC32(4) + 包尾(1)
     * 总长度：17字节
     */
    ESP_LOGI(TAG, "Step 4: Sending firmware info...");
    ret = send_firmware_info(uart_num, (int)file_size, g_firmware_version, g_firmware_crc32);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send firmware info");
        goto cleanup;
    }
    ESP_LOGI(TAG, "Step 4: Firmware info sent successfully");
    
    /*==================== 阶段3：数据传输 ====================*/
    
    /* Step 5: 循环发送固件数据包 */
    ESP_LOGI(TAG, "Step 5: Starting firmware data transmission...");
    
    /* 计算总包数：(文件大小 + 每包大小 - 1) / 每包大小
     * 这种计算方式可以正确处理文件大小不是每包大小整数倍的情况
     * 例如：文件大小2049字节，每包1024字节
     * (2049 + 1024 - 1) / 1024 = 3072 / 1024 = 3包
     */
    uint16_t total_packets = (file_size + SERIAL_DATA_CHUNK_SIZE - 1) / SERIAL_DATA_CHUNK_SIZE;
    uint16_t current_packet = 0;  /* 当前包序号，从0开始 */
    size_t total_sent = 0;        /* 已发送的总字节数 */
    
    /* 循环读取并发送数据包，直到所有数据发送完毕 */
    while (total_sent < file_size)
    {
        /* 从文件读取一块数据（最多1024字节） */
        size_t read_len = read(fd, data_buffer, SERIAL_DATA_CHUNK_SIZE);
        if (read_len <= 0)
        {
            ESP_LOGE(TAG, "Failed to read firmware data at offset %d", total_sent);
            ret = ESP_FAIL;
            goto cleanup;
        }
        
        /* 如果读取的数据不足1024字节（通常是最后一包），用0xFF填充
         * STM32根据固件大小字段知道实际有效数据长度，会忽略填充数据
         */
        if (read_len < SERIAL_DATA_CHUNK_SIZE)
        {
            memset(data_buffer + read_len, 0xFF, SERIAL_DATA_CHUNK_SIZE - read_len);
        }
        
        /* 发送数据包并等待ACK */
        ret = send_firmware_data_packet(uart_num, current_packet, data_buffer, SERIAL_DATA_CHUNK_SIZE);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to send packet %d", current_packet);
            goto cleanup;
        }
        
        /* 更新进度计数器 */
        current_packet++;
        total_sent += read_len;
        
        /* 打印传输进度，方便调试和监控 */
        ESP_LOGI(TAG, "Progress: %d/%d packets (%d/%d bytes, %.1f%%)", 
                 current_packet, total_packets, total_sent, (int)file_size,
                 (float)total_sent * 100 / file_size);
    }
    
    ESP_LOGI(TAG, "Step 5: Firmware data transmission completed");
    
    /*==================== 阶段4：结束阶段 ====================*/
    
    /* Step 6: 发送升级结束命令
     * 目的：通知STM32固件传输完成，STM32将重启并运行新固件
     * 数据包：0x66 0x07 0xBB
     * STM32收到后会验证固件完整性，然后重启
     */
    ESP_LOGI(TAG, "Step 6: Sending upgrade end command...");
    ret = send_upgrade_end(uart_num);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send upgrade end");
        goto cleanup;
    }
    ESP_LOGI(TAG, "Step 6: Upgrade end acknowledged, STM32 will restart");
    
    ESP_LOGI(TAG, "=== Firmware transmission completed successfully! ===");
    
    /*==================== 清理资源 ====================*/
cleanup:
    /* 关闭文件描述符 */
    if (fd >= 0)
    {
        close(fd);
    }
    
    /* 释放数据缓冲区内存 */
    if (data_buffer != NULL)
    {
        free(data_buffer);
    }
    /* 传输失败退出前，先发ABORT让STM32重置回IDLE，保证下次重试时双端状态对齐 */
    if (ret != ESP_OK)
    {
        send_abort(uart_num);
    }

    /*===== 旧方案（已废弃，注释保留对比）=====
    vTaskResume(recive_buff1_handle);
    ESP_LOGI(TAG, "Resumed UART receive task");
     * 问题：任务恢复后要消化挂起期间积压的过期UART_DATA事件，
     * 拿过期事件读空ringbuffer会持锁阻塞1秒，还可能解析出垃圾
     * 数据误发0xFF重发请求给STM32。
     *=======================================*/

    /* 新方案：解锁三件套
     * 1. 清标志：让接收任务恢复正常处理（顺序必须在send_abort之后，
     *    否则ABORT的ACK会被接收任务抢走）
     * 2. uart_flush_input：清掉传输期间可能残留的字节
     * 3. xQueueReset：丢弃积压的过期事件，防止接收任务醒来后
     *    拿陈旧事件去读空缓冲区、误发0xFF给STM32
     */
    g_ota_uart_busy = false;
    uart_flush_input(uart_num);
    xQueueReset(uart_queue);
    ESP_LOGI(TAG, "UART unlocked, stale events flushed");
    
    /* 打印最终结果 */
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Firmware transmission failed!");
    }
    
    return ret;
}







/**
 * @brief 设置固件信息（在传输前调用）
 * @param firmware_size 固件大小（字节）
 * @param version 固件版本号字符串
 * @param fw_crc 整个固件文件的CRC32（端到端校验值）
 * @note 该函数将固件信息保存到全局变量，供后续传输使用
 */
void set_firmware_info(int firmware_size, const char* version, uint32_t fw_crc)
{
    g_firmware_size = firmware_size;
    g_firmware_crc32 = fw_crc;
    if (version != NULL)
    {
        /* 安全复制版本号字符串，防止缓冲区溢出 */
        strncpy(g_firmware_version, version, sizeof(g_firmware_version) - 1);
        g_firmware_version[sizeof(g_firmware_version) - 1] = '\0';  /* 确保字符串以NULL结尾 */
    }
}
