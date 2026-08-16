#include "onenet_mqtt.h"

#define TAG     "onenet_mqtt"


//连接成功标志位
static bool onenet_connected_flg = false;

//mqtt前端连接ESP32客户端   /*mqtt操作句柄,用于全文上下进行操作，所以必须静态*/
static esp_mqtt_client_handle_t s_onenet_client = NULL;


/*声明*/
static void onenet_property_ack(const char* id,int code,const char* message);
static void onenet_ota_ack(const char* id,int code,const char* message);
static esp_err_t onenet_subscribe(void);
void periodic_report_task(void* pvParameters);/*定时上报任务*/



/** 未写完
 * mqtt连接事件处理函数
 * @param event 事件参数
 * @return 无
 */

static void onenet_mqtt_event_handler(void *event_handler_arg,
                                        esp_event_base_t event_base, 
                                        int32_t event_id, 
                                        void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:  //连接成功
            ESP_LOGI(TAG, "Onenet mqtt connected");
            onenet_connected_flg = true;
            onenet_subscribe();     /*先订阅话题*/
            //onenet_ota_upload_version();
            onenet_ota_start();//开始OTA升级
            
            
            //cJSON* property_js = onenet_property_upload_dm(); /*先准备好物联网设备的初始工作状态的数据*/     
            //char* data = cJSON_PrintUnformatted(property_js);   /*先将数据格式化*/
            //onenet_post_property_data(data);/*发布数据*/
            //cJSON_free(data);
            //cJSON_Delete(property_js);
            // xTaskCreate(periodic_report_task,"periodic_report_task",4096,NULL,5,NULL);/*开启定时上报*/
            break;
        
        case MQTT_EVENT_DISCONNECTED:  //连接断开
            ESP_LOGI(TAG, "Onenet mqtt disconnected");
            onenet_connected_flg = false;           
            break;  
            
        case MQTT_EVENT_SUBSCRIBED:     //收到订阅消息ACK
            ESP_LOGI(TAG, "Onenet mqtt subscribed ack, msg_id=%d", event->msg_id);
            break;
          
        case MQTT_EVENT_UNSUBSCRIBED:   //收到解除订阅消息ACK

            break;

        case MQTT_EVENT_PUBLISHED:      //收到发布消息ACK
            ESP_LOGI(TAG, "Onenet mqtt publish ack, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
          ESP_LOGI(TAG, "MQTT_EVENT_DATA"); 
          printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
          printf("DATA=%.*s\r\n", event->data_len, event->data);

          /*接下来就判断oneNET平台下发的Topic 是否 属于“设置属性”的主题，如果是的话，就把传感器之类的数据上传*/
          if(strstr(event->topic,"/property/set"))      /*strstr()的用途是于查找一个字符串是否包含另一个字符串，找到就返回第一次出现 目标字符串位置 的 指针；没有就返回null*/
          {
             
            cJSON* property_js = cJSON_Parse(event->data);  /* cJSON_Parse() 用于解析，解析后才可以获取成员和对象*/
            cJSON* id_js = cJSON_GetObjectItem(property_js,"id");

            onenet_property_handle(property_js);
            /*应答具体数据规则看平台手册*/
            onenet_property_ack(cJSON_GetStringValue(id_js),200,"success");
            cJSON_Delete(property_js);

          }

          /*这里原本是打算接收OTA下发通知，但是平台这个通知十分不准时经常很久才下发通知，而且
          并没提供到上传到平台最新固件的版本，所以不建议去等待下发通知才开始升级
          
          目前打算onenet平台MQTT_EVENT_CONNECTED后，就直接检查平台是否有最新固件需要进行升级*/
          //else if (strstr(event->topic,"/ota/inform"))//OTA升级通知
          //{
          //  cJSON* ota_js = cJSON_Parse(event->data);  /* cJSON_Parse() 用于解析，解析后才可以获取成员和对象*/
          //  cJSON* id_js = cJSON_GetObjectItem(ota_js,"id");
          //  onenet_ota_ack(cJSON_GetStringValue(id_js),200,"success");
          //  cJSON_Delete(ota_js);

          //  /*处理OTA升级 */
          //  //onenet_ota_start();
          //}
          
            
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;      
            
        default:
            break;
        }
}


/** 
 * 应答属性设置确认
 * @param code 错误码
 * @param message 信息
 * @return mqtt连接参数
 */
static void onenet_property_ack(const char* id,int code,const char* message)
{
    char topic[128];
    /*注意：记得写上应答话题的标记路径,具体数据规则看平台手册*/
    snprintf(topic,sizeof(topic),"$sys/%s/%s/thing/property/set_reply",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);

    cJSON *reply_js = cJSON_CreateObject();     /*创建回应对象，里面填充回应的数据*/
    cJSON_AddStringToObject(reply_js,"id",id);
    cJSON_AddNumberToObject(reply_js,"code",code);
    cJSON_AddStringToObject(reply_js,"message",message);
    char* data = cJSON_PrintUnformatted(reply_js);/*这一步是去掉换行那些字符，节省发送字节数*/
    esp_mqtt_client_publish(s_onenet_client,topic,data,strlen(data),1,0);
    cJSON_free(data);
    cJSON_Delete(reply_js);

}

/** 
 * OTA应答属性设置确认
 * @param code 错误码
 * @param message 信息
 * @return mqtt连接参数
 */
static void onenet_ota_ack(const char* id,int code,const char* message)
{
    char topic[128];
    /*注意：记得写上应答话题的标记路径,具体数据规则看平台手册*/
    snprintf(topic,sizeof(topic),"$sys/%s/%s/ota/inform_reply",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);

    cJSON *reply_js = cJSON_CreateObject();     /*创建回应对象，里面填充回应的数据*/
    cJSON_AddStringToObject(reply_js,"id",id);
    cJSON_AddNumberToObject(reply_js,"code",code);
    cJSON_AddStringToObject(reply_js,"message",message);
    char* data = cJSON_PrintUnformatted(reply_js);/*这里data不填也可以，因为看ONENET平台手册没有特别要求*/
    esp_mqtt_client_publish(s_onenet_client,topic,data,strlen(data),1,0);
    cJSON_free(data);
    cJSON_Delete(reply_js);

}


/**
 * 启动mqtt连接
 * @param 无
 * @return 错误码
 */
esp_err_t onenet_start(void)
{
    esp_mqtt_client_config_t  mqtt_client = {0};

    //onenet平台连接的mqtt地址,要手动加上mqtt://
    mqtt_client.broker.address.uri = "mqtt://mqtts.heclouds.com";
    //onenet平台连接的mqtt端口号
    mqtt_client.broker.address.port = 1883;
    //client id用设备名称
    mqtt_client.credentials.client_id = ONENET_DEVICE_NAME;
    //用户名用产品id
    mqtt_client.credentials.username = ONENET_PRODUCT_ID;
    //token有效时间（2030年1月1日）
    //密码，onenet平台使用token来作为密码，token直接使用dev_token_generate计算即可
    static char token[256];
    dev_token_generate(token, SIG_METHOD_SHA256, TM_EXPIRE_TIME, ONENET_PRODUCT_ID,
     ONENET_DEVICE_NAME, ONENET_ACCESS_KEY);    /*第三个变量是有效的时间戳，当ESP32S3用token去访问时，
                                            平台先判断这个时间戳是否正常*/
    mqtt_client.credentials.authentication.password = token;
    //将鉴权信息打印出来
    ESP_LOGI(TAG,"onenet connect->clientId:%s,username:%s,password:%s",
        mqtt_client.credentials.client_id,mqtt_client.credentials.username,
        mqtt_client.credentials.authentication.password);
    //设置mqtt的配置，返回一个mqtt句柄，此句柄后续用来发送数据、注册事件、断开连接使用
    s_onenet_client = esp_mqtt_client_init(&mqtt_client);
    //注册mqtt事件   ESP_EVENT_ANY_ID为响应所有事件
    esp_mqtt_client_register_event(s_onenet_client,ESP_EVENT_ANY_ID,
    onenet_mqtt_event_handler,NULL);
    //启动mqtt连接，注意此函数会创建一个mqtt任务，这个任务会在底层调度其他函数，并不会启动mqtt连接，不会阻塞等到任务完成之后才返回函数
    return esp_mqtt_client_start(s_onenet_client);
}


/**
 * 停止onenet连接
 * @param 无
 * @return 错误码
 */
esp_err_t onenet_stop(void)
{
    esp_err_t   ret  =  ESP_OK;
    if (s_onenet_client)
    {
        ret = esp_mqtt_client_destroy(s_onenet_client);
        s_onenet_client = NULL;
    }
    return ret;
    
}

/**
 * 上报数据
 * @param data 数据
 * @return 错误
 */
/*esp_mqtt_client_publish： 用于向特定主题发布消息
                            将数据发送到指定主题
                            是发送数据的操作
*/
esp_err_t onenet_post_property_data(const char* data)
{
    if (!onenet_connected_flg)  
        return ESP_FAIL;
    
    char topic[128];
    snprintf(topic,sizeof(topic),"$sys/%s/%s/thing/property/post",
            ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);  
    ESP_LOGI(TAG,"Upload topic:%s,payload:%s",topic,data);
    /*最后将物联网设备的工作情况上报一遍给平台*/
    return esp_mqtt_client_publish(s_onenet_client,topic,data,strlen(data),1,0);
}

/**
 * 订阅相关主题，有要订阅的主题可以放在这个函数
 * @param 无
 * @return 错误
 */
/*esp_mqtt_client_subscribe_single：
        用于订阅特定主题（topic）
        让客户端能够接收来自指定主题的消息
        是接收数据的操作*/
esp_err_t onenet_subscribe(void)
{
    if (!onenet_connected_flg)
        return ESP_FAIL;
    char topic[128];
    //订阅  “上报属性回复主题”
    snprintf(topic,sizeof(topic),"$sys/%s/%s/thing/property/post/reply",
        ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);
    esp_mqtt_client_subscribe_single(s_onenet_client,topic,1);  /*先发送客户端的属性reply给前端，让前端知道是谁发信息*/
    //订阅  “下行设置属性主题”
    snprintf(topic,sizeof(topic),"$sys/%s/%s/thing/property/set",
        ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);/*客户端前端发起设置主题属性*/
    esp_mqtt_client_subscribe_single(s_onenet_client,topic,1);
    //订阅  “OTA升级通知主题”
    snprintf(topic,sizeof(topic),"$sys/%s/%s/ota/inform",
        ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);
    return esp_mqtt_client_subscribe_single(s_onenet_client,topic,1);



}


/**
 * 定时上报传感器数据任务
 * @param 无
 * @return 错误
 */
void periodic_report_task(void* pvParameters)
{
     ESP_LOGI(TAG, "Periodic report task started");

     for(;;)
     {      
            cJSON* property_js = onenet_property_upload_dm();   /*先准备好物联网设备的初始工作状态的数据*/     
            char* data = cJSON_PrintUnformatted(property_js);   /*先将数据格式化*/
            onenet_post_property_data(data);                    /*发布数据*/
            cJSON_free(data);                                   
            cJSON_Delete(property_js);
            vTaskDelay(pdMS_TO_TICKS(500));                    /*间隔0.5s上报一次*/
     }
}