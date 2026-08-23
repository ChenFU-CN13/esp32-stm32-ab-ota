/**
 * @file    ota_storage.h
 * @brief   固件存储模块头文件（Flash 落盘层）
 * @details 归口：A 区 Flash 擦除 / 写入 / 整体 CRC 校验。
 *          ★未来 B 区镜像读写、回滚拷贝也放本模块，这是 A/B 分区回滚的地基。
 *          （逻辑由原 TaskTest.h 拆分而来）
 */

#ifndef OTA_STORAGE_H
#define OTA_STORAGE_H

#include "main.h"


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


#endif


