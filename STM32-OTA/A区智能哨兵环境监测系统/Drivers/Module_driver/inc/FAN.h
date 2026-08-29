
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __FAN_H
#define __FAN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fan_pwm.h"

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

#define FAN_ON()    do { FAN_PWM_SetDuty(1U, 100U); FAN_PWM_SetDuty(2U, 100U); } while (0)
#define FAN_OFF()   do { FAN_PWM_SetDuty(1U, 0U); FAN_PWM_SetDuty(2U, 0U); } while (0)

/********模块函数**********/											
/**
 * @brief       初始化 PE5/PE6 双路风扇 PWM
 * @param       无
 * @retval      无
 *              
 */
void FAN_init(void);





#ifdef __cplusplus
}
#endif

#endif 
