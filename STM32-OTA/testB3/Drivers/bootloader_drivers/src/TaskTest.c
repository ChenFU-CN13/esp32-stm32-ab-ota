/**
 * @file    TaskTest.c
 * @brief   任务入口层（薄壳）
 * @details 仅保留任务入口函数 Task_Main / Task_While。
 *          其余 Bootloader 核心逻辑已拆分为独立模块：
 *            - bootloader   ：跳转（BootLoader_Clear / Load_A / MSR_SP）、Flash 分区
 *            - ota_proto    ：协议帧构造 / 解析 / CRC32（纯函数无状态）
 *            - ota_storage  ：Flash 擦除 / 写入 / 整体 CRC 校验（回滚地基）
 *            - ota_core     ：升级状态机（OTA_Init / ProcessFrame / WaitForHandshake / RunUpgradeProcess）
 */

#include "TaskTest.h"
#include "ota_core.h"


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
 *          只初始化OTA上下文和持久化恢复状态
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
	/* USART2 DMA环形缓冲区已在main.c中启动。
	 * OTA_Init()可能执行EEPROM检查和镜像CRC，此处不能再次初始化，
	 * 否则会清空启动期间已收到的二次握手帧。 */
}
