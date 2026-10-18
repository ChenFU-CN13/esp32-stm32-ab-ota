
#include "main.h"
#include "cmsis_os.h"
#include "gpio.h"
#include "fan_pwm.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */


/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* 与Bootloader保持相同IWDG配置，避免跳转交接时出现超时窗口突变。 */
#define APP_IWDG_PRESCALER              4U      //看门狗四分频
#define APP_IWDG_RELOAD                 2499U   //重装载计数值
#define APP_IWDG_UPDATE_TIMEOUT         100U    //看门狗超时时间

/**
 * @brief 初始化正式APP独立看门狗。
 * @return HAL_OK-初始化成功；HAL_TIMEOUT-配置寄存器更新超时。
 * @note 若由Bootloader跳转而来，本函数会接管已运行的IWDG；冷启动时则主动启动IWDG。
 */
HAL_StatusTypeDef APP_Watchdog_Init(void)
{
	uint32_t start_tick;

	__HAL_DBGMCU_FREEZE_IWDG();
	IWDG->KR = 0xCCCCU;
	IWDG->KR = 0x5555U;
	IWDG->PR = APP_IWDG_PRESCALER;
	IWDG->RLR = APP_IWDG_RELOAD;

	start_tick = HAL_GetTick();
	while ((IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0U)
	{
		if ((HAL_GetTick() - start_tick) >= APP_IWDG_UPDATE_TIMEOUT)
		{
			return HAL_TIMEOUT;
		}
	}

	IWDG->KR = 0xAAAAU;
	return HAL_OK;
}

/**
 * @brief 刷新正式APP独立看门狗。
 * @return 无。
 * @note 写入重装载键不会在冷启动时自行启动IWDG，可安全用于Bootloader跳转早期接管。
 */
void APP_Watchdog_Refresh(void)
{
	IWDG->KR = 0xAAAAU;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
/* USER CODE BEGIN 1 */

    /*
    * 正式OTA模式下，APP固定运行在0x08010000。
    * 必须在HAL_Init()前切换向量表，避免HAL初始化期间产生中断时跳回Bootloader向量表。
    */
    SCB->VTOR = FLASH_BASE | 0x00010000U;
    __DSB();
    __ISB();

    /*
    * Bootloader已经启动IWDG，跳转APP后IWDG不会停止。
    * APP必须尽早喂一次狗，防止后续时钟和外设初始化时间过长导致复位。
    */
    APP_Watchdog_Refresh();

/* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /*
   * Bootloader跳转前关闭了全局中断，跳转到APP后该状态不会自动恢复。
   * 此时APP向量表和HAL基础环境已经就绪，可以重新开启全局中断。
   */
  __enable_irq();

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();
	if (APP_Watchdog_Init() != HAL_OK)
	{
		Error_Handler();
	}

 
	MX_GPIO_Init();
	APP_Watchdog_Refresh();     //看门狗喂狗
	delay_init();
	usart_init(115200);//注意:这个是串口1
	USART2_DMA1_Init();
	USART2_Init(115200);
	USART3_Init(9600);
	APP_Watchdog_Refresh();
	sram_init();
	FAN_PWM_Init();
	APP_Watchdog_Refresh();

  
	printf("============================= \r\n");
	printf("enter A code is successfully! \r\n");
  //printf("调试:该固件版本为v2.0.0! \r\n");
  printf("调试:该固件版本为v2.0.2! \r\n");
	printf("[IWDG] APP已接管，超时限制约5000 ms\r\n");
	printf("============================= \r\n");

	
  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in freertos.c) */
	APP_Watchdog_Refresh();
  MX_FREERTOS_Init();
	APP_Watchdog_Refresh();
	
  /* Start scheduler */
  osKernelStart();    //开启调度器
  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  

  while (1)
  {
    /* USER CODE END WHILE */


    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */


/**
 * @brief  定时器回调函数   
 * @param   对象句柄      
 * @retval   无     
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    /*
     * 工程将 TIM7 配置为 HAL 的 1 ms 时基。
     * HAL_GetTick()、UART ACK 超时和 HAL 内部超时都依赖此处递增 uwTick。
     * FreeRTOS 使用 SysTick 维护任务节拍，因此不能依赖 SysTick 来驱动 HAL 时基。
     */
    if (htim->Instance == TIM7)
    {
        HAL_IncTick();
    }
    else if (htim->Instance == TIM6)
    {
//        lv_tick_inc(1);                 
    }
}




/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
		printf("Init_fail");
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
