#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

/**
 * @brief STM32 APP版本报告。
 * @note generation由Manager入口生成，用于区分升级前缓存和升级后新报告。
 */
typedef struct
{
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    uint32_t generation;
    bool valid;
} stm32_version_state_t;

/**
 * @brief OTA Manager高层状态。
 */
typedef enum
{
    OTA_MANAGER_STATE_UNINITIALIZED = 0,
    OTA_MANAGER_STATE_IDLE,
    OTA_MANAGER_STATE_WAIT_STM32_VERSION,
    OTA_MANAGER_STATE_SYNC_VERSION,
    OTA_MANAGER_STATE_CHECK_TASK,
    OTA_MANAGER_STATE_DOWNLOAD,
    OTA_MANAGER_STATE_UART_TRANSFER,
    OTA_MANAGER_STATE_WAIT_NEW_VERSION,
    OTA_MANAGER_STATE_REPORT_RESULT
} ota_manager_state_t;

typedef void (*ota_manager_round_callback_t)(void);
typedef bool (*ota_manager_connected_callback_t)(void);

/**
 * @brief 初始化常驻OTA Manager任务和长度为1的STM32版本队列。
 * @param round_callback 单轮OTA执行函数，只在Manager任务上下文调用。
 * @param connected_callback 查询OneNET连接状态的函数。
 * @return ESP_OK表示初始化完成，其他值表示参数或RTOS资源创建失败。
 * @note 重复调用不会重复创建任务或队列。
 */
esp_err_t ota_manager_init(ota_manager_round_callback_t round_callback,
                           ota_manager_connected_callback_t connected_callback);

/**
 * @brief 请求Manager执行一次平台检查。
 * @return ESP_OK表示通知已锁存，ESP_ERR_INVALID_STATE表示Manager未初始化。
 * @note 多次通知通过任务通知计数合并；Manager忙时不会并发执行。
 */
esp_err_t ota_manager_request_check(void);

/**
 * @brief 将STM32 APP版本写入长度为1的覆盖队列。
 * @param major 主版本号。
 * @param minor 次版本号。
 * @param patch 修订版本号。
 * @return ESP_OK表示已写入，其他值表示Manager未初始化或队列写入失败。
 */
esp_err_t ota_manager_notify_stm32_version(uint8_t major,
                                           uint8_t minor,
                                           uint8_t patch);

/**
 * @brief 获取Manager当前掌握的最新STM32版本。
 * @param version 输出版本。
 * @return true表示已有有效版本。
 * @note 只应由Manager单轮回调所在任务调用。
 */
bool ota_manager_get_stm32_version(stm32_version_state_t *version);

/**
 * @brief 等待generation大于指定值的新版本报告。
 * @param after_generation 只接受大于该序号的报告。
 * @param timeout_ticks 最大等待Tick数。
 * @param version 输出版本。
 * @return true表示收到符合条件的新报告。
 * @note 只应由Manager单轮回调所在任务调用。
 */
bool ota_manager_wait_stm32_version(uint32_t after_generation,
                                    TickType_t timeout_ticks,
                                    stm32_version_state_t *version);

/**
 * @brief 更新并打印Manager高层状态。
 * @param state 新状态。
 * @return 无。
 */
void ota_manager_set_state(ota_manager_state_t state);

#endif
