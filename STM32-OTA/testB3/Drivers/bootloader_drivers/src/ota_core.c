/**
 * @file    ota_core.c
 * @brief   OTA 核心状态机实现
 * @details 归口：升级状态机（OTA_Init / OTA_ProcessFrame / OTA_WaitForHandshake / OTA_RunUpgradeProcess）、
 *          升级上下文与全局变量。
 *          ★未来回滚决策（收到 0x04/0x05 → 触发 ota_storage 回滚）也放本模块。
 *          （由原 TaskTest.c 拆分而来，函数体一字未改动）
 */

#include "ota_core.h"
#include "ota_proto.h"
#include "ota_storage.h"
#include "bootloader.h"
#include "AT24C02.h"
#include "command.h"


/*============================================================================*/
/*                          全局变量定义                                       */
/*============================================================================*/

OTA_INFOCB OTA_INFO;
OTA_Context_t g_ota_ctx;


/**
 * @brief OTA接收缓冲区
 *
 * 用于临时存储从串口环形缓冲区读取的数据
 * 大小设置为最大数据帧长度+4字节余量，防止溢出
 */
static uint8_t ota_rx_buffer[OTA_DATA_FRAME_MAX_LEN + 4];

/* A区固件完整性标志：整体CRC校验失败后置1，阻止Bootloader超时跳转到损坏的APP
 * 置1后Bootloader会一直停留在IDLE状态等待新的升级请求（或SWD恢复），避免变砖
 * 待W25Q128 A/B分区回滚机制实现后，此标志可演进为"回滚到上一可用版本"
 */
static uint8_t s_app_firmware_invalid = 0;


/*============================================================================*/
/*                          Bootloader核心函数                                 */
/*============================================================================*/

/**
 * @brief 初始化OTA上下文
 *
 * @details 初始化内容:
 *   - 状态机状态设为IDLE
 *   - 固件大小、CRC32、已接收大小清零
 *   - 期望包序号、总包数清零
 *   - 写入地址设为A区起始地址
 *   - 重试计数、升级标志清零
 *   - 版本号清零
 *   - 打印启动信息
 */
void OTA_Init(void)
{
	g_ota_ctx.state = OTA_STATE_IDLE;
	g_ota_ctx.firmware_size = 0;
	g_ota_ctx.firmware_crc32 = 0;
	g_ota_ctx.received_size = 0;
	g_ota_ctx.expected_packet = 0;
	g_ota_ctx.total_packets = 0;
	g_ota_ctx.write_addr = FLASH_ACode_STARTADDR;
	g_ota_ctx.retry_count = 0;
	g_ota_ctx.upgrade_flag = 0;
	memset(g_ota_ctx.version, 0, sizeof(g_ota_ctx.version));

	U1_Printf("\r\n========================================\r\n");
	U1_Printf("Bootloader启动...\r\n");
	U1_Printf("B区起始地址: 0x%08X\r\n", F407_FLASH_STARTADDR);
	U1_Printf("A区起始地址: 0x%08X\r\n", FLASH_ACode_STARTADDR);
	U1_Printf("========================================\r\n");
}




/**
 * @brief 处理接收到的协议帧
 * @param data 数据指针
 * @param len 数据长度
 *
 * @details 根据帧长度判断帧类型:
 *   - 3字节: 简单命令帧(握手、开始传输、结束等)
 *   - 25字节(OTA_INFO_FRAME_LEN): 固件信息帧
 *   - 其他: 固件数据帧
 *
 * @note 状态机处理:
 *   每种命令只在特定状态下处理，防止状态混乱
 */
void OTA_ProcessFrame(uint8_t *data, uint16_t len)
{
	uint8_t cmd;
	OTA_FirmwareInfo_t fw_info;		/*存储固件信息*/
	OTA_DataFrame_t data_frame;		/*存储固件数据帧*/
	HAL_StatusTypeDef status;
	uint8_t i;

	if (data == NULL || len == 0)
	{
		U1_Printf("[警告] OTA_ProcessFrame: 空数据\r\n");
		return;
	}

	U1_Printf("[调试] 收到数据长度: %d, 当前状态: %d\r\n", len, g_ota_ctx.state);

	if (len == OTA_SIMPLE_FRAME_LEN)
	{
		if (OTA_ParseSimpleFrame(data, len, &cmd))
		{
			switch (cmd)
			{
				case OTA_CMD_CONFIRM:
					if (g_ota_ctx.state == OTA_STATE_WAIT_CONFIRM)
					{
						U1_Printf("收到二次握手确认(0x03)\r\n");
						OTA_SendACK();
						g_ota_ctx.state = OTA_STATE_WAIT_START;
					}
					break;

				case OTA_CMD_START_TRANSFER:
					if (g_ota_ctx.state == OTA_STATE_WAIT_START)
					{
						U1_Printf("收到三次握手，开始传输(0x04)\r\n");
						U1_Printf("开始擦除A区Flash...\r\n");

						status = OTA_EraseAppRegion();
						if (status == HAL_OK)
						{
							OTA_SendACK();
							g_ota_ctx.state = OTA_STATE_WAIT_INFO;
							U1_Printf("等待固件信息帧...\r\n");
						}
						else
						{
							g_ota_ctx.state = OTA_STATE_UPGRADE_FAILED;
							U1_Printf("Flash擦除失败，升级终止!\r\n");
						}
					}
					break;

			case OTA_CMD_UPGRADE_END:
				if (g_ota_ctx.state == OTA_STATE_WAIT_DATA || g_ota_ctx.state == OTA_STATE_WAIT_END)
				{
					U1_Printf("收到升级结束命令(0x07)\r\n");
					U1_Printf("\r\n");
					U1_Printf("========================================\r\n");
					U1_Printf("       固件接收完成!\r\n");
					U1_Printf("========================================\r\n");
					U1_Printf("固件大小: %lu 字节\r\n", g_ota_ctx.received_size);
					U1_Printf("版本号: %s\r\n", g_ota_ctx.version);
					U1_Printf("========================================\r\n");

					/* 整体CRC端到端校验：从A区Flash读取整份固件流式计算CRC32，
					 * 与信息帧下发的固件整体CRC32比对。
					 * 逐帧CRC只保护传输链路，整体CRC保护落盘正确性（Flash写入/地址累加/末包截断等环节）。
					 */
					        /* 先做CRC校验，通过才发ACK，失败发NACK——不能先ACK后校验，
        				 * 否则ESP32收到ACK就认为成功退出了，NACK发得太晚没人听 */
					if (OTA_VerifyFirmwareCRC())
					{
						OTA_SendACK();       /* CRC通过才发ACK，ESP32收到后确认成功 */
						U1_Printf("========================================\r\n");
						U1_Printf("升级成功，即将跳转到A区APP...\r\n");
						U1_Printf("========================================\r\n");
						g_ota_ctx.state = OTA_STATE_UPGRADE_SUCCESS;
					}
					else
					{
						/* 整体CRC校验失败：A区固件已损坏，绝不可跳转，否则会跑飞/变砖。
						 * 置损坏标志，停在Bootloader等待重新升级（或SWD恢复）。
						 * 这是W25Q128 A/B分区回滚机制上线前的安全策略。
						 */
						s_app_firmware_invalid = 1;
						OTA_SendNACK(OTA_NACK_FW_CRC_FAIL);	/* 通知ESP32,当前STM32固件校验失败，不要认为传输成功 */
						g_ota_ctx.state = OTA_STATE_UPGRADE_FAILED;
						U1_Printf("========================================\r\n");
						U1_Printf("[严重] 固件整体CRC校验失败！A区固件不可用。\r\n");
						U1_Printf("[安全] 停在Bootloader不跳转，等待重新升级/SWD恢复...\r\n");
						U1_Printf("========================================\r\n");
					}
				}
				break;

				case OTA_CMD_ABORT:
					/* ABORT不检查状态：任何状态收到都立即执行
					* 危险性判断：只要已经到WAIT_INFO(含)之后，说明A区已被擦除/写入过，
					* 原APP已不存在，此时若回IDLE后超时跳转会跑飞！必须置损坏标志阻止跳转 */
					if (g_ota_ctx.state >= OTA_STATE_WAIT_INFO)
					{
						s_app_firmware_invalid = 1;
						U1_Printf("[安全] 传输中途放弃，A区已不完整，禁止跳转\r\n");
					}
					U1_Printf("收到ABORT(0x08)，重置回IDLE\r\n");
					OTA_Init();
					OTA_SendACK();
					/*注意目前还在测试，目前依然停留在bootloader程序，等回滚程序实现后才完整*/
					break;



				default:
					U1_Printf("未知命令: 0x%02X\r\n", cmd);
					OTA_SendNACK(OTA_NACK_UNKNOWN_CMD);
					break;
			}
		}
	}
	else if (len == OTA_INFO_FRAME_LEN)
	{
		U1_Printf("[调试] 识别为固件信息帧(%d字节)\r\n", OTA_INFO_FRAME_LEN);
		U1_Printf("  帧数据: ");
		for (i = 0; i < len && i < 32; i++)
		{
			U1_Printf("%02X ", data[i]);
		}
		U1_Printf("\r\n");

		if (g_ota_ctx.state == OTA_STATE_WAIT_INFO)
		{
			if (OTA_ParseFirmwareInfo(data, len, &fw_info))
			{
				U1_Printf("收到固件信息帧:\r\n");
				U1_Printf("  固件大小: %lu 字节\r\n", fw_info.firmware_size);
				U1_Printf("  固件CRC32: 0x%08X\r\n", fw_info.firmware_crc32);
				U1_Printf("  版本号: %s\r\n", fw_info.version);

				g_ota_ctx.firmware_size = fw_info.firmware_size;
				g_ota_ctx.firmware_crc32 = fw_info.firmware_crc32;
				memcpy(g_ota_ctx.version, fw_info.version, OTA_VERSION_LEN);

				g_ota_ctx.total_packets = (g_ota_ctx.firmware_size + OTA_DATA_PAYLOAD_LEN - 1) / OTA_DATA_PAYLOAD_LEN;
				g_ota_ctx.expected_packet = 0;
				g_ota_ctx.received_size = 0;
				g_ota_ctx.write_addr = FLASH_ACode_STARTADDR;

				OTA_INFO.Firelen[0] = g_ota_ctx.firmware_size;
				memcpy(OTA_INFO.OTA_VERSION, fw_info.version, OTA_VERSION_LEN);
				EP24C_WriteLongData(0, (uint8_t*)&OTA_INFO, OTA_INFOCB_SIZE);

				OTA_SendACK();
				g_ota_ctx.state = OTA_STATE_WAIT_DATA;
				U1_Printf("等待固件数据帧... (共%d包)\r\n", g_ota_ctx.total_packets);
			}
			else
			{
				U1_Printf("[错误] 固件信息帧解析失败!\r\n");
				OTA_SendNACK(OTA_NACK_FRAME_CRC);	/*通知ESP32,当前STM32解析数据出错*/
			}
		}
		else
		{
			U1_Printf("[警告] 当前状态不是WAIT_INFO, 忽略固件信息帧\r\n");
			OTA_SendNACK(OTA_NACK_STATE_MISMATCH);
		}
	}
	else if (len >= OTA_DATA_FRAME_HEADER_LEN + OTA_DATA_FRAME_TAIL_LEN)/* 数据帧长度检查 */
	{
		if (g_ota_ctx.state == OTA_STATE_WAIT_DATA)
		{
			if (OTA_ParseDataFrame(data, len, &data_frame))
			{
				if (data_frame.packet_num == g_ota_ctx.expected_packet)
				{
					if (g_ota_ctx.received_size + data_frame.data_len > g_ota_ctx.firmware_size)
					{
						uint32_t original_len = data_frame.data_len;
						data_frame.data_len = g_ota_ctx.firmware_size - g_ota_ctx.received_size;
						U1_Printf("[调试] 最后一包截断: %lu -> %lu 字节\r\n",
							original_len, data_frame.data_len);
					}

					status = OTA_WriteFirmwareData(g_ota_ctx.write_addr,
					                               data_frame.data,
					                               data_frame.data_len);

					if (status == HAL_OK)
					{
						g_ota_ctx.write_addr += data_frame.data_len;
						g_ota_ctx.received_size += data_frame.data_len;
						g_ota_ctx.expected_packet++;

						if (g_ota_ctx.expected_packet % 10 == 0 ||
						    g_ota_ctx.expected_packet >= g_ota_ctx.total_packets)
						{
							U1_Printf("接收进度: %d/%d 包 (%lu/%lu 字节, %d%%)\r\n",
								g_ota_ctx.expected_packet, g_ota_ctx.total_packets,
								g_ota_ctx.received_size, g_ota_ctx.firmware_size,
								(g_ota_ctx.received_size * 100) / g_ota_ctx.firmware_size);
						}

						OTA_SendACK();

						if (g_ota_ctx.received_size >= g_ota_ctx.firmware_size)
						{
							g_ota_ctx.state = OTA_STATE_WAIT_END;
							U1_Printf("所有数据接收完成，等待结束命令...\r\n");
						}
					}
					else
					{
						U1_Printf("[错误] Flash写入失败! 包序号: %d\r\n", data_frame.packet_num);
						OTA_SendNACK(OTA_NACK_FLASH_ERROR);
					}
				}
				else
				{
						//U1_Printf("[警告] 包序号不匹配: 期望%d, 收到%d\r\n",
						//g_ota_ctx.expected_packet, data_frame.packet_num);
						if (data_frame.packet_num + 1 == g_ota_ctx.expected_packet)
						{
								/*进入这个条件说明重复包：上一包数据已正确写入，只是ACK丢了，补ACK让ESP32继续*/
								U1_Printf("[调试]重复包%d(对应的ACK丢失)，补发ACK\r\n", data_frame.packet_num);
								OTA_SendACK();
						}
						else
						{
								U1_Printf("[警告] 包序号不匹配: 期望%d, 收到%d\r\n",
								g_ota_ctx.expected_packet, data_frame.packet_num);
								OTA_SendNACK(OTA_NACK_PACKET_SEQ);
						}

				}
			}
			else
			{
				U1_Printf("[错误] 数据帧解析失败!\r\n");
				OTA_SendNACK(OTA_NACK_FRAME_CRC);

			}
		}
		else
		{
			U1_Printf("[警告] 当前状态不是WAIT_DATA, 忽略数据帧 (长度%d)\r\n", len);
			OTA_SendNACK(OTA_NACK_STATE_MISMATCH);

		}
	}
	else
	{
		U1_Printf("[警告] 无法识别的帧长度: %d\r\n", len);
		U1_Printf("  帧数据: ");
		for (i = 0; i < len && i < 20; i++)
		{
			U1_Printf("%02X ", data[i]);
		}
		U1_Printf("\r\n");
		OTA_SendNACK(OTA_NACK_BAD_LENGTH);

	}
}




/**
 * @brief 等待ESP32二次握手
 * @param timeout_ms 超时时间(毫秒)
 * @return 1-收到握手，0-超时
 *
 * @details 等待流程:
 *   1. 记录开始时间
 *   2. 循环检查串口缓冲区
 *   3. 如果收到3字节数据，尝试解析
 *   4. 如果是二次握手命令(0x03)，返回成功
 *   5. 超时则返回失败
 *
 * @note 二次握手的作用:
 *   - A区收到升级通知后重启进入B区
 *   - B区启动后等待ESP32发送二次握手
 *   - 如果超时未收到，说明不需要升级，直接跳转A区
 *   - 这样可以防止意外重启后卡在Bootloader
 */
uint8_t OTA_WaitForHandshake(uint32_t timeout_ms)
{
	uint32_t start_tick = HAL_GetTick();
	uint16_t rx_len;
	uint8_t cmd;

	U1_Printf("等待ESP32二次握手... (超时: %lu ms)\r\n", timeout_ms);

	while ((HAL_GetTick() - start_tick) < timeout_ms)
	{
		rx_len = UART_circular_buffer_GetRxLen();

		if (rx_len >= OTA_SIMPLE_FRAME_LEN)
		{
			UART_circular_buffer_ReadData(ota_rx_buffer, OTA_SIMPLE_FRAME_LEN);

			if (OTA_ParseSimpleFrame(ota_rx_buffer, OTA_SIMPLE_FRAME_LEN, &cmd))
			{
				if (cmd == OTA_CMD_CONFIRM)
				{
					U1_Printf("收到二次握手确认!\r\n");
					return 1;
				}
				else if (cmd == OTA_CMD_ABORT)
				{
					/* IDLE本来就是初始状态，回ACK让ESP32放心 */
					OTA_SendACK();
				}

			}
		}

		HAL_Delay(10);
	}

	U1_Printf("等待二次握手超时!\r\n");
	return 0;
}




/**
 * @brief 运行升级主流程
 *
 * @details 状态机主循环:
 *   1. IDLE状态: 等待二次握手，超时则跳转APP
 *   2. WAIT_START状态: 等待三次握手(开始传输命令)
 *   3. WAIT_INFO状态: 等待固件信息帧
 *   4. WAIT_DATA状态: 循环接收数据帧
 *   5. WAIT_END状态: 等待升级结束命令
 *   6. UPGRADE_SUCCESS: 整体CRC校验通过，跳转APP
 *      UPGRADE_FAILED: 整体CRC校验失败，停在Bootloader回到IDLE等待重试（A区已损坏不可跳转）
 *   7. JUMP_TO_APP: 直接跳转APP
 *
 * @note 这是阻塞函数，在Task_While()中调用
 *       只有升级完成或跳转APP后才返回
 */
void OTA_RunUpgradeProcess(void)
{
	uint16_t rx_len;

	while (1)
	{
		switch (g_ota_ctx.state)
		{
			case OTA_STATE_IDLE:
				if (OTA_WaitForHandshake(OTA_HANDSHAKE_TIMEOUT_MS))
				{
					OTA_SendACK();
					g_ota_ctx.state = OTA_STATE_WAIT_START;
					U1_Printf("等待三次握手...\r\n");
				}
				else
				{
					/* 超时未收到升级请求 */
					if (s_app_firmware_invalid)
					{
						/* A区固件已损坏（整体CRC校验失败过），绝不可跳转，
						 * 留在IDLE持续监听新的升级请求，避免变砖。
						 * 待W25Q128 A/B回滚机制上线后，此处可改为回滚到上一可用版本。 */
						U1_Printf("[安全] A区固件损坏，停在Bootloader等待恢复...\r\n");
						HAL_Delay(2000);
						/* 保持IDLE状态，主循环再次等待握手 */
					}
					else
					{
						g_ota_ctx.state = OTA_STATE_JUMP_TO_APP;
					}
				}
				break;

			case OTA_STATE_WAIT_START:
			case OTA_STATE_WAIT_INFO:
			case OTA_STATE_WAIT_DATA:
			case OTA_STATE_WAIT_END:
				rx_len = UART_circular_buffer_GetRxLen();
				if (rx_len > 0)
				{
					if (rx_len > sizeof(ota_rx_buffer))
					{
						rx_len = sizeof(ota_rx_buffer);
					}
					UART_circular_buffer_ReadData(ota_rx_buffer, rx_len);
					OTA_ProcessFrame(ota_rx_buffer, rx_len);
				}
				break;

			case OTA_STATE_UPGRADE_SUCCESS:

				HAL_Delay(500);
				Load_A(FLASH_ACode_STARTADDR);
				return;

			case OTA_STATE_UPGRADE_FAILED:
				/* 升级失败（整体CRC校验失败等）：A区固件不可用，绝不跳转，否则跑飞变砖。
				 * 重新初始化上下文回到IDLE，持续监听新的升级请求（或SWD恢复）。
				 * 注：此分支不 return，让主循环继续转，回到IDLE等待。
				 */
				U1_Printf("[安全] 重新进入升级监听模式，等待新的固件...\r\n");
				OTA_Init();   /* 重置上下文 state→IDLE，准备接收下一次升级 */
				HAL_Delay(500);
				break;

			case OTA_STATE_JUMP_TO_APP:
				U1_Printf("无升级请求，跳转到A区APP...\r\n");
				Load_A(FLASH_ACode_STARTADDR);
				return;

			default:
				break;
		}

		HAL_Delay(1);
	}
}

