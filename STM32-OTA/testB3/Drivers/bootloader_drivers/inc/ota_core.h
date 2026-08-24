/**
 * @file    ota_core.h
 * @brief   OTA 核心状态机模块头文件
 * @details 归口：升级状态机枚举、升级上下文结构体、EEPROM 升级信息结构体、
 *          升级主流程五个核心函数及其全局变量。
 *          ★未来回滚决策（收到 0x04/0x05 → 触发 ota_storage 回滚）也放本模块。
 *          （逻辑由原 TaskTest.h 拆分而来）
 */

#ifndef OTA_CORE_H
#define OTA_CORE_H

#include "main.h"
#include "ota_proto.h"


/*============================================================================*/
/*                          数据结构定义                                       */
/*============================================================================*/

/**
 * @brief OTA升级信息结构体
 *
 * 存储在AT24C02 EEPROM中，用于记录升级状态和版本信息
 * 即使断电也能保持，下次启动可恢复
 *
 * 成员说明:
 *   - Firelen[0]: 最新固件大小(字节)
 *   - Firelen[1-10]: 预留，可用于存储历史固件大小
 *   - OTA_VERSION: 版本号字符串，格式如 "v1.0.0"
 */
typedef struct {
    uint32_t Firelen[11];
    uint8_t OTA_VERSION[32];
} OTA_INFOCB;

#define OTA_INFOCB_SIZE                 sizeof(OTA_INFOCB)


/**
 * @brief Bootloader状态机枚举
 *
 * 状态转换流程:
 *   IDLE -> WAIT_CONFIRM -> WAIT_START -> WAIT_INFO -> WAIT_DATA -> WAIT_END -> UPGRAD_SUCCESS/FAILED -> JUMP_TO_APP
 *
 *   或者在IDLE状态超时后直接跳转:
 *   IDLE -> JUMP_TO_APP
 */
typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_WAIT_CONFIRM,
    OTA_STATE_WAIT_START,
    OTA_STATE_WAIT_INFO,
    OTA_STATE_WAIT_DATA,
    OTA_STATE_WAIT_END,
    OTA_STATE_UPGRADE_SUCCESS,
    OTA_STATE_UPGRADE_FAILED,
    OTA_STATE_ROLLBACK_PENDING,
    OTA_STATE_RECOVERY_FAILED,
    OTA_STATE_JUMP_TO_APP
} OTA_State_t;


/**
 * @brief OTA升级上下文结构体
 *
 * 维护整个升级过程中的状态和数据
 *
 * 成员说明:
 *   - state: 当前状态机状态
 *   - firmware_size: 待接收的固件总大小
 *   - firmware_crc32: 固件的期望CRC32值
 *   - received_size: 已接收的数据大小
 *   - expected_packet: 期望接收的下一个包序号
 *   - total_packets: 固件总包数
 *   - write_addr: 当前Flash写入地址
 *   - retry_count: 重试计数器
 *   - upgrade_flag: 升级标志位
 *   - version: 固件版本号(ASCII字符串)
 */
typedef struct {
    OTA_State_t state;
    uint32_t firmware_size;
    uint32_t firmware_crc32;
    uint32_t received_size;
    uint16_t expected_packet;
    uint16_t total_packets;
    uint32_t write_addr;
    uint8_t retry_count;
    uint8_t upgrade_flag;
    uint8_t version[OTA_VERSION_LEN];
} OTA_Context_t;


/**
 * @brief 全局OTA上下文变量声明
 */
extern OTA_Context_t g_ota_ctx;

/**
 * @brief EEPROM 升级信息全局变量声明（记录最新固件大小/版本）
 */
extern OTA_INFOCB OTA_INFO;


/*============================================================================*/
/*                          函数声明                                           */
/*============================================================================*/

/**
 * @brief 初始化OTA上下文
 */
void OTA_Init(void);

/**
 * @brief 处理接收到的协议帧
 * @param data 数据指针
 * @param len 数据长度
 * @note 根据帧类型和当前状态进行相应处理
 */
void OTA_ProcessFrame(uint8_t *data, uint16_t len);

/**
 * @brief 等待ESP32二次握手
 * @param timeout_ms 超时时间(毫秒)
 * @return 1-收到握手，0-超时
 */
uint8_t OTA_WaitForHandshake(uint32_t timeout_ms);

/**
 * @brief 运行升级主流程
 * @note 这是阻塞函数，直到升级完成或跳转APP才返回
 */
void OTA_RunUpgradeProcess(void);


#endif
