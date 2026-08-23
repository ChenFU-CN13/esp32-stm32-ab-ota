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

	status = MyFlash_EraseSectors(FLASH_ACode_STARTNUM, FLASH_ACode_NUM);

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

	/* ======== [测试用例3] 固件整体CRC失败注入 ======== */
	/* 读取起始地址偏移+1，让校验读到的数据与写入的不一致，整体CRC必然失败
	 * 用于验证: 数据传输全程正常ACK -> 0x07后CRC失败回NACK(0x05)
	 *           -> ESP32立即失败不重试单帧 -> ABORT -> 3次上限 -> 206
	 * 注意：用例2测完后，删掉用例2的注入，解开下面这行再测本用例
	 * 测完删除下面这行 */
	 //addr += 1;

	while (remaining > 0)
	{
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