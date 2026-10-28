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

/* W25Q128单备份区固定占用0x000000~0x0EFFFF，共960KB。
 * 用途：在0x04开始传输、擦除A区之前，把当前已验证的A区旧镜像整体备份到此区
 *       （OTA_Storage_CreateBackup）；升级失败/掉电时再从这里拷回A区（Rollback）。 */
#define OTA_BACKUP_REGION_ADDR          0x000000U   /* 备份区在W25Q128片内的起始字节偏移 */
#define OTA_BACKUP_REGION_SIZE          (960U * 1024U)  /* 备份区容量960KB，固件大小上限，
                                                           信息帧firmware_size超过此值直接拒绝 */

/**
 * @brief A区镜像校验结果（OTA_Storage_VerifyActiveImage返回值）
 * @note  UNKNOWN与INVALID要区别对待：UNKNOWN是"从无登记过基线"（如首次烧录/旧数据迁移），
 *        此时保持历史行为允许跳转；INVALID是"有基线但CRC对不上"，必须阻止跳转并回滚。
 */
typedef enum {
    OTA_STORAGE_IMAGE_UNKNOWN = 0,   /* 未知：EEPROM无已登记基线，无法证明A区好坏，保持旧跳转行为 */
    OTA_STORAGE_IMAGE_VALID,         /* 有效：A区实际CRC与EEPROM登记元数据一致，可信任/可跳转 */
    OTA_STORAGE_IMAGE_INVALID        /* 无效：CRC不一致，A区镜像损坏，禁止跳转，需回滚 */
} OTA_StorageImageState_t;

/**
 * @brief 回滚原因（OTA_ScheduleRollback -> MarkRollbackPending写入EEPROM）
 * @note  作用：①掉电重启后InitAndRecover据此做保守恢复；②日志定位升级失败环节。
 */
typedef enum {
    OTA_STORAGE_REASON_NONE = 0,          /* 无回滚原因（正常状态） */
    OTA_STORAGE_REASON_ABORT,             /* ESP32主动发0x08放弃本次传输 */
    OTA_STORAGE_REASON_FLASH_ERROR,       /* A区擦除/写入失败，或新镜像元数据提交失败 */
    OTA_STORAGE_REASON_FW_CRC_ERROR,      /* 收齐后整体CRC32端到端校验失败（落盘内容与信息帧不符） */
    OTA_STORAGE_REASON_BOOT_CRC_ERROR,    /* 启动握手超时前校验A区CRC失败（跳转前最后一道防线） */
    OTA_STORAGE_REASON_TRANSFER_TIMEOUT   /* WAIT_INFO/WAIT_DATA/WAIT_END阶段超时，传输中断 */
} OTA_StorageReason_t;

/**
 * @brief 恢复通知标志（持久化在EEPROM，用非0魔数防止空片/随机数据误判）
 * @note  非NONE期间，ota_core对旧会话的任何帧统一回NACK 0x08，
 *        只有收到旧会话0x08 ABORT并Acknowledge清除后，才放行新会话。
 *        POWER_LOSS由存储层启动恢复路径写入；TRANSFER_TIMEOUT由状态机超时回滚后写入。
 */
typedef enum {
    OTA_STORAGE_RECOVERY_NOTICE_NONE = 0x00U,              /* 无未结束会话，正常收发 */
    OTA_STORAGE_RECOVERY_NOTICE_POWER_LOSS = 0xA1U,        /* 上次升级擦除后掉电，已在启动时恢复旧镜像，等待ESP32用ABORT结束旧会话 */
    OTA_STORAGE_RECOVERY_NOTICE_TRANSFER_TIMEOUT = 0xA2U   /* 上次传输超时已回滚，等待ESP32用ABORT收尾 */
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
 * @brief 提交当前A区镜像的元数据（大小/CRC32/版本号），确立为新的可信基线
 *
 * @param size    镜像实际字节数（来自固件信息帧，必须通过大小范围检查）
 * @param crc32   镜像整体CRC32（来自固件信息帧，已由OTA_VerifyFirmwareCRC比对通过）
 * @param version 版本号字符串指针，取前OTA_VERSION_LEN(8)字节，目标区先清零再拷贝
 *
 * @return HAL_OK 成功；HAL_ERROR EEPROM未就绪/version为NULL/size非法，或EEPROM写入失败
 *
 * @details 执行三步持久化（任一失败即返回，调用方须按失败处理并回滚）：
 *            1. 填充RAM中的OTA_INFO（Firelen[0]=大小、Firelen[1]=CRC32、OTA_VERSION）
 *               并整体写入AT24C02；
 *            2. 写入格式版本控制字节；
 *            3. 把持久化升级状态清回IDLE、清除回滚原因——
 *               这一步同时解除擦除前登记的UPGRADING，使下次上电不再触发掉电恢复。
 *
 * @note 两个调用时机，语义不同但作用相同（都是"把当前A区登记为可信基线"）：
 *         - 新固件整体CRC通过（收到0x07）：提交新镜像，之后才允许跳转新APP；
 *         - OTA_Storage_Rollback恢复成功：用备份记录重新提交旧镜像元数据。
 *       必须在CRC校验通过之后才调用；CRC未过时提交会把坏镜像登记成可信基线。
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
