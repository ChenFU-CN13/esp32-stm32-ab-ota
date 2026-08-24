#include "onenet_dm.h"
#include "freertos/task.h"

#define TAG "onenet_dm"

//灯的开关状态
static bool led_status = 0;
//蜂鸣器
static bool Alarm_status = 0;
//湿度
static int hum = 0;
//温度
static int temp = 0;

//保存当前灯的亮度
static int led_brightness = 0;

//保存WS2812的RGB值
static uint8_t ws2812_red = 0;
static uint8_t ws2812_green = 0;
static uint8_t ws2812_blue = 0;

//ws2812操作句柄
static ws2812_strip_handle_t ws2812_handle;
static TaskHandle_t s_onenet_update_task_handle = NULL;
static TaskHandle_t s_property_report_task_handle = NULL;


/*函数声明*/
void oneNET_update_task(void* pvParameters);
static void onenet_property_report_task(void *pvParameters);

/**
 * 物模型数据初始化 (明亮度使用普通LED显示，RGB用WS2812显示)
 * @param 无
 * @return 无
 */
void onenet_dm_init(void)
{
    /*初始化WS2812*/
    ws2812_init(GPIO_NUM_18,3,&ws2812_handle);

    gpio_config_t GPIO16 = 
    {
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1<<GPIO_NUM_16)
    };
    gpio_config(&GPIO16);

    /*PWM定时器初始化*/
    ledc_timer_config_t led_timer = 
    {
        .clk_cfg = LEDC_AUTO_CLK,
        .duty_resolution = LEDC_TIMER_12_BIT,
        .freq_hz = 5000,
        .timer_num = LEDC_TIMER_0,
    };
    ledc_timer_config(&led_timer);

    /*PWM通道初始化*/
    ledc_channel_config_t led_channel = 
    {
        .channel = LEDC_CHANNEL_0,
        .duty = 0,
        .gpio_num = GPIO_NUM_15,
        .timer_sel = LEDC_TIMER_0,
    };
    ledc_channel_config(&led_channel);
    //启用渐变功能，如果要调节占空比，必须启用这句
    ledc_fade_func_install(0);

    if (s_onenet_update_task_handle == NULL)
    {
        if (xTaskCreatePinnedToCore(oneNET_update_task,
                                    "oneNET_update_task",
                                    1024,
                                    NULL,
                                    5,
                                    &s_onenet_update_task_handle,
                                    0) != pdPASS)
        {
            s_onenet_update_task_handle = NULL;
            ESP_LOGE(TAG, "物模型UART更新任务创建失败");
        }
    }

    if (s_property_report_task_handle == NULL)
    {
        if (xTaskCreatePinnedToCore(onenet_property_report_task,
                                    "property_report_task",
                                    4096,
                                    NULL,
                                    5,
                                    &s_property_report_task_handle,
                                    0) != pdPASS)
        {
            s_property_report_task_handle = NULL;
            ESP_LOGE(TAG, "物模型周期上报任务创建失败");
        }
    }
}


/**     
 * 处理onenet下行的数据,一定要看平台创建的产品物理模型属性
 * @param property_js 包含下行数据的json
 * @return 无
 */
void onenet_property_handle(cJSON* property_js)
{   /*下发的JSON数据格式一般如下*/
    /*{
        "id":"123",
        "version":"1.0",
        "params":{
                    "LightSwitch":true,
                    "Brightness":50,

                    //以下是结构体类型
                    "RGBColor":{"Red":100,
                                "Green":50
                                "Blue":50
                                 }

                }   
    }
     */
    
    
    cJSON* params_js = cJSON_GetObjectItem(property_js,"params");
    if(params_js)   /*先判断下发的数据对象 是否 有效*/
    {/*然后开始采用链表的遍历方式遍历所有主对象*/
        cJSON* name_js = params_js->child;      /*child 为 头指针指向二级成员*/
                                                /*注意：像LightSwitch，Brightness，RGBColor是params这个主对象的二级成员*/
        while (name_js)
        {   
            if (strcmp(name_js->string,"LightSwitch") == 0) /*接收开关量*/  /*strcmp（）如果两个参数比对一样会返回0*/
            {       
                    if(cJSON_IsTrue(name_js))   /*判断name_js的值是1 或 0*/
                    {   
                        /*操作Led的PWM通道*/
                        //led_brightness = 50;
                        led_status = 1;
                        //int duty = 50*4095/100; /*默认占空比是一半*/
                        //ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,duty,0);
                        gpio_set_level(GPIO_NUM_16,1);
                        
                    }
                    else
                    {   
                        gpio_set_level(GPIO_NUM_16,0);
                        //ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,0,0);
                        led_brightness = 0;
                        led_status = 0;
                        
                    }
            }  
           
            else if (strcmp(name_js->string,"Brightness") == 0) /*接收亮度数据*/
            {
                led_brightness = cJSON_GetNumberValue(name_js);
                //int duty = led_brightness*4095/100;             /*下发的Brightness值是int32型0-100,所以要转换为占空比*/
                //ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,duty,0);
            }

            else if (strcmp(name_js->string,"RGBColor") == 0) /*接收rgb数据*/
            {
                ws2812_red = cJSON_GetNumberValue(cJSON_GetObjectItem(name_js,"Red"));
                ws2812_green = cJSON_GetNumberValue(cJSON_GetObjectItem(name_js,"Green"));
                ws2812_blue = cJSON_GetNumberValue(cJSON_GetObjectItem(name_js,"Blue"));

                for (int i = 0; i < 10; i++)
                {
                    ws2812_write(ws2812_handle,i,ws2812_red,ws2812_green,ws2812_blue);
                }
            }
            name_js = name_js->next;/*开始寻找下一个一级对象，这里只是把指针准备指向下一个，只需
                                    等再次下发数据时，又会回到开头去指向二级的成员*/
        }

            cJSON* property_js = onenet_property_upload_dm(); /*准备好物联网设备数据*/     
            char* data = cJSON_PrintUnformatted(property_js);   /*先将数据格式化*/
            onenet_post_property_data(data);/*发布数据*/
            cJSON_free(data);
            cJSON_Delete(property_js);
    }
}


/**
 * 生成上报所有数据的cJSON对象
 * @param 无
 * @return cJSON对象，包含所有属性值
 */
cJSON* onenet_property_upload_dm(void)
{
    /*上发的JSON数据格式一般如下*/
    /*{
        "id":"123",
        "version":"1.0",
        "params":{
                    "LightSwitch":true,
                    "Brightness":50,

                    //以下是结构体类型
                    "RGBColor":{"Red":100,
                                "Green":50
                                "Blue":50
                                 }

                }   
    }
     */



    cJSON* root = cJSON_CreateObject();     /*创建根对象*/
    cJSON_AddStringToObject(root,"id","123");
    cJSON_AddStringToObject(root,"version","1.0");
    cJSON* params_js = cJSON_AddObjectToObject(root,"params");  /*添加二级对象给根对象*/


    /*湿度*/
    cJSON* Hum = cJSON_AddObjectToObject(params_js,"hum");/*添加三级对象给二级对象*/
    cJSON_AddNumberToObject(Hum,"value",hum);     /*写入数据*/
    ESP_LOGI(TAG,"hum value:%d",hum);

    /*温度*/
    cJSON* Temp = cJSON_AddObjectToObject(params_js,"temp");/*添加三级对象给二级对象*/
    cJSON_AddNumberToObject(Temp,"value",temp);     /*写入数据*/
    ESP_LOGI(TAG,"temp value:%d",temp);

    /*照明灯*/
    cJSON* Led_status = cJSON_AddObjectToObject(params_js,"LED");/*添加三级对象给二级对象*/
    cJSON_AddBoolToObject(Led_status,"value",led_status);     /*写入数据*/
    ESP_LOGI(TAG,"LED value:%d",led_status);

    /*报警器*/
    cJSON* alarm_status = cJSON_AddObjectToObject(params_js,"Alarm");/*添加三级对象给二级对象*/
    cJSON_AddBoolToObject(alarm_status,"value",Alarm_status);     /*写入数据*/
    ESP_LOGI(TAG,"Alarm value:%d",Alarm_status);


   //cJSON* root = cJSON_CreateObject();     /*创建根对象*/
   //cJSON_AddStringToObject(root,"id","123");
   //cJSON_AddStringToObject(root,"version","1.0");
   //cJSON* params_js = cJSON_AddObjectToObject(root,"params");  /*添加二级对象给根对象*/

   ///*灯开关*/
   //cJSON* light_js = cJSON_AddObjectToObject(params_js,"LightSwitch");/*添加三级对象给二级对象*/
   //cJSON_AddBoolToObject(light_js,"value",led_status);         /*写入数据*/
   //ESP_LOGI(TAG,"LightSwitch value:%d",led_status);
   ///*亮度*/
   //cJSON* brightness_js = cJSON_AddObjectToObject(params_js,"Brightness");/*添加三级对象给二级对象*/
   //cJSON_AddNumberToObject(brightness_js,"value",led_brightness);     /*写入数据*/
   //ESP_LOGI(TAG,"Brightness value:%d",led_brightness);

   ///*RGB*/
   //cJSON* color_js = cJSON_AddObjectToObject(params_js,"RGBColor");/*添加三级对象给二级对象*/
   //cJSON* color_value_js = cJSON_AddObjectToObject(color_js,"value");/*添加四级对象给三级对象*/
   //cJSON_AddNumberToObject(color_value_js,"Red",ws2812_red);    /*写入数据*/
   //cJSON_AddNumberToObject(color_value_js,"Green",ws2812_green);    /*写入数据*/
   //cJSON_AddNumberToObject(color_value_js,"Blue",ws2812_blue);       /*写入数据*/
    return root;
}


/**
 * 从串口接收传感器数据进行更新
 * @param 无
 * @return 无
 */
void oneNET_update_task(void* pvParameters)
{
    uart_oneNET_data_t  data;

        // 检查队列是否已初始化
    if (uart_oneNET_data_queue == NULL) {
        ESP_LOGE(TAG, "uart_oneNET_data_queue not initialized");
        vTaskDelete(NULL);
        return;
    }

    for (;;)
    {
        if (xQueueReceive(uart_oneNET_data_queue,&data,portMAX_DELAY))
        {
            switch (data.type)
            {
            case 0x01:
                hum = data.value[0];
                break;

            case 0x02:
                temp = data.value[0];
                break;  

            case 0x03:
                led_status = data.value[0];
                //ws2812_red = data.value[0];
                //ws2812_green = data.value[1];
                //ws2812_blue = data.value[2];
                break;
                
            case 0x04:
                Alarm_status = data.value[0];
                break; 

            default:
                break;
            }
        }
        
    }
    
}

/**
 * @brief 周期上报OneNET物模型数据。
 * @param pvParameters 未使用。
 * @return 无；任务永久运行。
 * @note 任务由物模型模块创建一次；MQTT断线期间只等待，不创建重复任务。
 */
static void onenet_property_report_task(void *pvParameters)
{
    (void)pvParameters;
    ESP_LOGI(TAG, "Property report task started");

    for (;;)
    {
        if (onenet_is_connected())
        {
            cJSON *property_js = onenet_property_upload_dm();
            if (property_js != NULL)
            {
                char *data = cJSON_PrintUnformatted(property_js);
                if (data != NULL)
                {
                    (void)onenet_post_property_data(data);
                    cJSON_free(data);
                }
                cJSON_Delete(property_js);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
