#include "onenet_mqtt.h"

#define TAG     "onenet_mqtt"
#define ONENET_MQTT_TOPIC_MAX_LENGTH       160U
#define ONENET_MQTT_PAYLOAD_MAX_LENGTH     512U


//连接成功标志位
static volatile bool onenet_connected_flg = false;

//mqtt前端连接ESP32客户端   /*mqtt操作句柄,用于全文上下进行操作，所以必须静态*/
static esp_mqtt_client_handle_t s_onenet_client = NULL;

typedef struct
{
    char topic[ONENET_MQTT_TOPIC_MAX_LENGTH];
    char payload[ONENET_MQTT_PAYLOAD_MAX_LENGTH];
    int total_length;
    int received_length;
    int message_id;
    bool active;
} onenet_mqtt_rx_context_t;

static onenet_mqtt_rx_context_t s_mqtt_rx_context = {0};


/*声明*/
static void onenet_property_ack(const char* id,int code,const char* message);
static esp_err_t onenet_ota_ack(const char* id,int code,const char* message);
static int onenet_subscribe(void);
static void onenet_mqtt_reset_rx_context(void);
static esp_err_t onenet_mqtt_feed_data_event(const esp_mqtt_event_t *event);
static void onenet_mqtt_process_complete_message(const char *topic, const char *payload);



/**
 * @brief 处理OneNET MQTT连接、订阅和下行数据事件。
 * @param event_handler_arg 事件处理器注册参数，本模块未使用。
 * @param event_base 事件基础类型，本模块未使用。
 * @param event_id MQTT事件编号。
 * @param event_data MQTT事件数据。
 * @return 无。
 */

static void onenet_mqtt_event_handler(void *event_handler_arg,
                                        esp_event_base_t event_base, 
                                        int32_t event_id, 
                                        void *event_data)
{
    (void)event_handler_arg;
    (void)event_base;

    esp_mqtt_event_handle_t event = event_data;
    switch((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:  //连接成功
            ESP_LOGI(TAG, "Onenet mqtt connected");
            onenet_connected_flg = true;
            if (onenet_subscribe() < 0)
            {
                ESP_LOGE(TAG, "OneNET主题订阅请求失败");
            }

            /* MQTT回调只投递检查事件，HTTP下载和UART升级均由常驻Manager执行。 */
            onenet_ota_start();

            break;
        
        case MQTT_EVENT_DISCONNECTED:  //连接断开
            ESP_LOGI(TAG, "Onenet mqtt disconnected");
            onenet_connected_flg = false;
            onenet_mqtt_reset_rx_context();
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
            ESP_LOGI(TAG, "MQTT_EVENT_DATA offset=%d len=%d total=%d",
                     event->current_data_offset,
                     event->data_len,
                     event->total_data_len);
            if (onenet_mqtt_feed_data_event(event) != ESP_OK)
            {
                ESP_LOGW(TAG, "MQTT下行消息重组失败，已丢弃本条消息");
                onenet_mqtt_reset_rx_context();
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;      
            
        default:
            break;
        }
}

/**
 * @brief 清空MQTT下行消息重组状态。
 * @param 无。
 * @return 无。
 */
static void onenet_mqtt_reset_rx_context(void)
{
    memset(&s_mqtt_rx_context, 0, sizeof(s_mqtt_rx_context));
}

/**
 * @brief 处理一条已经完整重组并补零结尾的MQTT消息。
 * @param topic 完整Topic字符串。
 * @param payload 完整JSON字符串。
 * @return 无。
 * @note 仅做JSON校验、平台ACK和Manager事件投递，不在MQTT回调中执行OTA主体。
 */
static void onenet_mqtt_process_complete_message(const char *topic, const char *payload)
{
    char property_set_topic[ONENET_MQTT_TOPIC_MAX_LENGTH];
    char ota_inform_topic[ONENET_MQTT_TOPIC_MAX_LENGTH];
    cJSON *root;
    cJSON *id_js;
    const char *message_id;

    snprintf(property_set_topic,
             sizeof(property_set_topic),
             "$sys/%s/%s/thing/property/set",
             ONENET_PRODUCT_ID,
             ONENET_DEVICE_NAME);
    snprintf(ota_inform_topic,
             sizeof(ota_inform_topic),
             "$sys/%s/%s/ota/inform",
             ONENET_PRODUCT_ID,
             ONENET_DEVICE_NAME);

    if (strcmp(topic, property_set_topic) == 0)
    {
        root = cJSON_Parse(payload);
        if (root == NULL)
        {
            ESP_LOGW(TAG, "物模型设置JSON无效");
            return;
        }

        id_js = cJSON_GetObjectItemCaseSensitive(root, "id");
        message_id = cJSON_GetStringValue(id_js);
        if (message_id == NULL)
        {
            ESP_LOGW(TAG, "物模型设置缺少字符串id");
            cJSON_Delete(root);
            return;
        }

        onenet_property_handle(root);
        onenet_property_ack(message_id, 200, "success");
        cJSON_Delete(root);
        return;
    }

    if (strcmp(topic, ota_inform_topic) == 0)
    {
        root = cJSON_Parse(payload);
        if (root == NULL)
        {
            ESP_LOGW(TAG, "OTA通知JSON无效");
            return;
        }

        id_js = cJSON_GetObjectItemCaseSensitive(root, "id");
        message_id = cJSON_GetStringValue(id_js);
        if (message_id == NULL)
        {
            ESP_LOGW(TAG, "OTA通知缺少字符串id");
            cJSON_Delete(root);
            return;
        }

        if (onenet_ota_request_check(ONENET_OTA_TRIGGER_MQTT_INFORM) == ESP_OK)
        {
            (void)onenet_ota_ack(message_id, 200, "success");
        }
        else
        {
            ESP_LOGE(TAG, "OTA Manager未就绪，通知未被接受");
            (void)onenet_ota_ack(message_id, 500, "ota manager unavailable");
        }

        cJSON_Delete(root);
    }
}

/**
 * @brief 按offset和total长度重组MQTT_EVENT_DATA分片。
 * @param event 当前MQTT数据事件。
 * @return ESP_OK表示分片已接收或完整消息已处理，其他值表示本条消息非法。
 * @note ESP-MQTT后续分片可能不再携带Topic，因此首片必须保存Topic副本。
 */
static esp_err_t onenet_mqtt_feed_data_event(const esp_mqtt_event_t *event)
{
    int total_length;
    int next_length;

    if ((event == NULL) || (event->data == NULL) ||
        (event->data_len < 0) || (event->current_data_offset < 0))
    {
        return ESP_ERR_INVALID_ARG;
    }

    total_length = (event->total_data_len > 0) ?
                   event->total_data_len : event->data_len;

    if (event->current_data_offset == 0)
    {
        onenet_mqtt_reset_rx_context();
        if ((event->topic == NULL) || (event->topic_len <= 0) ||
            ((size_t)event->topic_len >= sizeof(s_mqtt_rx_context.topic)) ||
            (total_length < 0) ||
            ((size_t)total_length >= sizeof(s_mqtt_rx_context.payload)))
        {
            return ESP_ERR_INVALID_SIZE;
        }

        memcpy(s_mqtt_rx_context.topic, event->topic, (size_t)event->topic_len);
        s_mqtt_rx_context.topic[event->topic_len] = '\0';
        s_mqtt_rx_context.total_length = total_length;
        s_mqtt_rx_context.message_id = event->msg_id;
        s_mqtt_rx_context.active = true;
    }
    else if ((!s_mqtt_rx_context.active) ||
             (event->current_data_offset != s_mqtt_rx_context.received_length) ||
             (total_length != s_mqtt_rx_context.total_length) ||
             ((event->msg_id != 0) &&
              (s_mqtt_rx_context.message_id != 0) &&
              (event->msg_id != s_mqtt_rx_context.message_id)))
    {
        return ESP_ERR_INVALID_STATE;
    }

    next_length = event->current_data_offset + event->data_len;
    if ((next_length < event->current_data_offset) ||
        (next_length > s_mqtt_rx_context.total_length))
    {
        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(&s_mqtt_rx_context.payload[event->current_data_offset],
           event->data,
           (size_t)event->data_len);
    s_mqtt_rx_context.received_length = next_length;

    if (s_mqtt_rx_context.received_length == s_mqtt_rx_context.total_length)
    {
        s_mqtt_rx_context.payload[s_mqtt_rx_context.total_length] = '\0';
        onenet_mqtt_process_complete_message(s_mqtt_rx_context.topic,
                                             s_mqtt_rx_context.payload);
        onenet_mqtt_reset_rx_context();
    }

    return ESP_OK;
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
    cJSON *reply_js;
    char *data;

    if ((id == NULL) || (message == NULL) || (s_onenet_client == NULL))
    {
        ESP_LOGW(TAG, "属性设置ACK参数无效");
        return;
    }

    /*注意：记得写上应答话题的标记路径,具体数据规则看平台手册*/
    snprintf(topic,sizeof(topic),"$sys/%s/%s/thing/property/set_reply",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);

    reply_js = cJSON_CreateObject();     /*创建回应对象，里面填充回应的数据*/
    if (reply_js == NULL)
    {
        ESP_LOGE(TAG, "属性设置ACK JSON创建失败");
        return;
    }

    cJSON_AddStringToObject(reply_js,"id",id);
    cJSON_AddNumberToObject(reply_js,"code",code);
    cJSON_AddStringToObject(reply_js,"message",message);
    data = cJSON_PrintUnformatted(reply_js);/*这一步是去掉换行那些字符，节省发送字节数*/
    if (data == NULL)
    {
        cJSON_Delete(reply_js);
        ESP_LOGE(TAG, "属性设置ACK序列化失败");
        return;
    }

    if (esp_mqtt_client_publish(s_onenet_client,topic,data,strlen(data),1,0) < 0)
    {
        ESP_LOGW(TAG, "属性设置ACK发布失败");
    }
    cJSON_free(data);
    cJSON_Delete(reply_js);

}

/** 
 * OTA应答属性设置确认
 * @param code 错误码
 * @param message 信息
 * @return mqtt连接参数
 */
static esp_err_t onenet_ota_ack(const char* id,int code,const char* message)
{
    char topic[128];
    int publish_id;

    if ((id == NULL) || (message == NULL) || (s_onenet_client == NULL))
    {
        return ESP_ERR_INVALID_ARG;
    }

    snprintf(topic,sizeof(topic),"$sys/%s/%s/ota/inform_reply",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);

    cJSON *reply_js = cJSON_CreateObject();
    if (reply_js == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(reply_js,"id",id);
    cJSON_AddNumberToObject(reply_js,"code",code);
    cJSON_AddStringToObject(reply_js,"msg",message);
    (void)cJSON_AddObjectToObject(reply_js, "data");

    char* data = cJSON_PrintUnformatted(reply_js);
    if (data == NULL)
    {
        cJSON_Delete(reply_js);
        return ESP_ERR_NO_MEM;
    }

    publish_id = esp_mqtt_client_publish(s_onenet_client,topic,data,strlen(data),1,0);
    cJSON_free(data);
    cJSON_Delete(reply_js);
    return (publish_id >= 0) ? ESP_OK : ESP_FAIL;
}


/**
 * 启动mqtt连接
 * @param 无
 * @return 错误码
 */
esp_err_t onenet_start(void)
{
    esp_mqtt_client_config_t  mqtt_client = {0};
    esp_err_t ret;

    /* Wi-Fi重新获取IP时会再次调用本函数，已有客户端由ESP-MQTT自行重连。 */
    if (s_onenet_client != NULL)
    {
        ESP_LOGI(TAG, "OneNET MQTT client already initialized");
        return ESP_OK;
    }

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
    /* 认证token不得写入串口日志。 */
    ESP_LOGI(TAG,"onenet connect->clientId:%s,username:%s",
        mqtt_client.credentials.client_id,mqtt_client.credentials.username);
    //设置mqtt的配置，返回一个mqtt句柄，此句柄后续用来发送数据、注册事件、断开连接使用
    s_onenet_client = esp_mqtt_client_init(&mqtt_client);
    if (s_onenet_client == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    //注册mqtt事件   ESP_EVENT_ANY_ID为响应所有事件
    ret = esp_mqtt_client_register_event(s_onenet_client,ESP_EVENT_ANY_ID,
                                         onenet_mqtt_event_handler,NULL);
    if (ret != ESP_OK)
    {
        (void)esp_mqtt_client_destroy(s_onenet_client);
        s_onenet_client = NULL;
        return ret;
    }

    //启动mqtt连接，注意此函数会创建一个mqtt任务，这个任务会在底层调度其他函数，并不会启动mqtt连接，不会阻塞等到任务完成之后才返回函数
    ret = esp_mqtt_client_start(s_onenet_client);
    if (ret != ESP_OK)
    {
        (void)esp_mqtt_client_destroy(s_onenet_client);
        s_onenet_client = NULL;
    }
    return ret;
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
    onenet_connected_flg = false;
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
static int onenet_subscribe(void)
{
    int msg_id;

    if (!onenet_connected_flg)
        return -1;
    char topic[128];
    //订阅  “上报属性回复主题”
    snprintf(topic,sizeof(topic),"$sys/%s/%s/thing/property/post/reply",
        ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);
    msg_id = esp_mqtt_client_subscribe_single(s_onenet_client,topic,1);  /*先发送客户端的属性reply给前端，让前端知道是谁发信息*/
    if (msg_id < 0)
    {
        return -1;
    }
    //订阅  “下行设置属性主题”
    snprintf(topic,sizeof(topic),"$sys/%s/%s/thing/property/set",
        ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);/*客户端前端发起设置主题属性*/
    msg_id = esp_mqtt_client_subscribe_single(s_onenet_client,topic,1);
    if (msg_id < 0)
    {
        return -1;
    }
    //订阅  “OTA升级通知主题”
    snprintf(topic,sizeof(topic),"$sys/%s/%s/ota/inform",
        ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);
    return esp_mqtt_client_subscribe_single(s_onenet_client,topic,1);



}


/**
 * @brief 查询OneNET MQTT当前连接状态。
 * @param 无。
 * @return true表示已连接，false表示未连接。
 */
bool onenet_is_connected(void)
{
    return onenet_connected_flg;
}
