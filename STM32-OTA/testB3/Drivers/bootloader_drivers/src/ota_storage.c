/**
 * @file    ota_storage.c
 * @brief   固件存储实现（Flash 落盘层）
 * @details 归口：A 区 Flash 擦除 / 写入 / 整体 CRC 校验。
 *          ★未来 B 区镜像读写、回滚拷贝也放本模块，这是 A/B 分区回滚的地基。
 *          （由原 TaskTest.c 拆分而来，函数体一字未改动）
 *
 * @note 依赖的 g_ota_ctx 在 ota_core.c 定义，见 ota_core.h：
 *       - g_ota_ctx.firmware_size  ：待校验固件总大小
 *       - g_ota_ctx.firmware_crc32 ：信息帧下发的期望整体 CRC32
 */

#include "ota_storage.h"
#include "ota_proto.h"
#include "ota_core.h"
#include "bootloader.h"
#include "MyFLASH.h"
#include "AT24C02.h"
#include "W25Q128.h"


/*============================================================================*/
/*                     回滚元数据与内部辅助函数                                */
/*============================================================================*/

#define OTA_EEPROM_INFO_ADDR                0x00U
#define OTA_EEPROM_BACKUP_RECORD_ADDR       0x4CU
#define OTA_EEPROM_BACKUP_RECORD_SIZE       32U
#define OTA_EEPROM_BACKUP_VALID_ADDR        0xB4U
#define OTA_EEPROM_UPGRADE_STATE_ADDR       0xB5U
#define OTA_EEPROM_REASON_ADDR              0xB6U
#define OTA_EEPROM_FORMAT_ADDR              0xB7U
#define OTA_EEPROM_RECOVERY_NOTICE_ADDR     0xB8U

#define OTA_STORAGE_FORMAT_VERSION          0x31U
#define OTA_BACKUP_VALID_VALUE              0xA5U
#define OTA_BACKUP_INVALID_VALUE            0x00U

#define OTA_BACKUP_MAGIC0                   'R'
#define OTA_BACKUP_MAGIC1                   'B'
#define OTA_BACKUP_MAGIC2                   'K'
#define OTA_BACKUP_MAGIC3                   '1'

typedef enum {
	OTA_PERSIST_STATE_IDLE = 0x00U,
	OTA_PERSIST_STATE_BACKUP_IN_PROGRESS = 0x11U,
	OTA_PERSIST_STATE_UPGRADING = 0x22U,
	OTA_PERSIST_STATE_NEED_ROLLBACK = 0x33U
} OTA_PersistentState_t;

typedef struct {
	uint8_t version[OTA_VERSION_LEN];
	uint32_t size;
	uint32_t crc32;
} OTA_BackupRecord_t;

static uint8_t s_eeprom_ready = 0U;
static uint8_t s_baseline_ready = 0U;
static OTA_StorageRecoveryNotice_t s_recovery_notice = OTA_STORAGE_RECOVERY_NOTICE_NONE;


static uint32_t OTA_StorageCRC32Update(uint32_t crc, const uint8_t *data, uint32_t len)
{
	uint32_t i;
	uint8_t bit;

	for (i = 0U; i < len; i++)
	{
		crc ^= data[i];
		for (bit = 0U; bit < 8U; bit++)
		{
			crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320U : 0U);
		}
	}

	return crc;
}


static uint32_t OTA_StorageCRC32(const uint8_t *data, uint32_t len)
{
	return OTA_StorageCRC32Update(0xFFFFFFFFU, data, len) ^ 0xFFFFFFFFU;
}


static void OTA_StoragePutU32LE(uint8_t *buffer, uint32_t value)
{
	buffer[0] = (uint8_t)(value & 0xFFU);
	buffer[1] = (uint8_t)((value >> 8) & 0xFFU);
	buffer[2] = (uint8_t)((value >> 16) & 0xFFU);
	buffer[3] = (uint8_t)((value >> 24) & 0xFFU);
}


static uint32_t OTA_StorageGetU32LE(const uint8_t *buffer)
{
	return ((uint32_t)buffer[0]) |
	       ((uint32_t)buffer[1] << 8) |
	       ((uint32_t)buffer[2] << 16) |
	       ((uint32_t)buffer[3] << 24);
}


static uint8_t OTA_StorageIsAppVectorValid(void)
{
	uint32_t msp = MyFLASH_ReadWord(FLASH_ACode_STARTADDR);
	uint32_t reset = MyFLASH_ReadWord(FLASH_ACode_STARTADDR + 4U);

	if (msp < 0x20000000U || msp > 0x2002FFFFU)
	{
		return 0U;
	}

	if ((reset & 1U) == 0U || reset < FLASH_ACode_STARTADDR || reset >= 0x08100000U)
	{
		return 0U;
	}

	return 1U;
}


static uint8_t OTA_StorageIsImageSizeValid(uint32_t size)
{
	return (size > 0U && size <= OTA_BACKUP_REGION_SIZE) ? 1U : 0U;
}


static HAL_StatusTypeDef OTA_StorageCalculateAppCRC(uint32_t size, uint32_t *crc32)
{
	uint8_t buffer[256];
	uint32_t addr = FLASH_ACode_STARTADDR;
	uint32_t remaining = size;
	uint32_t chunk;
	uint32_t i;
	uint32_t crc = 0xFFFFFFFFU;

	if (crc32 == NULL || !OTA_StorageIsImageSizeValid(size))
	{
		return HAL_ERROR;
	}

	while (remaining > 0U)
	{
		Bootloader_WatchdogRefresh();
		chunk = (remaining > sizeof(buffer)) ? sizeof(buffer) : remaining;
		for (i = 0U; i < chunk; i++)
		{
			buffer[i] = MyFLASH_ReadByte(addr + i);
		}
		crc = OTA_StorageCRC32Update(crc, buffer, chunk);
		addr += chunk;
		remaining -= chunk;
	}

	*crc32 = crc ^ 0xFFFFFFFFU;
	return HAL_OK;
}


static HAL_StatusTypeDef OTA_StorageCalculateBackupCRC(uint32_t size, uint32_t *crc32)
{
	uint8_t buffer[256];
	uint32_t addr = OTA_BACKUP_REGION_ADDR;
	uint32_t remaining = size;
	uint32_t chunk;
	uint32_t crc = 0xFFFFFFFFU;
	HAL_StatusTypeDef status;

	if (crc32 == NULL || !OTA_StorageIsImageSizeValid(size))
	{
		return HAL_ERROR;
	}

	while (remaining > 0U)
	{
		Bootloader_WatchdogRefresh();
		chunk = (remaining > sizeof(buffer)) ? sizeof(buffer) : remaining;
		status = W25Q128_Read(addr, buffer, chunk, sizeof(buffer));
		if (status != HAL_OK)
		{
			return status;
		}
		crc = OTA_StorageCRC32Update(crc, buffer, chunk);
		addr += chunk;
		remaining -= chunk;
	}

	*crc32 = crc ^ 0xFFFFFFFFU;
	return HAL_OK;
}


static HAL_StatusTypeDef OTA_StorageWriteControlByte(uint16_t address, uint8_t value)
{
	if (!s_eeprom_ready)
	{
		return HAL_ERROR;
	}

	return EP24C_WriteBytes(address, &value, 1U);
}


static HAL_StatusTypeDef OTA_StorageLoadRecoveryNotice(void)
{
	uint8_t raw_notice;
	HAL_StatusTypeDef status;

	status = EP24C_ReadOneByte(OTA_EEPROM_RECOVERY_NOTICE_ADDR, &raw_notice);
	if (status != HAL_OK)
	{
		return status;
	}

	if (raw_notice == (uint8_t)OTA_STORAGE_RECOVERY_NOTICE_POWER_LOSS ||
		raw_notice == (uint8_t)OTA_STORAGE_RECOVERY_NOTICE_TRANSFER_TIMEOUT)
	{
		s_recovery_notice = (OTA_StorageRecoveryNotice_t)raw_notice;
	}
	else
	{
		s_recovery_notice = OTA_STORAGE_RECOVERY_NOTICE_NONE;
	}

	return HAL_OK;
}


static HAL_StatusTypeDef OTA_StorageSetPersistentState(OTA_PersistentState_t state,
											OTA_StorageReason_t reason)
{
	HAL_StatusTypeDef status;

	status = OTA_StorageWriteControlByte(OTA_EEPROM_FORMAT_ADDR, OTA_STORAGE_FORMAT_VERSION);
	if (status != HAL_OK)
	{
		return status;
	}

	status = OTA_StorageWriteControlByte(OTA_EEPROM_REASON_ADDR, (uint8_t)reason);
	if (status != HAL_OK)
	{
		return status;
	}

	/* 状态最后写入；掉电时宁可保留旧状态并走保守恢复。 */
	return OTA_StorageWriteControlByte(OTA_EEPROM_UPGRADE_STATE_ADDR, (uint8_t)state);
}


static HAL_StatusTypeDef OTA_StorageReadControl(uint8_t *backup_valid,
										 uint8_t *state,
										 uint8_t *reason,
										 uint8_t *format)
{
	HAL_StatusTypeDef status;

	if (backup_valid == NULL || state == NULL || reason == NULL || format == NULL)
	{
		return HAL_ERROR;
	}

	status = EP24C_ReadOneByte(OTA_EEPROM_BACKUP_VALID_ADDR, backup_valid);
	if (status != HAL_OK)
	{
		return status;
	}
	status = EP24C_ReadOneByte(OTA_EEPROM_UPGRADE_STATE_ADDR, state);
	if (status != HAL_OK)
	{
		return status;
	}
	status = EP24C_ReadOneByte(OTA_EEPROM_REASON_ADDR, reason);
	if (status != HAL_OK)
	{
		return status;
	}
	return EP24C_ReadOneByte(OTA_EEPROM_FORMAT_ADDR, format);
}


static HAL_StatusTypeDef OTA_StorageWriteBackupRecord(const OTA_BackupRecord_t *record)
{
	uint8_t raw[OTA_EEPROM_BACKUP_RECORD_SIZE];
	uint8_t verify[OTA_EEPROM_BACKUP_RECORD_SIZE];
	HAL_StatusTypeDef status;

	if (record == NULL || !OTA_StorageIsImageSizeValid(record->size))
	{
		return HAL_ERROR;
	}

	memset(raw, 0xFF, sizeof(raw));
	raw[0] = OTA_BACKUP_MAGIC0;
	raw[1] = OTA_BACKUP_MAGIC1;
	raw[2] = OTA_BACKUP_MAGIC2;
	raw[3] = OTA_BACKUP_MAGIC3;
	raw[4] = OTA_STORAGE_FORMAT_VERSION;
	memcpy(&raw[8], record->version, OTA_VERSION_LEN);
	OTA_StoragePutU32LE(&raw[16], record->size);
	OTA_StoragePutU32LE(&raw[20], record->crc32);
	OTA_StoragePutU32LE(&raw[24], OTA_StorageCRC32(raw, 24U));

	status = EP24C_WriteBytes(OTA_EEPROM_BACKUP_RECORD_ADDR, raw, sizeof(raw));
	if (status != HAL_OK)
	{
		return status;
	}

	status = EP24C_ReadBytes(OTA_EEPROM_BACKUP_RECORD_ADDR, verify, sizeof(verify), sizeof(verify));
	if (status != HAL_OK || memcmp(raw, verify, sizeof(raw)) != 0)
	{
		return HAL_ERROR;
	}

	return HAL_OK;
}


static HAL_StatusTypeDef OTA_StorageReadBackupRecord(OTA_BackupRecord_t *record)
{
	uint8_t raw[OTA_EEPROM_BACKUP_RECORD_SIZE];
	HAL_StatusTypeDef status;
	uint32_t record_crc;

	if (record == NULL)
	{
		return HAL_ERROR;
	}

	status = EP24C_ReadBytes(OTA_EEPROM_BACKUP_RECORD_ADDR, raw, sizeof(raw), sizeof(raw));
	if (status != HAL_OK)
	{
		return status;
	}

	if (raw[0] != OTA_BACKUP_MAGIC0 || raw[1] != OTA_BACKUP_MAGIC1 ||
		raw[2] != OTA_BACKUP_MAGIC2 || raw[3] != OTA_BACKUP_MAGIC3 ||
		raw[4] != OTA_STORAGE_FORMAT_VERSION)
	{
		return HAL_ERROR;
	}

	record_crc = OTA_StorageGetU32LE(&raw[24]);
	if (record_crc != OTA_StorageCRC32(raw, 24U))
	{
		return HAL_ERROR;
	}

	record->size = OTA_StorageGetU32LE(&raw[16]);
	record->crc32 = OTA_StorageGetU32LE(&raw[20]);
	if (!OTA_StorageIsImageSizeValid(record->size))
	{
		return HAL_ERROR;
	}
	memcpy(record->version, &raw[8], OTA_VERSION_LEN);

	return HAL_OK;
}


static uint8_t OTA_StorageHasValidBackup(void)
{
	uint8_t backup_valid;
	uint8_t state;
	uint8_t reason;
	uint8_t format;
	OTA_BackupRecord_t record;

	if (!s_eeprom_ready || OTA_StorageReadControl(&backup_valid, &state, &reason, &format) != HAL_OK)
	{
		return 0U;
	}

	if (format != OTA_STORAGE_FORMAT_VERSION || backup_valid != OTA_BACKUP_VALID_VALUE)
	{
		return 0U;
	}

	return (OTA_StorageReadBackupRecord(&record) == HAL_OK) ? 1U : 0U;
}


/*============================================================================*/
/*                          Flash操作函数                                      */
/*============================================================================*/

/**
 * @brief 擦除A区(APP)Flash
 * @return HAL_OK-成功，HAL_ERROR-失败
 *
 * @details 擦除范围:
 *   - 起始扇区: 4 (对应地址0x08010000)
 *   - 扇区数量: 8 (扇区4-11)
 *   - 擦除大小: 约960KB
 *
 * @note STM32F407扇区分布:
 *   - 扇区0-3: 每个16KB
 *   - 扇区4: 64KB
 *   - 扇区5-11: 每个128KB
 *
 *   擦除是耗时操作，可能需要几秒钟
 */
HAL_StatusTypeDef OTA_EraseAppRegion(void)
{
	HAL_StatusTypeDef status;

	U1_Printf("开始擦除A区Flash...\r\n");
	U1_Printf("起始扇区: %d, 扇区数量: %d\r\n", FLASH_ACode_STARTNUM, FLASH_ACode_NUM);

	Bootloader_WatchdogRefresh();
	status = MyFlash_EraseSectors(FLASH_ACode_STARTNUM, FLASH_ACode_NUM);
	Bootloader_WatchdogRefresh();

	if (status == HAL_OK)
	{
		U1_Printf("A区Flash擦除成功!\r\n");
	}
	else
	{
		U1_Printf("A区Flash擦除失败!\r\n");
	}

	return status;
}




/**
 * @brief 将固件数据写入Flash
 * @param addr 目标地址(Flash地址)
 * @param data 数据指针
 * @param len 数据长度
 * @return HAL_OK-成功，HAL_ERROR-失败
 *
 * @details 写入流程:
 *   1. 复制数据到临时缓冲区
 *   2. 计算4字节对齐后的长度: (len + 3) & ~0x03
 *   3. 如果长度不是4的倍数，用0xFF填充末尾
 *   4. 调用MyFlash_WriteBuffer写入
 *
 * @note STM32F4的Flash写入要求:
 *   - 地址必须4字节对齐
 *   - 每次写入一个字(32位)
 *   - 0xFF是Flash擦除后的默认值，填充0xFF不影响程序
 */
HAL_StatusTypeDef OTA_WriteFirmwareData(uint32_t addr, uint8_t *data, uint32_t len)
{
	uint32_t aligned_len;
	uint8_t padded_data[OTA_DATA_PAYLOAD_LEN + 4];

	if (data == NULL || len == 0)
	{
		return HAL_ERROR;
	}

	memcpy(padded_data, data, len);

	aligned_len = (len + 3) & ~0x03;	//4字节对齐后的长度
										//原理：比如说实际的len=1023，那么+3就变成1026，再进行& ~0x03（由1026的二进制表示为11111100），就是1024
										//这样就是4字节对齐后的长度，确保写入时地址是4的倍数

	if (aligned_len > len)				//如果对齐后的长度大于实际长度，用0xFF填充末尾
	{									//0xFF是Flash擦除后的默认值，填充0xFF不影响程序
		memset(&padded_data[len], 0xFF, aligned_len - len);
	}

	Bootloader_WatchdogRefresh();
	return MyFlash_WriteBuffer(addr, padded_data, aligned_len);
}




/**
 * @brief 校验已写入Flash的固件CRC32
 * @return 1-校验成功，0-校验失败
 *
 * @details 校验流程:
 *   1. 从A区Flash起始地址开始读取
 *   2. 每次读取256字节到缓冲区
 *   3. 对每个字节进行CRC32计算
 *   4. 最终与期望CRC32值比较
 *
 * @note 为什么分块读取:
 *   - 避免一次性分配大缓冲区
 *   - 256字节是合理的块大小
 *   - 流式计算CRC32，内存占用小
 */
uint8_t OTA_VerifyFirmwareCRC(void)
{
	uint32_t addr;
	uint32_t remaining;
	uint32_t chunk_size;
	uint32_t calculated_crc = 0xFFFFFFFF;
	uint8_t read_buffer[256];
	uint32_t i;

	U1_Printf("开始校验固件CRC32...\r\n");
	U1_Printf("固件大小: %lu 字节\r\n", g_ota_ctx.firmware_size);

	addr = FLASH_ACode_STARTADDR;
	remaining = g_ota_ctx.firmware_size;

	while (remaining > 0)
	{
		Bootloader_WatchdogRefresh();
		chunk_size = (remaining > 256) ? 256 : remaining;

		for (i = 0; i < chunk_size; i++)
		{
			read_buffer[i] = MyFLASH_ReadByte(addr + i);
		}

		for (i = 0; i < chunk_size; i++)
		{
			calculated_crc = calculated_crc ^ read_buffer[i];
			calculated_crc = (calculated_crc >> 1) ^ ((calculated_crc & 1) ? 0xEDB88320 : 0);
			calculated_crc = (calculated_crc >> 1) ^ ((calculated_crc & 1) ? 0xEDB88320 : 0);
			calculated_crc = (calculated_crc >> 1) ^ ((calculated_crc & 1) ? 0xEDB88320 : 0);
			calculated_crc = (calculated_crc >> 1) ^ ((calculated_crc & 1) ? 0xEDB88320 : 0);
			calculated_crc = (calculated_crc >> 1) ^ ((calculated_crc & 1) ? 0xEDB88320 : 0);
			calculated_crc = (calculated_crc >> 1) ^ ((calculated_crc & 1) ? 0xEDB88320 : 0);
			calculated_crc = (calculated_crc >> 1) ^ ((calculated_crc & 1) ? 0xEDB88320 : 0);
			calculated_crc = (calculated_crc >> 1) ^ ((calculated_crc & 1) ? 0xEDB88320 : 0);
		}

		addr += chunk_size;
		remaining -= chunk_size;
	}

	calculated_crc = calculated_crc ^ 0xFFFFFFFF;

	U1_Printf("计算CRC32: 0x%08X\r\n", calculated_crc);
	U1_Printf("期望CRC32: 0x%08X\r\n", g_ota_ctx.firmware_crc32);

	if (calculated_crc == g_ota_ctx.firmware_crc32)
	{
		U1_Printf("CRC32校验成功!\r\n");
		return 1;
	}
	else
	{
		U1_Printf("CRC32校验失败!\r\n");
		return 0;
	}
}


/*============================================================================*/
/*                          W25Q128单备份回滚                                 */
/*============================================================================*/

HAL_StatusTypeDef OTA_Storage_CommitActiveImage(uint32_t size,
												 uint32_t crc32,
												 const uint8_t *version)
{
	HAL_StatusTypeDef status;

	if (!s_eeprom_ready || version == NULL || !OTA_StorageIsImageSizeValid(size))
	{
		return HAL_ERROR;
	}

	OTA_INFO.Firelen[0] = size;
	OTA_INFO.Firelen[1] = crc32;
	memset(OTA_INFO.OTA_VERSION, 0, sizeof(OTA_INFO.OTA_VERSION));
	memcpy(OTA_INFO.OTA_VERSION, version, OTA_VERSION_LEN);

	status = EP24C_WriteBytes(OTA_EEPROM_INFO_ADDR, (const uint8_t *)&OTA_INFO, OTA_INFOCB_SIZE);
	if (status != HAL_OK)
	{
		return status;
	}

	status = OTA_StorageWriteControlByte(OTA_EEPROM_FORMAT_ADDR, OTA_STORAGE_FORMAT_VERSION);
	if (status != HAL_OK)
	{
		return status;
	}

	return OTA_StorageSetPersistentState(OTA_PERSIST_STATE_IDLE, OTA_STORAGE_REASON_NONE);
}


OTA_StorageImageState_t OTA_Storage_VerifyActiveImage(void)
{
	uint32_t calculated_crc;

	if (!s_baseline_ready)
	{
		return OTA_STORAGE_IMAGE_UNKNOWN;
	}

	if (!OTA_StorageIsAppVectorValid() ||
		OTA_StorageCalculateAppCRC(OTA_INFO.Firelen[0], &calculated_crc) != HAL_OK)
	{
		return OTA_STORAGE_IMAGE_INVALID;
	}

	return (calculated_crc == OTA_INFO.Firelen[1]) ?
		OTA_STORAGE_IMAGE_VALID : OTA_STORAGE_IMAGE_INVALID;
}


uint8_t OTA_Storage_IsUpgradeAllowed(void)
{
	return (s_eeprom_ready && s_baseline_ready &&
		OTA_Storage_VerifyActiveImage() == OTA_STORAGE_IMAGE_VALID) ? 1U : 0U;
}


OTA_StorageRecoveryNotice_t OTA_Storage_GetRecoveryNotice(void)
{
	return s_recovery_notice;
}


HAL_StatusTypeDef OTA_Storage_SetRecoveryNotice(OTA_StorageRecoveryNotice_t notice)
{
	HAL_StatusTypeDef status;

	if (notice != OTA_STORAGE_RECOVERY_NOTICE_NONE &&
		notice != OTA_STORAGE_RECOVERY_NOTICE_POWER_LOSS &&
		notice != OTA_STORAGE_RECOVERY_NOTICE_TRANSFER_TIMEOUT)
	{
		return HAL_ERROR;
	}

	status = OTA_StorageWriteControlByte(OTA_EEPROM_RECOVERY_NOTICE_ADDR, (uint8_t)notice);
	if (status == HAL_OK)
	{
		s_recovery_notice = notice;
	}

	return status;
}


HAL_StatusTypeDef OTA_Storage_AcknowledgeRecoveryNotice(void)
{
	return OTA_Storage_SetRecoveryNotice(OTA_STORAGE_RECOVERY_NOTICE_NONE);
}


HAL_StatusTypeDef OTA_Storage_CreateBackup(void)
{
	OTA_BackupRecord_t record;
	uint8_t buffer[W25Q128_PAGE_SIZE];
	uint32_t offset = 0U;
	uint32_t erase_offset;
	uint32_t chunk;
	uint32_t source_crc = 0xFFFFFFFFU;
	uint32_t backup_crc = 0U;
	uint32_t i;
	HAL_StatusTypeDef status;

	/* 上一轮回滚通知尚未被ESP32确认时，不能覆盖唯一备份。 */
	if (OTA_Storage_GetRecoveryNotice() != OTA_STORAGE_RECOVERY_NOTICE_NONE)
	{
		U1_Printf("[回滚] 上一轮恢复通知尚未确认，拒绝覆盖备份\r\n");
		return HAL_ERROR;
	}

	if (!OTA_Storage_IsUpgradeAllowed())
	{
		U1_Printf("[回滚] 当前A区基线不可用，拒绝擦除升级\r\n");
		return HAL_ERROR;
	}

	U1_Printf("[回滚] 开始备份当前A区: %lu 字节\r\n", OTA_INFO.Firelen[0]);
	status = OTA_StorageSetPersistentState(OTA_PERSIST_STATE_BACKUP_IN_PROGRESS,
										 OTA_STORAGE_REASON_NONE);
	if (status != HAL_OK)
	{
		return status;
	}

	status = OTA_StorageWriteControlByte(OTA_EEPROM_BACKUP_VALID_ADDR, OTA_BACKUP_INVALID_VALUE);
	if (status != HAL_OK)
	{
		return status;
	}

	/* 960KB刚好是15个64KB块；复用已有块擦除接口以缩短START_TRANSFER等待时间。 */
	for (erase_offset = 0U; erase_offset < OTA_INFO.Firelen[0];
		 erase_offset += W25Q128_64KB_BLOCK_SIZE)
	{
		Bootloader_WatchdogRefresh();
		status = W25Q128_64KBBlockErase(OTA_BACKUP_REGION_ADDR + erase_offset);
		if (status != HAL_OK)
		{
			U1_Printf("[回滚] W25Q128块擦除失败: 0x%06lX\r\n", OTA_BACKUP_REGION_ADDR + erase_offset);
			return status;
		}
	}

	while (offset < OTA_INFO.Firelen[0])
	{
		Bootloader_WatchdogRefresh();
		chunk = OTA_INFO.Firelen[0] - offset;
		if (chunk > sizeof(buffer))
		{
			chunk = sizeof(buffer);
		}

		for (i = 0U; i < chunk; i++)
		{
			buffer[i] = MyFLASH_ReadByte(FLASH_ACode_STARTADDR + offset + i);
		}
		source_crc = OTA_StorageCRC32Update(source_crc, buffer, chunk);

		status = W25Q128_PageWrite(OTA_BACKUP_REGION_ADDR + offset, buffer, (uint16_t)chunk);
		if (status != HAL_OK)
		{
			U1_Printf("[回滚] W25Q128写入失败: 0x%06lX\r\n", OTA_BACKUP_REGION_ADDR + offset);
			return status;
		}

		offset += chunk;
	}

	source_crc ^= 0xFFFFFFFFU;
	if (source_crc != OTA_INFO.Firelen[1])
	{
		U1_Printf("[回滚] A区基线CRC变化: 0x%08lX != 0x%08lX\r\n", source_crc, OTA_INFO.Firelen[1]);
		return HAL_ERROR;
	}

	status = OTA_StorageCalculateBackupCRC(OTA_INFO.Firelen[0], &backup_crc);
	if (status != HAL_OK || backup_crc != source_crc)
	{
		U1_Printf("[回滚] 备份CRC校验失败: 0x%08lX\r\n", backup_crc);
		return HAL_ERROR;
	}

	memcpy(record.version, OTA_INFO.OTA_VERSION, OTA_VERSION_LEN);
	record.size = OTA_INFO.Firelen[0];
	record.crc32 = source_crc;
	status = OTA_StorageWriteBackupRecord(&record);
	if (status != HAL_OK)
	{
		return status;
	}

	status = OTA_StorageWriteControlByte(OTA_EEPROM_BACKUP_VALID_ADDR, OTA_BACKUP_VALID_VALUE);
	if (status == HAL_OK)
	{
		U1_Printf("[回滚] A区备份完成，CRC32=0x%08lX\r\n", source_crc);
	}
	return status;
}


HAL_StatusTypeDef OTA_Storage_MarkUpgradeInProgress(void)
{
	if (s_recovery_notice != OTA_STORAGE_RECOVERY_NOTICE_NONE || !OTA_StorageHasValidBackup())
	{
		return HAL_ERROR;
	}

	return OTA_StorageSetPersistentState(OTA_PERSIST_STATE_UPGRADING,
										 OTA_STORAGE_REASON_NONE);
}


HAL_StatusTypeDef OTA_Storage_MarkRollbackPending(OTA_StorageReason_t reason)
{
	return OTA_StorageSetPersistentState(OTA_PERSIST_STATE_NEED_ROLLBACK, reason);
}


HAL_StatusTypeDef OTA_Storage_Rollback(void)
{
	OTA_BackupRecord_t record;
	uint8_t buffer[256];
	uint32_t offset = 0U;
	uint32_t chunk;
	uint32_t backup_crc = 0U;
	uint32_t restored_crc = 0U;
	HAL_StatusTypeDef status;

	if (!OTA_StorageHasValidBackup())
	{
		U1_Printf("[回滚] 无有效备份记录，拒绝跳转A区\r\n");
		return HAL_ERROR;
	}

	status = OTA_StorageReadBackupRecord(&record);
	if (status != HAL_OK)
	{
		return status;
	}

	status = OTA_StorageCalculateBackupCRC(record.size, &backup_crc);
	if (status != HAL_OK || backup_crc != record.crc32)
	{
		U1_Printf("[回滚] W25Q128备份损坏: 0x%08lX\r\n", backup_crc);
		return HAL_ERROR;
	}

	U1_Printf("[回滚] 开始恢复旧固件: %lu 字节\r\n", record.size);
	status = OTA_EraseAppRegion();
	if (status != HAL_OK)
	{
		return status;
	}

	while (offset < record.size)
	{
		Bootloader_WatchdogRefresh();
		chunk = record.size - offset;
		if (chunk > sizeof(buffer))
		{
			chunk = sizeof(buffer);
		}

		status = W25Q128_Read(OTA_BACKUP_REGION_ADDR + offset, buffer, chunk, sizeof(buffer));
		if (status != HAL_OK)
		{
			return status;
		}

		status = OTA_WriteFirmwareData(FLASH_ACode_STARTADDR + offset, buffer, chunk);
		if (status != HAL_OK)
		{
			return status;
		}

		offset += chunk;
	}

	status = OTA_StorageCalculateAppCRC(record.size, &restored_crc);
	if (status != HAL_OK || restored_crc != record.crc32)
	{
		U1_Printf("[回滚] A区写回CRC失败: 0x%08lX\r\n", restored_crc);
		return HAL_ERROR;
	}

	status = OTA_Storage_CommitActiveImage(record.size, record.crc32, record.version);
	if (status == HAL_OK)
	{
		s_baseline_ready = 1U;
		U1_Printf("[回滚] 恢复成功，CRC32=0x%08lX\r\n", restored_crc);
	}

	return status;
}


HAL_StatusTypeDef OTA_Storage_InitAndRecover(void)
{
	uint8_t backup_valid;
	uint8_t state;
	uint8_t reason;
	uint8_t format;
	uint32_t current_crc;
	HAL_StatusTypeDef status;

	s_eeprom_ready = 0U;
	s_baseline_ready = 0U;
	s_recovery_notice = OTA_STORAGE_RECOVERY_NOTICE_NONE;

	if (EP24C_IsDeviceReady() != HAL_OK)
	{
		U1_Printf("[回滚] AT24C02未就绪，保持旧跳转能力并禁止升级\r\n");
		return HAL_OK;
	}
	s_eeprom_ready = 1U;

	status = EP24C_ReadBytes(OTA_EEPROM_INFO_ADDR, (uint8_t *)&OTA_INFO,
							 OTA_INFOCB_SIZE, OTA_INFOCB_SIZE);
	if (status != HAL_OK)
	{
		U1_Printf("[回滚] 读取OTA_INFO失败，禁止升级\r\n");
		return HAL_OK;
	}

	status = OTA_StorageReadControl(&backup_valid, &state, &reason, &format);
	if (status != HAL_OK)
	{
		U1_Printf("[回滚] 读取回滚控制字失败，禁止升级\r\n");
		return HAL_OK;
	}

	if (format == OTA_STORAGE_FORMAT_VERSION && OTA_StorageLoadRecoveryNotice() != HAL_OK)
	{
		U1_Printf("[回滚] 读取恢复通知失败，禁止升级\r\n");
		s_eeprom_ready = 0U;
		return HAL_OK;
	}

	if (format == OTA_STORAGE_FORMAT_VERSION &&
		(state == OTA_PERSIST_STATE_UPGRADING || state == OTA_PERSIST_STATE_NEED_ROLLBACK))
	{
		U1_Printf("[回滚] 检测到未完成升级，进入启动恢复\r\n");
		if (backup_valid != OTA_BACKUP_VALID_VALUE || OTA_Storage_Rollback() != HAL_OK)
		{
			U1_Printf("[回滚] 启动恢复失败，停在Bootloader\r\n");
			return HAL_ERROR;
		}
		if (OTA_Storage_SetRecoveryNotice(OTA_STORAGE_RECOVERY_NOTICE_POWER_LOSS) != HAL_OK)
		{
			U1_Printf("[回滚] 无法保存恢复通知，停在Bootloader\r\n");
			return HAL_ERROR;
		}
		U1_Printf("[回滚] 已恢复旧镜像，等待ESP32结束上一轮会话\r\n");
		return HAL_OK;
	}

	if (format == OTA_STORAGE_FORMAT_VERSION && state == OTA_PERSIST_STATE_BACKUP_IN_PROGRESS)
	{
		/* 此状态下A区尚未被擦除；清除未完成备份状态后按当前固件继续启动。 */
		(void)OTA_StorageSetPersistentState(OTA_PERSIST_STATE_IDLE, OTA_STORAGE_REASON_NONE);
	}

	/* 元数据完全不可解释时保留旧跳转能力，但禁止覆盖式升级。 */
	if (!OTA_StorageIsImageSizeValid(OTA_INFO.Firelen[0]))
	{
		U1_Printf("[回滚] 未发现可解释的旧基线，保持旧跳转能力并禁止覆盖升级\r\n");
		return HAL_OK;
	}

	if (format != OTA_STORAGE_FORMAT_VERSION)
	{
		if (!OTA_StorageIsAppVectorValid())
		{
			U1_Printf("[回滚] 旧基线向量无效，保持旧跳转能力并禁止覆盖升级\r\n");
			return HAL_OK;
		}

		status = OTA_StorageCalculateAppCRC(OTA_INFO.Firelen[0], &current_crc);
		if (status != HAL_OK)
		{
			return HAL_OK;
		}
		OTA_INFO.Firelen[1] = current_crc;
		status = EP24C_WriteBytes(OTA_EEPROM_INFO_ADDR, (const uint8_t *)&OTA_INFO, OTA_INFOCB_SIZE);
		if (status != HAL_OK ||
			OTA_StorageWriteControlByte(OTA_EEPROM_BACKUP_VALID_ADDR, OTA_BACKUP_INVALID_VALUE) != HAL_OK ||
			OTA_StorageSetPersistentState(OTA_PERSIST_STATE_IDLE, OTA_STORAGE_REASON_NONE) != HAL_OK ||
			OTA_Storage_AcknowledgeRecoveryNotice() != HAL_OK)
		{
			U1_Printf("[回滚] 旧基线迁移写入失败，禁止升级\r\n");
			return HAL_OK;
		}
		U1_Printf("[回滚] 已迁移旧基线: size=%lu, CRC32=0x%08lX\r\n",
				  OTA_INFO.Firelen[0], current_crc);
	}
	else
	{
		/* 当前格式下，A区必须同时满足向量和登记CRC；否则优先恢复唯一备份。 */
		if (!OTA_StorageIsAppVectorValid() ||
			OTA_StorageCalculateAppCRC(OTA_INFO.Firelen[0], &current_crc) != HAL_OK ||
			current_crc != OTA_INFO.Firelen[1])
		{
			U1_Printf("[回滚] 启动检测到A区向量/CRC不匹配，尝试恢复备份\r\n");
			if (!OTA_StorageHasValidBackup() || OTA_Storage_Rollback() != HAL_OK)
			{
				U1_Printf("[回滚] 无可信备份可恢复，停在Bootloader\r\n");
				return HAL_ERROR;
			}

			if (OTA_Storage_SetRecoveryNotice(OTA_STORAGE_RECOVERY_NOTICE_POWER_LOSS) != HAL_OK)
			{
				U1_Printf("[回滚] 无法保存启动恢复通知，停在Bootloader\r\n");
				return HAL_ERROR;
			}
			U1_Printf("[回滚] 启动恢复完成，等待ESP32结束旧会话\r\n");
			return HAL_OK;
		}
	}

	s_baseline_ready = 1U;
	return HAL_OK;
}
