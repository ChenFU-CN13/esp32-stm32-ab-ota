#include "main.h"


        #if 1
        static EventGroupHandle_t wifi_ev = NULL;
        static void wifi_state_callback(WIFI_STATE state)
        {
            if (state ==  WIFI_STATE_CONNECTED) /*当捕获到了WiFi连接才发起连接oneNet平台*/
            {
                xEventGroupSetBits(wifi_ev,EV_WIFI_CONNECTED_BIT);  /*记得：回调函数一般只做通知，不做太多耗时的操作*/
            }
            
        }
        #endif

        
      #if 1
            static uart_param_config_t myuart = 
            {
                .baud_rate = 115200,
                .data_bits = UART_DATA_8_BITS,
                .parity = UART_PARITY_DISABLE,
                .stop_bits = UART_STOP_BITS_1,
                .UART_PORT = UART_NUM_1,

                .GPIO_RX = GPIO_NUM_46,
                .GPIO_TX = GPIO_NUM_45       
            };
            
            
      #endif




void vMQTT_ONENET_Task(void* pvParameters)
{
        esp_err_t nvs_ret;

        (void)pvParameters;
        nvs_ret = nvs_flash_init();
        if ((nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES) ||
            (nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND))
        {
            /* 仅在NVS页耗尽或格式版本不兼容时擦除并重建，避免Wi-Fi初始化继续使用坏状态。 */
            ESP_LOGW(TAG, "NVS需要重建: %s", esp_err_to_name(nvs_ret));
            nvs_ret = nvs_flash_erase();
            if (nvs_ret == ESP_OK)
            {
                nvs_ret = nvs_flash_init();
            }
        }
        if (nvs_ret != ESP_OK)
        {
            ESP_LOGE(TAG, "NVS初始化失败，停止Wi-Fi/OneNET启动: %s",
                     esp_err_to_name(nvs_ret));
            vTaskDelete(NULL);
            return;
        }

        //onenet物模型数据初始化
        onenet_dm_init();//包含物模型数据传输任务
        wifi_ev = xEventGroupCreate();
        if (wifi_ev == NULL)
        {
            ESP_LOGE(TAG, "Wi-Fi事件组创建失败");
            vTaskDelete(NULL);
            return;
        }
        wifi_manager_init(wifi_state_callback);
        wifi_manager_connect(DEFAULT_SSID,DEFAULT_PASSWORD);

    EventBits_t ev;
    for (;;)
    {
        ev = xEventGroupWaitBits(wifi_ev,EV_WIFI_CONNECTED_BIT,pdTRUE,pdFALSE,portMAX_DELAY);
        if (ev & EV_WIFI_CONNECTED_BIT)
        {
            onenet_start();
        }
        
    }
    
}      

            

void app_main(void)
{
    BaseType_t task_ret;

    #if 1
        /* 先建立STM32版本同步状态，再启动UART，避免APP上电即上报时丢失事件。 */
        if (onenet_ota_init() != ESP_OK)
        {
            ESP_LOGE(TAG, "OTA Manager初始化失败");
            return;
        }

        UART_config_init_AND_UART_recive_TASK(&myuart,UART_NUM_1);/*开启串口接收任务，已包含串口缓冲区任务创建*/
        task_ret = xTaskCreatePinnedToCore(vMQTT_ONENET_Task,
                                           "vMQTT_ONENET_Task",
                                           4096,
                                           NULL,
                                           5,
                                           NULL,
                                           0);
        if (task_ret != pdPASS)
        {
            ESP_LOGE(TAG, "Wi-Fi/OneNET启动任务创建失败");
        }

    #endif
}
