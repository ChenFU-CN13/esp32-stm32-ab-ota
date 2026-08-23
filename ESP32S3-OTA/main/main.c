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

        nvs_flash_init();
        //onenet物模型数据初始化
        onenet_dm_init();//包含物模型数据传输任务
        wifi_ev = xEventGroupCreate();
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

    #if 1
        
        UART_config_init_AND_UART_recive_TASK(&myuart,UART_NUM_1);/*开启串口接收任务，已包含串口缓冲区任务创建*/
        xTaskCreate(vMQTT_ONENET_Task,"vMQTT_ONENET_Task",4096,NULL,5,NULL);

    #endif
}
