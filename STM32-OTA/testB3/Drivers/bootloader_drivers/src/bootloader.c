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
load_a LOAD_A;		//定义一个函数指针变量 LOAD_A，类型是 load_a

/* IWDG由32kHz标称LSI驱动：/64、Reload=2499时名义约5秒。
 * STM32F407数据手册给出的LSI范围为17~47kHz，实际约3.4~9.4秒。 */
#define BOOTLOADER_IWDG_PRESCALER       4U
#define BOOTLOADER_IWDG_RELOAD          2499U
#define BOOTLOADER_IWDG_UPDATE_TIMEOUT  100U

static uint8_t s_iwdg_reset_detected = 0U;


/*============================================================================*/
/*                          底层硬件操作函数                                   */
/*============================================================================*/

/**
 * @brief 初始化Bootloader独立看门狗。
 * @return HAL_OK-初始化成功；HAL_TIMEOUT-配置寄存器更新超时。
 * @note IWDG启动后不能由软件关闭，A区APP必须在启动早期接管喂狗。
 */
HAL_StatusTypeDef Bootloader_WatchdogInit(void)
{
	uint32_t start_tick;

	/*先判断MCU 这次启动，是不是因为 IWDG 看门狗超时导致的复位。*/
	s_iwdg_reset_detected = (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET) ? 1U : 0U;
	__HAL_RCC_CLEAR_RESET_FLAGS();	//清除复位标志
	__HAL_DBGMCU_FREEZE_IWDG();		//开启这个看门狗调试功能后，在keil选择断电调试时，CPU虽然停止工作，但是不会因为CPU没有及时喂狗而导致单片机不断复位

	IWDG->KR = 0xCCCCU;			//相当于HAL 初始化 HAL_IWDG_Init() 内部会完成类似操作。
	IWDG->KR = 0x5555U;			//解除 IWDG 的 PR、RLR 寄存器写保护，允许修改分频值和重载值。
	IWDG->PR = BOOTLOADER_IWDG_PRESCALER;
	IWDG->RLR = BOOTLOADER_IWDG_RELOAD;

	start_tick = HAL_GetTick();
	while ((IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0U)
	{
		if ((HAL_GetTick() - start_tick) >= BOOTLOADER_IWDG_UPDATE_TIMEOUT)
		{
			return HAL_TIMEOUT;
		}
	}

	IWDG->KR = 0xAAAAU;			//喂狗，相当于HAL_IWDG_Refresh(&hiwdg);
	return HAL_OK;
}

/**
 * @brief 刷新独立看门狗计数器。
 * @return 无。
 */
void Bootloader_WatchdogRefresh(void)
{
	IWDG->KR = 0xAAAAU;		//相当于HAL_IWDG_Refresh(&hiwdg);
}

/**
 * @brief 查询本次启动是否由IWDG复位触发。
 * @return 1-是IWDG复位；0-不是IWDG复位。
 */
uint8_t Bootloader_WasIwdgReset(void)
{
	return s_iwdg_reset_detected;
}

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
    Bootloader_WatchdogRefresh();
    U1_Printf("A区首地址: 0x%08X\r\n", address);
    U1_Printf("栈指针(地址+0): 0x%08X\r\n", *(uint32_t*)address);
    U1_Printf("复位向量(地址+4): 0x%08X\r\n", *(uint32_t*)(address+4));
    U1_Printf("地址+8: 0x%08X\r\n", *(uint32_t*)(address+8));
    U1_Printf("地址+12: 0x%08X\r\n", *(uint32_t*)(address+12));

	if(*(uint32_t *)address >= 0x20000000  && *(uint32_t *)address <= 0x2002FFFF)
    {
        U1_Printf("Acode stack address: 0x%08X\r\n", *(uint32_t*)address);
        U1_Printf("Reset vector in Acode: 0x%08X\r\n", *(uint32_t*)(address+4));

		__disable_irq();				//关闭全局中断，避免影响，在跳转A区后要重新开启！

		SCB->VTOR = address;			//VTOR寄存器用于告诉CPU中断向量表存放在内存中哪个位置
		//这里要指向A区的起始地址0x08010000，如果不改的话，CPU如果发生中断，需要去中断向量表中进入中断，会从
		//默认的0x0800000（B区）开始找，这样就一定会跑飞程序

		MSR_SP(*(uint32_t*)address);	//这里就是改堆栈指针SP，作用是：把“临时存放数据的栈从B区切换到A区”
		//去 0x08010000 读4个字节 → 得到 A区向量表第1个字 = A区初始栈顶（比如0x20005000）

		LOAD_A = (load_a)*(uint32_t*)(address+4);
		//将A区的复位向量地址对应的值(还要强转为load_a，不然数据类型不一样)，当做函数地址赋值给LOAD_A
		//注意：这里是读取向量地址对应的值，而且是这个值刚刚好是 系统复位中断函数的地址；
		//不要认为是把这个向量地址直接赋值给LOAD_A，这个是两码事

		BootLoader_Clear();				//卸载B区环境

		LOAD_A();
		//这里是改PC指针（程序计数器）：“把执行位置从B区跳到A区的Reset_Handler中”
		//因为函数名等于地址，在上一个步骤中已经是把系统复位中断函数的地址赋值给它
		//因此LOAD_A();此时就类似于HAL_NVIC_reset();

    }
	else
	{
		U1_Printf("===========================================================\r\n");
		U1_Printf("B to A code is error \r\n");
		U1_Printf("Please check if the code in Acode has been erased \r\n");
		U1_Printf("===========================================================\r\n");
	}
}
