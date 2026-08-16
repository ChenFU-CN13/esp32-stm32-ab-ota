/**
 * @file    TaskTest.h
 * @brief   STM32F407 Bootloader OTA升级协议头文件
 * @details 定义了ESP32与STM32之间的固件传输协议、数据结构、状态机等
 * 
 * @note    协议流程说明:
 *          1. A区(APP)收到升级通知(0x66 0x01 0xBB)后应答ACK，然后重启进入B区(Bootloader)
 *          2. B区启动后等待二次握手(0x66 0x03 0xBB)，超时则跳转A区
 *          3. 收到二次握手后应答ACK，等待三次握手(0x66 0x04 0xBB)
 *          4. 收到三次握手后擦除A区Flash，应答ACK
 *          5. 接收固件信息帧，存储到EEPROM，应答ACK
 *          6. 循环接收数据帧，写入Flash，每包应答ACK
 *          7. 收到升级结束命令(0x66 0x07 0xBB)，校验CRC32，跳转A区
 */

#ifndef TASKTEST_H
#define TASKTEST_H

#include "main.h"
#include "AT24C02.h"
#include "W25Q128.h"
#include "MyFLASH.h"
#include "command.h"


/*============================================================================*/
/*                          Flash分区配置                                      */
/*============================================================================*/

/**
 * @brief STM32F407ZGT6 Flash布局说明
 * 
 * Flash总容量: 1MB (0x08000000 ~ 0x080FFFFF)
 * 共12个扇区，分布如下:
 *   - 扇区0-3:  每个16KB  (共64KB)  -> B区(Bootloader)
 *   - 扇区4:    64KB                 -> A区(APP)起始
 *   - 扇区5-11: 每个128KB (共896KB) -> A区(APP)剩余
 * 
 * B区(Bootloader): 0x08000000 ~ 0x0800FFFF (64KB)
 * A区(APP):        0x08010000 ~ 0x080FFFFF (960KB)
 */

#define F407_FLASH_STARTADDR            0x08000000    
#define F407_FLASH_16SIZE               16  * 1024    
#define F407_FLASH_64SIZE               64  * 1024    
#define F407_FLASH_128SIZE              128 * 1024    
#define FLASH_SECTOR_NUM                12            
#define UpData_single_size              1024          
#define STM32_PAGE_SIZE                 1024          


/**
 * @brief B区(Bootloader)分区配置
 * 
 * B区占用扇区0-3，共4个扇区，每个16KB，总计64KB
 * 存放Bootloader程序，负责接收固件、写入A区、跳转APP
 */
#define FLASH_BOOTLOADER_NUM            4                         
#define FLASH_BOOTLOADER_SIZE           (FLASH_BOOTLOADER_NUM * 16 * 1024)  


/**
 * @brief A区(APP)分区配置
 * 
 * A区占用扇区4-11，共8个扇区，总计960KB
 * 存放用户应用程序
 * 
 * 关键参数说明:
 *   - FLASH_ACode_NUM: A区占用的扇区数量 = 12 - 4 = 8
 *   - FLASH_ACode_STARTNUM: A区起始扇区号 = 4
 *   - FLASH_ACode_STARTADDR: A区起始地址 = 0x08010000
 */
#define FLASH_ACode_NUM                 (FLASH_SECTOR_NUM - FLASH_BOOTLOADER_NUM)
#define FLASH_ACode_STARTNUM            FLASH_BOOTLOADER_NUM    
#define FLASH_ACode_STARTADDR           (0x08000000 + FLASH_BOOTLOADER_SIZE)   


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
 */
#define OTA_CMD_UPGRADE_NOTIFY          0x01        
#define OTA_CMD_ACK                     0x02        
#define OTA_CMD_CONFIRM                 0x03        
#define OTA_CMD_START_TRANSFER          0x04        
#define OTA_CMD_FIRMWARE_INFO           0x05        
#define OTA_CMD_FIRMWARE_DATA           0x06        
#define OTA_CMD_UPGRADE_END             0x07        

/**
 * @brief 协议帧长度定义
 * 
 * 简单命令帧格式 (3字节):
 *   | 包头(0x66) | 命令字 | 包尾(0xBB) |
 * 
 * 固件信息帧格式 (21字节):
 *   | 0x66 | 0x05 | 固件大小(4B,大端序) | 分隔符(0x2F 0x2F) | 版本号(8B,ASCII) | CRC32(4B,大端序) | 0xBB |
 * 
 * 数据帧格式 (变长，最大1037字节):
 *   | 0x66 | 0x06 | 包序号(2B,大端序) | 数据长度(4B,大端序) | 数据(≤1024B) | CRC32(4B,大端序) | 0xBB |
 * 
 * @note 大端序说明: 高字节在前，低字节在后
 *       例如: 固件大小0x00012345，存储为：00 01 23 45
 * @note 版本号为ASCII字符串，如 "v1.0.2"，不足8字节用0x00填充
 */
#define OTA_SIMPLE_FRAME_LEN            3                   
#define OTA_INFO_FRAME_LEN              21                  
#define OTA_DATA_FRAME_HEADER_LEN       12                  
#define OTA_DATA_FRAME_TAIL_LEN         1                   
#define OTA_DATA_PAYLOAD_LEN            1024                
#define OTA_DATA_FRAME_MAX_LEN          (OTA_DATA_FRAME_HEADER_LEN + OTA_DATA_PAYLOAD_LEN + OTA_DATA_FRAME_TAIL_LEN)

#define OTA_VERSION_LEN                 8

/**
 * @brief 超时和重试配置
 */
#define OTA_HANDSHAKE_TIMEOUT_MS        5000        
#define OTA_FRAME_TIMEOUT_MS            3000        
#define OTA_MAX_RETRY_COUNT             3           


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
 * @brief 函数指针类型定义
 * 
 * 用于跳转到A区APP的复位中断向量
 * 函数指针指向APP的复位中断处理函数入口地址
 */
typedef void (*load_a) (void);        


/*============================================================================*/
/*                          函数声明                                           */
/*============================================================================*/

/*============================ CRC32计算函数 ============================*/
/**
 * @brief 计算数据的CRC32校验值
 * @param data 待计算的数据指针
 * @param len 数据长度
 * @return CRC32校验值
 * @note 使用多项式0xEDB88320，与标准CRC32算法兼容
 */
uint32_t OTA_CalcCRC32(uint8_t *data, uint32_t len);


/*============================ 协议帧发送函数 ============================*/
/**
 * @brief 发送简单命令帧
 * @param cmd 命令字(如OTA_CMD_ACK)
 */
void OTA_SendSimpleFrame(uint8_t cmd);

/**
 * @brief 发送ACK应答帧
 */
void OTA_SendACK(void);


/*============================ 协议帧解析函数 ============================*/
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


/*============================ Flash操作函数 ============================*/
/**
 * @brief 擦除A区(APP)Flash
 * @return HAL_OK-成功，HAL_ERROR-失败
 * @note 擦除扇区4-11，这是耗时操作
 */
HAL_StatusTypeDef OTA_EraseAppRegion(void);

/**
 * @brief 将固件数据写入Flash
 * @param addr 目标地址
 * @param data 数据指针
 * @param len 数据长度
 * @return HAL_OK-成功，HAL_ERROR-失败
 * @note 自动处理4字节对齐，不足部分填充0xFF
 */
HAL_StatusTypeDef OTA_WriteFirmwareData(uint32_t addr, uint8_t *data, uint32_t len);

/**
 * @brief 校验已写入Flash的固件CRC32
 * @return 1-校验成功，0-校验失败
 */
uint8_t OTA_VerifyFirmwareCRC(void);


/*============================ Bootloader核心函数 ============================*/
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


/*============================ 跳转函数 ============================*/
/**
 * @brief 卸载外设配置环境
 * @note 在跳转APP前调用，清理Bootloader的外设配置，避免与APP冲突
 */
void BootLoader_Clear(void);

/**
 * @brief 跳转到A区APP
 * @param address APP起始地址
 * @note 会检查栈指针合法性，设置向量表重映射，跳转到复位中断
 */
void Load_A(uint32_t address);

/**
 * @brief 主循环函数
 * @note 在main函数的while(1)中调用
 */
void Task_While(void);

/**
 * @brief 主任务初始化函数
 * @note 在main函数中，外设初始化后调用
 */
void Task_Main(void);




#endif
