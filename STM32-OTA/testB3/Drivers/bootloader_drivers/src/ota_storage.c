/**
 * @file    ota_storage.c
 * @brief   固件存储实现（Flash 落盘层 + A/B 单备份回滚）
 * @details 归口：
 *            1. A 区内部 Flash 的擦除 / 写入 / 整体 CRC32 校验；
 *            2. W25Q128 外部 SPI Flash 的旧镜像单备份与回滚拷贝；
 *            3. AT24C02 EEPROM 持久化元数据（基线信息/备份记录/升级状态机/恢复通知）；
 *            4. 上电启动恢复 InitAndRecover（掉电、CRC 损坏等场景）。
 *          这是 A/B 分区回滚思想的落地：A 区=待升级运行区，W25Q128 固定 960KB=备份区。
 *          （由原 TaskTest.c 拆分而来，函数体一字未改动）
 *
 * @note 依赖的 g_ota_ctx 在 ota_core.c 定义，见 ota_core.h：
 *       - g_ota_ctx.firmware_size  ：待校验固件总大小
 *       - g_ota_ctx.firmware_crc32 ：信息帧下发的期望整体 CRC32
 *
 * @note 存储介质分工：
 *       - STM32 内部 Flash A 区：当前运行/正在写入的 APP 镜像；
 *       - W25Q128 0x000000~0x0EFFFF：旧镜像备份（擦 A 区前整体拷贝，失败时拷回）；
 *       - AT24C02（256 字节）：全部"掉电不能丢"的小元数据，地址布局见本文件宏定义区。
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

/*------------------------- AT24C02 EEPROM 地址布局 -------------------------
 *  0x00~0x4B  OTA_INFO 结构体(OTA_INFOCB_SIZE=76字节)：Firelen[11]+OTA_VERSION[32]
 *             其中 Firelen[0]=当前基线大小，Firelen[1]=当前基线CRC32
 *  0x4C~0x6B  备份记录(32字节，格式见 OTA_StorageWriteBackupRecord 内的 raw 布局)
 *  0x6C~0xB3  本模块未使用（历史保留，勿占用）
 *  0xB4       备份有效标志：0xA5=备份可信，0x00=不可信/写备份前置无效
 *  0xB5       持久化升级状态（OTA_PersistentState_t）
 *  0xB6       回滚原因（OTA_StorageReason_t）
 *  0xB7       元数据格式版本（OTA_STORAGE_FORMAT_VERSION），区分空片/旧版数据
 *  0xB8       恢复通知（0x00/0xA1掉电/0xA2超时，见 ota_storage.h）
 *  控制字全部使用"魔数值"而非0/1连续值，防止EEPROM空白(0xFF)或随机数据被误判。
 *--------------------------------------------------------------------------*/
#define OTA_EEPROM_INFO_ADDR                0x00U   /* OTA_INFO 基线信息结构体起始地址 */
#define OTA_EEPROM_BACKUP_RECORD_ADDR       0x4CU   /* W25Q128备份的"身份证"记录起始地址 */
#define OTA_EEPROM_BACKUP_RECORD_SIZE       32U     /* 备份记录固定32字节 */
#define OTA_EEPROM_BACKUP_VALID_ADDR        0xB4U   /* 备份有效标志字节地址 */
#define OTA_EEPROM_UPGRADE_STATE_ADDR       0xB5U   /* 持久化升级状态字节地址 */
#define OTA_EEPROM_REASON_ADDR              0xB6U   /* 回滚原因字节地址 */
#define OTA_EEPROM_FORMAT_ADDR              0xB7U   /* 元数据格式版本字节地址 */
#define OTA_EEPROM_RECOVERY_NOTICE_ADDR     0xB8U   /* 恢复通知字节地址 */

#define OTA_STORAGE_FORMAT_VERSION          0x31U   /* 当前元数据格式版本(字符'1')，
													   与该值不符视为旧版/空片，走迁移逻辑 */
#define OTA_BACKUP_VALID_VALUE              0xA5U   /* 备份已写完且读回校验通过才置此值 */
#define OTA_BACKUP_INVALID_VALUE            0x00U   /* 开始写备份前先清成无效，避免写到一半掉电
													   留下"看似有效实则残缺"的备份 */

/* 备份记录4字节魔数"RBK1"：读记录时首先校验，用来识别记录是否已被写过 */
#define OTA_BACKUP_MAGIC0                   'R'
#define OTA_BACKUP_MAGIC1                   'B'
#define OTA_BACKUP_MAGIC2                   'K'
#define OTA_BACKUP_MAGIC3                   '1'

/**
 * @brief 持久化在EEPROM(0xB5)中的升级状态机
 * @note  与RAM中的OTA_State_t不同：这里记录的是"掉电前进行到哪一步"，
 *        上电时 OTA_Storage_InitAndRecover 据此决定是否恢复。
 *        取值刻意分散(0x00/0x11/0x22/0x33)，避免空片0xFF被误识别。
 */
typedef enum {
	OTA_PERSIST_STATE_IDLE = 0x00U,                 /* 空闲：无进行中的升级（正常运行态） */
	OTA_PERSIST_STATE_BACKUP_IN_PROGRESS = 0x11U,   /* 正在写W25Q128备份；此态掉电时A区还没擦，
													   上电只需清状态按当前A区继续启动 */
	OTA_PERSIST_STATE_UPGRADING = 0x22U,            /* A区已开始擦除/写入；此态掉电必须回滚 */
	OTA_PERSIST_STATE_NEED_ROLLBACK = 0x33U         /* 已确认需要回滚（失败/ABORT/超时） */
} OTA_PersistentState_t;

/**
 * @brief W25Q128备份镜像的元数据记录（运行时结构体形态）
 * @note  落EEPROM时不是直接按结构体写，而是打包成32字节raw帧（小端序+魔数+记录CRC），
 *        具体字节布局见 OTA_StorageWriteBackupRecord()。
 */
typedef struct {
	uint8_t version[OTA_VERSION_LEN];   /* 备份固件的版本号(8字节ASCII) */
	uint32_t size;                      /* 备份固件大小(字节) */
	uint32_t crc32;                     /* 备份固件整体CRC32，回滚前后都要据此校验 */
} OTA_BackupRecord_t;

static uint8_t s_eeprom_ready = 0U;        /* EEPROM是否可用：探测失败/关键数据读失败时清0，
											  为0时所有写元数据/升级动作一律拒绝，但保留旧跳转 */
static uint8_t s_baseline_ready = 0U;     /* 是否存在已验证可信的A区基线：
											  InitAndRecover走完且A区CRC通过才置1；
											  为0时VerifyActiveImage返回UNKNOWN、禁止覆盖式升级 */
static OTA_StorageRecoveryNotice_t s_recovery_notice = OTA_STORAGE_RECOVERY_NOTICE_NONE;
										  /* 恢复通知的RAM缓存（EEPROM 0xB8的镜像）：
											  上电由LoadRecoveryNotice载入，SetRecoveryNotice双写RAM+EEPROM，
											  ota_core每帧查询，非NONE时对旧会话回NACK 0x08 */


/**
 * @brief CRC32 流式更新（标准IEEE 802.3，多项式0xEDB88320，与OTA协议同一算法）
 * @param crc  上一段计算后的中间值（首段传0xFFFFFFFF）
 * @param data 本段数据
 * @param len  本段长度
 * @return 本段计算后的CRC中间值（还未异或收尾值）
 * @note  拆成Update+首尾异或的形式，是为了支持256字节分块流式计算几百KB镜像，
 *        不必开整镜像大小的缓冲区。
 */
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


/**
 * @brief 计算一段完整数据的CRC32（初值/收尾各异或0xFFFFFFFF）
 */
static uint32_t OTA_StorageCRC32(const uint8_t *data, uint32_t len)
{
	return OTA_StorageCRC32Update(0xFFFFFFFFU, data, len) ^ 0xFFFFFFFFU;
}


/**
 * @brief 将32位整数按小端序（低字节在前）写入4字节缓冲区
 * @note  EEPROM备份记录固定使用小端序存储，与MCU原生字节序解耦，
 *        保证记录格式在不同编译器/平台上可重复解析。
 */
static void OTA_StoragePutU32LE(uint8_t *buffer, uint32_t value)
{
	buffer[0] = (uint8_t)(value & 0xFFU);
	buffer[1] = (uint8_t)((value >> 8) & 0xFFU);
	buffer[2] = (uint8_t)((value >> 16) & 0xFFU);
	buffer[3] = (uint8_t)((value >> 24) & 0xFFU);
}


/**
 * @brief 从4字节缓冲区按小端序还原32位整数（OTA_StoragePutU32LE的逆操作）
 */
static uint32_t OTA_StorageGetU32LE(const uint8_t *buffer)
{
	return ((uint32_t)buffer[0]) |
	       ((uint32_t)buffer[1] << 8) |
	       ((uint32_t)buffer[2] << 16) |
	       ((uint32_t)buffer[3] << 24);
}


/**
 * @brief 粗检A区开头的中断向量表是否像一个合法APP
 * @return 1-像合法向量；0-明显是擦空(0xFF)或垃圾数据
 *
 * @details 检查向量表前两个字（Cortex-M 复位序列）：
 *            - 第0个字=初始MSP，必须落在STM32F407的SRAM区间0x20000000~0x2002FFFF(192KB)；
 *            - 第1个字=复位向量，必须满足：
 *                ① bit0=1（Thumb指令集标志，Cortex-M硬性要求）；
 *                ② 落在A区范围 [FLASH_ACode_STARTADDR, 0x08100000) 之内。
 * @note 这只是"跳过去会不会直接硬fault"的廉价预判，镜像完整性仍以整体CRC为准。
 */
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


/**
 * @brief 镜像大小合法性：非0且不超过备份区容量960KB
 * @note  这是所有元数据/信息帧入库前的统一闸门，防止0长度或越界大小引发后续误擦写。
 */
static uint8_t OTA_StorageIsImageSizeValid(uint32_t size)
{
	return (size > 0U && size <= OTA_BACKUP_REGION_SIZE) ? 1U : 0U;
}


/**
 * @brief 流式计算A区内部Flash中镜像的整体CRC32
 * @param size  镜像字节数（从A区起始地址算起）
 * @param crc32 输出：计算结果
 * @return HAL_OK 成功；HAL_ERROR 参数非法
 * @note  每读256字节喂一次狗，防止校验整份镜像期间IWDG复位。
 */
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


/**
 * @brief 流式计算W25Q128备份区中镜像的整体CRC32
 * @param size  镜像字节数（从备份区偏移0算起）
 * @param crc32 输出：计算结果
 * @return HAL_OK 成功（含SPI读取失败）；HAL_ERROR 参数非法
 * @note  用途：备份写完后回读校验、回滚前确认备份未损坏、回滚后与记录CRC比对。
 *        W25Q128_Read末参数传入本地buffer大小作为读上限。
 */
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


/**
 * @brief 写EEPROM单个控制字节（0xB4~0xB8各标志的统一入口）
 * @return HAL_ERROR 表示EEPROM未就绪；其余返回AT24C02驱动的写入结果
 */
static HAL_StatusTypeDef OTA_StorageWriteControlByte(uint16_t address, uint8_t value)
{
	if (!s_eeprom_ready)
	{
		return HAL_ERROR;
	}

	return EP24C_WriteBytes(address, &value, 1U);
}


/**
 * @brief 上电时把EEPROM 0xB8的恢复通知载入RAM缓存 s_recovery_notice
 * @details 只承认两个合法魔数0xA1/0xA2，其它任何值（含空片0xFF）一律归一为NONE。
 * @return AT24C02读取结果；读取失败时不修改缓存
 */
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


/**
 * @brief 登记持久化升级状态（格式版本→回滚原因→状态，按固定顺序三字节写）
 * @param state  新的持久化状态
 * @param reason 回滚原因（无回滚时传OTA_STORAGE_REASON_NONE）
 * @return 任一字节写入失败即返回，已写入的前序字节不回滚
 * @note 状态字节故意最后写：若写的过程中掉电，EEPROM保留旧状态，
 *       上电恢复逻辑按"更早、更保守"的状态处理（宁可多恢复一次，不可漏掉恢复）。
 */
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


/**
 * @brief 一次性读出4个启动恢复用的控制字节（备份有效/状态/原因/格式版本）
 * @param backup_valid 输出：0xB4备份有效标志
 * @param state        输出：0xB5持久化状态
 * @param reason       输出：0xB6回滚原因
 * @param format       输出：0xB7格式版本
 * @return HAL_ERROR 任一输出指针为NULL；否则返回AT24C02读取结果（任一字节读失败即返回）
 */
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


/**
 * @brief 打包并写入32字节备份记录到EEPROM 0x4C，写完立即回读比对
 * @param record 待写入的备份元数据（size必须合法）
 * @return HAL_OK 写入且回读一致；HAL_ERROR 参数非法/写入失败/回读不一致
 *
 * @details raw[32] 字节布局（整帧先填0xFF，再按偏移写字段）：
 *            [0..3]   魔数 'R''B''K''1'
 *            [4]      格式版本 0x31
 *            [5..7]   保留(0xFF)
 *            [8..15]  版本号 8字节
 *            [16..19] 镜像大小，小端
 *            [20..23] 镜像CRC32，小端
 *            [24..27] 记录自身CRC32（对raw[0..23]计算），小端——防止记录被半写/篡改
 *            [28..31] 保留(0xFF)
 * @note 回读memcmp是AT24C02写后验证的最后一道保险，避免"写成功"的假结论。
 */
static HAL_StatusTypeDef OTA_StorageWriteBackupRecord(const OTA_BackupRecord_t *record)
{
	uint8_t raw[OTA_EEPROM_BACKUP_RECORD_SIZE];
	uint8_t verify[OTA_EEPROM_BACKUP_RECORD_SIZE];
	HAL_StatusTypeDef status;

	if (record == NULL || !OTA_StorageIsImageSizeValid(record->size))
	{
		return HAL_ERROR;
	}

	memset(raw, 0xFF, sizeof(raw));            /* 未使用字节统一填0xFF，与擦除态一致 */
	raw[0] = OTA_BACKUP_MAGIC0;
	raw[1] = OTA_BACKUP_MAGIC1;
	raw[2] = OTA_BACKUP_MAGIC2;
	raw[3] = OTA_BACKUP_MAGIC3;
	raw[4] = OTA_STORAGE_FORMAT_VERSION;
	memcpy(&raw[8], record->version, OTA_VERSION_LEN);
	OTA_StoragePutU32LE(&raw[16], record->size);
	OTA_StoragePutU32LE(&raw[20], record->crc32);
	OTA_StoragePutU32LE(&raw[24], OTA_StorageCRC32(raw, 24U));   /* 对前24字节算记录CRC */

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


/**
 * @brief 读取并校验32字节备份记录，校验通过才解析成OTA_BackupRecord_t
 * @param record 输出：解析后的备份元数据
 * @return HAL_OK 魔数/格式版本/记录CRC/大小全部合法；HAL_ERROR 任一环节不通过
 * @note  校验顺序：读EEPROM → 魔数RBK1+格式版本 → 记录CRC(前24字节) → size范围，
 *         任何一项失败都视为"没有可信备份"，调用方据此拒绝回滚。
 */
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


/**
 * @brief 综合判断"W25Q128备份是否可用于回滚"
 * @return 1-EEPROM就绪 且 格式版本正确 且 备份标志=0xA5 且 备份记录完整合法；否则0
 * @note  只检查EEPROM侧的"备份账本"，不读W25Q128实际内容；
 *        备份镜像本身的CRC在回滚前由 OTA_StorageCalculateBackupCRC 再验一次。
 */
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
			/* 与 OTA_StorageCRC32Update 等价的逐位算法：先异或当前字节，
			 * 再连续右移8次（这里手工展开8行，省掉内层for循环开销）。
			 * 右侧的0即"最低位为0时不异或多项式"，保持与标准CRC32结果一致。 */
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

/**
 * @brief 提交当前A区镜像为可信基线（完整契约见 ota_storage.h 同名声明）
 * @note  .c侧实现顺序：参数/就绪闸门 → 填OTA_INFO并写EEPROM → 写格式版本
 *        → 持久化状态清IDLE（同时清除回滚原因，解除UPGRADING）。
 *        新固件提交与回滚后重新登记旧镜像共用本函数。
 */
HAL_StatusTypeDef OTA_Storage_CommitActiveImage(uint32_t size,
												 uint32_t crc32,
												 const uint8_t *version)
{
	HAL_StatusTypeDef status;

	/* 三道参数闸门，任一不过就原地返回、一个字节都不写：
	 *   s_eeprom_ready==0 —— EEPROM不在线/通信失败，写了也读不回来，
	 *                        此时登记基线等于制造假账本，直接拒绝；
	 *   version==NULL     —— 版本号是必填字段，不允许空指针；
	 *   size非法          —— 0或超过A区容量，登记进去会让下次启动校验必失败。
	 * 注意本函数是"提交"，调用方必须已自行完成CRC验证（新固件或回滚后的旧镜像），
	 * 这里不再重复算CRC——只做登记。*/
	if (!s_eeprom_ready || version == NULL || !OTA_StorageIsImageSizeValid(size))
	{
		return HAL_ERROR;
	}

	/* 第一步：先填RAM里的OTA_INFO镜像。
	 * 版本号先memset再memcpy的原因：version可能不满8字节，
	 * 不清零会把上一次残留的版本尾巴带到EEPROM里，日志和比对都会失真。*/
	OTA_INFO.Firelen[0] = size;
	OTA_INFO.Firelen[1] = crc32;
	memset(OTA_INFO.OTA_VERSION, 0, sizeof(OTA_INFO.OTA_VERSION));
	memcpy(OTA_INFO.OTA_VERSION, version, OTA_VERSION_LEN);

	/* 第二步：OTA_INFO整块落盘（大小+CRC+版本号）。
	 * 这一步失败必须直接返回，绝不能继续清状态——否则会把"未完成"的证据抹掉。*/
	status = EP24C_WriteBytes(OTA_EEPROM_INFO_ADDR, (const uint8_t *)&OTA_INFO, OTA_INFOCB_SIZE);
	if (status != HAL_OK)
	{
		return status;
	}

	/* 第三步：写格式版本字节0xB7=0x31。
	 * 它的作用是给整段元数据"盖章"：启动时只有format等于本版本，
	 * 才会采信0xB4/0xB5/0xB6这些控制字，否则当作空片或旧格式处理。*/
	status = OTA_StorageWriteControlByte(OTA_EEPROM_FORMAT_ADDR, OTA_STORAGE_FORMAT_VERSION);
	if (status != HAL_OK)
	{
		return status;
	}

	/* 第四步（最后一步）：状态清回IDLE并清掉回滚原因。
	 * 这一步才是"提交"的实质——解除CreateBackup写入的UPGRADING登记，
	 * 也解除NeedRollback的NEED_ROLLBACK登记，下次上电就不会再触发回滚。
	 * 顺序为什么必须放最后：若前面任何一步失败就返回，状态仍保持UPGRADING/NEED_ROLLBACK，
	 * 下次上电 InitAndRecover 会走"未完成升级"分支去恢复旧镜像——宁可多恢复一次，
	 * 也不允许出现"元数据写了一半却标记为已完成"的假账本。*/
	return OTA_StorageSetPersistentState(OTA_PERSIST_STATE_IDLE, OTA_STORAGE_REASON_NONE);
}


/**
 * @brief 校验当前A区镜像是否可信（跳转APP/允许升级前的门禁）
 * @return OTA_STORAGE_IMAGE_UNKNOWN ：从未登记过基线（s_baseline_ready=0），保持旧跳转行为；
 *         OTA_STORAGE_IMAGE_INVALID ：向量表非法 / CRC计算失败 / CRC与登记值不符，禁止跳转；
 *         OTA_STORAGE_IMAGE_VALID   ：向量合法且整体CRC32与EEPROM登记一致。
 * @note 判定分两层：先做向量表粗检（拦截擦空/半写镜像），再做整体CRC细检。
 */
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


/**
 * @brief 是否允许开始一次覆盖式升级（擦A区前的最终门禁）
 * @details 必须三者同时成立：EEPROM可用 + 存在可信基线 + A区实时校验VALID。
 *          含义是"手里必须攥着一个已验证的好镜像，才允许把A区擦掉"。
 */
uint8_t OTA_Storage_IsUpgradeAllowed(void)
{
	return (s_eeprom_ready && s_baseline_ready &&
		OTA_Storage_VerifyActiveImage() == OTA_STORAGE_IMAGE_VALID) ? 1U : 0U;
}


/**
 * @brief 获取当前恢复通知（RAM缓存，不触发EEPROM读）
 * @note ota_core每帧/状态机轮询都会调用，因此只读RAM保证实时且无I2C开销。
 */
OTA_StorageRecoveryNotice_t OTA_Storage_GetRecoveryNotice(void)
{
	return s_recovery_notice;
}


/**
 * @brief 设置恢复通知并双写EEPROM(0xB8)+RAM缓存
 * @param notice 只允许NONE/0xA1/0xA2三个合法值
 * @return HAL_ERROR 值非法或EEPROM写入失败（此时RAM缓存也保持不变）
 */
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


/**
 * @brief 确认并清除恢复通知（ESP32用ABORT结束旧会话后调用）
 * @note  本质是SetRecoveryNotice(NONE)；清除成功后ota_core才放行新的升级会话。
 */
HAL_StatusTypeDef OTA_Storage_AcknowledgeRecoveryNotice(void)
{
	return OTA_Storage_SetRecoveryNotice(OTA_STORAGE_RECOVERY_NOTICE_NONE);
}


/**
 * @brief 擦A区之前：把当前已验证A区镜像整体备份到W25Q128并登记备份记录
 * @return HAL_OK 备份完成且双重CRC验证通过；HAL_ERROR 任一前置条件或操作失败（此时A区尚未被动过）
 *
 * @details 完整阶段（严格顺序，任何一步失败都在擦A区之前中止）：
 *   阶段1 前置门禁：恢复通知必须已清除（不覆盖上一轮唯一备份）+ 当前基线必须可信；
 *   阶段2 登记 BACKUP_IN_PROGRESS 持久化状态，并把备份有效标志先清0；
 *   阶段3 按64KB块擦除W25Q128备份区（960KB约15块，按镜像大小向上覆盖）；
 *   阶段4 按W25Q128页大小分块：A区读出→边算源CRC→页编程写入W25Q128；
 *   阶段5 源CRC必须与EEPROM登记基线CRC一致（防止备份的就是已悄悄损坏的A区）；
 *   阶段6 回读W25Q128算备份CRC，必须与源CRC一致（防止写Flash过程出错）；
 *   阶段7 写32字节备份记录(含记录CRC)，最后才把备份有效标志置0xA5。
 * @note 备份有效标志"先清0、最后置A5"的顺序，保证写到一半掉电时备份一定被判为无效。
 */
HAL_StatusTypeDef OTA_Storage_CreateBackup(void)
{
	OTA_BackupRecord_t record;              /* 备份元数据：镜像大小/CRC/版本，最后写进EEPROM 0x4C */
	uint8_t buffer[W25Q128_PAGE_SIZE];      /* 一页=256字节，既是W25Q128的页写边界，也是读A区的搬运块 */
	uint32_t offset = 0U;                   /* 备份偏移，从0开始按页推进，始终256字节对齐 */
	uint32_t erase_offset;                  /* 块擦除游标，按64KB步进 */
	uint32_t chunk;                         /* 本次实际搬运字节数（末页可能不足256） */
	uint32_t source_crc = 0xFFFFFFFFU;      /* 边读A区边算的CRC，初值为CRC32标准初值 */
	uint32_t backup_crc = 0U;               /* 回读W25Q128后重算的CRC，用于交叉验证 */
	uint32_t i;
	HAL_StatusTypeDef status;

	/* 门禁1：上一轮的恢复通知还没被ESP32用ABORT确认时，禁止开新升级。
	 * 因为本次备份会覆盖W25Q128里那份"上一轮恢复用的唯一凭据"：
	 * 一旦覆盖，旧会话的就地恢复能力就没了，只能拒绝。 */
	if (OTA_Storage_GetRecoveryNotice() != OTA_STORAGE_RECOVERY_NOTICE_NONE)
	{
		U1_Printf("[回滚] 上一轮恢复通知尚未确认，拒绝覆盖备份\r\n");
		return HAL_ERROR;
	}

	/* 门禁2：必须"手里攥着一个已验证的好镜像"才允许往下走。
	 * IsUpgradeAllowed要求三者同时成立：EEPROM可用 + 有基线 + A区实时CRC=VALID。
	 * 任何一条不满足，接下来要擦的就是唯一能跑的固件，绝不允许。*/
	if (!OTA_Storage_IsUpgradeAllowed())
	{
		U1_Printf("[回滚] 当前A区基线不可用，拒绝擦除升级\r\n");
		return HAL_ERROR;
	}

	U1_Printf("[回滚] 开始备份当前A区: %lu 字节\r\n", OTA_INFO.Firelen[0]);
	/* 阶段2：登记BACKUP_IN_PROGRESS持久化状态（掉电重启时据此判断A区尚未擦） */
	status = OTA_StorageSetPersistentState(OTA_PERSIST_STATE_BACKUP_IN_PROGRESS,
										 OTA_STORAGE_REASON_NONE);
	if (status != HAL_OK)
	{
		return status;
	}

	/* 阶段3前奏：先把"备份有效"标志清成无效(0x00)。
	 * 顺序原因：后面要擦要写W25Q128，任何一步掉电都会留下一份半截镜像；
	 * 此时标志若还是0xA5，下次上电就会拿半截镜像去恢复A区——
	 * 先把门关上，最后验证全过才重新挂上0xA5。*/
	status = OTA_StorageWriteControlByte(OTA_EEPROM_BACKUP_VALID_ADDR, OTA_BACKUP_INVALID_VALUE);
	if (status != HAL_OK)
	{
		return status;
	}

	/* 阶段3：按64KB块擦除备份区，只擦到"实际镜像大小"为止，不擦满960KB。
	 * 这里没有用整片擦除/扇区擦除，而是复用64KB块擦接口：
	 * 单块擦除耗时远小于整片擦，能明显缩短ESP32等待START_TRANSFER的ACK时间。
	 * 循环内每块都喂狗：960KB/64KB=15次擦除，单次擦除可能几百ms，
	 * 不喂狗会直接触发IWDG复位。
	 * 注意就是"擦多少由Firelen[0]决定"，所以备份区尾部可能残留旧数据，
	 * 但回滚只按record.size读取，尾部残留不会被用到。*/
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

	/* 阶段4：按页从A区读出并写入W25Q128，同时累加"源镜像"CRC（offset始终页对齐） */
	while (offset < OTA_INFO.Firelen[0])
	{
		Bootloader_WatchdogRefresh();	/* 960KB要搬3000多页，必须每页喂狗 */
		/* chunk = min(剩余字节, 一页256)：末页不足256时只搬剩余部分。
		 * 之所以必须按页切：W25Q128_PageWrite只能在一页边界内写，
		 * 一次跨页会在页尾回卷，把数据写到错误的物理地址去。*/
		chunk = OTA_INFO.Firelen[0] - offset;
		if (chunk > sizeof(buffer))
		{
			chunk = sizeof(buffer);
		}

		/* 从STM32内部Flash逐字节读出。用读接口而不是直接指针访问，
		 * 是为了和工程里其它Flash访问方式保持一致（地址也可能位于需特殊访问的区域）。*/
		for (i = 0U; i < chunk; i++)
		{
			buffer[i] = MyFLASH_ReadByte(FLASH_ACode_STARTADDR + offset + i);
		}
		/* 边搬边算：省掉为了算CRC而再遍历一遍A区的时间 */
		source_crc = OTA_StorageCRC32Update(source_crc, buffer, chunk);

		status = W25Q128_PageWrite(OTA_BACKUP_REGION_ADDR + offset, buffer, (uint16_t)chunk);
		if (status != HAL_OK)
		{
			U1_Printf("[回滚] W25Q128写入失败: 0x%06lX\r\n", OTA_BACKUP_REGION_ADDR + offset);
			/* 此时backup_valid仍是无效值，所以这份半截备份不会被误用；
			 * 但EEPROM状态还是BACKUP_IN_PROGRESS，下次上电会走阶段D清回IDLE。*/
			return status;
		}

		offset += chunk;
	}

	/* 阶段5：源镜像CRC收尾，必须与EEPROM登记的基线CRC一致。
	 * CRC32算法用反射式(初值0xFFFFFFFF)，所以最终要异或0xFFFFFFFF。
	 * 这一比对的意义：确认"我刚刚备份下来的这份内容，和我账本上记的那个好镜像
	 * 确实是同一个"。若不一致，说明A区在进入本函数前后已经悄悄变了
	 * （典型是上次升级没回滚干净），备份它毫无意义，必须中止——
	 * 否则会把一个坏镜像当成回滚依据。*/
	source_crc ^= 0xFFFFFFFFU;
	if (source_crc != OTA_INFO.Firelen[1])
	{
		U1_Printf("[回滚] A区基线CRC变化: 0x%08lX != 0x%08lX\r\n", source_crc, OTA_INFO.Firelen[1]);
		return HAL_ERROR;
	}

	/* 阶段6：回读整片备份再算一次CRC，与源CRC比对，验证W25Q128写入无误。
	 * 为什么必须"回读"而不是信任PageWrite的返回值：SPI Flash写失败常常表现为
	 * "命令时序成功但位没翻过去"（写保护、页损坏、掉电中途），只有回读才算实证；
	 * 这一步不通过，就绝不能写记录、更不能置有效标志。*/
	status = OTA_StorageCalculateBackupCRC(OTA_INFO.Firelen[0], &backup_crc);
	if (status != HAL_OK || backup_crc != source_crc)
	{
		U1_Printf("[回滚] 备份CRC校验失败: 0x%08lX\r\n", backup_crc);
		return HAL_ERROR;
	}

	/* 阶段7：镜像验证全部通过后才写备份记录，最后置备份有效标志（顺序不可颠倒）。
	 * record只装三个字段：版本号(供日志/显示)、大小(回滚时按它读)、CRC(回滚时按它验)；
	 * 真正落盘时由 WriteBackupRecord 打包成32字节并附上记录自身CRC，
	 * 这样记录被半写/篡改时能被识别，而非直接拿去用。*/
	memcpy(record.version, OTA_INFO.OTA_VERSION, OTA_VERSION_LEN);
	record.size = OTA_INFO.Firelen[0];
	record.crc32 = source_crc;
	status = OTA_StorageWriteBackupRecord(&record);
	if (status != HAL_OK)
	{
		/* 记录没写成：有效标志仍是无效，本次备份作废，但A区未被动过，可继续跑旧固件 */
		return status;
	}

	/* 最后一步：置0xA5。这是"提交"动作——此字节一写成功，这份备份才正式可用于回滚。
	 * 顺序逻辑：内容(镜像CRC) → 记录(元数据+记录CRC) → 标志(0xA5)，
	 * 标志永远最后写，保证任何中途掉电都停在"备份无效"这一侧。*/
	status = OTA_StorageWriteControlByte(OTA_EEPROM_BACKUP_VALID_ADDR, OTA_BACKUP_VALID_VALUE);
	if (status == HAL_OK)
	{
		U1_Printf("[回滚] A区备份完成，CRC32=0x%08lX\r\n", source_crc);
	}
	return status;
}


/**
 * @brief 擦A区前最后一道登记：状态置UPGRADING（掉电重启即触发启动回滚）
 * @return HAL_ERROR 仍有未确认的恢复通知（旧会话未结束）或备份账本无效；HAL_OK 登记成功
 * @note 调用方（ota_core收到0x04）必须在本函数成功后才允许擦除A区。
 */
HAL_StatusTypeDef OTA_Storage_MarkUpgradeInProgress(void)
{
	/* 两个前置条件，缺一不可：
	 *   ① s_recovery_notice==NONE：上一轮会话还没被ESP32用ABORT收尾。
	 *      此时若开新升级，STM32会对旧会话帧继续回NACK，双方永远对不上；
	 *   ② HasValidBackup()：必须有0xA5有效的备份账本。
	 *      因为本函数之后调用方就要擦A区，没有备份=没有退路，绝不允许。
	 * 用RAM缓存 s_recovery_notice 而不是 GetRecoveryNotice()：
	 * 两者值相同，这里少一次函数调用；该缓存已由InitAndRecover/SetRecoveryNotice维护。*/
	if (s_recovery_notice != OTA_STORAGE_RECOVERY_NOTICE_NONE || !OTA_StorageHasValidBackup())
	{
		return HAL_ERROR;
	}

	/* 只写"状态+原因"，原因填NONE——UPGRADING本身不是失败，
	 * 是正常流程中的一个阶段标记；失败原因由后续 MarkRollbackPending 写入。
	 * SetPersistentState内部的写序是 格式→原因→状态，
	 * 所以这里返回HAL_OK就代表"掉电后一定能被识别为升级中"。*/
	return OTA_StorageSetPersistentState(OTA_PERSIST_STATE_UPGRADING,
										 OTA_STORAGE_REASON_NONE);
}


/**
 * @brief 登记"待回滚"及原因（CRC失败/Flash错误/ABORT/超时/启动CRC失败）
 * @note 只持久化决策状态，不执行实际拷贝；真正的镜像恢复由 OTA_Storage_Rollback() 完成，
 *       这样即使在登记后立刻掉电，重启时 InitAndRecover 仍会补上恢复。
 */
HAL_StatusTypeDef OTA_Storage_MarkRollbackPending(OTA_StorageReason_t reason)
{
	return OTA_StorageSetPersistentState(OTA_PERSIST_STATE_NEED_ROLLBACK, reason);
}


/**
 * @brief 执行回滚：用W25Q128备份镜像重建A区，并重新提交旧镜像为可信基线
 * @return HAL_OK 恢复成功；HAL_ERROR 备份不存在/备份CRC损坏/擦写失败/A区回读CRC不符
 *
 * @details 阶段顺序：
 *   1. 检查EEPROM备份账本有效（HasValidBackup）+ 读出备份记录；
 *   2. 先算W25Q128备份镜像CRC并与记录值比对——备份本身坏了就绝不能动A区；
 *   3. 擦除A区；
 *   4. 256字节分块：W25Q128读出 → 写入A区（复用带4字节对齐填充的OTA_WriteFirmwareData）；
 *   5. 整体回读A区算CRC，与记录值比对；
 *   6. CommitActiveImage重新登记旧镜像元数据、状态清IDLE、置s_baseline_ready=1。
 * @note 失败时不跳转、不修改基线标志，由调用方进入RECOVERY_FAILED停机态。
 */
HAL_StatusTypeDef OTA_Storage_Rollback(void)
{
	OTA_BackupRecord_t record;          /* 从EEPROM 0x4C读出的备份账本：size/crc32/version */
	uint8_t buffer[256];                /* 回拷搬运块；256是4的倍数，天然满足写Flash对齐要求 */
	uint32_t offset = 0U;               /* 恢复偏移，A区与备份区同偏移一一对应 */
	uint32_t chunk;                     /* 本次搬运字节数（末块可能不足256） */
	uint32_t backup_crc = 0U;           /* 擦A区之前：W25Q128里那份备份的实测CRC */
	uint32_t restored_crc = 0U;         /* 写回之后：A区现内容的实测CRC */
	HAL_StatusTypeDef status;

	/* 第1步：账本门禁。0xB4不等于0xA5就说明根本没有"已验证可用的备份"，
	 * 此时A区坏了也无药可救——直接失败返回，由调用方进入RECOVERY_FAILED停机，
	 * 绝不带着坏镜像跳转。*/
	if (!OTA_StorageHasValidBackup())
	{
		U1_Printf("[回滚] 无有效备份记录，拒绝跳转A区\r\n");
		return HAL_ERROR;
	}

	/* 第2步：读出32字节备份记录。ReadBackupRecord内部会校验魔数'RBK1'、
	 * 格式版本和记录自身CRC，任一不符即返回错误——防止拿一条半写/脏记录
	 * 去解释后面的镜像。*/
	status = OTA_StorageReadBackupRecord(&record);
	if (status != HAL_OK)
	{
		return status;
	}

	/* 第3步（全函数最关键的一步）：在擦除A区之前，先验证W25Q128里的备份本身完好。
	 * 擦除是不可逆的：若先擦A区再发现备份也坏了，就等于两个镜像同时毁掉，
	 * 设备只能返厂。所以顺序只能是"先验备份，后毁A区"。*/
	status = OTA_StorageCalculateBackupCRC(record.size, &backup_crc);
	if (status != HAL_OK || backup_crc != record.crc32)
	{
		U1_Printf("[回滚] W25Q128备份损坏: 0x%08lX\r\n", backup_crc);
		return HAL_ERROR;
	}

	/* 第4步：备份确认可用，才允许擦除受损的A区。
	 * 注意此处没有再做"是否允许升级"的门禁——回滚是救火动作，
	 * 前提是A区已经不可信，不需要它自己是好的。*/
	U1_Printf("[回滚] 开始恢复旧固件: %lu 字节\r\n", record.size);
	status = OTA_EraseAppRegion();
	if (status != HAL_OK)
	{
		return status;
	}

	/* 第5步：256字节分块回拷：W25Q128备份 → A区对应偏移。
	 * 按偏移而不是按序号搬运，因为每次读写的块大小由 record.size 决定；
	 * 每块都喂狗，大镜像恢复动辄数秒，不喂会触发IWDG。*/
	while (offset < record.size)
	{
		Bootloader_WatchdogRefresh();
		chunk = record.size - offset;
		if (chunk > sizeof(buffer))
		{
			chunk = sizeof(buffer);
		}

		/* 第4个参数传buffer容量，告诉驱动本次可用缓冲多大 */
		status = W25Q128_Read(OTA_BACKUP_REGION_ADDR + offset, buffer, chunk, sizeof(buffer));
		if (status != HAL_OK)
		{
			return status;
		}

		/* 复用OTA_WriteFirmwareData而不是直接调HAL_FLASH_Program：
		 * 它内部统一处理按4字节对齐补齐、按页/半字边界写入等细节，
		 * 保证"新固件写入"和"回滚写入"两条路径用同一套规则，行为一致。*/
		status = OTA_WriteFirmwareData(FLASH_ACode_STARTADDR + offset, buffer, chunk);
		if (status != HAL_OK)
		{
			return status;
		}

		offset += chunk;
	}

	/* 第6步：回读A区算CRC，与记录值比对。
	 * 这是"写进去了"的实证：擦写过程同样可能出现命令成功但内容不对，
	 * 只有回读一致，A区才算真正恢复。*/
	status = OTA_StorageCalculateAppCRC(record.size, &restored_crc);
	if (status != HAL_OK || restored_crc != record.crc32)
	{
		U1_Printf("[回滚] A区写回CRC失败: 0x%08lX\r\n", restored_crc);
		return HAL_ERROR;
	}

	/* 第7步：把恢复回来的旧镜像重新登记为可信基线。
	 * CommitActiveImage会写回OTA_INFO、盖格式章、把状态从NEED_ROLLBACK/UPGRADING
	 * 清回IDLE并清原因——做完这一步，下次上电才不会又触发一次回滚。
	 * 换句话说：镜像恢复(Copy)只是"文件回来了"，登记(Commit)才是"账本对上了"。*/
	status = OTA_Storage_CommitActiveImage(record.size, record.crc32, record.version);
	if (status == HAL_OK)
	{
		/* 内存标志同步置位，让本次启动无需再跑一遍VerifyActiveImage即可跳转 */
		s_baseline_ready = 1U;
		U1_Printf("[回滚] 恢复成功，CRC32=0x%08lX\r\n", restored_crc);
	}

	return status;
}


/**
 * @brief 上电初始化与启动恢复（本次上电只由OTA_Init调用一次）
 * @return HAL_OK 可继续启动（可能已恢复、已迁移、或仅是"无基线保持旧跳转"）；
 *         HAL_ERROR A区已损坏且没有可信备份可恢复，调用方必须进入RECOVERY_FAILED停机
 *
 * @details 决策流程（按代码先后）：
 *   阶段A 探测EEPROM：不在线/读INFO失败/读控制字失败 → 保持旧跳转能力，禁止升级，返回OK；
 *   阶段B 载入恢复通知(0xB8)；读失败则直接禁止升级；
 *   阶段C 持久化状态=UPGRADING/NEED_ROLLBACK：上次升级未完成，无条件Rollback()，
 *          成功后置POWER_LOSS通知并返回（等ESP32用ABORT结束旧会话）；失败返回HAL_ERROR；
 *   阶段D 状态=BACKUP_IN_PROGRESS：备份阶段掉电，A区没被动过，仅清回IDLE继续；
 *   阶段E OTA_INFO大小不可解释：无基线，保持旧跳转、禁止覆盖升级；
 *   阶段F 格式版本不符（旧版/空片数据）：向量有效时实测A区CRC并"迁移"成新格式元数据
 *          （备份标志置无效、状态清IDLE、通知清除）；
 *   阶段G 当前格式下A区向量/CRC任一不匹配：优先Rollback()恢复，成功置POWER_LOSS通知，
 *          无备份或恢复失败则返回HAL_ERROR；
 *   阶段H 全部通过：s_baseline_ready=1，允许后续跳转与升级。
 *
 * @note 设计原则：EEPROM有问题时"降级保活"（照跳A区但禁止升级）；
 *       而能确认A区损坏时"宁缺毋滥"（无备份就停机，绝不跳到坏镜像）。
 *       reason随控制字一并读出（当前恢复决策只依据state，reason保留供日志/扩展使用）。
 */
HAL_StatusTypeDef OTA_Storage_InitAndRecover(void)
{
	uint8_t backup_valid;
	uint8_t state;
	uint8_t reason;
	uint8_t format;
	uint32_t current_crc;
	HAL_StatusTypeDef status;

	/* 每次进入先复位三个RAM标志，按本次EEPROM实际内容重新判定。
	 * 为什么必须复位而不是沿用上次值：本函数是"上电唯一裁判"，
	 * 上一次上电留下的s_baseline_ready=1如果没有被清，EEPROM已经换了/坏了
	 * 也会被误当成"基线可信"，进而允许覆盖擦除A区。*/
	s_eeprom_ready = 0U;
	s_baseline_ready = 0U;
	s_recovery_notice = OTA_STORAGE_RECOVERY_NOTICE_NONE;

	/* 阶段A：探测EEPROM。不在线属于"降级保活"场景：照跳A区，但s_eeprom_ready=0
	 * 会让后续所有升级/备份/提交动作被拒，不会在无账本保护时覆盖A区。
	 * 返回HAL_OK而不是HAL_ERROR的原因：EEPROM坏不代表固件坏，
	 * 直接停机反而把一个能用的设备变成砖；只是从此不能再升级。*/
	if (EP24C_IsDeviceReady() != HAL_OK)
	{
		U1_Printf("[回滚] AT24C02未就绪，保持旧跳转能力并禁止升级\r\n");
		return HAL_OK;
	}
	s_eeprom_ready = 1U;

	/* 读OTA_INFO（大小/CRC/版本号）。失败同样降级保活：
	 * 拿不到基线信息就无法校验A区，强行升级会失去回滚依据。*/
	status = EP24C_ReadBytes(OTA_EEPROM_INFO_ADDR, (uint8_t *)&OTA_INFO,
							 OTA_INFOCB_SIZE, OTA_INFOCB_SIZE);
	if (status != HAL_OK)
	{
		U1_Printf("[回滚] 读取OTA_INFO失败，禁止升级\r\n");
		return HAL_OK;
	}

	/* 一次性读出4个控制字节：0xB4备份有效、0xB5持久化状态、0xB6回滚原因、0xB7格式版本。
	 * 后面所有阶段的分支判据全部来自这4个字节，不再单独读EEPROM。*/
	status = OTA_StorageReadControl(&backup_valid, &state, &reason, &format);//读取回滚控制字
	if (status != HAL_OK)
	{
		U1_Printf("[回滚] 读取回滚控制字失败，禁止升级\r\n");
		return HAL_OK;
	}

	/* 阶段B：仅当元数据格式可信时才采信恢复通知；读失败说明EEPROM通信不可靠，
	 * 主动把s_eeprom_ready清0，退化为"可跳转、禁升级"。
	 * 为什么不采信就直接跳过：format不是0x31代表这段控制字是旧版/空片数据，
	 * 里面的0xB8自然也没有意义，硬读进来只会拿垃圾值去拦新会话。*/
	if (format == OTA_STORAGE_FORMAT_VERSION && OTA_StorageLoadRecoveryNotice() != HAL_OK)
	{
		U1_Printf("[回滚] 读取恢复通知失败，禁止升级\r\n");
		s_eeprom_ready = 0U;	/* 通信不可靠，不能允许后续任何写EEPROM的动作 */
		return HAL_OK;
	}

	/* 阶段C：发现UPGRADING/NEED_ROLLBACK——上次升级在擦A区后中断（掉电/失败），
	 * 无条件执行回滚；成功后挂POWER_LOSS通知，让ESP32用ABORT给旧会话收尾。
	 * 为什么"无条件"：这两个状态是"先登记、后动手"留下的铁证，
	 * 只要看到就说明A区确实被擦过，不存在"可能白回滚"的顾虑。*/
	if (format == OTA_STORAGE_FORMAT_VERSION &&
		(state == OTA_PERSIST_STATE_UPGRADING || state == OTA_PERSIST_STATE_NEED_ROLLBACK))
	{
		U1_Printf("[回滚] 检测到未完成升级，进入启动恢复\r\n");
		/* 先看备份标志0xB4是否0xA5，再调Rollback()。
		 * 其实Rollback内部也会用HasValidBackup()再查一次，这里提前判是为了
		 * 把"没有备份"和"恢复过程失败"区分开打印，方便现场判断是哪种损坏。
		 * 两者任一失败都返回HAL_ERROR → 调用方进RECOVERY_FAILED停机，
		 * 因为A区已残，跳过去必死。*/
		if (backup_valid != OTA_BACKUP_VALID_VALUE || OTA_Storage_Rollback() != HAL_OK)
		{
			U1_Printf("[回滚] 启动恢复失败，停在Bootloader\r\n");
			return HAL_ERROR;
		}
		/* 恢复成功后挂"掉电类型"通知(0xA1)。这条通知的作用不是给本机看，
		 * 而是让IDLE阶段的ProcessFrame对旧会话帧回NACK 0x08，
		 * 逼ESP32发ABORT收尾——因为ESP32那边根本不知道刚才掉过电。
		 * 通知落盘失败就再没有拦住旧会话的凭据，只能停机。*/
		if (OTA_Storage_SetRecoveryNotice(OTA_STORAGE_RECOVERY_NOTICE_POWER_LOSS) != HAL_OK)
		{
			U1_Printf("[回滚] 无法保存恢复通知，停在Bootloader\r\n");
			return HAL_ERROR;
		}
		/* 直接返回，跳过E/F/G/H：A区已经恢复成旧镜像且被CommitActiveImage
		 * 重新登记（Rollback内部已完成），无需再跑一遍基线校验。
		 * s_baseline_ready此时已由Rollback置1，本次可正常跳转。*/
		U1_Printf("[回滚] 已恢复旧镜像，等待ESP32结束上一轮会话\r\n");
		return HAL_OK;
	}

	/* 阶段D：备份阶段掉电——此时A区原封不动，只需把残留的"备份中"状态清回IDLE。
	 * 这是"持久化状态区分掉电阶段"的核心价值：同样是一次中途掉电，
	 * 状态是BACKUP_IN_PROGRESS就说明还没擦A区，绝不能像阶段C那样去回滚，
	 * 否则会把本来完好的A区平白覆盖一遍。*/
	if (format == OTA_STORAGE_FORMAT_VERSION && state == OTA_PERSIST_STATE_BACKUP_IN_PROGRESS)
	{
		/* 此状态下A区尚未被擦除；清除未完成备份状态后按当前固件继续启动。
		 * 返回值故意用(void)忽略：万一这次没清成，下次上电还是这个状态，
		 * 仍然只会再清一次，既不会误回滚也不影响本次跳转，无需升级为故障。*/
		(void)OTA_StorageSetPersistentState(OTA_PERSIST_STATE_IDLE, OTA_STORAGE_REASON_NONE);
	}

	/* 阶段E：基线大小非法（空片0xFF/历史脏数据），元数据完全不可解释：
	 * 保留旧跳转能力，但s_baseline_ready保持0，禁止覆盖式升级。
	 * IsImageSizeValid同时排除了0和超过备份区容量(960KB)两种值；
	 * 全新EEPROM读出的是0xFFFFFFFF，正好落在这里。
	 * 注意这里是"什么都不写就返回"——不迁移、不清理，保持原样最安全。*/
	if (!OTA_StorageIsImageSizeValid(OTA_INFO.Firelen[0]))
	{
		U1_Printf("[回滚] 未发现可解释的旧基线，保持旧跳转能力并禁止覆盖升级\r\n");
		return HAL_OK;
	}

	/* 阶段F：格式版本不符，说明是旧版固件留下的OTA_INFO（只有大小、CRC字段可能过时）。
	 * 向量有效时实测一遍A区CRC，用实测值补全元数据并整体迁移为新格式；
	 * 迁移任何一步写失败都只"禁止升级"，不阻断本次跳转。 */
	if (format != OTA_STORAGE_FORMAT_VERSION)
	{
		/* 迁移前先做向量表粗检：连中断向量都不像合法APP（MSP/复位向量越界），
		 * 就说明A区里不是可跑的镜像，此时实测CRC也没有意义，
		 * 更不能把它登记成基线——直接保持"无基线、禁升级"。*/
		if (!OTA_StorageIsAppVectorValid())
		{
			U1_Printf("[回滚] 旧基线向量无效，保持旧跳转能力并禁止覆盖升级\r\n");
			return HAL_OK;
		}

		/* 实测A区真实CRC。为什么必须实测而不是沿用OTA_INFO.Firelen[1]：
		 * 旧格式的CRC字段可能来自更早的镜像或根本没写过，不可信；
		 * "迁移"的本质就是拿A区当前的真实内容重建账本。*/
		status = OTA_StorageCalculateAppCRC(OTA_INFO.Firelen[0], &current_crc);
		if (status != HAL_OK)
		{
			/* 算CRC都失败（读Flash出错），保持无基线跳过，不动EEPROM */
			return HAL_OK;
		}
		/* 四步迁移写入，任一步失败都只降级为"禁升级"，不阻断跳转：
		 *   ① 把实测CRC回写OTA_INFO（大小沿用原值）；
		 *   ② 备份标志置无效——旧格式时代没有可信备份，不能留着误导后续回滚；
		 *   ③ 状态清IDLE——清掉可能存在的旧状态残留；
		 *   ④ 清恢复通知——旧格式下的通知值没有意义，防止拦新会话。*/
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
		/* 迁移成功：format虽仍是旧值，但元数据已按实测值补齐。
		 * 这里没有return，会继续走到阶段H把s_baseline_ready置1，
		 * 因为A区刚刚被实测过，已经具备"可信"资格。*/
		U1_Printf("[回滚] 已迁移旧基线: size=%lu, CRC32=0x%08lX\r\n",
				  OTA_INFO.Firelen[0], current_crc);
	}
	else
	{
		/* 阶段G：当前格式下，A区必须同时满足向量合法+CRC与登记一致；
		 * 任一不匹配说明A区在非升级时刻也已损坏（如Flash老化），优先恢复唯一备份，
		 * 没有可信备份时返回HAL_ERROR停机——绝不带伤跳转。
		 * 三个条件用 || 短路串联，顺序是有讲究的：
		 *   向量检查最便宜（只读两个word）且能最先拦住"擦空/半写"这类明显损坏；
		 *   CRC计算要遍历整片A区（960KB，耗时几十~上百ms），放最后；
		 *   只有前两者都过才做CRC比对。*/
		if (!OTA_StorageIsAppVectorValid() ||
			OTA_StorageCalculateAppCRC(OTA_INFO.Firelen[0], &current_crc) != HAL_OK ||
			current_crc != OTA_INFO.Firelen[1])
		{
			U1_Printf("[回滚] 启动检测到A区向量/CRC不匹配，尝试恢复备份\r\n");
			/* HasValidBackup()与Rollback()的||组合：没有备份就直接短路停机；
			 * 有备份则尝试恢复，恢复失败一样停机。
			 * 注意这条路径和阶段C的区别：阶段C是"我们知道自己在升级时被打断"，
			 * 这里是"根本没在升级，A区却坏了"——说明是硬件级损坏，
			 * 此时如果有备份还能救，没有就只能停。*/
			if (!OTA_StorageHasValidBackup() || OTA_Storage_Rollback() != HAL_OK)
			{
				U1_Printf("[回滚] 无可信备份可恢复，停在Bootloader\r\n");
				return HAL_ERROR;
			}

			/* 同阶段C：恢复成功必须挂掉电类型通知，否则ESP32可能拿着旧会话
			 * 继续发数据，把刚恢复好的A区再覆盖一遍。*/
			if (OTA_Storage_SetRecoveryNotice(OTA_STORAGE_RECOVERY_NOTICE_POWER_LOSS) != HAL_OK)
			{
				U1_Printf("[回滚] 无法保存启动恢复通知，停在Bootloader\r\n");
				return HAL_ERROR;
			}
			U1_Printf("[回滚] 启动恢复完成，等待ESP32结束旧会话\r\n");
			return HAL_OK;
		}
	}

	/* 阶段H：所有检查通过——A区存在已验证可信基线，此后才允许跳转APP与发起覆盖式升级。
	 * 走到这里的路径有两条：① 阶段A~G全部通过（当前格式且CRC一致）；
	 * ② 阶段F旧格式迁移成功。两者都代表"A区内容已被实测确认"，
	 * 因此置位后 IsUpgradeAllowed() 才会放行擦除。*/
	s_baseline_ready = 1U;
	return HAL_OK;
}
