/**
 * @file    ota_proto.c
 * @brief   协议帧实现（纯函数、无状态）
 * @details 归口：CRC32 计算、协议帧发送（SimpleFrame/ACK/NACK）、
 *          协议帧解析（SimpleFrame/固件信息帧/数据帧）。
 *          （由原 TaskTest.c 拆分而来，函数体一字未改动）
 */

#include "ota_proto.h"


/*============================================================================*/
/*                          CRC32计算函数                                      */
/*============================================================================*/

/**
 * @brief 计算数据的CRC32校验值
 * @param data 待计算的数据指针
 * @param len 数据长度(字节)
 * @return CRC32校验值
 *
 * @details CRC32算法说明:
 *   - 多项式: 0xEDB88320 (反向表示)
 *   - 初始值: 0xFFFFFFFF
 *   - 最终异或: 0xFFFFFFFF
 *   - 输入/输出反转: 是
 *
 * @note 这里使用软件计算方式，展开循环以提高速度
 *       每个字节处理8次位移和条件异或
 *       与标准CRC32(如zlib)兼容
 */
uint32_t OTA_CalcCRC32(uint8_t *data, uint32_t len)
{
	uint32_t crc = 0xFFFFFFFF;

	if (data == NULL || len == 0)
	{
		return 0;
	}

	for (uint32_t i = 0; i < len; i++)
	{
		crc = crc ^ data[i];
		crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
		crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
		crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
		crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
		crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
		crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
		crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
		crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
	}

	return crc ^ 0xFFFFFFFF;
}




/*============================================================================*/
/*                          协议帧发送函数                                     */
/*============================================================================*/

/**
 * @brief 发送简单命令帧
 * @param cmd 命令字
 *
 * @details 帧格式: | 包头(0x66) | 命令字 | 包尾(0xBB) |
 *          总长度: 3字节
 *
 * @note 使用阻塞方式发送，超时100ms
 */
void OTA_SendSimpleFrame(uint8_t cmd)
{
	uint8_t frame[OTA_SIMPLE_FRAME_LEN];

	frame[0] = OTA_PACKET_HEAD;
	frame[1] = cmd;
	frame[2] = OTA_PACKET_TAIL;

	HAL_UART_Transmit(&huart2, frame, OTA_SIMPLE_FRAME_LEN, 100);
}




/**
 * @brief 发送ACK应答帧
 *
 * @details 发送格式: 0x66 0x02 0xBB
 *          用于告知ESP32命令已成功接收
 */
void OTA_SendACK(void)
{
	OTA_SendSimpleFrame(OTA_CMD_ACK);
}


/**
 * @brief 发送NACK应答帧
 *
 * @details 格式: | 0x66 | 0x0A | err_code | 0xBB |
 *          用于告知ESP32命令接收失败，包含错误码err_code
 */
void OTA_SendNACK(uint8_t err_code)
{
	uint8_t frame[OTA_NACK_FRAME_LEN];

	frame[0] = OTA_PACKET_HEAD;
	frame[1] = OTA_CMD_NACK;
	frame[2] = err_code;
	frame[3] = OTA_PACKET_TAIL;

	HAL_UART_Transmit(&huart2,frame,OTA_NACK_FRAME_LEN,100);
}


/*============================================================================*/
/*                          协议帧解析函数                                     */
/*============================================================================*/

/**
 * @brief 解析简单命令帧
 * @param data 接收到的数据
 * @param len 数据长度
 * @param cmd 输出命令字
 * @return 1-解析成功，0-解析失败
 *
 * @details 验证步骤:
 *   1. 检查指针非空
 *   2. 检查长度为3字节
 *   3. 检查包头为0x66
 *   4. 检查包尾为0xBB
 *   5. 提取命令字
 */
uint8_t OTA_ParseSimpleFrame(uint8_t *data, uint16_t len, uint8_t *cmd)
{
	if (data == NULL || cmd == NULL)
	{
		return 0;
	}

	if (len != OTA_SIMPLE_FRAME_LEN)
	{
		return 0;
	}

	if (data[0] != OTA_PACKET_HEAD || data[2] != OTA_PACKET_TAIL)
	{
		return 0;
	}

	*cmd = data[1];
	return 1;
}




/**
 * @brief 解析固件信息帧
 * @param data 接收到的数据
 * @param len 数据长度
 * @param info 输出固件信息结构体
 * @return 1-解析成功，0-解析失败
 *
 * @details 帧格式(25字节):
 *   | 字节偏移 | 内容          | 说明                            |
 *   |----------|---------------|--------------------------------|
 *   | 0        | 0x66          | 包头                            |
 *   | 1        | 0x05          | 命令字(固件信息)                |
 *   | 2-5      | firmware_size | 固件大小(大端序)                |
 *   | 6-7      | 0x2F 0x2F     | 分隔符                          |
 *   | 8-15     | version       | 版本号(8字节ASCII)              |
 *   | 16-19    | firmware_crc32| 固件整体CRC32(大端序,端到端校验)|
 *   | 20-23    | frame_crc32   | 帧CRC32(大端序,帧完整性校验)    |
 *   | 24       | 0xBB          | 包尾                            |
 *
 * @note 大端序解析: 高字节在前，低字节在后
 *       例如: 固件大小0x00012345，存储为：00 01 23 45
 *       版本号为ASCII字符串，如"v1.0.2"，不足8字节用0x00填充
 *       firmware_crc32 = 整个固件文件的CRC32，收完数据后用它做端到端校验；
 *       frame_crc32 = 对 data[1]~data[19]（命令+大小+分隔符+版本号+固件CRC32，共19字节）计算的CRC。
 */
uint8_t OTA_ParseFirmwareInfo(uint8_t *data, uint16_t len, OTA_FirmwareInfo_t *info)
{
	uint8_t i;
	uint32_t recv_frame_crc;
	uint32_t calc_frame_crc;

	if (data == NULL || info == NULL)
	{
		U1_Printf("[错误] 解析固件信息帧: 参数为空\r\n");
		return 0;
	}



	if (len != OTA_INFO_FRAME_LEN)
	{
		U1_Printf("[错误] 解析固件信息帧: 长度错误, 期望%d, 实际%d\r\n", OTA_INFO_FRAME_LEN, len);
		U1_Printf("  接收数据: ");
		for (i = 0; i < len && i < 32; i++)
		{
			U1_Printf("%02X ", data[i]);
		}
		U1_Printf("\r\n");
		return 0;
	}

	if (data[0] != OTA_PACKET_HEAD)
	{
		U1_Printf("[错误] 解析固件信息帧: 包头错误, 期望0x%02X, 实际0x%02X\r\n", OTA_PACKET_HEAD, data[0]);
		return 0;
	}

	if (data[OTA_INFO_FRAME_LEN - 1] != OTA_PACKET_TAIL)
	{
		U1_Printf("[错误] 解析固件信息帧: 包尾错误, 期望0x%02X, 实际0x%02X\r\n", OTA_PACKET_TAIL, data[OTA_INFO_FRAME_LEN - 1]);
		return 0;
	}

	if (data[1] != OTA_CMD_FIRMWARE_INFO)
	{
		U1_Printf("[错误] 解析固件信息帧: 命令字错误, 期望0x%02X, 实际0x%02X\r\n", OTA_CMD_FIRMWARE_INFO, data[1]);
		return 0;
	}

	if (data[6] != 0x2F || data[7] != 0x2F)
	{
		U1_Printf("[错误] 解析固件信息帧: 分隔符错误, 期望0x2F 0x2F, 实际0x%02X 0x%02X\r\n", data[6], data[7]);
		U1_Printf("  完整帧数据: ");
		for (i = 0; i < OTA_INFO_FRAME_LEN; i++)
		{
			U1_Printf("%02X ", data[i]);
		}
		U1_Printf("\r\n");
		return 0;
	}

	/* 固件大小：data[2-5]，大端序 */
	info->firmware_size = ((uint32_t)data[2] << 24) |
	                      ((uint32_t)data[3] << 16) |
	                      ((uint32_t)data[4] << 8) |
	                      (uint32_t)data[5];

	/* 版本号：data[8-15]，8字节ASCII */
	memcpy(info->version, &data[8], OTA_VERSION_LEN);

	/* 固件整体CRC32：data[16-19]，大端序，收完数据后用它做端到端校验 */
	info->firmware_crc32 = ((uint32_t)data[16] << 24) |
	                       ((uint32_t)data[17] << 16) |
	                       ((uint32_t)data[18] << 8) |
	                       (uint32_t)data[19];

	/* 帧CRC32：data[20-23]，大端序 */
	recv_frame_crc = ((uint32_t)data[20] << 24) |
	                 ((uint32_t)data[21] << 16) |
	                 ((uint32_t)data[22] << 8) |
	                 (uint32_t)data[23];

	/* 帧CRC自校验：对 data[1]~data[19] 共19字节计算CRC，与收到的帧CRC比对
	 * 校验范围 = 命令字 + 固件大小 + 分隔符 + 版本号 + 固件整体CRC32
	 * 不含包头(0x66)、帧CRC自身、包尾(0xBB)
	 */
	calc_frame_crc = OTA_CalcCRC32(&data[1], 19);
	if (calc_frame_crc != recv_frame_crc)
	{
		U1_Printf("[错误] 固件信息帧CRC校验失败: 计算值=0x%08X, 接收值=0x%08X\r\n",
			(unsigned int)calc_frame_crc, (unsigned int)recv_frame_crc);
		U1_Printf("  完整帧数据: ");
		for (i = 0; i < OTA_INFO_FRAME_LEN; i++)
		{
			U1_Printf("%02X ", data[i]);
		}
		U1_Printf("\r\n");
		return 0;
	}

	U1_Printf("[成功] 固件信息帧解析成功(含帧CRC校验)\r\n");
	return 1;
}




/**
 * @brief 解析固件数据帧
 * @param data 接收到的数据
 * @param len 数据长度
 * @param frame 输出数据帧结构体
 * @return 1-解析成功，0-解析失败
 *
 * @details 帧格式(变长):
 *   | 字节偏移     | 内容        | 说明                    |
 *   |--------------|-------------|------------------------|
 *   | 0            | 0x66        | 包头                   |
 *   | 1            | 0x06        | 命令字(固件数据)        |
 *   | 2-3          | packet_num  | 包序号(大端序)          |
 *   | 4-7          | data_len    | 数据长度(大端序)        |
 *   | 8~8+len-1    | data        | 固件数据(≤1024字节)     |
 *   | 8+len~11+len | data_crc32  | 数据CRC32(大端序)       |
 *   | 12+len       | 0xBB        | 包尾                   |
 *
 * @note 会进行CRC32校验，确保数据传输正确
 *       校验失败会打印错误信息并返回0
 */
uint8_t OTA_ParseDataFrame(uint8_t *data, uint16_t len, OTA_DataFrame_t *frame)
{
	uint32_t received_crc;
	uint32_t calculated_crc;
	uint8_t i;

	if (data == NULL || frame == NULL)
	{
		U1_Printf("[错误] 解析数据帧: 参数为空\r\n");
		return 0;
	}

	if (len < OTA_DATA_FRAME_HEADER_LEN + OTA_DATA_FRAME_TAIL_LEN)
	{
		U1_Printf("[错误] 解析数据帧: 长度不足, 最小%d, 实际%d\r\n",
			OTA_DATA_FRAME_HEADER_LEN + OTA_DATA_FRAME_TAIL_LEN, len);
		return 0;
	}

	if (data[0] != OTA_PACKET_HEAD)
	{
		U1_Printf("[错误] 解析数据帧: 包头错误, 期望0x%02X, 实际0x%02X\r\n", OTA_PACKET_HEAD, data[0]);
		return 0;
	}

	if (data[1] != OTA_CMD_FIRMWARE_DATA)
	{
		U1_Printf("[错误] 解析数据帧: 命令字错误, 期望0x%02X, 实际0x%02X\r\n", OTA_CMD_FIRMWARE_DATA, data[1]);
		return 0;
	}

	frame->packet_num = ((uint16_t)data[2] << 8) | (uint16_t)data[3];

	frame->data_len = ((uint32_t)data[4] << 24) |
	                  ((uint32_t)data[5] << 16) |
	                  ((uint32_t)data[6] << 8) |
	                  (uint32_t)data[7];

	if (frame->data_len > OTA_DATA_PAYLOAD_LEN)
	{
		U1_Printf("[错误] 解析数据帧: 数据长度超限, 最大%d, 实际%lu\r\n", OTA_DATA_PAYLOAD_LEN, frame->data_len);
		return 0;
	}

	if (len < OTA_DATA_FRAME_HEADER_LEN + frame->data_len + OTA_DATA_FRAME_TAIL_LEN)
	{
		U1_Printf("[错误] 解析数据帧: 帧长度不匹配, 期望%d, 实际%d\r\n",
			OTA_DATA_FRAME_HEADER_LEN + frame->data_len + OTA_DATA_FRAME_TAIL_LEN, len);
		return 0;
	}

	memcpy(frame->data, &data[8], frame->data_len);

	received_crc = ((uint32_t)data[8 + frame->data_len] << 24) |
	               ((uint32_t)data[9 + frame->data_len] << 16) |
	               ((uint32_t)data[10 + frame->data_len] << 8) |
	               (uint32_t)data[11 + frame->data_len];

	if (data[12 + frame->data_len] != OTA_PACKET_TAIL)
	{
		U1_Printf("[错误] 解析数据帧: 包尾错误, 期望0x%02X, 实际0x%02X\r\n",
			OTA_PACKET_TAIL, data[12 + frame->data_len]);
		return 0;
	}

	calculated_crc = OTA_CalcCRC32(frame->data, frame->data_len);
	if (calculated_crc != received_crc)
	{
		U1_Printf("[错误] 数据帧CRC校验失败: 计算值=0x%08X, 接收值=0x%08X\r\n", calculated_crc, received_crc);
		U1_Printf("  包序号: %d, 数据长度: %lu\r\n", frame->packet_num, frame->data_len);
		return 0;
	}

	frame->data_crc32 = received_crc;
	return 1;
}