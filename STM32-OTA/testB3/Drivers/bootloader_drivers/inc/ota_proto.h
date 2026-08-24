/**
 * @file    ota_proto.h
 * @brief   协议帧模块头文件
 * @details 归口：协议常量（帧头尾/命令码/NACK码/帧长/超时）、
 *          固件信息帧/数据帧数据结构、CRC32 计算、帧发送与解析。
 *          本模块为纯函数、无状态（逻辑由原 TaskTest.h 拆分而来）。
 */

#ifndef OTA_PROTO_H
#define OTA_PROTO_H

#include "main.h"


/*============================================================================*/
/*                          OTA协议常量定义                                    */
/*============================================================================*/

/**
 * @brief 协议帧头尾标识
 *
 * 所有协议帧都以0x66开头，以0xBB结尾
 * 这样设计便于帧同步和边界识别
 */
#define OTA_PACKET_HEAD                 0x66
#define OTA_PACKET_TAIL                 0xBB

/**
 * @brief 协议命令字定义
 *
 * 命令流程:
 *   1. 0x01: 升级通知 - ESP32通知A区有新固件(A区处理)
 *   2. 0x02: ACK应答 - STM32收到命令后的确认响应
 *   3. 0x03: 二次握手 - ESP32确认升级，STM32重启后等待此命令
 *   4. 0x04: 开始传输 - ESP32通知开始传输固件，STM32擦除Flash
 *   5. 0x05: 固件信息 - 包含固件大小、版本号、CRC32校验值
 *   6. 0x06: 固件数据 - 实际的固件数据包(每包最多1024字节)
 *   7. 0x07: 升级结束 - ESP32通知所有数据发送完毕
 *   8. 0x08: ESP32→STM32: 放弃本次传输，STM32重置回IDLE
 *   9. 0x0A: STM32→ESP32: 否定应答，携带1字节错误码
 */
#define OTA_CMD_UPGRADE_NOTIFY          0x01    /* ESP32→STM32: 升级通知，A区处理 */
#define OTA_CMD_ACK                     0x02    /* STM32→ESP32: 确认应答 */
#define OTA_CMD_CONFIRM                 0x03    /* ESP32→STM32: 确认升级，STM32重启后等待此命令 */
#define OTA_CMD_START_TRANSFER          0x04    /* ESP32→STM32: 开始传输，STM32擦除Flash */
#define OTA_CMD_FIRMWARE_INFO           0x05    /* ESP32→STM32: 固件信息，包含固件大小、版本号、CRC32校验值 */
#define OTA_CMD_FIRMWARE_DATA           0x06    /* ESP32→STM32: 固件数据，实际的固件数据包(每包最多1024字节) */
#define OTA_CMD_UPGRADE_END             0x07    /* ESP32→STM32: 升级结束，STM32通知所有数据发送完毕 */
#define OTA_CMD_ABORT                   0x08    /* ESP32→STM32: 放弃本次传输，STM32重置回IDLE */
#define OTA_CMD_NACK                    0x0A    /* STM32→ESP32: 否定应答，携带1字节错误码 */


/*NACK错误码定义*/
#define  OTA_NACK_FRAME_CRC             0x01        /* 帧CRC校验失败(传输干扰，可重发恢复) */
#define  OTA_NACK_STATE_MISMATCH        0x02         /* 状态不匹配(时序错位，需ABORT对齐) */
#define  OTA_NACK_BAD_LENGTH            0x03          /* 帧长非法 */
#define OTA_NACK_FLASH_ERROR            0x04        /* Flash擦除/写入失败(硬件问题) */
#define OTA_NACK_FW_CRC_FAIL            0x05        /* 固件整体CRC校验失败 */
#define OTA_NACK_UNKNOWN_CMD            0x06        /* 未知命令 */
#define OTA_NACK_PACKET_SEQ             0x07        /* 包序号异常(乱序) */


/* NACK帧格式: | 0x66 | 0x0A | err_code | 0xBB | 共4字节 */
#define OTA_NACK_PREVIOUS_ROLLBACK      0x08        /* STM32已恢复旧镜像，要求结束上一轮会话 */
#define OTA_NACK_FRAME_LEN             4


/**
 * @brief 协议帧长度定义
 *
 * 简单命令帧格式 (3字节):
 *   | 包头(0x66) | 命令字 | 包尾(0xBB) |
 *
 * 固件信息帧格式 (25字节):
 *   | 0x66 | 0x05 | 固件大小(4B,大端序) | 分隔符(0x2F 0x2F) | 版本号(8B,ASCII) | 固件CRC32(4B,大端序) | 帧CRC32(4B,大端序) | 0xBB |
 *   说明：固件CRC32 = 整个固件文件的CRC32（端到端校验）；
 *         帧CRC32  = 对"命令+大小+分隔符+版本号+固件CRC32"共19字节计算的CRC（帧完整性）。
 *
 * 数据帧格式 (变长，最大1037字节):
 *   | 0x66 | 0x06 | 包序号(2B,大端序) | 数据长度(4B,大端序) | 数据(≤1024B) | CRC32(4B,大端序) | 0xBB |
 *
 * @note 大端序说明: 高字节在前，低字节在后
 *       例如: 固件大小0x00012345，存储为：00 01 23 45
 * @note 版本号为ASCII字符串，如 "v1.0.2"，不足8字节用0x00填充
 */
#define OTA_SIMPLE_FRAME_LEN            3
#define OTA_INFO_FRAME_LEN              25
#define OTA_DATA_FRAME_HEADER_LEN       8       // 数据帧真头部：包头1+命令1+序号2+长度4=8字节（CRC与包尾另算）
#define OTA_DATA_FRAME_TAIL_LEN         5       // 数据帧尾部：CRC32(4)+包尾(1)=5字节
#define OTA_DATA_PAYLOAD_LEN            1024
#define OTA_DATA_FRAME_MAX_LEN          (OTA_DATA_FRAME_HEADER_LEN + OTA_DATA_PAYLOAD_LEN + OTA_DATA_FRAME_TAIL_LEN)

#define OTA_VERSION_LEN                 8

#define OTA_WAIT_START_TIMEOUT_MS       15000U
#define OTA_WAIT_INFO_TIMEOUT_MS        15000U
#define OTA_WAIT_DATA_TIMEOUT_MS        15000U
#define OTA_WAIT_END_TIMEOUT_MS         10000U

/**
 * @brief 超时和重试配置
 */
#define OTA_HANDSHAKE_TIMEOUT_MS        5000        // 二次握手超时时间
#define OTA_FRAME_TIMEOUT_MS            3000        // 数据帧超时时间
#define OTA_MAX_RETRY_COUNT             3           // 最大重试次数


/*============================================================================*/
/*                          数据结构定义                                       */
/*============================================================================*/

/**
 * @brief OTA固件信息帧数据结构
 *
 * 解析固件信息帧(命令0x05)后的数据存储结构
 *
 * 成员说明:
 *   - firmware_size: 固件总大小(字节)
 *   - firmware_crc32: 整个固件的CRC32校验值
 *   - version[8]: 版本号ASCII字符串，如"v1.0.2"，不足8字节用0x00填充
 */
typedef struct {
    uint32_t firmware_size;
    uint32_t firmware_crc32;
    uint8_t version[OTA_VERSION_LEN];
} OTA_FirmwareInfo_t;


/**
 * @brief OTA数据帧数据结构
 *
 * 解析固件数据帧(命令0x06)后的数据存储结构
 *
 * 成员说明:
 *   - packet_num: 包序号(0~N)，用于检测丢包和乱序
 *   - data_len: 本包数据长度(≤1024字节)
 *   - data[1024]: 固件数据缓冲区
 *   - data_crc32: 本包数据的CRC32校验值
 */
typedef struct {
    uint16_t packet_num;
    uint32_t data_len;
    uint8_t data[OTA_DATA_PAYLOAD_LEN];
    uint32_t data_crc32;
} OTA_DataFrame_t;


/*============================================================================*/
/*                          函数声明                                           */
/*============================================================================*/

/**
 * @brief 计算数据的CRC32校验值
 * @param data 待计算的数据指针
 * @param len 数据长度
 * @return CRC32校验值
 * @note 使用多项式0xEDB88320，与标准CRC32算法兼容
 */
uint32_t OTA_CalcCRC32(uint8_t *data, uint32_t len);

/**
 * @brief 发送简单命令帧
 * @param cmd 命令字(如OTA_CMD_ACK)
 */
void OTA_SendSimpleFrame(uint8_t cmd);

/**
 * @brief 发送ACK应答帧
 */
void OTA_SendACK(void);

/**
 * @brief 发送NACK应答帧
 */
void OTA_SendNACK(uint8_t err_code);

/**
 * @brief 解析简单命令帧
 * @param data 接收到的数据
 * @param len 数据长度
 * @param cmd 输出命令字
 * @return 1-解析成功，0-解析失败
 */
uint8_t OTA_ParseSimpleFrame(uint8_t *data, uint16_t len, uint8_t *cmd);

/**
 * @brief 解析固件信息帧
 * @param data 接收到的数据
 * @param len 数据长度
 * @param info 输出固件信息结构体
 * @return 1-解析成功，0-解析失败
 */
uint8_t OTA_ParseFirmwareInfo(uint8_t *data, uint16_t len, OTA_FirmwareInfo_t *info);

/**
 * @brief 解析固件数据帧
 * @param data 接收到的数据
 * @param len 数据长度
 * @param frame 输出数据帧结构体
 * @return 1-解析成功，0-解析失败
 * @note 会进行CRC32校验，校验失败返回0
 */
uint8_t OTA_ParseDataFrame(uint8_t *data, uint16_t len, OTA_DataFrame_t *frame);


#endif
