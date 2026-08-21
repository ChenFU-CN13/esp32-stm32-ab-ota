/**
 * @file    TaskTest.c
 * @brief   STM32F407 Bootloader OTA升级协议实现
 * @details 实现ESP32与STM32之间的固件传输协议、Flash操作、状态机等
 */

#include "TaskTest.h"


/*============================================================================*/
/*                          全局变量定义                                       */
/*============================================================================*/

OTA_INFOCB OTA_INFO;                    
load_a LOAD_A;                          

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
/*                          底层硬件操作函数                                   */
/*============================================================================*/

/**
 * @brief SP指针赋值函数（内联汇编）
 * @param address 栈指针地址
 * 
 * @details 关键操作说明:
 *   1. MSR MSP, r0: 将r0寄存器的值(参数address)写入主栈指针(MSP)
 *   2. BX r14: 跳转回调用地址(r14/LR寄存器保存了返回地址)
 * 
 * @note 这个函数用于将CPU的主栈指针从B区切换到A区
 *       在跳转APP前必须执行，否则APP会使用错误的栈空间
 */
__asm void MSR_SP(uint32_t address)
{	
	MSR MSP , r0	
	BX r14			
} 




/**
 * @brief 卸载外设配置环境
 * 
 * @details 在跳转到APP前必须执行以下清理:
 *   1. 反初始化所有使用的外设(UART, I2C, SPI等)
 *   2. 禁用所有中断
 *   3. 调用HAL_DeInit()清理HAL库全局状态
 *   4. 关闭GPIO时钟
 * 
 * @note 如果不清理，APP启动时可能会因为外设状态冲突而异常
 */
void BootLoader_Clear(void)
{
    HAL_UART_MspDeInit(&huart1);
	HAL_UART_MspDeInit(&huart2);
    HAL_I2C_MspDeInit(&hi2c1);
    HAL_SPI_MspDeInit(&hspi1);
    __HAL_UART_DISABLE_IT(&huart1, UART_IT_IDLE);
    __HAL_UART_DISABLE_IT(&huart2, UART_IT_IDLE);
    HAL_DeInit(); 
    __HAL_RCC_GPIOF_CLK_DISABLE();
    __HAL_RCC_GPIOA_CLK_DISABLE();
}




/**
 * @brief 跳转到A区APP
 * @param address APP起始地址(通常为FLASH_ACode_STARTADDR)
 * 
 * @details 跳转流程:
 *   1. 打印调试信息，显示A区首地址和关键数据
 *   2. 检查栈指针合法性(必须在SRAM范围内: 0x20000000~0x2002FFFF)
 *   3. 关闭全局中断(__disable_irq)
 *   4. 重映射向量表到A区(SCB->VTOR = address)
 *   5. 设置栈指针(MSR_SP)
 *   6. 获取复位中断向量地址(地址+4处)
 *   7. 清理外设配置
 *   8. 跳转到复位中断
 * 
 * @note A区Flash布局:
 *       地址+0: 初始栈指针值(指向SRAM顶部)
 *       地址+4: 复位中断向量(程序入口点)
 *       地址+8: NMI中断向量
 *       地址+12: HardFault中断向量
 */
void Load_A(uint32_t address)
{
    U1_Printf("A区首地址: 0x%08X\r\n", address);
    U1_Printf("栈指针(地址+0): 0x%08X\r\n", *(uint32_t*)address);
    U1_Printf("复位向量(地址+4): 0x%08X\r\n", *(uint32_t*)(address+4));
    U1_Printf("地址+8: 0x%08X\r\n", *(uint32_t*)(address+8));
    U1_Printf("地址+12: 0x%08X\r\n", *(uint32_t*)(address+12));

	if(*(uint32_t *)address >= 0x20000000  && *(uint32_t *)address <= 0x2002FFFF)
    {
        U1_Printf("Acode stack address: 0x%08X\r\n", *(uint32_t*)address);
        U1_Printf("Reset vector in Acode: 0x%08X\r\n", *(uint32_t*)(address+4));
		
		__disable_irq(); 
		SCB->VTOR = address; 
		MSR_SP(*(uint32_t*)address);
		LOAD_A = (load_a)*(uint32_t*)(address+4);	
		BootLoader_Clear();	
		LOAD_A();				
    }
	else
	{
		U1_Printf("===========================================================\r\n");
		U1_Printf("B to A code is error \r\n");
		U1_Printf("Please check if the code in Acode has been erased \r\n");
		U1_Printf("===========================================================\r\n");
	}
}




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
	OTA_FirmwareInfo_t fw_info;
	OTA_DataFrame_t data_frame;
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
					OTA_SendACK();
					
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
					if (OTA_VerifyFirmwareCRC())
					{
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
						g_ota_ctx.state = OTA_STATE_UPGRADE_FAILED;
						U1_Printf("========================================\r\n");
						U1_Printf("[严重] 固件整体CRC校验失败！A区固件不可用。\r\n");
						U1_Printf("[安全] 停在Bootloader不跳转，等待重新升级/SWD恢复...\r\n");
						U1_Printf("========================================\r\n");
					}
				}
				break;
					
				default:
					U1_Printf("未知命令: 0x%02X\r\n", cmd);
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
			}
		}
		else
		{
			U1_Printf("[警告] 当前状态不是WAIT_INFO, 忽略固件信息帧\r\n");
		}
	}
	else if (len >= OTA_DATA_FRAME_HEADER_LEN + OTA_DATA_FRAME_TAIL_LEN)
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
					}
				}
				else
				{
					U1_Printf("[警告] 包序号不匹配: 期望%d, 收到%d\r\n", 
						g_ota_ctx.expected_packet, data_frame.packet_num);
				}
			}
			else
			{
				U1_Printf("[错误] 数据帧解析失败!\r\n");
			}
		}
		else
		{
			U1_Printf("[警告] 当前状态不是WAIT_DATA, 忽略数据帧 (长度%d)\r\n", len);
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
				U1_Printf("升级成功，即将跳转到A区APP...\r\n");
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




/*============================================================================*/
/*                          任务接口函数                                       */
/*============================================================================*/

/**
 * @brief 主循环函数
 * 
 * @details 在main函数的while(1)中调用
 *          使用first_run标志确保只执行一次升级流程
 * 
 * @note 调用示例:
 *   while(1) {
 *       Task_While();
 *   }
 */
void Task_While(void)
{
	static uint8_t first_run = 1;
	
	if (first_run)
	{
		first_run = 0;
		OTA_RunUpgradeProcess();
	}
}




/**
 * @brief 主任务初始化函数
 * 
 * @details 在main函数中，外设初始化后调用
 *          初始化OTA上下文和串口环形缓冲区
 * 
 * @note 调用示例:
 *   int main(void) {
 *       HAL_Init();
 *       SystemClock_Config();
 *       MX_GPIO_Init();
 *       MX_USART1_UART_Init();
 *       MX_USART2_UART_Init();
 *       // ... 其他初始化
 *       Task_Main();  // 初始化OTA
 *       while(1) {
 *           Task_While();
 *       }
 *   }
 */
void Task_Main(void)
{
	OTA_Init();
	UART_circular_buffer_Init();
}
