/**
 * @file    TaskTest.h
 * @brief   任务入口层头文件（薄壳）
 * @details 仅声明任务入口函数 Task_Main / Task_While。
 *          其余 Bootloader 核心数据结构 / 函数已拆分到：
 *            - bootloader.h   ：跳转函数、Flash 分区宏
 *            - ota_proto.h    ：协议常量、帧结构体、CRC/帧构造解析
 *            - ota_storage.h  ：Flash 擦除/写入/整体 CRC 校验
 *            - ota_core.h     ：状态机枚举、升级上下文、状态机函数
 */

#ifndef TASKTEST_H
#define TASKTEST_H

#include "main.h"


/*============================================================================*/
/*                          函数声明                                           */
/*============================================================================*/

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


