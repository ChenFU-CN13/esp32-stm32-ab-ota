
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BUZZER_H
#define __BUZZER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/


/* USER CODE BEGIN EFP */
/******************************************************************************************/
/* 引脚 定义 */

#define BEEP_GPIO_PORT                  GPIOF
#define BEEP_GPIO_PIN                   GPIO_PIN_8
#define BEEP_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOF_CLK_ENABLE(); }while(0)         /* PF口时钟使能 */

/******************************************************************************************/

/* 蜂鸣器控制 */
#define BEEP(x)         do{ x ? \
                            HAL_GPIO_WritePin(BEEP_GPIO_PORT, BEEP_GPIO_PIN, GPIO_PIN_SET) : \
                            HAL_GPIO_WritePin(BEEP_GPIO_PORT, BEEP_GPIO_PIN, GPIO_PIN_RESET); \
                        }while(0)

/* BEEP状态翻转 */
#define BEEP_TOGGLE()   do{ HAL_GPIO_TogglePin(BEEP_GPIO_PORT, BEEP_GPIO_PIN); }while(0)    /* BEEP = !BEEP */

void beep_init(void);   /* 初始化蜂鸣器 */
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif 
