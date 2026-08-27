
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __FAN_H
#define __FAN_H

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
/* DHT11 引脚 定义 */

#define FAN_PORT                  		GPIOG
#define FAN_GPIO_PIN_1                   GPIO_PIN_14
#define FAN_GPIO_PIN_2                   GPIO_PIN_15
#define FAN_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOG_CLK_ENABLE(); }while(0)     /* PG口时钟使能 */

/******************************************************************************************/

/* IO操作函数 */
#define FAN_ON()						do{\
											HAL_GPIO_WritePin(FAN_PORT,FAN_GPIO_PIN_1|FAN_GPIO_PIN_2,GPIO_PIN_SET);}while(0)

#define FAN_OFF()						do{\
											HAL_GPIO_WritePin(FAN_PORT,FAN_GPIO_PIN_1|FAN_GPIO_PIN_2,GPIO_PIN_RESET);}while(0)										

/********模块函数**********/											
/**
 * @brief       初始化风扇IO口
 * @param       无
 * @retval      无
 *              
 */
void FAN_init(void);





#ifdef __cplusplus
}
#endif

#endif 
