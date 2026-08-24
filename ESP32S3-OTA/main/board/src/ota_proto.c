#include "myuart.h"
#include <stdint.h>
#include "driver/uart.h"
#include "esp_crc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/projdefs.h"
#include <inttypes.h>

/*============================================================================
 *                           全局变量定义
 *============================================================================*/

static const char *TAG = "ota_proto";
volatile uint8_t g_last_nack_err = 0;       /* NACK错误标志位 */
volatile uint8_t g_start_transmission_timeout = 0;

/**
 * @brief 获取 OTA 简单命令的人类可读名称
 * @param cmd 协议命令字
 * @return 命令名称字符串
 * @note 仅用于任务上下文日志，不参与协议判定。
 */
static const char *OTA_GetCommandName(uint8_t cmd)
{
    switch (cmd)
    {
        case SERIAL_CMD_UPGRADE_NOTIFY: return "upgrade notify";
        case SERIAL_CMD_CONFIRM:        return "handshake confirm";
        case SERIAL_CMD_START_TRANS:    return "start transfer";
        case SERIAL_CMD_FW_INFO:        return "firmware info";
        case SERIAL_CMD_UPGRADE_END:    return "upgrade end";
        case SERIAL_CMD_ABORT:          return "abort";
        case SERIAL_CMD_VERSION_REQUEST:return "version request";
        default:                        return "firmware data/unknown";
    }
}

/**
 * @brief 获取 NACK 错误码的可读说明
 * @param err_code STM32 返回的 NACK 错误码
 * @return 错误说明字符串
 * @note 说明与 ota_proto.h 中的 NACK_ERR_xxx 常量保持一一对应。
 */
static const char *OTA_GetNackReason(uint8_t err_code)
{
    switch (err_code)
    {
        case NACK_ERR_FRAME_CRC:      return "frame CRC error, retryable";
        case NACK_ERR_STATE_MISMATCH: return "state mismatch, abort then retry";
        case NACK_ERR_BAD_LENGTH:     return "invalid frame length, retryable";
        case NACK_ERR_FLASH_ERROR:    return "Flash/storage error, unrecoverable";
        case NACK_ERR_FW_CRC_FAIL:    return "firmware CRC error, unrecoverable";
        case NACK_ERR_UNKNOWN_CMD:    return "unknown command";
        case NACK_ERR_PACKET_SEQ:     return "packet sequence error, retryable";
        case NACK_ERR_PREVIOUS_ROLLBACK: return "previous OTA rolled back, close this session";
        default:                      return "undefined NACK code";
    }
}

/*============================================================================
 *                       固件传输协议实现函数
 *============================================================================*/

/**
 * @brief 带重试机制的数据包发送函数
 * @param uart_num 串口号
 * @param data 待发送数据指针
 * @param len 数据长度
 * @param expected_cmd 期望收到的应答命令码
 * @return ESP_OK成功，ESP_FAIL失败
 * 
 * @note 该函数实现了停止等待协议（Stop-and-Wait）：
 *       1. 发送数据包
 *       2. 等待接收方返回ACK
 *       3. 如果超时未收到正确ACK，则重发
 *       4. 重复步骤1-3，直到成功或达到最大重试次数
 *       
 *       这种协议虽然效率较低，但实现简单且可靠性高，
 *       适合固件传输这种对可靠性要求极高的场景。
 */
static esp_err_t send_packet_with_retry_timeout_limit(uart_port_t uart_num,
                                                       const uint8_t *data,
                                                       size_t len,
                                                       uint8_t expected_cmd,
                                                       uint32_t response_timeout_ms,
                                                       uint8_t max_retry_count)
{
    uint8_t recv_buffer[8] = {0};
    int retry_count = 0;
    uint8_t tx_cmd = data[1];

     g_last_nack_err = 0;    /* 每次进入先清除上次错误码 */


    if (max_retry_count == 0U)
    {
        max_retry_count = 1U;
    }

    while (retry_count < max_retry_count)
    {
        /* 清空接收缓冲区，避免上次残留数据干扰 */
        uart_flush_input(uart_num);

        /* 发送数据包 */
        uart_write_bytes(uart_num,data,len);
        if (tx_cmd != SERIAL_CMD_FW_DATA)
        {
            ESP_LOGI(TAG, "[OTA][TX] 66 %02X BB (%s), waiting for ACK, retry=%d",
                     tx_cmd, OTA_GetCommandName(tx_cmd), retry_count);
        }

        /* 先读3字节：ACK是3字节(66 02 BB)，NACK前3字节是(66 0A err)，读3字节就可以辨认出是正常ACK，还是NACK了 */
        int recv_len  = uart_read_bytes(uart_num,recv_buffer,3,pdMS_TO_TICKS(response_timeout_ms));

        /* 情况1：收到期望的ACK，成功 */
        if (recv_len == 3 && 
            recv_buffer[0] == SERIAL_PACKET_HEAD &&
            recv_buffer[1] == expected_cmd &&
            recv_buffer[2] == SERIAL_PACKET_TAIL)
        {
            if (tx_cmd != SERIAL_CMD_FW_DATA)
            {
                ESP_LOGI(TAG, "[OTA][RX] 66 02 BB ACK for 0x%02X (%s)",
                         tx_cmd, OTA_GetCommandName(tx_cmd));
            }
            return ESP_OK; // 成功直接返回！
        }


        /* 情况2：收到NACK，读第4字节(包尾)确认并提取错误码 */
        if (recv_len == 3 && 
            recv_buffer[0] == SERIAL_PACKET_HEAD &&
            recv_buffer[1] == SERIAL_CMD_NACK 
            )
        {
            uint8_t err_code = recv_buffer[2];
            int n = uart_read_bytes(uart_num,&recv_buffer[3],1,pdMS_TO_TICKS(200));

            if (n == 1 && recv_buffer[3] == SERIAL_PACKET_TAIL)
            {
                g_last_nack_err = err_code;
                ESP_LOGE(TAG, "[OTA][RX] NACK for 0x%02X (%s): error=0x%02X, %s",
                         tx_cmd, OTA_GetCommandName(tx_cmd), err_code,
                         OTA_GetNackReason(err_code));

                /* 可恢复错误(传输干扰)：重发同一帧可能恢复，继续重试 */
                if (err_code == NACK_ERR_FRAME_CRC ||
                    err_code == NACK_ERR_BAD_LENGTH ||
                    err_code == NACK_ERR_PACKET_SEQ
                    ) 
                {
                    retry_count++;
                    continue;   /* 走正常重试 */


                }

                /* 不可恢复错误(状态错位/Flash/固件问题)：重发无意义，立即失败
                 * 上层会发ABORT对齐状态或直接放弃上报 */
                return ESP_FAIL;


            }


        }


        /* 情况3：超时或垃圾数据：可能丢包，重试 */
        ESP_LOGW(TAG, "Invalid response or timeout, retry %d/%d", retry_count + 1, max_retry_count);
        retry_count++;






    }

    ESP_LOGE(TAG, "Max retry count reached, failed to get ACK");
    return ESP_FAIL;



}


static esp_err_t send_packet_with_retry_timeout(uart_port_t uart_num,
                                                 const uint8_t *data,
                                                 size_t len,
                                                 uint8_t expected_cmd,
                                                 uint32_t response_timeout_ms)
{
    return send_packet_with_retry_timeout_limit(uart_num, data, len, expected_cmd,
                                                response_timeout_ms,
                                                SERIAL_MAX_RETRY_COUNT);
}


esp_err_t send_packet_with_retry(uart_port_t uart_num, const uint8_t *data, size_t len, uint8_t expected_cmd)
{
    return send_packet_with_retry_timeout(uart_num, data, len, expected_cmd, SERIAL_TIMEOUT_MS);
}





/**
 * @brief 发送ABORT命令，通知STM32放弃本次传输并重置回IDLE
 * @note 用在两种场景：
 *       1. 传输彻底失败后，对齐双端状态再退出
 *       2. 不可恢复NACK(状态不匹配)后，拉回IDLE准备整体重试
 */
esp_err_t send_abort(uart_port_t uart_num)
{
    uint8_t packet[3] = {SERIAL_PACKET_HEAD, SERIAL_CMD_ABORT, SERIAL_PACKET_TAIL};
    uint8_t saved_nack = g_last_nack_err;   /* 保存，防止被本次调用清掉 */
    esp_err_t ret;
    ESP_LOGW(TAG, "Sending ABORT command...");
    /* STM32掉电时不应在这里再进行5×8秒的无意义等待。 */
    ret = send_packet_with_retry_timeout_limit(uart_num, packet, 3, SERIAL_CMD_ACK,
                                               SERIAL_ABORT_TIMEOUT_MS,
                                               SERIAL_ABORT_MAX_RETRY);
    g_last_nack_err = saved_nack;           /* 恢复，让上层日志打印真实错误码 */
    return ret;
}

/**
 * @brief 请求运行中的STM32 APP重新上报当前版本。
 * @param uart_num 串口号。
 * @return ESP_OK表示查询帧已完整写入UART发送缓冲区，其他值表示发送失败。
 * @note 查询响应仍使用现有55 05版本帧，不能在这里直接读取UART，否则会和常驻接收任务争抢字节。
 */
esp_err_t send_version_request(uart_port_t uart_num)
{
    const uint8_t packet[3] = {
        SERIAL_PACKET_HEAD,
        SERIAL_CMD_VERSION_REQUEST,
        SERIAL_PACKET_TAIL
    };
    int written_length;

    if (g_ota_uart_busy)
    {
        ESP_LOGW(TAG, "[VERSION][TX] OTA owns UART, request rejected");
        return ESP_ERR_INVALID_STATE;
    }

    written_length = uart_write_bytes(uart_num,
                                      (const char *)packet,
                                      sizeof(packet));
    if (written_length != (int)sizeof(packet))
    {
        ESP_LOGE(TAG, "[VERSION][TX] request write failed, length=%d",
                 written_length);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "[VERSION][TX] 66 09 BB request current APP version");
    return ESP_OK;
}

















/**
 * @brief 发送升级通知帧
 * @param uart_num 串口号
 * @return ESP_OK成功，ESP_FAIL失败
 * @note 数据包格式：0x66 0x01 0xBB（包头 + 升级通知命令 + 包尾）
 */
esp_err_t send_handshake_notify(uart_port_t uart_num)
{
    uint8_t packet[3] = {SERIAL_PACKET_HEAD, SERIAL_CMD_UPGRADE_NOTIFY, SERIAL_PACKET_TAIL};
    return send_packet_with_retry(uart_num, packet, 3, SERIAL_CMD_ACK);
}

/**
 * @brief 发送握手确认帧
 * @param uart_num 串口号
 * @return ESP_OK成功，ESP_FAIL失败
 * @note 数据包格式：0x66 0x03 0xBB（包头 + 确认命令 + 包尾）
 */
esp_err_t send_handshake_confirm(uart_port_t uart_num)
{
    uint8_t packet[3] = {SERIAL_PACKET_HEAD, SERIAL_CMD_CONFIRM, SERIAL_PACKET_TAIL};
    /* Bootloader只等待二次握手5秒，不能沿用普通命令的8秒单次超时。 */
    return send_packet_with_retry_timeout_limit(uart_num, packet, 3, SERIAL_CMD_ACK,
                                                SERIAL_HANDSHAKE_TIMEOUT_MS,
                                                SERIAL_HANDSHAKE_MAX_RETRY);
}

/**
 * @brief 发送固件信息帧
 * @param uart_num 串口号
 * @param firmware_size 固件大小（字节）
 * @param version 固件版本号字符串
 * @param fw_crc 整个固件文件的CRC32（端到端校验值）
 * @return ESP_OK成功，ESP_FAIL失败
 * 
 * @note 数据包格式（共25字节）：
 *       | 字节偏移 | 内容          | 说明                            |
 *       |----------|---------------|--------------------------------|
 *       | 0        | 0x66          | 包头                            |
 *       | 1        | 0x05          | 固件信息命令                     |
 *       | 2-5      | firmware_size | 固件大小（大端序，4字节）         |
 *       | 6-7      | 0x2F 0x2F     | 分隔符                          |
 *       | 8-15     | version       | 版本号（8字节，不足补0）         |
 *       | 16-19    | fw_crc        | 固件整体CRC32（大端序，4字节）   |
 *       | 20-23    | frame_crc     | 帧CRC32（大端序，4字节）         |
 *       | 24       | 0xBB          | 包尾                            |
 *       
 *       固件整体CRC32：对整个固件文件计算，STM32端收完数据后用它做端到端校验。
 *       帧CRC32：对 packet[1]~packet[19]（命令+大小+分隔符+版本号+固件CRC32，共19字节）计算，保证帧自身完整。
 *       
 *       大端序说明：高字节在前，低字节在后
 *       例如：固件大小0x00012345，存储为：00 01 23 45
 */
esp_err_t send_firmware_info(uart_port_t uart_num, int firmware_size, const char* version, uint32_t fw_crc)
{
    uint8_t packet[SERIAL_FW_INFO_PACKET_SIZE];
    int pos = 0;
    
    /* 构建数据包头部 */
    packet[pos++] = SERIAL_PACKET_HEAD;      /* 包头 */
    packet[pos++] = SERIAL_CMD_FW_INFO;      /* 命令码 */
    
    /* 固件大小：4字节，大端序存储
     * 大端序：高位字节存储在低地址
     * >> 24: 取最高字节
     * >> 16: 取次高字节
     * >> 8:  取次低字节
     * & 0xFF: 取最低字节
     */
    packet[pos++] = (firmware_size >> 24) & 0xFF;  /* 最高字节 */
    packet[pos++] = (firmware_size >> 16) & 0xFF;
    packet[pos++] = (firmware_size >> 8) & 0xFF;
    packet[pos++] = firmware_size & 0xFF;          /* 最低字节 */
    
    /* 分隔符：用于区分固件大小和版本号 */
    packet[pos++] = SERIAL_PACKET_SEPARATOR;
    packet[pos++] = SERIAL_PACKET_SEPARATOR;
    
    /* 版本号：固定8字节
     * 例如版本号"v1.0.2"存储为：'v' '1' '.' '0' '.' '2' 0x00 0x00 0x00
     * 版本号"v1.0"存储为：'v' '1' '.' '0' 0x00 0x00 0x00 0x00
     * 不足8字节时后面填充0x00
     */
    int version_len = strlen(version);
    for (int i = 0; i < 8; i++)
    {
        if (i < version_len)
        {
            packet[pos++] = version[i];
        }
        else
        {
            packet[pos++] = 0x00;  /* 填充0x00 */
        }
    }

    /* 固件整体CRC32：4字节，大端序存储
     * 这是整个固件文件的CRC32，STM32端收完数据后用它做端到端校验
     */
    packet[pos++] = (fw_crc >> 24) & 0xFF;
    packet[pos++] = (fw_crc >> 16) & 0xFF;
    packet[pos++] = (fw_crc >> 8) & 0xFF;
    packet[pos++] = fw_crc & 0xFF;
    
    /* 帧CRC32校验值
     * 校验范围：从命令码开始到固件CRC32结束（不包含包头和帧CRC32本身）
     * 即：packet[1] 到 packet[19]，共19字节
     */
    uint32_t frame_crc = calculate_crc32(&packet[1], pos - 1);
    
    /* 帧CRC32：4字节，大端序存储 */
    packet[pos++] = (frame_crc >> 24) & 0xFF;
    packet[pos++] = (frame_crc >> 16) & 0xFF;
    packet[pos++] = (frame_crc >> 8) & 0xFF;
    packet[pos++] = frame_crc & 0xFF;
    
    /* 包尾 */
    packet[pos++] = SERIAL_PACKET_TAIL;
    
    ESP_LOGI(TAG, "Sending firmware info: size=%d, version=%s, fw_crc=0x%08" PRIx32 ", frame_crc=0x%08" PRIx32,
             firmware_size, version, fw_crc, frame_crc);
    
    return send_packet_with_retry(uart_num, packet, pos, SERIAL_CMD_ACK);
}

/**
 * @brief 发送固件数据包
 * @param uart_num 串口号
 * @param packet_seq 数据包序号（0~N-1）
 * @param data 数据指针（1024字节）
 * @param data_len 数据长度
 * @return ESP_OK成功，ESP_FAIL失败
 * 
 * @note 数据包格式（共1037字节）：
 *       | 字节偏移   | 内容        | 说明                      | CRC校验 |
 *       |------------|-------------|--------------------------|---------|
 *       | 0          | 0x66        | 包头                      | 不参与  |
 *       | 1          | 0x06        | 固件数据命令               | 不参与  |
 *       | 2-3        | packet_seq  | 包序号（大端序，2字节）     | 不参与  |
 *       | 4-7        | data_len    | 数据长度（大端序，4字节）   | 不参与  |
 *       | 8-1031     | data        | 固件数据（1024字节）       | 参与    |
 *       | 1032-1035  | CRC32       | 校验值（大端序，4字节）    | 不参与  |
 *       | 1036       | 0xBB        | 包尾                      | 不参与  |
 *       
 *       CRC32校验说明：
 *       - 只对固件数据部分（1024字节）计算CRC
 *       - 命令字、包序号、数据长度由协议保证正确性，无需校验
 *       - 使用标准CRC-32算法，与STM32硬件CRC兼容
 *       
 *       包序号的作用：
 *       - 用于检测丢包：如果STM32收到的序号不连续，说明有丢包
 *       - 用于检测乱序：如果STM32收到的序号顺序不对，说明数据乱序
 *       - 支持断点续传：记录最后成功接收的序号，可以从该位置继续
 */
esp_err_t send_firmware_data_packet(uart_port_t uart_num, uint16_t packet_seq, const uint8_t* data, size_t data_len)
{
    /* 动态分配数据包缓冲区 */
    uint8_t* packet = malloc(SERIAL_FW_DATA_PACKET_SIZE);
    if (packet == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate packet buffer");
        return ESP_FAIL;
    }
    
    int pos = 0;
    
    /* 构建数据包头部 */
    packet[pos++] = SERIAL_PACKET_HEAD;      /* 包头 */
    packet[pos++] = SERIAL_CMD_FW_DATA;      /* 命令码 */
    
    /* 包序号：2字节，大端序存储 */
    packet[pos++] = (packet_seq >> 8) & 0xFF;   /* 高字节 */
    packet[pos++] = packet_seq & 0xFF;          /* 低字节 */
    
    /* 数据长度：4字节，大端序存储 */
    uint32_t len_val = data_len;
    packet[pos++] = (len_val >> 24) & 0xFF;
    packet[pos++] = (len_val >> 16) & 0xFF;
    packet[pos++] = (len_val >> 8) & 0xFF;
    packet[pos++] = len_val & 0xFF;
    
    /* 复制固件数据 */
    memcpy(&packet[pos], data, data_len);
    pos += SERIAL_DATA_CHUNK_SIZE;
    
    /* 计算CRC32校验值 - 只校验固件数据部分（1024字节）
     * 命令字、包序号、数据长度由协议保证正确性，无需校验
     */
    uint32_t crc = calculate_crc32(data, data_len);
    
    /* CRC32：4字节，大端序存储 */
    packet[pos++] = (crc >> 24) & 0xFF;
    packet[pos++] = (crc >> 16) & 0xFF;
    packet[pos++] = (crc >> 8) & 0xFF;
    packet[pos++] = crc & 0xFF;
    
    /* 包尾 */
    packet[pos++] = SERIAL_PACKET_TAIL;
    
    ESP_LOGI(TAG, "Sending data packet: seq=%d, len=%d, CRC=0x%08" PRIx32, packet_seq, data_len, crc);
    
    /* 发送数据包并等待ACK */
    esp_err_t ret = send_packet_with_retry(uart_num, packet, pos, SERIAL_CMD_ACK);
    
    /* 释放缓冲区 */
    free(packet);
    
    
    return ret;
}

/**
 * @brief 发送升级结束帧
 * @param uart_num 串口号
 * @return ESP_OK成功，ESP_FAIL失败
 * @note 数据包格式：0x66 0x07 0xBB（包头 + 升级结束命令 + 包尾）
 *       STM32收到此命令后会重启并运行新固件
 */
esp_err_t send_upgrade_end(uart_port_t uart_num)
{
    uint8_t packet[3] = {SERIAL_PACKET_HEAD, SERIAL_CMD_UPGRADE_END, SERIAL_PACKET_TAIL};
    return send_packet_with_retry(uart_num, packet, 3, SERIAL_CMD_ACK);
}

/**
 * @brief 发送固件开始命令
 * @param uart_num 串口号
 * @return ESP_OK成功，ESP_FAIL失败
 * @note 数据包格式：0x66 0x04 0xBB（包头 + 开始传输命令 + 包尾）
 *       该命令通知STM32即将开始发送固件数据，STM32完成檫除后才会应答
 */
esp_err_t send_start_transmission(uart_port_t uart_num)
{
    uint8_t packet[3] = {SERIAL_PACKET_HEAD, SERIAL_CMD_START_TRANS, SERIAL_PACKET_TAIL};
    esp_err_t ret;

    g_start_transmission_timeout = 0U;
    ret = send_packet_with_retry_timeout_limit(uart_num, packet, 3, SERIAL_CMD_ACK,
                                               SERIAL_START_TRANS_TIMEOUT_MS,
                                               SERIAL_START_TRANS_MAX_RETRY);
    if (ret != ESP_OK && g_last_nack_err == 0U)
    {
        /* 没有收到任何NACK，说明STM32在整个备份/擦除窗口内无响应。 */
        g_start_transmission_timeout = 1U;
    }
    return ret;
}
