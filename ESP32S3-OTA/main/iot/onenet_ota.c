#include "onenet_ota.h"
#include "freertos/idf_additions.h"
#include "ota_manager.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>



#define TAG     "onenet_ota"

#define     MAX_DATA_BUFF   1024    //http接收数据缓存大小
#define     FIRMWARE_FILE_PATH   "/spiffs/stm32_firmware.bin"//SPIFFS文件系统中固件文件路径

//ota基础url
#define     ONENET_OTA_URL  "http://iot-api.heclouds.com/fuse-ota"
//token合法时间戳
#define     TOKEN_TIMESTAMP     2074855187
#define STM32_VERSION_CONFIRM_TIMEOUT_MS    30000U
#define STM32_INITIAL_VERSION_WAIT_MS        30000U
#define STM32_VERSION_QUERY_RETRY_COUNT          3U
#define STM32_VERSION_QUERY_INTERVAL_MS        500U
#define STM32_OTA_VERSION_TEXT_LENGTH       8U
#define OTA_CLOUD_RETRY_COUNT               3U
#define OTA_CLOUD_RETRY_DELAY_MS            2000U
#define ONENET_OTA_CODE_TASK_FINISHED       20

typedef struct
{
    int task_id;
    char target_version[16];
    int firmware_size;
    char firmware_md5[64];
    uint32_t firmware_crc32;
} onenet_ota_job_t;

//接收到的http 数据
static uint8_t data_buff[MAX_DATA_BUFF];
//接收到的http数据长度
static size_t   data_buff_len = 0;
static bool data_buff_overflow = false;

// 下载相关的上下文数据
static int download_file_fd = -1;
static size_t download_total_size = 0;
static mbedtls_md5_context md5_ctx;
static uint32_t g_dl_crc_running;   // 下载过程中流式累加的CRC32中间值（未做最终异或）

static void onenet_ota_run_once(void);
static esp_err_t onenet_ota_mount_spiffs_once(void);
static esp_err_t onenet_ota_prepare_job(const char *current_version_text,
                                        onenet_ota_job_t *job,
                                        stm32_version_state_t *target_semver);
static esp_err_t onenet_ota_download_job(onenet_ota_job_t *job);

/**
 * @brief 挂载OTA固件使用的SPIFFS分区。
 * @param 无。
 * @return ESP_OK表示已挂载或本次挂载成功，其他值表示挂载失败。
 * @note 文件系统生命周期与单轮OTA解耦，设备运行期间保持挂载且不重复注册。
 */
static esp_err_t onenet_ota_mount_spiffs_once(void)
{
    const esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = false
    };
    esp_err_t ret;

    if (esp_spiffs_mounted(conf.partition_label))
    {
        return ESP_OK;
    }

    ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SPIFFS挂载失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "SPIFFS mounted once for OTA manager");
    return ESP_OK;
}

/**
 * @brief 初始化STM32版本同步状态、SPIFFS和常驻OTA Manager。（已加入app测试阶段关闭）
 * @param 无。
 * @return ESP_OK表示初始化成功，其他值表示资源初始化失败。
 * @note 在UART任务创建前调用；重复调用不会清除版本、重复挂载或重复创建任务。
 */
esp_err_t onenet_ota_init(void)
{
#if ONENET_OTA_DISABLED
    ESP_LOGI(TAG, "[OTA已停用] 调试模式，跳过OTA Manager初始化、SPIFFS挂载和任务创建");
    return ESP_OK;
#else
    esp_err_t ret;

    ret = onenet_ota_mount_spiffs_once();
    if (ret != ESP_OK)
    {
        return ret;
    }

    return ota_manager_init(onenet_ota_run_once, onenet_is_connected);
#endif
}

/**
 * @brief 缓存STM32 APP上报的版本并唤醒OTA任务。（已加入app测试阶段关闭）
 * @param major 主版本号。
 * @param minor 次版本号。
 * @param patch 修订号。
 * @return ESP_OK表示缓存成功，ESP_ERR_INVALID_STATE表示尚未初始化。
 * @note UART任务只写本地状态，不在此处执行HTTP请求。
 */
esp_err_t onenet_ota_notify_stm32_version(uint8_t major,
                                          uint8_t minor,
                                          uint8_t patch)
{
#if ONENET_OTA_DISABLED
    /* 停用后静默丢弃版本报告，返回OK避免UART任务打印无效状态日志 */
    return ESP_OK;
#else
    return ota_manager_notify_stm32_version(major, minor, patch);
#endif
}


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
                        // 流式累加固件整体CRC32（与MD5同一段数据，零额外IO）
                        g_dl_crc_running = crc32_stream_update(g_dl_crc_running, (const uint8_t *)evt->data, evt->data_len);
                        ESP_LOGI(TAG, "Downloaded %zu bytes", download_total_size);
                    }
                } else {
                    // 否则，将数据复制到缓冲区
                    if ((size_t)evt->data_len >
                        (MAX_DATA_BUFF - 1U - data_buff_len))
                    {
                        copy_len = MAX_DATA_BUFF - 1U - data_buff_len;
                        data_buff_overflow = true;
                    }
                    else
                    {
                        copy_len = evt->data_len;
                    }
                    memcpy(&data_buff[data_buff_len],evt->data,copy_len);
                    data_buff_len += copy_len;
                    data_buff[data_buff_len] = '\0';
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
        .timeout_ms = 10000,//超时时间10秒
    };
    //初始化结构体
    esp_http_client_handle_t http_client = esp_http_client_init(&config);	//初始化http连接
    if(!http_client)
    {
        ESP_LOGI(TAG,"http_client init fail!");
        return ESP_FAIL;
    }

    char* token = (char*)malloc(256);
    if (token == NULL)
    {
        esp_http_client_cleanup(http_client);
        return ESP_ERR_NO_MEM;
    }
    memset(token,0,256);
    dev_token_generate(token,SIG_METHOD_SHA256,TOKEN_TIMESTAMP,ONENET_PRODUCT_ID,NULL,ONENET_ACCESS_KEY);//设备ID填NULL因为用的是产品id
    ESP_LOGI(TAG,"OneNET HTTP authorization token generated");
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
    data_buff_overflow = false;
    memset(data_buff,0,sizeof(data_buff));//清空接收数据缓存
    esp_err_t err  = esp_http_client_perform(http_client);//执行http连接
    if ((err == ESP_OK) && data_buff_overflow)
    {
        ESP_LOGE(TAG, "OneNET HTTP JSON响应超过%u字节",
                 (unsigned int)(MAX_DATA_BUFF - 1U));
        err = ESP_ERR_INVALID_SIZE;
    }
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
    char version_info[128];
    char url[256];
    esp_err_t ret = ESP_FAIL;
    snprintf(version_info,sizeof(version_info),"{\"s_version\":\"%s\", \"f_version\": \"%s\"}",version,version);
    snprintf(url,256,ONENET_OTA_URL"/%s/%s/version",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME);
    ret = onenet_ota_http_connect(url,HTTP_METHOD_POST,version_info);
    if (ret != ESP_OK)
        return ret;
    ret = ESP_FAIL;
    if(data_buff_len > 0U)
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
static esp_err_t onenet_ota_check_task(const char *type,
                                       const char *version,
                                       onenet_ota_job_t *job)
{
    char url[256];//创建缓冲区
    esp_err_t ret = ESP_FAIL;//默认返回失败
    if (job == NULL)
        return ESP_ERR_INVALID_ARG;
    snprintf(url,256,ONENET_OTA_URL"/%s/%s/check?type=%s&version=%s",ONENET_PRODUCT_ID,ONENET_DEVICE_NAME,type,version);//拼接URL
    ret = onenet_ota_http_connect(url,HTTP_METHOD_GET,NULL);//发起 HTTP GET 请求
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Check OTA HTTP请求失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = ESP_FAIL;
    if(data_buff_len > 0U)//如果收到数据,就解析JSON数据
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
                if(cJSON_IsString(target_js) &&
                   cJSON_IsNumber(tid_js) &&
                   cJSON_IsNumber(size_js) &&
                   cJSON_IsString(md5_js) &&
                   (cJSON_GetNumberValue(size_js) > 0))
                {
                    snprintf(job->target_version,
                             sizeof(job->target_version),
                             "%s",
                             cJSON_GetStringValue(target_js));
                    job->task_id = (int)cJSON_GetNumberValue(tid_js);
                    job->firmware_size = (int)cJSON_GetNumberValue(size_js);
                    snprintf(job->firmware_md5,
                             sizeof(job->firmware_md5),
                             "%s",
                             cJSON_GetStringValue(md5_js));
                    ESP_LOGI(TAG,
                             "Firmware info - target version: %s, task id: %d, size: %d bytes, md5: %s",
                             job->target_version,
                             job->task_id,
                             job->firmware_size,
                             job->firmware_md5);
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
    ret = onenet_ota_http_connect(url,HTTP_METHOD_POST,payload);//发起 HTTP POST 请求
    if (ret != ESP_OK)
        return ret;
    ret = ESP_FAIL;
    if(data_buff_len > 0U)//如果收到数据,就解析JSON数据
    {
        cJSON *root = cJSON_Parse((const char*)data_buff);//解析JSON数据
        if(root)//如果root不为空说明有有效数据
        {
            cJSON* code_js =  cJSON_GetObjectItem(root,"code");
            if(code_js)
            {
                int response_code = (int)cJSON_GetNumberValue(code_js);

                if(response_code == 0)//code为0表示本次请求成功
                {
                    ret = ESP_OK;
                }
                else if((step == 201) &&
                        (response_code == ONENET_OTA_CODE_TASK_FINISHED))
                {
                    /* step 201可能已被平台处理成功但响应丢失；重试时任务已结束，按幂等成功收尾。 */
                    ESP_LOGI(TAG, "OneNET任务已完成(code=20)，step 201按幂等成功处理");
                    ret = ESP_OK;
                }
            }
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
    ESP_LOGI(TAG,"OneNET OTA download token generated");
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
static esp_err_t onenet_ota_download_stm32_firmware(onenet_ota_job_t *job)
{
    esp_err_t ret = ESP_OK;
    char url[512]; // 增加缓冲区大小以容纳完整的 URL
    esp_http_client_handle_t http_client = NULL;
    
    if ((job == NULL) || (job->task_id <= 0) ||
        (job->firmware_size <= 0) || (job->firmware_md5[0] == '\0'))
    {
        return ESP_ERR_INVALID_ARG;
    }

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
    // 初始化固件整体CRC32流式累加器
    g_dl_crc_running = crc32_stream_init();
    
    // 构建下载 URL
    snprintf(url, sizeof(url), ONENET_OTA_URL"/%s/%s/%d/download", 
             ONENET_PRODUCT_ID, ONENET_DEVICE_NAME, job->task_id);
   // ESP_LOGI(TAG, "Download URL: %s", url);

    
    // 生成认证 token（产品级别，不包含设备名称）
    char token[256];
    memset(token, 0, 256);
    dev_token_generate(token, SIG_METHOD_SHA256, TOKEN_TIMESTAMP, ONENET_PRODUCT_ID, NULL, ONENET_ACCESS_KEY);
    ESP_LOGI(TAG, "OneNET firmware download authorization token generated");
    
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
    
    // 计算最终的固件整体CRC32（做最终异或）
    job->firmware_crc32 = crc32_stream_final(g_dl_crc_running);
    ESP_LOGI(TAG, "Downloaded file CRC32: 0x%08X",
             (unsigned int)job->firmware_crc32);
    
    // 将MD5值转换为字符串
    char md5_str[33];
    for (int i = 0; i < 16; i++) {
        sprintf(&md5_str[i * 2], "%02x", md5_result[i]);
    }
    md5_str[32] = '\0';
    
    ESP_LOGI(TAG, "Downloaded file MD5: %s", md5_str);
    ESP_LOGI(TAG, "Expected MD5: %s", job->firmware_md5);
    
    // 验证下载是否成功
    if (download_total_size > 0 &&
        download_total_size >= (size_t)job->firmware_size) {
        // 验证MD5值
        if (strcmp(md5_str, job->firmware_md5) == 0) { //如果下载的固件MD5值与预期值相同
            ESP_LOGI(TAG, "STM32 firmware downloaded successfully, total size: %zu bytes", download_total_size);
            ESP_LOGI(TAG, "MD5 verification passed!");
            ret = ESP_OK;
        } else {//如果下载的固件MD5值与预期值不同
            ESP_LOGE(TAG, "MD5 verification failed!");
            ESP_LOGE(TAG, "Expected: %s, Got: %s", job->firmware_md5, md5_str);
            ret = ESP_FAIL;
        }
    } else {    //如果下载的固件大小小于预期值
        ESP_LOGE(TAG,
                 "STM32 firmware download failed, total size: %zu bytes (expected: %d bytes)",
                 download_total_size,
                 job->firmware_size);
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
 * @brief 读取当前缓存的STM32版本快照。
 * @param version 输出版本状态。
 * @return true表示已有有效版本，false表示APP尚未上报。
 * @note 临界区只复制少量标量，不包含日志、等待或网络操作。
 */
static bool onenet_ota_get_stm32_version(stm32_version_state_t *version)
{
    return ota_manager_get_stm32_version(version);
}

/**
 * @brief 等待指定报告序号之后的新STM32版本。
 * @param after_generation 只接受大于该序号的报告；传0表示等待首个版本。
 * @param timeout_ticks 最长等待时间，单位为FreeRTOS tick。
 * @param version 输出收到的版本状态。
 * @return true表示收到新版本，false表示超时或版本同步未初始化。
 * @note 使用总超时而不是每次事件重新计时，重复旧事件不会无限延长等待。
 */
static bool onenet_ota_wait_stm32_version(uint32_t after_generation,
                                          TickType_t timeout_ticks,
                                          stm32_version_state_t *version)
{
    return ota_manager_wait_stm32_version(after_generation,
                                          timeout_ticks,
                                          version);
}

/**
 * @brief 主动查询并等待STM32 APP返回本轮最新版本。
 * @param timeout_ticks 查询与等待共用的总超时时间，单位为FreeRTOS tick。
 * @param version 输出本轮查询之后收到的版本状态。
 * @return true表示收到新版本报告，false表示总超时或查询链路不可用。
 * @note 每次Manager检查都要求generation递增，避免ESP32重启或STM32状态变化后误用旧缓存。
 */
static bool onenet_ota_query_current_stm32_version(TickType_t timeout_ticks,
                                                   stm32_version_state_t *version)
{
    stm32_version_state_t cached_version;
    uint32_t after_generation = 0U;
    TickType_t start_tick;
    TickType_t elapsed_ticks;
    TickType_t remaining_ticks;
    TickType_t query_wait_ticks;
    unsigned int attempt;

    if (version == NULL)
    {
        return false;
    }

    /* 只接受查询开始后的新报告；缓存仅用于取得序号，不作为本轮真实版本直接使用。 */
    if (onenet_ota_get_stm32_version(&cached_version))
    {
        after_generation = cached_version.generation;
    }

    start_tick = xTaskGetTickCount();
    for (attempt = 1U; attempt <= STM32_VERSION_QUERY_RETRY_COUNT; attempt++)
    {
        if (send_version_request(UART_NUM_1) != ESP_OK)
        {
            ESP_LOGW(TAG, "STM32版本查询发送失败(%u/%u)",
                     attempt,
                     (unsigned int)STM32_VERSION_QUERY_RETRY_COUNT);
        }

        elapsed_ticks = xTaskGetTickCount() - start_tick;
        if (elapsed_ticks >= timeout_ticks)
        {
            return false;
        }

        remaining_ticks = timeout_ticks - elapsed_ticks;
        query_wait_ticks = pdMS_TO_TICKS(STM32_VERSION_QUERY_INTERVAL_MS);
        if (query_wait_ticks > remaining_ticks)
        {
            query_wait_ticks = remaining_ticks;
        }

        if (onenet_ota_wait_stm32_version(after_generation,
                                           query_wait_ticks,
                                           version))
        {
            return true;
        }
    }

    /* 快速查询结束后保留剩余总窗口，兼容STM32启动较慢或正在处理已有业务帧。 */
    elapsed_ticks = xTaskGetTickCount() - start_tick;
    if (elapsed_ticks >= timeout_ticks)
    {
        return false;
    }

    return onenet_ota_wait_stm32_version(after_generation,
                                          timeout_ticks - elapsed_ticks,
                                          version);
}

/**
 * @brief 将三段数值版本格式化为OneNET使用的X.X.X字符串。
 * @param version 数值版本。
 * @param text 输出字符串缓冲区。
 * @param text_size 输出缓冲区大小。
 * @return 无。
 */
static void onenet_ota_format_version(const stm32_version_state_t *version,
                                      char *text,
                                      size_t text_size)
{
    snprintf(text, text_size, "%u.%u.%u",
             (unsigned int)version->major,
             (unsigned int)version->minor,
             (unsigned int)version->patch);
}

/**
 * @brief 解析OneNET目标版本字符串。
 * @param text 目标版本，接受X.X.X或带v/V前缀的形式。
 * @param version 输出数值版本。
 * @return true表示格式有效，false表示格式错误或任一段超过255。
 * @note 固定8字节STM32协议只能可靠表达三段uint8_t版本号。
 */
static bool onenet_ota_parse_version(const char *text,
                                     stm32_version_state_t *version)
{
    uint16_t values[3] = {0U, 0U, 0U};
    uint8_t part;
    uint8_t digit_count;
    size_t index = 0U;

    if ((text == NULL) || (version == NULL))
    {
        return false;
    }

    if ((text[0] == '\0') ||
        (strlen(text) > STM32_OTA_VERSION_TEXT_LENGTH))
    {
        /* Bootloader信息帧中的版本字段固定为8字节，禁止静默截断。 */
        return false;
    }

    if ((text[index] == 'v') || (text[index] == 'V'))
    {
        index++;
    }

    for (part = 0U; part < 3U; part++)
    {
        digit_count = 0U;
        while ((text[index] >= '0') && (text[index] <= '9'))
        {
            values[part] = (uint16_t)(values[part] * 10U +
                                      (uint16_t)(text[index] - '0'));
            if (values[part] > 255U)
            {
                return false;
            }
            index++;
            digit_count++;
        }

        if (digit_count == 0U)
        {
            return false;
        }

        if (part < 2U)
        {
            if (text[index] != '.')
            {
                return false;
            }
            index++;
        }
    }

    if (text[index] != '\0')
    {
        return false;
    }

    memset(version, 0, sizeof(*version));
    version->major = (uint8_t)values[0];
    version->minor = (uint8_t)values[1];
    version->patch = (uint8_t)values[2];
    version->valid = true;
    return true;
}

/**
 * @brief 比较两个数值版本是否一致。
 * @param left 左侧版本。
 * @param right 右侧版本。
 * @return true表示三段版本完全相同。
 */
static bool onenet_ota_version_equal(const stm32_version_state_t *left,
                                     const stm32_version_state_t *right)
{
    return (left->major == right->major) &&
           (left->minor == right->minor) &&
           (left->patch == right->patch);
}

/**
 * @brief 有界重试OneNET版本上报。
 * @param version 版本字符串。
 * @return ESP_OK表示平台确认成功，否则返回最后一次错误。
 */
static esp_err_t onenet_ota_upload_version_with_retry(const char *version)
{
    esp_err_t ret = ESP_FAIL;

    for (uint8_t attempt = 0U; attempt < OTA_CLOUD_RETRY_COUNT; attempt++)
    {
        ret = onenet_ota_upload_version(version);
        if (ret == ESP_OK)
        {
            return ESP_OK;
        }

        if ((attempt + 1U) < OTA_CLOUD_RETRY_COUNT)
        {
            ESP_LOGW(TAG, "版本上报失败，%u ms后重试(%u/%u)",
                     (unsigned int)OTA_CLOUD_RETRY_DELAY_MS,
                     (unsigned int)(attempt + 1U),
                     (unsigned int)OTA_CLOUD_RETRY_COUNT);
            vTaskDelay(pdMS_TO_TICKS(OTA_CLOUD_RETRY_DELAY_MS));
        }
    }

    return ret;
}

/**
 * @brief 有界重试OneNET任务状态上报。
 * @param tid OneNET OTA任务编号。
 * @param step OneNET状态码。
 * @return ESP_OK表示平台确认成功，否则返回最后一次错误。
 */
static esp_err_t onenet_ota_upload_status_with_retry(int tid, int step)
{
    esp_err_t ret = ESP_FAIL;

    for (uint8_t attempt = 0U; attempt < OTA_CLOUD_RETRY_COUNT; attempt++)
    {
        ret = onenet_ota_upload_status(tid, step);
        if (ret == ESP_OK)
        {
            return ESP_OK;
        }

        if ((attempt + 1U) < OTA_CLOUD_RETRY_COUNT)
        {
            ESP_LOGW(TAG, "状态step=%d上报失败，%u ms后重试(%u/%u)",
                     step,
                     (unsigned int)OTA_CLOUD_RETRY_DELAY_MS,
                     (unsigned int)(attempt + 1U),
                     (unsigned int)OTA_CLOUD_RETRY_COUNT);
            vTaskDelay(pdMS_TO_TICKS(OTA_CLOUD_RETRY_DELAY_MS));
        }
    }

    return ret;
}

/**
 * @brief 在串口传输已决定终止后，等待APP给出最终运行版本并收尾OneNET任务。
 * @param tid OneNET OTA任务编号。
 * @param after_generation 传输前的版本报告序号。
 * @param target_version_state 本次OTA目标版本。
 * @return true表示目标APP最终运行，false表示仍应按失败结束任务。
 * @note 等待期间不再发送旧OTA命令；可覆盖回滚完成和升级结束ACK丢失两种情况。
 */
static bool onenet_ota_finish_terminal_transfer(int tid,
                                                uint32_t after_generation,
                                                const stm32_version_state_t *target_version_state)
{
    stm32_version_state_t actual_version;
    char actual_version_text[16];
    TickType_t start_tick;
    TickType_t timeout_ticks = pdMS_TO_TICKS(STM32_VERSION_CONFIRM_TIMEOUT_MS);
    TickType_t elapsed_ticks;
    uint32_t observed_generation = after_generation;
    bool received_version = false;
    bool target_running = false;

    ESP_LOGI(TAG, "等待STM32恢复并上报最终APP版本，超时=%u ms",
             (unsigned int)STM32_VERSION_CONFIRM_TIMEOUT_MS);

    start_tick = xTaskGetTickCount();
    for (;;)
    {
        elapsed_ticks = xTaskGetTickCount() - start_tick;
        if (elapsed_ticks >= timeout_ticks)
        {
            break;
        }

        if (!onenet_ota_wait_stm32_version(observed_generation,
                                           timeout_ticks - elapsed_ticks,
                                           &actual_version))
        {
            break;
        }

        received_version = true;
        observed_generation = actual_version.generation;
        if (onenet_ota_version_equal(&actual_version, target_version_state))
        {
            target_running = true;
            break;
        }

        /* 首个旧版本可能是升级前ACK丢失造成的重报，继续等到总超时。 */
        onenet_ota_format_version(&actual_version,
                                  actual_version_text,
                                  sizeof(actual_version_text));
        ESP_LOGW(TAG, "收到最终候选版本%s，继续等待目标APP",
                 actual_version_text);
    }

    if (received_version)
    {
        onenet_ota_format_version(&actual_version,
                                  actual_version_text,
                                  sizeof(actual_version_text));

        ESP_LOGI(TAG, "STM32最终运行APP版本: %s", actual_version_text);
        if (target_running)
        {
            /* step 201本身会把平台版本更新为任务目标版本，不再提前POST /version结束任务。 */
            ESP_LOGW(TAG, "串口收尾失败但目标APP已运行，按版本证据上报step 201");
            (void)onenet_ota_upload_status_with_retry(tid, 201);
            return true;
        }

        if (onenet_ota_upload_version_with_retry(actual_version_text) != ESP_OK)
        {
            /* 回滚旧版本仍需先把平台版本同步为实际值，再上报本次任务失败。 */
            ESP_LOGE(TAG, "最终APP版本已收到，但OneNET版本同步失败");
        }
    }
    else
    {
        ESP_LOGE(TAG, "未收到恢复后的APP版本报告");
    }

    (void)onenet_ota_upload_status_with_retry(tid, 206);
    return false;
}

/**
 * @brief 同步当前版本并从OneNET准备一项可执行的OTA任务。
 * @param current_version_text STM32当前版本字符串。
 * @param job 输出任务信息。
 * @param target_semver 输出解析后的目标版本。
 * @return ESP_OK表示任务可执行，其他值表示本轮应停止。
 * @note 该阶段统一处理版本同步、任务检查、目标版本校验和step 10上报。
 */
static esp_err_t onenet_ota_prepare_job(const char *current_version_text,
                                        onenet_ota_job_t *job,
                                        stm32_version_state_t *target_semver)
{
    esp_err_t ret;

    if ((current_version_text == NULL) || (job == NULL) ||
        (target_semver == NULL))
    {
        return ESP_ERR_INVALID_ARG;
    }

    ota_manager_set_state(OTA_MANAGER_STATE_SYNC_VERSION);
    ret = onenet_ota_upload_version_with_retry(current_version_text);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "当前版本上报失败，不使用硬编码版本继续OTA");
        return ret;
    }

    memset(job, 0, sizeof(*job));
    ota_manager_set_state(OTA_MANAGER_STATE_CHECK_TASK);
    ret = onenet_ota_check_task("2", current_version_text, job);
    if (ret != ESP_OK)
    {
        ESP_LOGI(TAG, "当前没有可执行的STM32 OTA任务，结束本轮检查");
        return ret;
    }

    if (!onenet_ota_parse_version(job->target_version, target_semver))
    {
        ESP_LOGE(TAG, "OneNET目标版本格式不支持: %s", job->target_version);
        (void)onenet_ota_upload_status_with_retry(job->task_id, 206);
        return ESP_FAIL;
    }

    ret = onenet_ota_upload_status_with_retry(job->task_id, 10);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "OTA开始状态上报失败，停止下载");
    }
    return ret;
}

/**
 * @brief 下载并校验一项STM32固件任务。
 * @param job OTA任务信息，成功后补充固件CRC32。
 * @return ESP_OK表示固件已准备好，其他值表示下载或校验失败。
 */
static esp_err_t onenet_ota_download_job(onenet_ota_job_t *job)
{
    esp_err_t ret;

    if (job == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    ota_manager_set_state(OTA_MANAGER_STATE_DOWNLOAD);
    ESP_LOGI(TAG, "发现OTA任务，开始下载STM32固件");
    ret = onenet_ota_download_stm32_firmware(job);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "STM32固件下载失败");
        (void)onenet_ota_upload_status_with_retry(job->task_id, 107);
        return ret;
    }

    ESP_LOGI(TAG, "STM32固件下载成功");
    test_read_firmware();
    return ESP_OK;
}

/**
 * @brief 将已校验固件发送给STM32并确认最终APP版本。
 * @param job 当前OTA任务。
 * @param target_semver 已解析的目标版本。
 * @return ESP_OK表示目标APP已运行，其他值表示传输或版本确认失败。
 * @note 串口重试、终止等待和OneNET最终状态上报统一收口在本阶段。
 */
static esp_err_t onenet_ota_transfer_job(const onenet_ota_job_t *job,
                                         const stm32_version_state_t *target_semver)
{
    int ota_total_retry = 0;
    bool stm32_in_bootloader = false;
    bool received_post_upgrade_version;
    bool target_version_confirmed;
    stm32_version_state_t reported_version;
    char reported_version_text[16];
    uint32_t version_generation_before_transfer = 0U;
    uint32_t observed_generation;
    TickType_t confirm_start_tick;
    TickType_t confirm_timeout_ticks = pdMS_TO_TICKS(STM32_VERSION_CONFIRM_TIMEOUT_MS);
    TickType_t elapsed_ticks;
    esp_err_t ret;

    if ((job == NULL) || (target_semver == NULL))
    {
        return ESP_ERR_INVALID_ARG;
    }

    for (;;)
    {
        if (ota_total_retry >= 3)
        {
            ESP_LOGE(TAG, "OTA传输重试%d次后仍失败", ota_total_retry);
            ota_manager_set_state(OTA_MANAGER_STATE_REPORT_RESULT);
            return onenet_ota_finish_terminal_transfer(job->task_id,
                                                       version_generation_before_transfer,
                                                       target_semver) ? ESP_OK : ESP_FAIL;
        }
        ota_total_retry++;

        /* 记录传输前最后一次APP报告；后续只接受更大的报告序号。 */
        if (onenet_ota_get_stm32_version(&reported_version))
        {
            version_generation_before_transfer = reported_version.generation;
        }

        set_firmware_info(job->firmware_size,
                          job->target_version,
                          job->firmware_crc32);
        ota_manager_set_state(OTA_MANAGER_STATE_UART_TRANSFER);
        ret = serial_send_firmware(UART_NUM_1,
                                   FIRMWARE_FILE_PATH,
                                   stm32_in_bootloader);
        if (ret == ESP_OK)
        {
            received_post_upgrade_version = false;
            target_version_confirmed = false;
            observed_generation = version_generation_before_transfer;
            confirm_start_tick = xTaskGetTickCount();

            ota_manager_set_state(OTA_MANAGER_STATE_WAIT_NEW_VERSION);
            ESP_LOGI(TAG, "固件传输完成，等待新版APP上报版本，超时=%u ms",
                     (unsigned int)STM32_VERSION_CONFIRM_TIMEOUT_MS);

            for (;;)
            {
                elapsed_ticks = xTaskGetTickCount() - confirm_start_tick;
                if (elapsed_ticks >= confirm_timeout_ticks)
                {
                    break;
                }

                if (!onenet_ota_wait_stm32_version(observed_generation,
                                                   confirm_timeout_ticks - elapsed_ticks,
                                                   &reported_version))
                {
                    break;
                }

                received_post_upgrade_version = true;
                observed_generation = reported_version.generation;
                onenet_ota_format_version(&reported_version,
                                          reported_version_text,
                                          sizeof(reported_version_text));

                if (onenet_ota_version_equal(&reported_version, target_semver))
                {
                    target_version_confirmed = true;
                    break;
                }

                ESP_LOGW(TAG, "收到APP版本%s，与目标%s不一致，继续等待",
                         reported_version_text,
                         job->target_version);
            }

            ota_manager_set_state(OTA_MANAGER_STATE_REPORT_RESULT);
            if (target_version_confirmed)
            {
                ESP_LOGI(TAG, "新版APP已启动并确认目标版本: %s",
                         reported_version_text);
                if (onenet_ota_upload_status_with_retry(job->task_id, 201) != ESP_OK)
                {
                    ESP_LOGE(TAG, "新版已确认，但OneNET step 201上报失败");
                }
                return ESP_OK;
            }

            if (received_post_upgrade_version)
            {
                onenet_ota_format_version(&reported_version,
                                          reported_version_text,
                                          sizeof(reported_version_text));
                ESP_LOGE(TAG, "APP最终版本%s与目标%s不一致，本次升级失败",
                         reported_version_text,
                         job->target_version);
                (void)onenet_ota_upload_version_with_retry(reported_version_text);
            }
            else
            {
                ESP_LOGE(TAG, "30秒内未收到新版APP版本报告，本次升级失败");
            }

            (void)onenet_ota_upload_status_with_retry(job->task_id, 206);
            return ESP_FAIL;
        }

        /* START_TRANSFER在单次窗口内无回应时，不再发送旧会话命令。 */
        if (g_start_transmission_timeout != 0U)
        {
            ESP_LOGE(TAG, "STM32在备份/擦除窗口内无回应，结束旧OTA会话");
            ota_manager_set_state(OTA_MANAGER_STATE_REPORT_RESULT);
            return onenet_ota_finish_terminal_transfer(job->task_id,
                                                       version_generation_before_transfer,
                                                       target_semver) ? ESP_OK : ESP_FAIL;
        }

        if (g_last_nack_err == NACK_ERR_FLASH_ERROR ||
            g_last_nack_err == NACK_ERR_FW_CRC_FAIL ||
            g_last_nack_err == NACK_ERR_PREVIOUS_ROLLBACK)
        {
            ESP_LOGE(TAG, "收到不可恢复NACK=0x%02X，立即结束", g_last_nack_err);
            ota_manager_set_state(OTA_MANAGER_STATE_REPORT_RESULT);
            return onenet_ota_finish_terminal_transfer(job->task_id,
                                                       version_generation_before_transfer,
                                                       target_semver) ? ESP_OK : ESP_FAIL;
        }

        stm32_in_bootloader = true;
        ESP_LOGW(TAG, "固件传输失败，准备重试(%d/3)，last NACK=0x%02X",
                 ota_total_retry,
                 g_last_nack_err);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}




/**
 * @brief 执行一轮完整的OneNET检查与STM32升级流程。
 * @param 无。
 * @return 无。
 * @note 仅由常驻OTA Manager调用；函数返回后Manager重新进入IDLE并处理pending请求。
 */
static void onenet_ota_run_once(void)
{
    esp_err_t ret;
    stm32_version_state_t current_version;
    stm32_version_state_t target_semver;
    char current_version_text[16];
    onenet_ota_job_t job = {0};

    ota_manager_set_state(OTA_MANAGER_STATE_WAIT_STM32_VERSION);
    ESP_LOGI(TAG, "主动查询并等待STM32 APP上报当前版本...");
    if (!onenet_ota_query_current_stm32_version(
            pdMS_TO_TICKS(STM32_INITIAL_VERSION_WAIT_MS),
            &current_version))
    {
        ESP_LOGE(TAG, "%u ms内未获得有效STM32版本，本轮检查结束",
                 (unsigned int)STM32_INITIAL_VERSION_WAIT_MS);
        return;
    }

    onenet_ota_format_version(&current_version,
                              current_version_text,
                              sizeof(current_version_text));
    ESP_LOGI(TAG, "使用STM32当前版本检查OneNET: %s", current_version_text);

    ret = onenet_ota_prepare_job(current_version_text, &job, &target_semver);
    if (ret != ESP_OK)
    {
        return;
    }

    ret = onenet_ota_download_job(&job);
    if (ret != ESP_OK)
    {
        return;
    }

    (void)onenet_ota_transfer_job(&job, &target_semver);
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
 * @brief 向OTA Manager投递一次检查请求。（已加入app测试阶段关闭）
 * @param trigger 触发来源。
 * @return ESP_OK表示请求已锁存，ESP_ERR_INVALID_STATE表示Manager未初始化。
 */
esp_err_t onenet_ota_request_check(onenet_ota_trigger_t trigger)
{
    #if ONENET_OTA_DISABLED
        static const char *const trigger_Names[] =
        {
            "mqtt-connected",
            "mqtt-inform",
            "periodic"
        };
        (void)trigger_Names;
        ESP_LOGI(TAG, "[OTA已停用] 忽略触发来源=%s", trigger_Names[trigger]);
        return ESP_OK;
#else


    static const char *const trigger_names[] = {
        "mqtt-connected",
        "mqtt-inform",
        "periodic"
    };

    if ((unsigned int)trigger >= (sizeof(trigger_names) / sizeof(trigger_names[0])))
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ota_manager_request_check();
    if (ret != ESP_OK)
    {
        return ret;
    }
    ESP_LOGI(TAG, "[MANAGER] check requested by %s", trigger_names[trigger]);
    return ESP_OK;
#endif

}

/**
 * @brief 兼容旧入口，按MQTT连接事件请求一次平台检查。(已加入app测试阶段关闭)
 * @param 无。
 * @return 无。
 */
void onenet_ota_start(void)
{
#if ONENET_OTA_DISABLED
    ESP_LOGI(TAG, "[OTA已停用] onenet_ota_start() 被忽略");
#else
    if (onenet_ota_request_check(ONENET_OTA_TRIGGER_MQTT_CONNECTED) != ESP_OK)
    {
        ESP_LOGE(TAG, "OTA Manager未初始化，无法投递检查请求");
    }
#endif
}

