/**
 * @file    bootloader.h
 * @brief   Bootloader 跳转模块头文件
 * @details 归口：Flash 分区布局配置、跳转函数指针类型、跳转相关函数与全局变量。
 *          （由原 TaskTest.h 拆分而来，逻辑未改动）
 */

#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include "main.h"


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
/*                          跳转函数指针类型                                   */
/*============================================================================*/

/**
 * @brief 函数指针类型定义
 *
 * 用于跳转到A区APP的复位中断向量
 * 函数指针指向APP的复位中断处理函数入口地址
 */
typedef void (*load_a) (void);


/**
 * @brief 跳转目标函数指针变量（由 bootloader.c 定义）
 * @note  值在跳转流程中赋值为 A 区 APP 的复位向量
 */
extern load_a LOAD_A;


/*============================================================================*/
/*                          函数声明                                           */
/*============================================================================*/

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
 * @brief 初始化Bootloader独立看门狗。
 * @return HAL_OK-初始化成功；HAL_TIMEOUT-配置寄存器更新超时。
 * @note 名义超时约5秒；IWDG启动后只能由复位停止，跳转APP后必须继续喂狗。
 */
HAL_StatusTypeDef Bootloader_WatchdogInit(void);

/**
 * @brief 刷新独立看门狗计数器。
 * @return 无。
 * @note 可在IWDG尚未启动时调用，不会因此启动IWDG。
 */
void Bootloader_WatchdogRefresh(void);

/**
 * @brief 查询本次启动是否由IWDG复位触发。
 * @return 1-是IWDG复位；0-不是IWDG复位。
 * @note 结果在Bootloader_WatchdogInit()中锁存，随后统一清除RCC复位标志。
 */
uint8_t Bootloader_WasIwdgReset(void);



#endif








