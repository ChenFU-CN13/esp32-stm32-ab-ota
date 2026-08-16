#ifndef _ONENET_OTA_H_
#define _ONENET_OTA_H_
#include "esp_err.h"

/**
 * 启动onenet ota升级流程
 * @param 无
 * @return 无
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