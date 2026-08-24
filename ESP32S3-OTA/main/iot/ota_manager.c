#include "ota_manager.h"

#include "esp_log.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define TAG                                  "ota_manager"
#define OTA_MANAGER_PERIODIC_CHECK_MS        300000U
#define OTA_MANAGER_TASK_STACK_DEPTH         8192U
#define OTA_MANAGER_TASK_PRIORITY            8U
#define OTA_MANAGER_TASK_CORE                1U

typedef struct
{
    TaskHandle_t task_handle;
    QueueHandle_t version_queue;
    ota_manager_round_callback_t round_callback;
    ota_manager_connected_callback_t connected_callback;
    stm32_version_state_t current_version;
    ota_manager_state_t state;
    portMUX_TYPE report_sequence_lock;
    uint32_t report_sequence;
} ota_manager_context_t;

static ota_manager_context_t s_manager = {
    .state = OTA_MANAGER_STATE_UNINITIALIZED,
    .report_sequence_lock = portMUX_INITIALIZER_UNLOCKED
};

static void ota_manager_task(void *argument);
static void ota_manager_update_cached_version(const stm32_version_state_t *version);
static bool ota_manager_sequence_is_newer(uint32_t candidate, uint32_t baseline);

/**
 * @brief 按32位回绕规则判断报告序号是否更新。
 */
static bool ota_manager_sequence_is_newer(uint32_t candidate, uint32_t baseline)
{
    if (baseline == 0U)
    {
        return candidate != 0U;
    }
    return ((int32_t)(candidate - baseline) > 0);
}

/**
 * @brief 更新Manager独占的版本缓存。
 * @param version 队列中取出的版本报告。
 * @return 无。
 */
static void ota_manager_update_cached_version(const stm32_version_state_t *version)
{
    if (version != NULL)
    {
        s_manager.current_version = *version;
    }
}

/**
 * @brief 初始化常驻OTA Manager任务和长度为1的版本队列。
 */
esp_err_t ota_manager_init(ota_manager_round_callback_t round_callback,
                           ota_manager_connected_callback_t connected_callback)
{
    if ((round_callback == NULL) || (connected_callback == NULL))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_manager.version_queue == NULL)
    {
        s_manager.version_queue = xQueueCreate(1U, sizeof(stm32_version_state_t));
        if (s_manager.version_queue == NULL)
        {
            return ESP_ERR_NO_MEM;
        }
    }

    s_manager.round_callback = round_callback;
    s_manager.connected_callback = connected_callback;

    if (s_manager.task_handle == NULL)
    {
        if (xTaskCreatePinnedToCore(ota_manager_task,
                                    "onenet_ota_manager",
                                    OTA_MANAGER_TASK_STACK_DEPTH,
                                    NULL,
                                    OTA_MANAGER_TASK_PRIORITY,
                                    &s_manager.task_handle,
                                    OTA_MANAGER_TASK_CORE) != pdPASS)
        {
            s_manager.task_handle = NULL;
            ESP_LOGE(TAG, "OTA Manager任务创建失败");
            return ESP_ERR_NO_MEM;
        }
    }

    return ESP_OK;
}

/**
 * @brief 请求Manager执行一次平台检查。
 */
esp_err_t ota_manager_request_check(void)
{
    if (s_manager.task_handle == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    xTaskNotifyGive(s_manager.task_handle);
    return ESP_OK;
}

/**
 * @brief 将STM32 APP版本写入覆盖队列。
 */
esp_err_t ota_manager_notify_stm32_version(uint8_t major,
                                           uint8_t minor,
                                           uint8_t patch)
{
    stm32_version_state_t version = {
        .major = major,
        .minor = minor,
        .patch = patch,
        .valid = true
    };

    if (s_manager.version_queue == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    portENTER_CRITICAL(&s_manager.report_sequence_lock);
    s_manager.report_sequence++;
    if (s_manager.report_sequence == 0U)
    {
        s_manager.report_sequence = 1U;
    }
    version.generation = s_manager.report_sequence;
    portEXIT_CRITICAL(&s_manager.report_sequence_lock);

    if (xQueueOverwrite(s_manager.version_queue, &version) != pdPASS)
    {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "STM32 APP version=%u.%u.%u report=%lu",
             (unsigned int)major,
             (unsigned int)minor,
             (unsigned int)patch,
             (unsigned long)version.generation);
    return ESP_OK;
}

/**
 * @brief 获取Manager当前掌握的最新STM32版本。
 */
bool ota_manager_get_stm32_version(stm32_version_state_t *version)
{
    stm32_version_state_t queued_version;

    if ((version == NULL) || (s_manager.version_queue == NULL))
    {
        return false;
    }

    /* 队列长度为1；非阻塞取走尚未消费的最新报告并更新Manager私有缓存。 */
    if (xQueueReceive(s_manager.version_queue, &queued_version, 0U) == pdPASS)
    {
        ota_manager_update_cached_version(&queued_version);
    }

    if (!s_manager.current_version.valid)
    {
        return false;
    }

    *version = s_manager.current_version;
    return true;
}

/**
 * @brief 等待指定序号之后的新STM32版本报告。
 */
bool ota_manager_wait_stm32_version(uint32_t after_generation,
                                    TickType_t timeout_ticks,
                                    stm32_version_state_t *version)
{
    stm32_version_state_t queued_version;
    TickType_t start_tick;
    TickType_t elapsed_ticks;
    TickType_t remaining_ticks;

    if ((version == NULL) || (s_manager.version_queue == NULL))
    {
        return false;
    }

    if (ota_manager_get_stm32_version(version) &&
        ota_manager_sequence_is_newer(version->generation, after_generation))
    {
        return true;
    }

    start_tick = xTaskGetTickCount();
    for (;;)
    {
        if (timeout_ticks == portMAX_DELAY)
        {
            remaining_ticks = portMAX_DELAY;
        }
        else
        {
            elapsed_ticks = xTaskGetTickCount() - start_tick;
            if (elapsed_ticks >= timeout_ticks)
            {
                return false;
            }
            remaining_ticks = timeout_ticks - elapsed_ticks;
        }

        if (xQueueReceive(s_manager.version_queue,
                          &queued_version,
                          remaining_ticks) != pdPASS)
        {
            return false;
        }

        ota_manager_update_cached_version(&queued_version);
        if (ota_manager_sequence_is_newer(queued_version.generation,
                                          after_generation))
        {
            *version = queued_version;
            return true;
        }
    }
}

/**
 * @brief 更新并打印Manager高层状态。
 */
void ota_manager_set_state(ota_manager_state_t state)
{
    static const char *const state_names[] = {
        "UNINITIALIZED",
        "IDLE",
        "WAIT_STM32_VERSION",
        "SYNC_VERSION",
        "CHECK_TASK",
        "DOWNLOAD",
        "UART_TRANSFER",
        "WAIT_NEW_VERSION",
        "REPORT_RESULT"
    };

    s_manager.state = state;
    if ((unsigned int)state < (sizeof(state_names) / sizeof(state_names[0])))
    {
        ESP_LOGI(TAG, "state=%s", state_names[state]);
    }
}

/**
 * @brief 常驻Manager任务，串行执行每轮OTA。
 * @param argument 未使用。
 * @return 无；任务永久运行。
 * @note ulTaskNotifyTake(pdTRUE)会把忙期间累计的多次通知合并成一次补查。
 */
static void ota_manager_task(void *argument)
{
    uint32_t notify_count;

    (void)argument;
    ota_manager_set_state(OTA_MANAGER_STATE_IDLE);

    for (;;)
    {
        notify_count = ulTaskNotifyTake(pdTRUE,
                                       pdMS_TO_TICKS(OTA_MANAGER_PERIODIC_CHECK_MS));
        if (notify_count == 0U)
        {
            if ((s_manager.connected_callback == NULL) ||
                !s_manager.connected_callback())
            {
                continue;
            }
            ESP_LOGI(TAG, "idle periodic check");
        }

        if (s_manager.round_callback != NULL)
        {
            s_manager.round_callback();
        }
        ota_manager_set_state(OTA_MANAGER_STATE_IDLE);
    }
}
