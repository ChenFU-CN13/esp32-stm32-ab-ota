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

/* W2临时故障注入：1=擦除A区后停止喂狗，验证IWDG复位与启动回滚；
 * 测试通过后必须改回0，恢复正常OTA。 */
#define OTA_IWDG_RESET_TEST_ENABLED  0U

/* 启动恢复后给ESP32更长的旧会话收尾窗口；正常启动仍沿用5秒握手超时。 */
#define OTA_RECOVERY_NOTICE_WAIT_MS  10000U


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

/* 回滚延迟只用于等待ESP32在收到NACK后发送ABORT；不在中断上下文执行恢复。 */
static uint32_t s_rollback_deadline = 0U;
static uint8_t s_storage_initialized = 0U;
static uint32_t s_stage_deadline = 0U;
static OTA_StorageRecoveryNotice_t s_pending_recovery_notice = OTA_STORAGE_RECOVERY_NOTICE_NONE;


static void OTA_ScheduleRollback(OTA_StorageReason_t reason, uint32_t delay_ms)
{
	if (OTA_Storage_MarkRollbackPending(reason) != HAL_OK)
	{
		/* 擦除前已写UPGRADING；即使本次原因写入失败，掉电后仍会走保守恢复。 */
		U1_Printf("[回滚] 写入待回滚状态失败，继续在本次启动中恢复\r\n");
	}

	s_rollback_deadline = HAL_GetTick() + delay_ms;
	s_stage_deadline = 0U;
	g_ota_ctx.state = OTA_STATE_ROLLBACK_PENDING;
}


static void OTA_SetStageDeadline(OTA_State_t state)
{
	uint32_t timeout_ms = 0U;

	switch (state)
	{
		case OTA_STATE_WAIT_START:
			timeout_ms = OTA_WAIT_START_TIMEOUT_MS;
			break;
		case OTA_STATE_WAIT_INFO:
			timeout_ms = OTA_WAIT_INFO_TIMEOUT_MS;
			break;
		case OTA_STATE_WAIT_DATA:
			timeout_ms = OTA_WAIT_DATA_TIMEOUT_MS;
			break;
		case OTA_STATE_WAIT_END:
			timeout_ms = OTA_WAIT_END_TIMEOUT_MS;
			break;
		default:
			s_stage_deadline = 0U;
			return;
	}

	s_stage_deadline = HAL_GetTick() + timeout_ms;
}


static uint8_t OTA_IsStageTimeout(void)
{
	return (s_stage_deadline != 0U &&
		(int32_t)(HAL_GetTick() - s_stage_deadline) >= 0) ? 1U : 0U;
}


static uint8_t OTA_HandleRecoveryNotice(uint8_t cmd)
{
	if (OTA_Storage_GetRecoveryNotice() == OTA_STORAGE_RECOVERY_NOTICE_NONE)
	{
		return 0U;
	}

	if (cmd == OTA_CMD_ABORT)
	{
		if (OTA_Storage_AcknowledgeRecoveryNotice() == HAL_OK)
		{
			/* 先确认通知已可靠清除，再给ESP32 ACK，避免假确认后仍持续NACK。 */
			OTA_SendACK();
			U1_Printf("[回滚] 已确认ABORT，上一轮会话结束\r\n");
		}
		else
		{
			U1_Printf("[回滚] 清除恢复通知失败，继续阻止旧会话\r\n");
			OTA_SendNACK(OTA_NACK_FLASH_ERROR);
		}
	}
	else
	{
		U1_Printf("[回滚] 上一轮已恢复旧镜像，拒绝继续旧会话\r\n");
		OTA_SendNACK(OTA_NACK_PREVIOUS_ROLLBACK);
	}

	return 1U;
}


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
	s_rollback_deadline = 0U;
	s_stage_deadline = 0U;
	s_pending_recovery_notice = OTA_STORAGE_RECOVERY_NOTICE_NONE;

	U1_Printf("\r\n========================================\r\n");
	U1_Printf("Bootloader启动...\r\n");
	U1_Printf("B区起始地址: 0x%08X\r\n", F407_FLASH_STARTADDR);
	U1_Printf("A区起始地址: 0x%08X\r\n", FLASH_ACode_STARTADDR);
	U1_Printf("========================================\r\n");

	/* 仅在本次上电的首次初始化执行恢复；确保上电后只执行一次 Flash/回滚存储的状态恢复，防止协议层面复位上下文时重复触发。 */
	if (!s_storage_initialized)
	{
		s_storage_initialized = 1U;
		if (OTA_Storage_InitAndRecover() != HAL_OK)
		{
			g_ota_ctx.state = OTA_STATE_RECOVERY_FAILED;
		}
	}
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

	/* 恢复通知对整个旧会话生效，不能只覆盖3字节命令；
	 * 否则旧的信息帧/数据帧会落入普通状态错误分支，ESP32无法明确收尾。 */
	if (OTA_Storage_GetRecoveryNotice() != OTA_STORAGE_RECOVERY_NOTICE_NONE &&
		len >= 2U && data[0] == OTA_PACKET_HEAD)
	{
		if (len == OTA_SIMPLE_FRAME_LEN && OTA_ParseSimpleFrame(data, len, &cmd) &&
			cmd == OTA_CMD_ABORT)
		{
			(void)OTA_HandleRecoveryNotice(cmd);
		}
		else
		{
			U1_Printf("[回滚] 收到旧会话帧0x%02X，返回NACK 0x08\r\n", data[1]);
			OTA_SendNACK(OTA_NACK_PREVIOUS_ROLLBACK);
		}
		return;
	}

	if (len == OTA_SIMPLE_FRAME_LEN)
	{
		if (OTA_ParseSimpleFrame(data, len, &cmd))
		{
			if (OTA_HandleRecoveryNotice(cmd))
			{
				return;
			}

			switch (cmd)
			{
				case OTA_CMD_CONFIRM:
					if (g_ota_ctx.state == OTA_STATE_WAIT_CONFIRM)
					{
						U1_Printf("收到二次握手确认(0x03)\r\n");
						OTA_SendACK();
						g_ota_ctx.state = OTA_STATE_WAIT_START;
						OTA_SetStageDeadline(OTA_STATE_WAIT_START);
					}
					break;

				case OTA_CMD_START_TRANSFER:
					if (g_ota_ctx.state == OTA_STATE_WAIT_START)
					{
						U1_Printf("收到三次握手，先备份A区再开始传输(0x04)\r\n");
						status = OTA_Storage_CreateBackup();
						if (status != HAL_OK)
						{
							U1_Printf("[回滚] 备份基线失败，拒绝擦除A区\r\n");
							OTA_SendNACK(OTA_NACK_FLASH_ERROR);
							OTA_Init();
							break;
						}

						status = OTA_Storage_MarkUpgradeInProgress();
						if (status != HAL_OK)
						{
							U1_Printf("[回滚] 无法登记UPGRADING状态，拒绝擦除A区\r\n");
							OTA_SendNACK(OTA_NACK_FLASH_ERROR);
							OTA_Init();
							break;
						}

						U1_Printf("开始擦除A区Flash...\r\n");
						status = OTA_EraseAppRegion();
						if (status == HAL_OK)
						{
#if OTA_IWDG_RESET_TEST_ENABLED
							U1_Printf("[IWDG测试] A区已擦除，停止喂狗等待复位\r\n");
							/* 故意不调用Bootloader_WatchdogRefresh()。
							 * 复位后EEPROM仍为UPGRADING，启动流程应从W25Q128恢复旧镜像。 */
							while (1)
							{
							}
#else
							OTA_SendACK();
							g_ota_ctx.state = OTA_STATE_WAIT_INFO;
							OTA_SetStageDeadline(OTA_STATE_WAIT_INFO);
							U1_Printf("等待固件信息帧...\r\n");
#endif
						}
						else
						{
							OTA_SendNACK(OTA_NACK_FLASH_ERROR);
							OTA_ScheduleRollback(OTA_STORAGE_REASON_FLASH_ERROR, 2000U);
							U1_Printf("Flash擦除失败，升级终止!\r\n");
						}
					}
					break;

			case OTA_CMD_UPGRADE_END:
				U1_Printf("收到上位机数据帧：66 07 BB，升级结束通知\r\n");
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
						if (OTA_Storage_CommitActiveImage(g_ota_ctx.firmware_size,
														  g_ota_ctx.firmware_crc32,
														  g_ota_ctx.version) == HAL_OK)
						{
							OTA_SendACK();       /* CRC和元数据提交都通过才确认成功 */
							U1_Printf("========================================\r\n");
							U1_Printf("升级成功，即将跳转到A区APP...\r\n");
							U1_Printf("========================================\r\n");
							g_ota_ctx.state = OTA_STATE_UPGRADE_SUCCESS;
						}
						else
						{
							U1_Printf("[回滚] 新固件元数据提交失败\r\n");
							OTA_SendNACK(OTA_NACK_FLASH_ERROR);
							OTA_ScheduleRollback(OTA_STORAGE_REASON_FLASH_ERROR, 2000U);
						}
					}
					else
					{
						OTA_SendNACK(OTA_NACK_FW_CRC_FAIL);	/* 通知ESP32,当前STM32固件校验失败，不要认为传输成功 */
						OTA_ScheduleRollback(OTA_STORAGE_REASON_FW_CRC_ERROR, 2000U);
						U1_Printf("========================================\r\n");
						U1_Printf("[严重] 固件校验或提交失败，等待ABORT后回滚旧版本。\r\n");
						U1_Printf("========================================\r\n");
					}
				}
				break;

				case OTA_CMD_ABORT:
				U1_Printf("收到上位机数据帧：66 08 BB，停止升级通知\r\n");
					/* 已登记UPGRADING后，ABORT必须先回ACK，再恢复旧镜像。 */
					if (g_ota_ctx.state == OTA_STATE_WAIT_INFO ||
						g_ota_ctx.state == OTA_STATE_WAIT_DATA ||
						g_ota_ctx.state == OTA_STATE_WAIT_END ||
						g_ota_ctx.state == OTA_STATE_ROLLBACK_PENDING)
					{
						OTA_SendACK();
						s_pending_recovery_notice = OTA_STORAGE_RECOVERY_NOTICE_NONE;
						OTA_ScheduleRollback(OTA_STORAGE_REASON_ABORT, 0U);
						U1_Printf("收到ABORT(0x08)，准备回滚A区\r\n");
					}
					else
					{
						U1_Printf("收到ABORT(0x08)，重置回IDLE\r\n");
						OTA_SendACK();
						OTA_Init();
					}
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

				if (fw_info.firmware_size == 0U || fw_info.firmware_size > OTA_BACKUP_REGION_SIZE)
				{
					U1_Printf("[错误] 固件大小超出A区范围\r\n");
					/* A区已在START阶段擦除，按不可恢复存储错误通知ESP32走ABORT。 */
					OTA_SendNACK(OTA_NACK_FLASH_ERROR);
					OTA_ScheduleRollback(OTA_STORAGE_REASON_FLASH_ERROR, 2000U);
					return;
				}

				g_ota_ctx.firmware_size = fw_info.firmware_size;
				g_ota_ctx.firmware_crc32 = fw_info.firmware_crc32;
				memcpy(g_ota_ctx.version, fw_info.version, OTA_VERSION_LEN);

				g_ota_ctx.total_packets = (g_ota_ctx.firmware_size + OTA_DATA_PAYLOAD_LEN - 1) / OTA_DATA_PAYLOAD_LEN;
				g_ota_ctx.expected_packet = 0;
				g_ota_ctx.received_size = 0;
				g_ota_ctx.write_addr = FLASH_ACode_STARTADDR;

				/* 新镜像信息只保存在RAM；整体CRC通过后才提交EEPROM。 */
				OTA_SendACK();
				g_ota_ctx.state = OTA_STATE_WAIT_DATA;
				OTA_SetStageDeadline(OTA_STATE_WAIT_DATA);
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
						OTA_SetStageDeadline(OTA_STATE_WAIT_DATA);

						if (g_ota_ctx.received_size >= g_ota_ctx.firmware_size)
						{
							g_ota_ctx.state = OTA_STATE_WAIT_END;
							OTA_SetStageDeadline(OTA_STATE_WAIT_END);
							U1_Printf("所有数据接收完成，等待结束命令...\r\n");
						}
					}
					else
					{
						U1_Printf("[错误] Flash写入失败! 包序号: %d\r\n", data_frame.packet_num);
						OTA_SendNACK(OTA_NACK_FLASH_ERROR);
						OTA_ScheduleRollback(OTA_STORAGE_REASON_FLASH_ERROR, 2000U);
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
								OTA_SetStageDeadline(OTA_STATE_WAIT_DATA);
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
 * @return 1-收到握手，0-超时或旧会话ABORT已确认
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
		Bootloader_WatchdogRefresh();
		rx_len = UART_circular_buffer_GetRxLen();

		if (rx_len >= OTA_SIMPLE_FRAME_LEN)
		{
			/* 有恢复通知时交给统一帧入口处理，避免把INFO/DATA只读3字节后
			 * 留下残帧；完整旧会话帧会得到NACK 0x08。 */
			if (OTA_Storage_GetRecoveryNotice() != OTA_STORAGE_RECOVERY_NOTICE_NONE)
			{
				if (rx_len > sizeof(ota_rx_buffer))
				{
					rx_len = sizeof(ota_rx_buffer);
				}
				UART_circular_buffer_ReadData(ota_rx_buffer, rx_len);
				OTA_ProcessFrame(ota_rx_buffer, rx_len);

				/* ABORT只有在恢复通知已可靠清除后才会收到ACK；此时无需继续
				 * 占用剩余等待窗口，可立即进入A区有效性检查并跳转旧APP。 */
				if (OTA_Storage_GetRecoveryNotice() == OTA_STORAGE_RECOVERY_NOTICE_NONE)
				{
					U1_Printf("[回滚] 旧会话ABORT已确认，结束恢复等待\r\n");
					return 0U;
				}
				continue;
			}

			UART_circular_buffer_ReadData(ota_rx_buffer, OTA_SIMPLE_FRAME_LEN);

			if (OTA_ParseSimpleFrame(ota_rx_buffer, OTA_SIMPLE_FRAME_LEN, &cmd))
			{
				if (OTA_HandleRecoveryNotice(cmd))
				{
					continue;
				}

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
				else
				{
					OTA_SendNACK(OTA_NACK_STATE_MISMATCH);
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
	uint32_t handshake_timeout_ms;

	while (1)
	{
		Bootloader_WatchdogRefresh();
		switch (g_ota_ctx.state)
		{
			case OTA_STATE_IDLE:
				handshake_timeout_ms =
					(OTA_Storage_GetRecoveryNotice() != OTA_STORAGE_RECOVERY_NOTICE_NONE) ?
					OTA_RECOVERY_NOTICE_WAIT_MS : OTA_HANDSHAKE_TIMEOUT_MS;

				if (OTA_WaitForHandshake(handshake_timeout_ms))
				{
					OTA_SendACK();
					g_ota_ctx.state = OTA_STATE_WAIT_START;
					OTA_SetStageDeadline(OTA_STATE_WAIT_START);
					U1_Printf("等待三次握手...\r\n");
				}
				else
				{
					/* 超时未收到升级请求：已登记基线必须先通过CRC，未知基线保持旧跳转行为。 */
					if (OTA_Storage_VerifyActiveImage() == OTA_STORAGE_IMAGE_INVALID)
					{
						U1_Printf("[回滚] 启动前A区CRC失败，准备恢复旧版本\r\n");
						OTA_ScheduleRollback(OTA_STORAGE_REASON_BOOT_CRC_ERROR, 0U);
					}
					else
					{
						/* 恢复通知窗口内没有收到旧会话命令，说明Bootloader侧可结束上一轮会话。
						 * 跳转APP前清除恢复通知，避免下一次由APP发起的新0x03被误判为旧会话。 */
						if (OTA_Storage_GetRecoveryNotice() != OTA_STORAGE_RECOVERY_NOTICE_NONE)
						{
							if (OTA_Storage_AcknowledgeRecoveryNotice() == HAL_OK)
							{
								U1_Printf("[回滚] 旧会话等待超时，已清除过期恢复通知\r\n");
							}
							else
							{
								/* A区已按原策略判定可跳转，通知清除失败不扩大为停机故障。 */
								U1_Printf("[回滚] 清除过期恢复通知失败，本次仍跳转A区\r\n");
							}
						}
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
				else if (OTA_IsStageTimeout())
				{
					if (g_ota_ctx.state == OTA_STATE_WAIT_START)
					{
						U1_Printf("[超时] 未收到开始传输命令，返回等待握手\r\n");
						s_stage_deadline = 0U;
						g_ota_ctx.state = OTA_STATE_IDLE;
					}
					else
					{
						U1_Printf("[超时] 升级传输中断，准备恢复旧镜像\r\n");
						s_pending_recovery_notice = OTA_STORAGE_RECOVERY_NOTICE_TRANSFER_TIMEOUT;
						OTA_ScheduleRollback(OTA_STORAGE_REASON_TRANSFER_TIMEOUT, 0U);
					}
				}
				break;

			case OTA_STATE_UPGRADE_SUCCESS:

				HAL_Delay(500);
				Load_A(FLASH_ACode_STARTADDR);
				return;

			case OTA_STATE_UPGRADE_FAILED:
				OTA_ScheduleRollback(OTA_STORAGE_REASON_FLASH_ERROR, 0U);
				break;

			case OTA_STATE_ROLLBACK_PENDING:
				/* 仍短暂处理串口，以便ESP32收到NACK后发ABORT并获得ACK。 */
				rx_len = UART_circular_buffer_GetRxLen();
				if (rx_len > 0U)
				{
					if (rx_len > sizeof(ota_rx_buffer))
					{
						rx_len = sizeof(ota_rx_buffer);
					}
					UART_circular_buffer_ReadData(ota_rx_buffer, rx_len);
					OTA_ProcessFrame(ota_rx_buffer, rx_len);
				}

				if ((int32_t)(HAL_GetTick() - s_rollback_deadline) >= 0)
				{
					if (OTA_Storage_Rollback() == HAL_OK)
					{
						if (s_pending_recovery_notice != OTA_STORAGE_RECOVERY_NOTICE_NONE)
						{
							if (OTA_Storage_SetRecoveryNotice(s_pending_recovery_notice) == HAL_OK)
							{
								U1_Printf("[回滚] 等待ESP32结束超时会话\r\n");
								s_pending_recovery_notice = OTA_STORAGE_RECOVERY_NOTICE_NONE;
								g_ota_ctx.state = OTA_STATE_IDLE;
							}
							else
							{
								U1_Printf("[回滚] 保存恢复通知失败，停在Bootloader\r\n");
								g_ota_ctx.state = OTA_STATE_RECOVERY_FAILED;
							}
						}
						else
						{
							g_ota_ctx.state = OTA_STATE_JUMP_TO_APP;
						}
					}
					else
					{
						U1_Printf("[回滚] 恢复失败，停在Bootloader等待人工恢复\r\n");
						g_ota_ctx.state = OTA_STATE_RECOVERY_FAILED;
					}
				}
				break;

			case OTA_STATE_RECOVERY_FAILED:
				/* 无可信备份时绝不跳转；保留串口日志供SWD/现场定位。 */
				HAL_Delay(2000);
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
