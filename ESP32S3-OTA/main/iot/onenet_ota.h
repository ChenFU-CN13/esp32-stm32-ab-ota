#ifndef _ONENET_OTA_H_
#define _ONENET_OTA_H_

/**
 * @brief OTA 功能总开关（调试阶段强烈建议停用，避免频繁重启或握手污染）
 * @note  1 = 完全停用 OTA：不创建Manager任务、不做5分钟轮询、所有OTA接口返回空成功；
 *        0 = 恢复正常 OTA 升级流程
 */
#define ONENET_OTA_DISABLED             0    // ← 调试APP=1，做OTA时改回0

#include "esp_err.h"
#include <stdint.h>





/**
 * @brief OTA Manager检查触发来源。
 * @note 所有来源最终只唤醒同一个常驻Manager任务，重复触发会被合并。
 */
typedef enum
{
    ONENET_OTA_TRIGGER_MQTT_CONNECTED = 0,
    ONENET_OTA_TRIGGER_MQTT_INFORM,
    ONENET_OTA_TRIGGER_PERIODIC
} onenet_ota_trigger_t;

/**
 * @brief 初始化STM32版本同步状态、SPIFFS和常驻OTA Manager。
 * @param 无。
 * @return ESP_OK表示初始化成功，其他值表示SPIFFS、版本队列或Manager任务初始化失败。
 * @note 必须在UART接收任务启动前调用；重复调用不会重复挂载文件系统或创建任务。
 */
esp_err_t onenet_ota_init(void);

/**
 * @brief 缓存STM32 APP上报的当前版本并通知OTA任务。
 * @param major 主版本号。
 * @param minor 次版本号。
 * @param patch 修订号。
 * @return ESP_OK表示版本已缓存，其他值表示尚未完成初始化。
 * @note 仅更新本地状态，不在UART任务中执行HTTP请求。
 */
esp_err_t onenet_ota_notify_stm32_version(uint8_t major,
                                          uint8_t minor,
                                          uint8_t patch);

/**
 * @brief 向OTA Manager投递一次平台检查请求。
 * @param trigger 本次触发来源。
 * @return ESP_OK表示请求已锁存，其他值表示Manager尚未初始化。
 * @note Manager忙时不会并发执行；请求位会保留到当前轮结束后补查一次。
 */
esp_err_t onenet_ota_request_check(onenet_ota_trigger_t trigger);

/**
 * @brief 兼容旧调用：按MQTT连接成功来源触发一次OTA检查。
 * @param 无。
 * @return 无。
 */
void onenet_ota_start(void);


/**
 * 设置合法启动分区
 * @param vaild 是否合法
 * @return 无
 */
void set_app_vaild(int vaild);

/**
 * 上报版本号
 * @param version 版本号字符串，格式为X.X.X（如"1.0.4"）
 * @return 错误码
 */
esp_err_t onenet_ota_upload_version(const char* version);

/**
 * 临时测试函数：读取并验证固件文件
 * 用于读取SPIFFS中的固件文件，处理数据碎片化问题
 */
void test_read_firmware(void);

#endif
