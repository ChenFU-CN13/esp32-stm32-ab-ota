#ifndef __UART2_COMM_H
#define __UART2_COMM_H

#include <stdint.h>

/* ============================= 帧协议总览 =============================
 * 1) 业务帧（STM32 <-> ESP32），固定 8 字节：
 *        0x55 | type | data[0..4] | 0xBB
 *    上行 type：0x01 湿度 / 0x02 温度 / 0x03 LED状态 / 0x04 蜂鸣器状态
 *              0x05 APP版本 / 0x06 烟雾ADC / 0x07 光照 / 0x08 风扇占空比
 *    下行 type：0x7E 业务ACK / 0x0A 心跳
 * 2) OTA 控制帧（上位机/ESP32 <-> STM32），固定 3 字节：
 *        0x66 | cmd | 0xBB
 *    cmd：0x01 升级通知 / 0x02 升级ACK / 0x09 版本查询
 * 3) 帧内不含校验和与序号：接收端靠"帧头 + 固定长度 + 帧尾"定界，
 *    可靠性由 200 ms ACK 超时 + 最多 3 次重发保证（业务帧串行发送）。
 * ===================================================================== */

/* ---------------------- 业务帧（0x55）公共字段 ---------------------- */
#define UART2_COMM_BUSINESS_HEAD       0x55U   /* 业务帧帧头，同时用于确定本帧固定收 8 字节 */
#define UART2_COMM_BUSINESS_TAIL       0xBBU   /* 业务帧帧尾，接收端用它做最后一道格式校验 */
#define UART2_COMM_BUSINESS_ACK_TYPE   0x7EU   /* 业务ACK类型(ESP32->STM32)：data[0]=结果(0成功/非0失败)，data[1..4]必须为0 */
#define UART2_COMM_HEARTBEAT_TYPE      0x0AU   /* 心跳类型(ESP32->STM32)：data[0]=在线状态(0离线/1在线)，data[1..4]必须为0 */

/* ------------------- OTA/控制帧（0x66）公共字段 ------------------- */
#define UART2_COMM_OTA_HEAD            0x66U   /* OTA/控制帧帧头，同时用于确定本帧固定收 3 字节 */
#define UART2_COMM_OTA_NOTIFY_CMD      0x01U   /* 升级通知(下行)：66 01 BB；STM32回ACK后复位进入Bootloader */
#define UART2_COMM_OTA_ACK_CMD         0x02U   /* 升级通知ACK(上行)：66 02 BB；DMA发完最后一个字节才允许复位 */
#define UART2_COMM_VERSION_REQUEST_CMD 0x09U   /* 版本查询(下行)：66 09 BB；STM32以业务帧 0x55 0x05 上报版本 */
#define UART2_COMM_OTA_TAIL            0xBBU   /* OTA/控制帧帧尾 */

/**
 * @brief 初始化 UART2 通讯任务、内部队列和单字节中断接收。
 * @param 无。
 * @return 无。
 * @note 应在原有业务消息队列 xUART2_TxQueue 创建完成后调用。
 */
void UART2_Comm_Start(void);

/**
 * @brief 从 USART2 接收中断转交一个字节给通讯模块。
 * @param byte 本次接收完成的字节。
 * @return 无。
 * @note 仅限 ISR/回调调用；函数不做完整帧解析。
 */
void UART2_Comm_RxByteFromISR(uint8_t byte);

/**
 * @brief 从 USART2 DMA 发送完成回调通知通讯模块。
 * @param 无。
 * @return 无。
 * @note 仅限 ISR/回调调用；不会在这里复位系统。
 */
void UART2_Comm_TxCompleteFromISR(void);

/**
 * @brief 从 USART2 错误回调通知通讯模块放弃当前半帧。
 * @param 无。
 * @return 无。
 * @note 仅限 ISR/回调调用；实际解析状态由通讯任务安全地重置。
 */
void UART2_Comm_RxErrorFromISR(void);

#endif
