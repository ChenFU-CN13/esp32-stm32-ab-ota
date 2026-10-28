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
static uint32_t s_rollback_deadline = 0U;        /* 回滚执行时刻(HAL_GetTick时间戳)：进入ROLLBACK_PENDING时
													设为"当前tick+延迟"，主循环到期后才调OTA_Storage_Rollback()；
													0表示当前没有挂起的回滚 */
static uint8_t s_storage_initialized = 0U;       /* 上电只恢复一次的守卫标志：OTA_Init可能因ABORT/失败被
													重复调用，但Flash状态扫描与掉电恢复只允许首次上电执行一次，
													防止协议层复位上下文时重复触发恢复 */
static uint32_t s_stage_deadline = 0U;           /* 当前接收阶段超时时刻：由OTA_SetStageDeadline()按状态
													装填(15s/10s)，OTA_IsStageTimeout()判断是否到期；
													每正确收到一帧会向后刷新，0表示不计时 */
static OTA_StorageRecoveryNotice_t s_pending_recovery_notice = OTA_STORAGE_RECOVERY_NOTICE_NONE;
												 /* 回滚完成后待写入EEPROM的恢复通知（RAM暂存）：
													仅传输超时路径置为TRANSFER_TIMEOUT，收到ABORT时清零；
													Rollback()成功后SetRecoveryNotice落盘，使重启后/IDLE期
													仍能对旧会话帧回NACK 0x08，直到ESP32发ABORT收尾 */


/**
 * @brief 安排一次回滚（所有升级失败/ABORT/超时路径的统一入口）
 * @param reason    回滚原因，写入EEPROM供掉电恢复与日志定位
 * @param delay_ms  延迟多少毫秒后才真正执行Rollback()；
 *                  传2000是给ESP32留出"收NACK→发ABORT→收ACK"的收尾窗口，
 *                  传0表示不等待（启动CRC失败、传输超时等无收尾必要的场景）
 *
 * @details 三步动作：
 *            1. EEPROM登记NEED_ROLLBACK+原因（失败也不停——擦A区前已登记UPGRADING，
 *               即使这里写失败，掉电重启仍会走保守恢复，这是"宁多恢复不漏恢复"原则）；
 *            2. 记录回滚执行时刻s_rollback_deadline，并清掉阶段超时计时；
 *            3. RAM状态机切到ROLLBACK_PENDING，由主循环到期后调OTA_Storage_Rollback()。
 * @note 本函数只做"决策与登记"，不直接拷贝镜像；真正恢复在主循环ROLLBACK_PENDING分支执行。
 */
static void OTA_ScheduleRollback(OTA_StorageReason_t reason, uint32_t delay_ms)
{
	if (OTA_Storage_MarkRollbackPending(reason) != HAL_OK)
	{
		/* 擦除前已写UPGRADING；即使本次原因写入失败，掉电后仍会走保守恢复。 */
		U1_Printf("[回滚] 写入待回滚状态失败，继续在本次启动中恢复\r\n");
	}

	s_rollback_deadline = HAL_GetTick() + delay_ms;	/* 到期时刻，主循环据此判断何时执行恢复 */
	s_stage_deadline = 0U;							/* 回滚期间阶段超时不再生效，避免互相干扰 */
	g_ota_ctx.state = OTA_STATE_ROLLBACK_PENDING;	/* 切状态，真正的Rollback()在主循环中完成 */
}


/**
 * @brief 按当前所处状态装填"阶段超时时刻"s_stage_deadline
 * @param state 即将进入/仍在等待的接收状态
 *
 * @details 四个等待态各有独立超时时限（见ota_proto.h，均为15s，WAIT_END为10s）；
 *          其余状态（IDLE/成功/回滚等）不参与阶段计时，直接把deadline清0。
 * @note 调用时机：每次状态迁移成功、以及每正确收到一包数据后——
 *       因此WAIT_DATA的15s是"相邻两包的最大间隔"，不是整份固件的传输总时长。
 */
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
			/* 非等待态：关闭阶段超时检测 */
			s_stage_deadline = 0U;
			return;
	}

	/* 以当前tick为起点重新计时（接收数据帧时调用即实现"逐包续命"） */
	s_stage_deadline = HAL_GetTick() + timeout_ms;
}


/**
 * @brief 判断当前接收阶段是否已超时
 * @return 1-已超时（deadline非0且当前时刻已越过它）；0-未超时或未在计时
 * @note 差值强转int32_t再比较，可正确处理HAL_GetTick()的32位毫秒计数器回绕，
 *       不会因约49.7天一次的tick溢出而误判。
 */
static uint8_t OTA_IsStageTimeout(void)
{
	return (s_stage_deadline != 0U &&
		(int32_t)(HAL_GetTick() - s_stage_deadline) >= 0) ? 1U : 0U;
}


/**
 * @brief 处理"恢复通知未结束"期间收到的简单命令（旧会话收尾握手）
 * @param cmd 已解析出的命令字
 * @return 0-当前没有挂起的恢复通知，本命令应按正常状态机逻辑处理；
 *         1-存在恢复通知且本帧已被本函数消费（已应答），调用方必须直接return，
 *           不得再进入正常命令分支
 *
 * @details 背景：STM32已把旧镜像恢复，但ESP32可能仍以为在升级而继续发帧。
 *          通知未清除期间：
 *            - 收到0x08 ABORT：先可靠清除EEPROM通知，成功才回ACK（先落盘后应答，
 *              防止ACK发了但通知没清，重启后又继续误拦新会话）；清除失败回NACK 0x04；
 *            - 收到其它任何命令：说明ESP32还想延续旧会话，回NACK 0x08
 *              (OTA_NACK_PREVIOUS_ROLLBACK)，明确要求它用ABORT结束。
 * @note 本函数只处理3字节简单命令帧；非简单帧的同类拦截在OTA_ProcessFrame入口处统一做。
 */
static uint8_t OTA_HandleRecoveryNotice(uint8_t cmd)
{
	/* 没有挂起通知：不干预，交回正常状态机 */
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
			/* EEPROM清除失败：不能放行，否则掉电后通知还在、新会话会被继续误拦 */
			U1_Printf("[回滚] 清除恢复通知失败，继续阻止旧会话\r\n");
			OTA_SendNACK(OTA_NACK_FLASH_ERROR);
		}
	}
	else
	{
		/* 旧会话的非ABORT命令（如继续发0x04/0x05）：一律拒绝并要求ABORT收尾 */
		U1_Printf("[回滚] 上一轮已恢复旧镜像，拒绝继续旧会话\r\n");
		OTA_SendNACK(OTA_NACK_PREVIOUS_ROLLBACK);
	}

	return 1U;	/* 本帧已消费，调用方不再走正常分支 */
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
	/*1.固件简单命令帧处理*/
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
	/*2.固件信息帧处理
	 * 为什么用"长度==25"来分流：三种帧的帧头0x66完全相同，命令字位置和CRC覆盖范围
	 * 却各不相同，解析前取不到可靠命令字，所以只能用固定长度先分类——
	 * 3字节走上面的简单命令分支，25字节走本分支，>=13字节走下面的数据帧分支，
	 * 其余长度落到最后的"无法识别"else。这是本函数最外层分流的唯一判据。*/
	else if (len == OTA_INFO_FRAME_LEN)
	{
		/* 先原样打印收到的字节：信息帧校验失败时，靠这行日志区分
		 * "串口丢/多字节"还是"上位机拼帧错误"（25字节全打，不截断）*/
		U1_Printf("[调试] 识别为固件信息帧(%d字节)\r\n", OTA_INFO_FRAME_LEN);
		U1_Printf("  帧数据: ");
		for (i = 0; i < len && i < 32; i++)
		{
			U1_Printf("%02X ", data[i]);
		}
		U1_Printf("\r\n");

		/* 状态闸门①：只有收到0x04、已备份并擦除A区之后才会处在WAIT_INFO。
		 * 若在别的状态收到信息帧，说明STM32与ESP32的流程不同步（典型是上位机
		 * 还在重发旧会话），此时一律不进内层逻辑、不改g_ota_ctx，
		 * 直接走本if的else回NACK 0x02，让上位机自己对齐到正确状态。*/
		if (g_ota_ctx.state == OTA_STATE_WAIT_INFO)
		{
			/* 状态闸门②：解析失败（帧头/帧尾/帧内CRC任一不符）时不改状态、
			 * 不推进任何游标，只回NACK 0x03要求重发；
			 * 注意阶段超时仍在走，ESP32一直发错帧最终会超时回滚。*/
			if (OTA_ParseFirmwareInfo(data, len, &fw_info))
			{
				U1_Printf("收到固件信息帧:\r\n");
				U1_Printf("  固件大小: %lu 字节\r\n", fw_info.firmware_size);
				U1_Printf("  固件CRC32: 0x%08X\r\n", fw_info.firmware_crc32);
				U1_Printf("  版本号: %s\r\n", fw_info.version);

				/* 大小合法性检查（写第一个数据包之前的最后一道闸）：
				 *   0字节 —— 无意义的镜像，ratio/包数计算会得到0，必须拒；
				 *   >960KB —— 超过W25Q128备份区容量(OTA_BACKUP_REGION_SIZE)，
				 *             后面的备份、回滚都会因空间不足而失败，也必须拒。*/
				if (fw_info.firmware_size == 0U || fw_info.firmware_size > OTA_BACKUP_REGION_SIZE)
				{
					U1_Printf("[错误] 固件大小超出A区范围\r\n");
					/* 关键：此刻A区在0x04阶段已被擦除，EEPROM也已登记UPGRADING，
					 * 这个会话已无法善终，唯一出路是回滚——
					 * 所以先回NACK 0x04(存储类错误)，让ESP32按协议发ABORT，
					 * 再安排"延迟2000ms"的回滚：这个延迟就是留给上位机
					 * 收NACK→发ABORT→收ACK的收尾窗口。*/
					OTA_SendNACK(OTA_NACK_FLASH_ERROR);
					OTA_ScheduleRollback(OTA_STORAGE_REASON_FLASH_ERROR, 2000U);
					/* 用return而非break：ScheduleRollback已把状态切成ROLLBACK_PENDING，
					 * 本帧不应再走后面任何逻辑，否则会与刚切的状态自相矛盾。*/
					return;
				}

				/* 把信息帧里的三个关键值搬进上下文——注意这里只是"记在RAM"，
				 * 不写EEPROM：新镜像还没收到、没校验，绝不能污染已登记的旧基线。*/
				g_ota_ctx.firmware_size = fw_info.firmware_size;
				g_ota_ctx.firmware_crc32 = fw_info.firmware_crc32;
				memcpy(g_ota_ctx.version, fw_info.version, OTA_VERSION_LEN);

				/* 总包数向上取整：最后一包可能不足1024字节，
				 * 用+1023再整除避免少算一包（例如1025字节 → 2包而不是1包）。*/
				g_ota_ctx.total_packets = (g_ota_ctx.firmware_size + OTA_DATA_PAYLOAD_LEN - 1) / OTA_DATA_PAYLOAD_LEN;
				/* 三个游标全部归零，与"刚擦完A区"的物理状态对齐： */
				g_ota_ctx.expected_packet = 0;              /* 下面期待第0包（序号从0开始） */
				g_ota_ctx.received_size = 0;                /* 已收字节数，用于判末包/判收满 */
				g_ota_ctx.write_addr = FLASH_ACode_STARTADDR; /* 写Flash游标回到A区起始 */

				/* 新镜像信息只保存在RAM；整体CRC通过后才提交EEPROM。 */
				/* 先ACK再迁移：ESP32收到ACK时，STM32的上下文已经准备好接收数据，
				 * 不会出现"ACK已发但expected_packet还没归零"的竞争窗口。*/
				OTA_SendACK();
				g_ota_ctx.state = OTA_STATE_WAIT_DATA;
				/* 进入WAIT_DATA必须重装超时：这里把deadline从"等信息帧"改为"等数据帧"，
				 * 之后每收对一包都会再刷一次，实现逐包续命。*/
				OTA_SetStageDeadline(OTA_STATE_WAIT_DATA);
				U1_Printf("等待固件数据帧... (共%d包)\r\n", g_ota_ctx.total_packets);
			}
			else
			{
				/* 解析失败的else：帧能收够25字节，但内部结构/CRC不对。
				 * 只回NACK 0x03，状态保持WAIT_INFO不变——ESP32会重发，
				 * 由"阶段超时"兜底，连续错帧最终会超时回滚。*/
				U1_Printf("[错误] 固件信息帧解析失败!\r\n");
				OTA_SendNACK(OTA_NACK_FRAME_CRC);	/*通知ESP32,当前STM32解析数据出错*/
			}
		}
		else
		{
			/* 外层状态闸门的else：帧长正确但状态不对（如IDLE时收到信息帧，
			 * 或已进入WAIT_DATA后ESP32重发旧信息帧）。
			 * 一律回NACK 0x02(状态不匹配)而不是解析或丢弃，
			 * 让上位机明确知道"帧没错，是流程不对齐"。*/
			U1_Printf("[警告] 当前状态不是WAIT_INFO, 忽略固件信息帧\r\n");
			OTA_SendNACK(OTA_NACK_STATE_MISMATCH);
		}
	}
	/*3.固件数据帧处理
	 * 数据帧是变长帧(8字节头+最多1024字节数据+5字节尾)，长度无法用"=="匹配，
	 * 所以只能把"最短可能长度(8+5=13，即data_len=0的空数据包)"作为下界兜底。
	 * 放在最后判断：前面的3字节、25字节已经先被精确长度认领，
	 * 这里剩下的>=13的帧就必然是数据帧；连13都不到的长度落到最后的else。*/
	else if (len >= OTA_DATA_FRAME_HEADER_LEN + OTA_DATA_FRAME_TAIL_LEN)/* 数据帧长度检查 */
	{
		/* 状态闸门①：数据帧只有WAIT_DATA才收。
		 * 否则(如IDLE/WAIT_START/ROLLBACK_PENDING)走本if的else回NACK 0x02，
		 * 关键是不解析、不写Flash——回滚期间ESP32仍在猛发数据时，
		 * 正是靠这道闸把旧会话数据全部挡在Flash之外。*/
		if (g_ota_ctx.state == OTA_STATE_WAIT_DATA)
		{
			/* 状态闸门②：解析失败(帧头/帧尾/帧内CRC任一不符，常见于串口丢字节)
			 * 时不推进expected_packet，只回NACK 0x03要求ESP32重发同一包。*/
			if (OTA_ParseDataFrame(data, len, &data_frame))
			{
				/* 包序号三态判定——整个数据帧处理的核心分叉：
				 *   ① == expected        → 正常包，写Flash(下面第一个if)
				 *   ② == expected-1      → 重复包，ACK丢失导致的重传，只补ACK不写Flash
				 *   ③ 其它               → 乱序/跳包，回NACK 0x07要求重发 */
				if (data_frame.packet_num == g_ota_ctx.expected_packet)/* 包序号匹配 */
				{
					/* 末包截断：上位机按1024字节切包，最后一包可能被补零/补齐，
					 * 若"已收字节+本包长度"超过固件实际大小，说明这是最后一包，
					 * 只取剩余的(firmware_size - received_size)字节写进Flash，
					 * 避免把补齐字节写进A区、导致后面整体CRC对不上。*/
					if (g_ota_ctx.received_size + data_frame.data_len > g_ota_ctx.firmware_size)
					{	/* 最后一包数据截断 ，通过判断是否超出固件大小 */
						uint32_t original_len = data_frame.data_len;
						data_frame.data_len = g_ota_ctx.firmware_size - g_ota_ctx.received_size;
						U1_Printf("[调试] 最后一包截断: %lu -> %lu 字节\r\n",
							original_len, data_frame.data_len);
					}
					/*正常处理：正常写入数据帧*/
					/* 写Flash用write_addr(而不是packet_num*1024推算)，
					 * 因为末包被截断过，包序号×包长与实际偏移不再相等。*/
					status = OTA_WriteFirmwareData(g_ota_ctx.write_addr,
					                               data_frame.data,
					                               data_frame.data_len);

					if (status == HAL_OK)
					{
						/* 写入成功才推进三个游标，且顺序无所谓但必须全部推进：
						 * 写地址累加、已收字节累加、期望包号加1(放松下一包的门)。*/
						g_ota_ctx.write_addr += data_frame.data_len;
						g_ota_ctx.received_size += data_frame.data_len;
						g_ota_ctx.expected_packet++;

						/* 进度打印节流：每10包一次，或最后一包必打，
						 * 避免每包都printf拖慢串口接收（1024字节/包时串口很容易被日志挤爆）。*/
						if (g_ota_ctx.expected_packet % 10 == 0 ||
						    g_ota_ctx.expected_packet >= g_ota_ctx.total_packets)
						{
							U1_Printf("接收进度: %d/%d 包 (%lu/%lu 字节, %d%%)\r\n",
								g_ota_ctx.expected_packet, g_ota_ctx.total_packets,
								g_ota_ctx.received_size, g_ota_ctx.firmware_size,
								(g_ota_ctx.received_size * 100) / g_ota_ctx.firmware_size);
						}

						/* 先ACK，再刷超时：
						 * ACK让ESP32继续发下一包；SetStageDeadline把15s重新起算，
						 * 这就是"逐包续命"——只要包持续到达，传输多久都不会超时。*/
						OTA_SendACK();
						OTA_SetStageDeadline(OTA_STATE_WAIT_DATA);

						/* 收满判定：字节数达到信息帧声明的大小就切WAIT_END。
						 * 注意这里不是看包数——末包可能被截断，包数≠字节数更可靠。
						 * 切状态后超时从15s改为10s，等ESP32发0x07结束命令。*/
						if (g_ota_ctx.received_size >= g_ota_ctx.firmware_size)
						{
							g_ota_ctx.state = OTA_STATE_WAIT_END;
							OTA_SetStageDeadline(OTA_STATE_WAIT_END);
							U1_Printf("所有数据接收完成，等待结束命令...\r\n");
						}
					}
					else
					{
						/* 写Flash失败的else：A区已擦且已写入部分数据，镜像必然不完整，
						 * 只能回滚。处理顺序同信息帧的大小非法分支——先NACK 0x04，
						 * 让ESP32发ABORT，再延迟2000ms执行恢复。
						 * 之后ESP32再发的数据帧会因状态已变(ROLLBACK_PENDING)
						 * 被外层状态闸门挡下并回NACK 0x02，不会污染A区。*/
						U1_Printf("[错误] Flash写入失败! 包序号: %d\r\n", data_frame.packet_num);
						OTA_SendNACK(OTA_NACK_FLASH_ERROR);
						OTA_ScheduleRollback(OTA_STORAGE_REASON_FLASH_ERROR, 2000U);
					}
				}
				/* 包序号不匹配的else（三态判定中的②和③都落到这里）：
				 * 不是"当前期望的包"，就要分两种可能处理。*/
				else
				{
						//U1_Printf("[警告] 包序号不匹配: 期望%d, 收到%d\r\n",
						//g_ota_ctx.expected_packet, data_frame.packet_num);
						/* 情形②：收到的正好是"期望包号-1"，说明它是上一包的重传——
						 * 上一包其实已写成功并推进了游标，只是ACK在链路丢了，
						 * ESP32超时重发。此时绝不能重写Flash(会重复写同一地址)，
						 * 只需补发一个ACK让它继续，并刷新超时。*/
						if (data_frame.packet_num + 1 == g_ota_ctx.expected_packet)
						{
								/*进入这个条件说明重复包：上一包数据已正确写入，只是ACK丢了，补ACK让ESP32继续*/
								U1_Printf("[调试]重复包%d(对应的ACK丢失)，补发ACK\r\n", data_frame.packet_num);
								OTA_SendACK();
								OTA_SetStageDeadline(OTA_STATE_WAIT_DATA);
						}
						/* 情形③：既不是期望包、也不是上一包(跳包/超前包/落后多包)，
						 * 回NACK 0x07告诉ESP32序号不对要求重发；
						 * 这里不写Flash、不推进游标，保证A区内容始终按序。*/
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
				/* 解析失败的else：长度够13但帧头/帧尾/帧内CRC校验不过，
				 * 多是串口丢字节造成；只回NACK 0x03等ESP32重发，不动任何游标。*/
				U1_Printf("[错误] 数据帧解析失败!\r\n");
				OTA_SendNACK(OTA_NACK_FRAME_CRC);

			}
		}
		else
		{
			/* 外层状态闸门的else：长度像数据帧，但当前不在WAIT_DATA
			 * （如IDLE时ESP32抢发、或已进WAIT_END/回滚中）。回NACK 0x02，
			 * 不解析、不写Flash——这是回滚期间保护A区不被旧会话继续写入的关键。*/
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
		Bootloader_WatchdogRefresh();//喂狗
		/* 状态机主循环：每轮只做"当前状态该做的一件事"，不在case里长时间阻塞，
		 * 靠结尾HAL_Delay(1)形成约1ms的轮询节拍。
		 * 注意：只有 UPGRADE_SUCCESS / JUMP_TO_APP 会return跳出本函数，
		 * 其余所有case只能改g_ota_ctx.state，绝不能continue/return，
		 * 否则喂狗和串口轮询都会停掉。*/
		switch (g_ota_ctx.state)
		{
			case OTA_STATE_IDLE:
				/* 握手等待时长按"有没有未收尾的旧会话"二选一：
				 *   有恢复通知(0xA1/0xA2) → 用更长的 OTA_RECOVERY_NOTICE_WAIT_MS，
				 *     给ESP32留出发ABORT结束旧会话的时间，避免刚回滚就跳进APP；
				 *   无恢复通知           → 用常规 OTA_HANDSHAKE_TIMEOUT_MS，
				 *     这段时间内收不到0x03就当"无升级请求"，直接跳APP。*/
				handshake_timeout_ms =
					(OTA_Storage_GetRecoveryNotice() != OTA_STORAGE_RECOVERY_NOTICE_NONE) ?
					OTA_RECOVERY_NOTICE_WAIT_MS : OTA_HANDSHAKE_TIMEOUT_MS;

				/* OTA_WaitForHandshake内部是阻塞的小循环(会自己喂狗)：
				 *   返回1 → 收到了0x03二次握手，可以进入升级流程；
				 *   返回0 → 等待超时，或期间确认掉了旧会话的0x08 ABORT，
				 *           两种情况都表示"本次没有可升级的请求"。*/
				if (OTA_WaitForHandshake(handshake_timeout_ms))
				{
					/* 握手成功的处理注意点：ACK是这里发的，不是ProcessFrame里，
					 * 因为0x03是在WaitForHandshake的独立小循环中解析的。
					 * ACK之后立刻切WAIT_START并装15s超时，等0x04三次握手。*/
					OTA_SendACK();
					g_ota_ctx.state = OTA_STATE_WAIT_START;
					OTA_SetStageDeadline(OTA_STATE_WAIT_START);
					U1_Printf("等待三次握手...\r\n");
				}
				else
				{
					/* 超时分支：没有升级请求，准备跳APP——但跳之前必须确认A区可跑。
					 * VerifyActiveImage返回值有三种，这里只拦 INVALID：
					 *   VALID   → A区与登记CRC一致，可跳；
					 *   UNKNOWN → 从未登记基线（首次烧录/EEPROM不可用），保持旧行为照跳；
					 *   INVALID → 有基线但CRC对不上，A区已损坏，跳过去必死。*/
					/* 超时未收到升级请求：已登记基线必须先通过CRC，未知基线保持旧跳转行为。 */
					if (OTA_Storage_VerifyActiveImage() == OTA_STORAGE_IMAGE_INVALID)
					{
						/* A区损坏：安排立即回滚(delay=0)，此时不需要等ESP32收尾，
						 * 因为它根本没在升级，直接进入ROLLBACK_PENDING执行恢复。*/
						U1_Printf("[回滚] 启动前A区CRC失败，准备恢复旧版本\r\n");
						OTA_ScheduleRollback(OTA_STORAGE_REASON_BOOT_CRC_ERROR, 0U);
					}
					else
					{
						/* 恢复通知窗口内没有收到旧会话命令，说明Bootloader侧可结束上一轮会话。
						 * 跳转APP前清除恢复通知，避免下一次由APP发起的新0x03被误判为旧会话。 */
						/* 为什么必须在跳转前清通知：通知是持久化在EEPROM里的，
						 * 不清的话APP下一次发起的新0x03会被当成旧会话帧，
						 * 一直被回NACK 0x08，导致以后再也升级不了。*/
						if (OTA_Storage_GetRecoveryNotice() != OTA_STORAGE_RECOVERY_NOTICE_NONE)
						{
							if (OTA_Storage_AcknowledgeRecoveryNotice() == HAL_OK)
							{
								U1_Printf("[回滚] 旧会话等待超时，已清除过期恢复通知\r\n");
							}
							else
							{
								/* A区已按原策略判定可跳转，通知清除失败不扩大为停机故障。 */
								/* 这里刻意不转RECOVERY_FAILED：A区是可跑的，
								 * 为一个EEPROM写失败把整机停掉属于过度惩罚。*/
								U1_Printf("[回滚] 清除过期恢复通知失败，本次仍跳转A区\r\n");
							}
						}
						/* 置跳转状态，真正的Load_A在下面的JUMP_TO_APP分支里做，
						 * 这样跳转只有一个出口，便于统一打印与后续加保护。*/
						g_ota_ctx.state = OTA_STATE_JUMP_TO_APP;
					}
				}
				break;

			case OTA_STATE_WAIT_START:
			case OTA_STATE_WAIT_INFO:
			case OTA_STATE_WAIT_DATA:
			case OTA_STATE_WAIT_END:
				/* 四个"等待接收"状态共用同一段处理：都是"有数据就解析、没数据就判超时"，
				 * 真正的差异在 OTA_ProcessFrame 内部按状态做的闸门，
				 * 以及下面超时分支里对 WAIT_START 的单独处理。*/
				rx_len = UART_circular_buffer_GetRxLen();
				if (rx_len > 0)
				{
					/* 环形缓冲里可能堆积多帧，但一次最多只取缓冲区大小：
					 * 多余字节留在环缓冲，下一轮(约1ms后)继续取，
					 * 避免memcpy越界，也避免长时间占着CPU不让主循环喂狗。*/
					if (rx_len > sizeof(ota_rx_buffer))
					{
						rx_len = sizeof(ota_rx_buffer);
					}
					UART_circular_buffer_ReadData(ota_rx_buffer, rx_len);
					/* 一并把"收数据+解析+应答+状态迁移"全部交给ProcessFrame，
					 * 本case不关心具体是哪条命令。*/
					OTA_ProcessFrame(ota_rx_buffer, rx_len);
				}
				/* 注意是 else if：只要缓冲里还有数据就优先处理数据、暂不判超时，
				 * 否则大包持续到达时可能被误判为"传输中断"。*/
				else if (OTA_IsStageTimeout())
				{
					/* 超时分两种，区别只看A区有没有被动过：*/
					if (g_ota_ctx.state == OTA_STATE_WAIT_START)
					{
						/* WAIT_START = 只收到0x03、还没收到0x04，
						 * 备份和擦除都没发生，A区完好，不需要回滚，
						 * 直接清掉阶段计时退回IDLE重新等握手即可。*/
						U1_Printf("[超时] 未收到开始传输命令，返回等待握手\r\n");
						s_stage_deadline = 0U;
						g_ota_ctx.state = OTA_STATE_IDLE;
					}
					else
					{
						/* WAIT_INFO/WAIT_DATA/WAIT_END 阶段A区已被擦除(甚至写了半截)，
						 * 传输中断只能回滚。
						 * 与ABORT路径的唯一差别：这里先把"超时"记入
						 * s_pending_recovery_notice，回滚成功后写进EEPROM，
						 * 让重启后/IDLE期仍能对旧会话帧回NACK 0x08，
						 * 逼ESP32发ABORT收尾；而ABORT路径本来就是上位机主动放弃，
						 * 不需要再挂这个通知。*/
						U1_Printf("[超时] 升级传输中断，准备恢复旧镜像\r\n");
						s_pending_recovery_notice = OTA_STORAGE_RECOVERY_NOTICE_TRANSFER_TIMEOUT;
						/* delay=0：传输都断了，没必要再等收尾窗口，立即进回滚。*/
						OTA_ScheduleRollback(OTA_STORAGE_REASON_TRANSFER_TIMEOUT, 0U);
					}
				}
				break;

			case OTA_STATE_UPGRADE_SUCCESS:
				/* 由ProcessFrame处理0x07、整体CRC通过且元数据提交成功后置入本状态。
				 * 延时500ms：让串口把最后一个ACK/日志完整发出去，
				 * 否则刚发完就跳进APP，Bootloader的发送会被打断成半帧，
				 * 上位机可能把"半帧"当失败，误以为升级没成功。*/

				HAL_Delay(500);
				/* Load_A会改MSP、改向量表并跳转，正常情况下永不返回；
				 * 真的返回只可能是跳转失败，所以这里直接return退出状态机，
				 * 不再回while(1)里继续转。*/
				Load_A(FLASH_ACode_STARTADDR);
				return;

			case OTA_STATE_UPGRADE_FAILED:
				/* 兜底分支：当前所有失败路径（CRC失败/写失败/ABORT/超时）
				 * 都已统一走 ROLLBACK_PENDING，代码里已没有任何地方会置入
				 * UPGRADE_FAILED，这里保留只是防止状态被意外写成该值后
				 * 卡死在switch里——一旦进来就当作Flash类失败去回滚。*/
				OTA_ScheduleRollback(OTA_STORAGE_REASON_FLASH_ERROR, 0U);
				break;

			case OTA_STATE_ROLLBACK_PENDING:
				/* 由 OTA_ScheduleRollback 置入。本状态做两件事：
				 * ① 继续收串口；② 等 s_rollback_deadline 到期后真正执行恢复。
				 * 继续收串口的目的不是继续升级，而是让ESP32的0x08 ABORT
				 * 能走到ProcessFrame → 拿到ACK（ScheduleRollback的延迟就是为这个）。*/
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

				/* 到期判定：同样用int32差值，兼容HAL_GetTick的32位回绕。
				 * 说明：此刻恢复通知还在RAM(s_pending_recovery_notice)里、
				 * 尚未写进EEPROM，所以ProcessFrame入口的"旧会话拦截"还没生效，
				 * ABORT会按正常命令分支处理：回ACK并重新ScheduleRollback(delay=0)，
				 * 于是本状态最多再转一圈就立刻执行恢复，不会干等满2秒。*/
				if ((int32_t)(HAL_GetTick() - s_rollback_deadline) >= 0)
				{
					/* 真正执行拷贝：W25Q128备份 → A区，并重新提交旧镜像元数据。
					 * 这一步是"擦A区之后一切失败路径"的唯一修复手段。*/
					if (OTA_Storage_Rollback() == HAL_OK)
					{
						/* 恢复成功后按"是否需要对旧会话喊停"分成两条路：*/
						if (s_pending_recovery_notice != OTA_STORAGE_RECOVERY_NOTICE_NONE)
						{
							/* 只有"传输超时"路径会走到这里（ABORT路径该值已被清0）：
							 * 把超时通知落盘，使IDLE期/重启后仍对旧会话帧回NACK 0x08，
							 * 逼ESP32发ABORT收尾，然后回IDLE等这个收尾。*/
							if (OTA_Storage_SetRecoveryNotice(s_pending_recovery_notice) == HAL_OK)
							{
								U1_Printf("[回滚] 等待ESP32结束超时会话\r\n");
								s_pending_recovery_notice = OTA_STORAGE_RECOVERY_NOTICE_NONE;
								g_ota_ctx.state = OTA_STATE_IDLE;
							}
							else
							{
								/* 通知写不进EEPROM，就等于失去了拦住旧会话的唯一凭据：
								 * ESP32可能继续以为在升级并重发数据，一旦重新走0x04
								 * 就会把刚恢复好的A区再覆盖掉。所以宁可停机等人工处理。*/
								U1_Printf("[回滚] 保存恢复通知失败，停在Bootloader\r\n");
								g_ota_ctx.state = OTA_STATE_RECOVERY_FAILED;
							}
						}
						else
						{
							/* ABORT路径：上位机已主动放弃，不会再来数据，
							 * 恢复完直接跳APP即可，不需要再喊停。*/
							g_ota_ctx.state = OTA_STATE_JUMP_TO_APP;
						}
					}
					else
					{
						/* 恢复失败：没有可信备份、备份CRC损坏或擦写A区失败。
						 * 此时A区内容不可信，绝对不能跳，只能停在Bootloader。*/
						U1_Printf("[回滚] 恢复失败，停在Bootloader等待人工恢复\r\n");
						g_ota_ctx.state = OTA_STATE_RECOVERY_FAILED;
					}
				}
				break;

			case OTA_STATE_RECOVERY_FAILED:
				/* 停机态（唯一出口是人工/看门狗复位）：
				 * A区已确认不可信，又回滚失败或通知无法落盘，跳过去必死、留在Bootloader才有救。
				 * 每轮延时2s而不是1ms，是为了把主循环节拍放慢、降低无意义轮询。
				 * 安全性说明：IWDG配置为PR=64、RLR=2499(约5s超时)，2s延时短于它，
				 * 且每轮循环开头都会喂狗，因此不会意外复位。*/
				/* 无可信备份时绝不跳转；保留串口日志供SWD/现场定位。 */
				HAL_Delay(2000);
				break;

			case OTA_STATE_JUMP_TO_APP:
				/* 跳转的唯一出口：正常无升级请求、ABORT回滚成功、恢复成功且无需喊停，
				 * 三条路径最后都汇聚到这里，保证"跳APP"这个动作只有一处实现。*/
				U1_Printf("无升级请求，跳转到A区APP...\r\n");
				Load_A(FLASH_ACode_STARTADDR);
				return;		/* 正常不会执行到：Load_A跳转后本函数栈已废弃 */

			default:
				/* 理论上不可达（上面的case已覆盖OTA_State_t所有取值）。
				 * 保留空分支，防止状态变量被写坏成非法值时直接掉出switch。*/
				break;
		}

		/* 约1ms的轮询节拍，决定两件事：
		 *   1) 响应延迟——串口数据先进环形缓冲区，最多晚1ms被取出解析，
		 *      期间不会丢字节（环缓冲容量足够装下多帧）；
		 *   2) CPU占用——Bootloader不做业务，慢节拍可以少占CPU、少干扰喂狗。
		 * 注意不要在case内部轻易加长延时（UPGRADE_SUCCESS的500ms、
		 * RECOVERY_FAILED的2s是刻意为之），否则会拖慢整包接收、
		 * 拉长相邻两包间隔，极端情况下触发WAIT_DATA超时误回滚。*/
		HAL_Delay(1);
	}
}
