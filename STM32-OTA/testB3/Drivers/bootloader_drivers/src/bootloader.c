/**
 * @file    bootloader.c
 * @brief   Bootloader 跳转实现
 * @details 归口：跳转相关（BootLoader_Clear / Load_A / MSR_SP）。
 *          （由原 TaskTest.c 拆分而来，函数体一字未改动）
 */

#include "bootloader.h"
#include "usart.h"
#include "i2c.h"
#include "spi.h"


/* 跳转目标函数指针变量：在 Load_A 中赋值为 A 区 APP 的复位向量 */
load_a LOAD_A;


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


