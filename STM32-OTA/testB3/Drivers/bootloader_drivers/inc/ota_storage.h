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

/* W25Q128单备份区固定占用0x000000~0x0EFFFF，共960KB。 */
#define OTA_BACKUP_REGION_ADDR          0x000000U
#define OTA_BACKUP_REGION_SIZE          (960U * 1024U)

typedef enum {
    OTA_STORAGE_IMAGE_UNKNOWN = 0,
    OTA_STORAGE_IMAGE_VALID,
    OTA_STORAGE_IMAGE_INVALID
} OTA_StorageImageState_t;

typedef enum {
    OTA_STORAGE_REASON_NONE = 0,
    OTA_STORAGE_REASON_ABORT,
    OTA_STORAGE_REASON_FLASH_ERROR,
    OTA_STORAGE_REASON_FW_CRC_ERROR,
    OTA_STORAGE_REASON_BOOT_CRC_ERROR,
    OTA_STORAGE_REASON_TRANSFER_TIMEOUT
} OTA_StorageReason_t;

typedef enum {
    OTA_STORAGE_RECOVERY_NOTICE_NONE = 0x00U,
    OTA_STORAGE_RECOVERY_NOTICE_POWER_LOSS = 0xA1U,
    OTA_STORAGE_RECOVERY_NOTICE_TRANSFER_TIMEOUT = 0xA2U
} OTA_StorageRecoveryNotice_t;


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

/**
 * @brief 启动时加载EEPROM元数据、迁移旧OTA_INFOCB并处理未完成升级。
 * @return HAL_OK-可继续启动；HAL_ERROR-A区损坏且无法安全恢复
 */
HAL_StatusTypeDef OTA_Storage_InitAndRecover(void);

/**
 * @brief 将当前已验证A区镜像备份到W25Q128，并写入备份记录。
 */
HAL_StatusTypeDef OTA_Storage_CreateBackup(void);

/**
 * @brief 校验W25Q128备份后将其写回A区，并提交旧版本为当前版本。
 */
HAL_StatusTypeDef OTA_Storage_Rollback(void);

/**
 * @brief 新固件整体CRC通过后，提交当前镜像的大小、CRC和版本号。
 */
HAL_StatusTypeDef OTA_Storage_CommitActiveImage(uint32_t size,
                                                 uint32_t crc32,
                                                 const uint8_t *version);

/**
 * @brief 校验当前A区镜像；未知表示尚无可信基线，保持旧跳转行为。
 */
OTA_StorageImageState_t OTA_Storage_VerifyActiveImage(void);

/** @brief 当前是否拥有可安全覆盖A区的已验证基线。 */
uint8_t OTA_Storage_IsUpgradeAllowed(void);

/** @brief 在擦除A区前写入UPGRADING状态，掉电重启将触发回滚。 */
HAL_StatusTypeDef OTA_Storage_MarkUpgradeInProgress(void);

/** @brief 记录待回滚原因，供掉电复位后的启动恢复使用。 */
HAL_StatusTypeDef OTA_Storage_MarkRollbackPending(OTA_StorageReason_t reason);

OTA_StorageRecoveryNotice_t OTA_Storage_GetRecoveryNotice(void);
HAL_StatusTypeDef OTA_Storage_SetRecoveryNotice(OTA_StorageRecoveryNotice_t notice);
HAL_StatusTypeDef OTA_Storage_AcknowledgeRecoveryNotice(void);


#endif
