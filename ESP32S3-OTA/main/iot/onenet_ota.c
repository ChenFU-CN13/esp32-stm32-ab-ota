#include "onenet_ota.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_http_client.h"
#include "onenet_token.h"
#include "onenet_mqtt.h"
#include "esp_log.h"
#include "cJSON.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include <sys/stat.h>
#include <fcntl.h>
#include "mbedtls/md5.h"
#include "myuart.h"



#define TAG     "onenet_ota"

#define     MAX_DATA_BUFF   1024    //http接收数据缓存大小
#define     FIRMWARE_FILE_PATH   "/spiffs/stm32_firmware.bin"//SPIFFS文件系统中固件文件路径

//ota基础url
#define     ONENET_OTA_URL  "http://iot-api.heclouds.com/fuse-ota"
//token合法时间戳
#define     TOKEN_TIMESTAMP     2074855187
//等待http返回的信号量
static SemaphoreHandle_t    onenet_sem = NULL;
//数据事件
#define   HTTP_RECEIVE_DATA_BIT       BIT0 
static EventGroupHandle_t   onenet_data_ev = NULL;

//接收到的http 数据
static uint8_t data_buff[MAX_DATA_BUFF];
//接收到的http数据长度
static size_t   data_buff_len = 0;
//ota升级任务id
static int  task_id = 0;
//要升级到的版本号
static char target_version[16] = {0};
//固件大小
static int  firmware_size = 0; 
//固件MD5值
static char firmware_md5[64] = {0};
//ota任务是否在运行
static bool ota_is_running = false;

//标志位，是否需要发送固件
static bool need_send_firmware = false;

// 下载相关的上下文数据
static int download_file_fd = -1;
static size_t download_total_size = 0;
static mbedtls_md5_context md5_ctx;


static esp_err_t http_client_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:    //错误事件
            //ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:    //连接成功事件
            //ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:    //发送头事件
            //ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:    //接收头事件
            //ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
            printf("%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:    //接收数据事件
            {
                size_t copy_len = 0;
                ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
                printf("HTTP_EVENT_ON_DATA data=%.*s\r\n", evt->data_len,(char*)evt->data);
                
                // 如果正在下载固件，将数据写入文件
                if (download_file_fd >= 0) {
                    ssize_t write_len = write(download_file_fd, evt->data, evt->data_len);
                    if (write_len != evt->data_len) {
                        ESP_LOGE(TAG, "Failed to write to file, written %d, expected %d", (int)write_len, evt->data_len);
                    } else {
                        download_total_size += evt->data_len;
                        // 更新MD5值
                        mbedtls_md5_update(&md5_ctx, (const unsigned char *)evt->data, evt->data_len);
                        ESP_LOGI(TAG, "Downloaded %zu bytes", download_total_size);
                    }
                } else {
                    // 否则，将数据复制到缓冲区
                    if(evt->data_len > MAX_DATA_BUFF - data_buff_len)
                    {
                        copy_len = MAX_DATA_BUFF - data_buff_len;
                    }
                    else
                    {
                        copy_len = evt->data_len;
                    }
                    memcpy(&data_buff[data_buff_len],evt->data,copy_len);
                    data_buff_len += copy_len;
                    xEventGroupSetBits(onenet_data_ev,HTTP_RECEIVE_DATA_BIT);
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:    //会话完成事件
            break;
        case HTTP_EVENT_DISCONNECTED:    //断开事件
            //ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            //ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

/**
 * http连接，封装请求头信息
 * @param url 连接url
 * @param method 连接方法
 * @param post_data 发送数据
 * @return 错误码
 */
static esp_err_t onenet_ota_http_connect(const char* url,esp_http_client_method_t method,char* post_data)
{
    esp_http_client_config_t config =
    {
        .url = url,
        .event_handler = http_client_event_handler,
    };
    //初始化结构体
    esp_http_client_handle_t http_client = esp_http_client_init(&config);	//初始化http连接
    if(!http_client)
    {
        ESP_LOGI(TAG,"http_client init fail!");
        return ESP_FAIL;
    }

    char* token = (char*)malloc(256);
    memset(token,0,256);
    dev_token_generate(token,SIG_METHOD_SHA256,TOKEN_TIMESTAMP,ONENET_PRODUCT_ID,NULL,ONENET_ACCESS_KEY);//设备ID填NULL因为用的是产品id
    ESP_LOGI(TAG,"user token:%s",token);
    //设置发送请求信息
    esp_http_client_set_method(http_client, method);
    esp_http_client_set_header(http_client,"Content-Type","application/json");
    esp_http_client_set_header(http_client,"Authorization",token);
    esp_http_client_set_header(http_client,"host","iot-api.heclouds.com");
    if(post_data)//如果有发送数据
    {
        ESP_LOGI(TAG,"post data:%s",post_data);//打印发送数据
        esp_http_client_set_post_field(http_client,post_data,strlen(post_data));//设置发送数据
    }
    data_buff_len = 0;
    memset(data_buff,0,sizeof(data_buff));//清空接收数据缓存
    esp_err_t err  = esp_http_client_perform(http_client);//执行http连接
    free(token);
    esp_http_client_cleanup(http_client);
    return err;
}

/**
 * 上报版本号
 * @param version 版本号字符串，格式为X.X.X（如"1.0.4"）
 * @return 错误码
 */
esp_err_t onenet_ota_upload_version(const char* version)
{
    //格式：{"s_version":"V1.0.0", "f_version": "V1.0.0"}
    if(!onenet_data_ev)
        onenet_data_ev = xEventGroupCreate();
    char version_info[128];
    char url[256];
    esp_err_t ret = ESP_FAIL;
    snprintf(version_info,sizeof(version_info),"{\"s_version\":\"%s\", \"f_version\": \"%s\"}",version,version);
    snprintf(url,256,ONENET_OTA_URL"/%s/%s/version",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);
    onenet_ota_http_connect(url,HTTP_METHOD_POST,version_info);
    EventBits_t ev = xEventGroupWaitBits(onenet_data_ev,HTTP_RECEIVE_DATA_BIT,pdTRUE,pdFALSE,0);
    if(ev == HTTP_RECEIVE_DATA_BIT)
    {
        cJSON *root = cJSON_Parse((const char*)data_buff);
        if(root)
        {
            cJSON* code_js =  cJSON_GetObjectItem(root,"code");
            if(code_js && cJSON_GetNumberValue(code_js) == 0)
                ret = ESP_OK;
            cJSON_Delete(root);
        }
    }
    else
    {
        ESP_LOGI(TAG,"Upload version fail!");
        return ret;
    }
    return ret;
}


/**
 * 检查OTA升级任务
 * @param type 升级类型
 * @param version 当前版本号
 * @return 错误码
 */
esp_err_t  onenet_ota_check_task(const char* type,const char* version)
{
    char url[256];//创建缓冲区
    esp_err_t ret = ESP_FAIL;//默认返回失败
    snprintf(url,256,ONENET_OTA_URL"/%s/%s/check?type=%s&version=%s",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME,type,version);//拼接URL
    onenet_ota_http_connect(url,HTTP_METHOD_GET,NULL);//发起 HTTP GET 请求
    EventBits_t ev = xEventGroupWaitBits(onenet_data_ev,HTTP_RECEIVE_DATA_BIT,pdTRUE,pdFALSE,0);//等待接收数据事件,超时时间为0,如果没有收到数据,则直接返回,不阻塞
    if(ev == HTTP_RECEIVE_DATA_BIT)//如果收到数据,就解析JSON数据
    {
        cJSON *root =  cJSON_Parse((const char*)data_buff);//解析JSON数据
        if(root)//如果root不为空说明有有效数据    
        {
            cJSON* code_js =  cJSON_GetObjectItem(root,"code");
            cJSON *data_js = cJSON_GetObjectItem(root,"data");
            cJSON* target_js = cJSON_GetObjectItem(data_js,"target");
            cJSON* tid_js = cJSON_GetObjectItem(data_js,"tid");
            cJSON* size_js = cJSON_GetObjectItem(data_js,"size");
            cJSON* md5_js = cJSON_GetObjectItem(data_js,"md5");
            if(code_js && cJSON_GetNumberValue(code_js) == 0)//判断code是否为0
            {
                if(target_js && tid_js && size_js && md5_js)//判断是否有目标版本号、任务id、固件大小和MD5值
                {
                    snprintf(target_version,sizeof(target_version),"%s",cJSON_GetStringValue(target_js));//保存最新固件版本号
                    task_id = cJSON_GetNumberValue(tid_js);//保存任务id
                    firmware_size = cJSON_GetNumberValue(size_js);//保存固件大小
                    snprintf(firmware_md5, sizeof(firmware_md5), "%s", cJSON_GetStringValue(md5_js));//保存固件MD5值
                    ESP_LOGI(TAG, "Firmware info - target version: %s, task id: %d, size: %d bytes, md5: %s", target_version, task_id, firmware_size, firmware_md5);
                    ret = ESP_OK;
                }
            }
            else 
            {
                //code ≠ 0：表示请求失败或有错误，data字段为空
                ESP_LOGI(TAG,"Check ota task invaild code");
            }
            cJSON_Delete(root);//删除JSON根对象,释放内存（无论是否请求code成功，最后都要释放内存空间）
        }
        else//如果root为空说明没有有效数据
        {
            ESP_LOGI(TAG,"Check ota task fail!");
            return ret;
        }
    }
    else//如果没有收到数据,则直接返回,不阻塞
    {
        return ret;
    }
    return ret;
}


/**
 * 上报升级状态 
 * @param tid 任务id
 * @param step 升级状态
 * @return 错误码
 */
esp_err_t onenet_ota_upload_status(int tid,int step)
{
    char url[256];//创建缓冲区
    char payload[32];//创建缓冲区
    esp_err_t ret = ESP_FAIL;
    snprintf(url,256,ONENET_OTA_URL"/%s/%s/%d/status",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME,tid);//拼接URL
    snprintf(payload,sizeof(payload),"{\"step\":%d}",step);//格式化payload
    onenet_ota_http_connect(url,HTTP_METHOD_POST,payload);//发起 HTTP POST 请求
    EventBits_t ev = xEventGroupWaitBits(onenet_data_ev,HTTP_RECEIVE_DATA_BIT,pdTRUE,pdFALSE,0);//等待接收数据事件,超时时间为0,如果没有收到数据,则直接返回,不阻塞
    if(ev == HTTP_RECEIVE_DATA_BIT)//如果收到数据,就解析JSON数据
    {
        cJSON *root = cJSON_Parse((const char*)data_buff);//解析JSON数据
        if(root)//如果root不为空说明有有效数据
        {
            cJSON* code_js =  cJSON_GetObjectItem(root,"code");
            if(code_js && cJSON_GetNumberValue(code_js) == 0)//判断code是否为0，0表示请求成功
                ret = ESP_OK;
            cJSON_Delete(root);//删除JSON根对象,释放内存（无论是否请求code成功，最后都要释放内存空间）
        }
    }
    else//如果没有收到数据,则直接返回,不阻塞
    {
        ESP_LOGI(TAG,"Upload status fail!");
        return ret;
    }
    return ret;
}

/**
 * 初始化http请求，封装请求头，加static限制在本c文件中使用
 * @param http_client http客户端句柄
 * @return 错误码
 */
static esp_err_t http_ota_init_callback(esp_http_client_handle_t http_client)
{
    static char token[256];
    memset(token,0,256);
    dev_token_generate(token,SIG_METHOD_SHA256,TOKEN_TIMESTAMP,ONENET_PRODUCT_ID,NULL,ONENET_ACCESS_KEY);
    ESP_LOGI(TAG,"user token:%s",token);
    //设置发送请求 
    esp_http_client_set_method(http_client, HTTP_METHOD_GET);//设置请求方法为GET
    esp_http_client_set_header(http_client,"Content-Type","application/json");//设置Content-Type为application/json
    esp_http_client_set_header(http_client,"Authorization",token);//设置Authorization头为token
    esp_http_client_set_header(http_client,"host","iot-api.heclouds.com");//设置host头为iot-api.heclouds.com
    return ESP_OK;
}


/**
 * 下载ota固件
 * @param tid 任务id，在接收检测升级任务时获取
 * @return 错误码
 */
esp_err_t onenet_ota_download(int tid)
{
    esp_err_t ota_finish_err = ESP_OK;
    char url[256];//开辟256字节空间，用于存储下载url

    //格式化url，将产品id、设备名、任务id拼接起来
    snprintf(url,sizeof(url),ONENET_OTA_URL"/%s/%s/%d/download",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME,tid);

    //配置http客户端的url和证书捆绑
    esp_http_client_config_t config =
    {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,//配置证书捆绑，用于验证服务器证书
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,//配置http客户端的参数
        .http_client_init_cb = http_ota_init_callback,//配置http客户端初始化回调函数
    };

    
    ota_finish_err = esp_https_ota(&ota_config);//进行https ota升级
    if(ota_finish_err == ESP_OK)
    {
        ESP_LOGI(TAG, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");

    }
    else
    {
        ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err);
    }
    return ota_finish_err;
}






/**
 * 功能:从 OneNET 下载 STM32 固件 -> 写入 SPIFFS /spiffs/stm32_firmware.bin
 * @param tid 任务id，在接收检测升级任务时获取
 * @return ESP_OK 下载并保存成功
 */
esp_err_t onenet_ota_download_stm32_firmware(int tid)
{
    esp_err_t ret = ESP_OK;
    char url[512]; // 增加缓冲区大小以容纳完整的 URL
    esp_http_client_handle_t http_client = NULL;
    
    // 重置下载状态
    download_file_fd = -1;
    download_total_size = 0;
    
    // 打开或创建 SPIFFS 文件
    download_file_fd = open(FIRMWARE_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (download_file_fd < 0) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        ret = ESP_FAIL;
        goto cleanup;
    }
    
    // 初始化MD5上下文
    mbedtls_md5_init(&md5_ctx);
    mbedtls_md5_starts(&md5_ctx);
    
    // 构建下载 URL
    snprintf(url, sizeof(url), ONENET_OTA_URL"/%s/%s/%d/download", 
             ONENET_PRODUCT_ID, ONENET_DEVICE_NAME, tid);
   // ESP_LOGI(TAG, "Download URL: %s", url);

    
    // 生成认证 token（产品级别，不包含设备名称）
    char token[256];
    memset(token, 0, 256);
    dev_token_generate(token, SIG_METHOD_SHA256, TOKEN_TIMESTAMP, ONENET_PRODUCT_ID, NULL, ONENET_ACCESS_KEY);
    ESP_LOGI(TAG, "user token: %s", token);
    
    // 初始化 HTTP 客户端结构体配置
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_client_event_handler,
        .buffer_size = 10240, // 设置接收缓冲区大小为10KB，以10kb分片下载
    };
    
    // 初始化 HTTP 客户端
    http_client = esp_http_client_init(&config);
    if (!http_client) {
        ESP_LOGE(TAG, "HTTP client init failed");
        ret = ESP_FAIL;
        goto cleanup;
    }
    
    // 设置 HTTP 请求头
    esp_http_client_set_method(http_client, HTTP_METHOD_GET);
    esp_http_client_set_header(http_client, "Accept", "application/octet-stream");
    esp_http_client_set_header(http_client, "Authorization", token);
    esp_http_client_set_header(http_client, "Host", "iot-api.heclouds.com");
    esp_http_client_set_header(http_client, "User-Agent", "ESP32-OTA/1.0");
    

    
    // 执行 HTTP 请求，下载固件
    ret = esp_http_client_perform(http_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP client perform failed: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    
    // 检查 HTTP 状态码，200表示成功，206表示部分内容
    int status_code = esp_http_client_get_status_code(http_client);
    ESP_LOGI(TAG, "HTTP status code: %d", status_code);
    if (status_code != 200 && status_code != 206) {
        ESP_LOGE(TAG, "HTTP request failed with status code: %d", status_code);
        ret = ESP_FAIL;
        goto cleanup;
    }
    
    // 手动读取响应数据
    int content_length = esp_http_client_get_content_length(http_client);
    ESP_LOGI(TAG, "Content length: %d", content_length);
    
    // 读取并写入响应数据到文件
    if (content_length > 0) {
        char *buffer = (char *)malloc(10240);
        if (buffer) {
            int read_len = 0;
            while ((read_len = esp_http_client_read(http_client, buffer, 10240)) > 0) {
                ESP_LOGI(TAG, "Read %d bytes", read_len);
                if (download_file_fd >= 0) {
                    ssize_t write_len = write(download_file_fd, buffer, read_len);
                    if (write_len != read_len) {
                        ESP_LOGE(TAG, "Failed to write to file, written %d, expected %d", (int)write_len, read_len);
                    } else {
                        download_total_size += read_len;
                        ESP_LOGI(TAG, "Downloaded %zu bytes", download_total_size);
                    }
                }
            }
            free(buffer);
        }
    }
    
    ESP_LOGI(TAG, "HTTP request succeeded with status code: %d", status_code);
    
    // 计算最终的MD5值
    unsigned char md5_result[16];
    mbedtls_md5_finish(&md5_ctx, md5_result);
    mbedtls_md5_free(&md5_ctx);
    
    // 将MD5值转换为字符串
    char md5_str[33];
    for (int i = 0; i < 16; i++) {
        sprintf(&md5_str[i * 2], "%02x", md5_result[i]);
    }
    md5_str[32] = '\0';
    
    ESP_LOGI(TAG, "Downloaded file MD5: %s", md5_str);
    ESP_LOGI(TAG, "Expected MD5: %s", firmware_md5);
    
    // 验证下载是否成功
    if (download_total_size > 0 && download_total_size >= firmware_size) {
        // 验证MD5值
        if (strcmp(md5_str, firmware_md5) == 0) { //如果下载的固件MD5值与预期值相同
            ESP_LOGI(TAG, "STM32 firmware downloaded successfully, total size: %zu bytes", download_total_size);
            ESP_LOGI(TAG, "MD5 verification passed!");
            ret = ESP_OK;
        } else {//如果下载的固件MD5值与预期值不同
            ESP_LOGE(TAG, "MD5 verification failed!");
            ESP_LOGE(TAG, "Expected: %s, Got: %s", firmware_md5, md5_str);
            ret = ESP_FAIL;
        }
    } else {    //如果下载的固件大小小于预期值
        ESP_LOGE(TAG, "STM32 firmware download failed, total size: %zu bytes (expected: %d bytes)", download_total_size, firmware_size);
        ret = ESP_FAIL;
    }
    
cleanup:
    if (download_file_fd >= 0) {
        close(download_file_fd);
        download_file_fd = -1;
    }
    if (http_client != NULL) {
        esp_http_client_cleanup(http_client);
    }
    return ret;
}




/**
 * ota任务
 * @param 无
 * @return 无
 */
static void onenet_ota_task(void *param)
{

    esp_err_t ret;
    //need_send_firmware = true;//需要发送固件（测试用）
    //上报当前版本号

    /*注意:上报当前版本号时，需要根据实际情况修改，不能直接使用"1.0.2"，而应该根据实际情况修改
    后续STM32跳转到A区固件时，系统开始会发送当前版本号，用于判断是否需要升级。*/
   ret = onenet_ota_upload_version("1.0.2");
   if(ret != ESP_OK)
   {
       ESP_LOGE(TAG,"Upload version faild!");
       goto delete_ota_task;
   }

   //检测升级任务
   ret = onenet_ota_check_task("2","1.0.2");

   if(ret != ESP_OK)
   {
       ESP_LOGE(TAG,"Check ota task faild!");
       //goto delete_ota_task;
   }

   //上报任务升级状态
  ret = onenet_ota_upload_status(task_id,10);
  if(ret != ESP_OK)
  {
       //出现情况1：上报任务升级状态失败 或 平台下发通知固件升级任务不存在；
      ESP_LOGE(TAG,"upload status faild!");
      //goto delete_ota_task;
  }
  else
  {
   //情况2：上报任务升级状态成功，升级任务存在，则进行任务下载
      ESP_LOGI(TAG,"Check ota task success and start download firmware!");
      //下载固件
      ret = onenet_ota_download_stm32_firmware(task_id);
      if(ret != ESP_OK)
      {
          ESP_LOGE(TAG,"STM32 firmware download failed!");
          // 即使下载失败，也上报失败状态
          onenet_ota_upload_status(task_id, 107);//code 107表示下载失败，未知异常
          goto delete_ota_task;
      }
      else
      {
          ESP_LOGI(TAG,"Download firmware success!");
          // 上报下载成功状态
          onenet_ota_upload_status(task_id, 100);//code 100表示下载成功
          test_read_firmware();//测试读取固件，是否碎片化固件、
          need_send_firmware = true;//需要发送固件
      }
  }





    for(;;)
    {
        if(need_send_firmware)
        {
            set_firmware_info(firmware_size, target_version);
            //set_firmware_info(firmware_size, "v1.0.4");//测试用
            esp_err_t ret = serial_send_firmware(UART_NUM_1, FIRMWARE_FILE_PATH);
            if (ret == ESP_OK)
            {
                /* 固件传输成功，清除标志 */
                need_send_firmware = false;
                ESP_LOGI(TAG, "Firmware transmission success, task completed");
                goto delete_ota_task;
            }
            else
            {
                /* 固件传输失败，保持标志为true，下次循环重新尝试握手 */
                ESP_LOGW(TAG, "Firmware transmission failed, will retry...");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

delete_ota_task:/*等到所有固件传输完成，再删除任务，避免重复发送*/
    ota_is_running = false;
    vTaskDelete(NULL);
}


/**
 * 临时测试函数：读取并验证固件文件
 * 用于读取SPIFFS中的固件文件，处理数据碎片化问题
 */
void test_read_firmware(void)
{
    ESP_LOGI(TAG, "=== Starting firmware file test ===");
    
    // 打开固件文件
    int fd = open(FIRMWARE_FILE_PATH, O_RDONLY);
    if (fd < 0) {
        ESP_LOGE(TAG, "Failed to open firmware file");
        return;
    }
    
    // 使用lseek获取文件大小（SPIFFS的stat可能不准确）
    off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);  // 重新定位到文件开头
    
    ESP_LOGI(TAG, "Firmware file size: %d bytes", (int)file_size);
    
    if (file_size <= 0) {
        ESP_LOGW(TAG, "File is empty or size is 0");
        close(fd);
        return;
    }
    
    // 读取文件数据
    unsigned char buffer[1024];
    int total_read = 0;
    int valid_chunks = 0;
    int invalid_chunks = 0;
    int consecutive_invalid = 0;
    
    while (1) {
        int read_len = read(fd, buffer, sizeof(buffer));
        if (read_len <= 0) {
            break;
        }
        
        total_read += read_len;
        
        // 检查数据块是否有效（非全0xFF）
        int is_valid = 0;
        for (int i = 0; i < read_len; i++) {
            if (buffer[i] != 0xFF) {
                is_valid = 1;
                break;
            }
        }
        
        if (is_valid) {
            valid_chunks++;
            consecutive_invalid = 0;
        } else {
            invalid_chunks++;
            consecutive_invalid++;
            
            // 如果连续出现多个无效块，可能是碎片化区域
            if (consecutive_invalid >= 3) {
                ESP_LOGW(TAG, "Detected fragmented data at offset %d", total_read - read_len);
            }
        }
    }
    
    ESP_LOGI(TAG, "=== Firmware file test results ===");
    ESP_LOGI(TAG, "Total bytes read: %d", total_read);
    ESP_LOGI(TAG, "Valid chunks: %d", valid_chunks);
    ESP_LOGI(TAG, "Invalid chunks (all 0xFF): %d", invalid_chunks);
    ESP_LOGI(TAG, "Fragmentation detected: %s", (invalid_chunks > valid_chunks / 4) ? "Yes" : "No");
    
    // 读取并打印前64字节
    lseek(fd, 0, SEEK_SET);
    read(fd, buffer, 64);
    ESP_LOGI(TAG, "=== First 64 bytes ===");
    for (int i = 0; i < 64; i++) {
        printf("%02x ", buffer[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    
    // 读取并打印后64字节
    if (file_size > 64) {
        lseek(fd, file_size - 64, SEEK_SET);
        read(fd, buffer, 64);
        ESP_LOGI(TAG, "=== Last 64 bytes ===");
        for (int i = 0; i < 64; i++) {
            printf("%02x ", buffer[i]);
            if ((i + 1) % 16 == 0) printf("\n");
        }
    }
    
    close(fd);
    ESP_LOGI(TAG, "=== Firmware file test completed ===");
}

/**
 * 启动onenet ota升级流程
 * @param 无
 * @return 无
 */
void onenet_ota_start(void)
{
    if(!onenet_sem)
        onenet_sem = xSemaphoreCreateBinary();
    if(ota_is_running)
        return;
    ota_is_running = true;
    ESP_LOGI(TAG,"Start OTA");
    
    // 挂载SPIFFS文件系统（保持挂载状态,不再卸载）
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = false
    };
    
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS: %s", esp_err_to_name(ret));
        ota_is_running = false;
        return;
    }
    
    ESP_LOGI(TAG, "SPIFFS mounted successfully");
    
    xTaskCreatePinnedToCore(onenet_ota_task,"onenet_ota",8192,NULL,2,NULL,1);
}




