#ifndef _ONENET_DM_H_
#define _ONENET_DM_H_

#include "cJSON.h"
#include "led_ws2812.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include <string.h>
#include "onenet_mqtt.h"
#include "myuart.h"


/**
 * 物模型数据初始化(实际就是初始化传感器)
 * @param 无
 * @return 无
 */
void onenet_dm_init(void);

/**
 * 处理onenet下行的数据
 * @param property_js 包含下行数据的json
 * @return 无
 */
void onenet_property_handle(cJSON* property_js);

/**
 * 生成上报所有数据的cJSON对象
 * @param 无
 * @return cJSON对象，包含所有属性值
 */
cJSON* onenet_property_upload_dm(void);


/**
 * 从串口接收传感器数据进行更新
 * @param 无
 * @return 无
 */
void oneNET_update(void);

#endif