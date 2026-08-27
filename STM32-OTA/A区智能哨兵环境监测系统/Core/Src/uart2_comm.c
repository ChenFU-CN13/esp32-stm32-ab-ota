#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>
#include "APP_Tasks.h"
#include "uart2_comm.h"
#include "uart2_ringbuf.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/24CXX/24cxx.h"

#define UART2_COMM_PACKET_QUEUE_LENGTH     16U
#define UART2_COMM_ACK_TIMEOUT_MS           200U
#define UART2_COMM_MAX_RETRY_COUNT          3U
#define UART2_COMM_VERSION_RETRY_MS       5000U
#define UART2_COMM_TASK_STACK_DEPTH         (128U * 2U)
#define UART2_COMM_TASK_PRIORITY            3U

#define UART2_COMM_VERSION_TYPE              0x05U
#define UART2_COMM_EEPROM_VERSION_ADDR       0x2CU
#define UART2_COMM_EEPROM_VERSION_LENGTH     8U

#define UART2_COMM_NOTIFY_RX                (1UL << 0)
#define UART2_COMM_NOTIFY_TX_COMPLETE       (1UL << 1)
#define UART2_COMM_NOTIFY_RX_ERROR          (1UL << 2)

typedef enum
{
    UART2_COMM_MODE_NORMAL = 0,
    UART2_COMM_MODE_OTA_ACK_PENDING,
    UART2_COMM_MODE_OTA_ACK_SENDING
} UART2_CommMode_t;

static TaskHandle_t s_uart2_comm_task_handle = NULL;
static QueueHandle_t s_uart2_packet_queue = NULL;

static UART_Packet_t s_active_business_packet;  /* DMA发送期间必须持续有效。 */
static uint8_t s_ota_ack_frame[3] = {
    UART2_COMM_OTA_HEAD,
    UART2_COMM_OTA_ACK_CMD,
    UART2_COMM_OTA_TAIL
};

static uint8_t s_rx_frame[sizeof(UART_Packet_t)];
static uint8_t s_rx_frame_length = 0U;
static uint8_t s_rx_expected_length = 0U;
static uint32_t s_last_overflow_count = 0U;
static uint32_t s_ack_deadline = 0U;
static uint8_t s_retry_count = 0U;
static uint8_t s_active_business_valid = 0U;
static uint8_t s_waiting_business_ack = 0U;
static uint8_t s_tx_dma_busy = 0U;
static volatile uint8_t s_rx_error_pending = 0U;
static UART2_CommMode_t s_comm_mode = UART2_COMM_MODE_NORMAL;
static uint32_t s_ack_drop_count = 0U;
static UART_Packet_t s_version_report_packet;
static uint8_t s_version_report_valid = 0U;
static uint8_t s_version_report_acked = 0U;
static uint8_t s_version_report_queued = 0U;
static uint32_t s_version_report_next_tick = 0U;

static void UART2_Comm_Task(void *pvParameters);
static void UART2_Comm_NotifyFromISR(uint32_t notify_bits);
static void UART2_Comm_ResetParser(void);
static void UART2_Comm_DrainRxBytes(void);
static void UART2_Comm_FeedRxByte(uint8_t byte);
static void UART2_Comm_ProcessCompletedFrame(void);
static void UART2_Comm_HandleBusinessAck(uint8_t result);
static void UART2_Comm_HandleVersionRequest(void);
static void UART2_Comm_QueueBusinessPackets(void);
static void UART2_Comm_TryStartBusinessTx(void);
static void UART2_Comm_CheckBusinessTimeout(void);
static void UART2_Comm_TryStartOtaAck(void);
static void UART2_Comm_PrepareVersionReport(void);
static void UART2_Comm_TryQueueVersionReport(void);
static uint8_t UART2_Comm_ParseVersion(const uint8_t *raw_version,
                                       uint8_t *major,
                                       uint8_t *minor,
                                       uint8_t *patch);
static BaseType_t UART2_Comm_StartDmaTx(uint8_t *data, uint16_t length);
static void UART2_Comm_QueuePacket(const UART_Packet_t *packet);
static void UART2_Comm_InitPacket(UART_Packet_t *packet, uint8_t type);
static void UART2_Comm_LogFrame(const char *tag, const uint8_t *data, uint16_t length);

/**
 * @brief 初始化 UART2 通讯模块，并启动唯一的 USART2 通讯任务。
 * @param 无。
 * @return 无。
 * @note 必须在 xUART2_TxQueue 已创建后调用；本函数最后才开启 USART2 单字节中断接收。
 */
void UART2_Comm_Start(void)
{
    UART2_RingBuffer_Init();
    s_last_overflow_count = 0U;
    UART2_Comm_ResetParser();

    s_uart2_packet_queue = xQueueCreate(UART2_COMM_PACKET_QUEUE_LENGTH,
                                        sizeof(UART_Packet_t));
    configASSERT(s_uart2_packet_queue != NULL);

    /* 触摸初始化已配置软件I2C；此处读取Bootloader成功提交的运行版本。 */
    UART2_Comm_PrepareVersionReport();

    if (xTaskCreate(UART2_Comm_Task,
                    "UART2_Comm_Task",
                    UART2_COMM_TASK_STACK_DEPTH,
                    NULL,
                    UART2_COMM_TASK_PRIORITY,
                    &s_uart2_comm_task_handle) != pdPASS)
    {
        configASSERT(0);
    }

    /* 任务句柄已经可用，再打开单字节中断接收。 */
    if (USART2_StartReceiveIT() != HAL_OK)
    {
        Error_Handler();
    }

    /* 该打印走 STM32 调试串口，用于确认业务 USART2 通讯任务已完成初始化。 */
    printf("[UART2][INIT] comm task started, ACK timeout=%lu ms, max retry=%u\r\n",
           (unsigned long)UART2_COMM_ACK_TIMEOUT_MS,
           (unsigned int)UART2_COMM_MAX_RETRY_COUNT);
}

/**
 * @brief 将 USART2 接收字节交给通讯模块。
 * @param byte 本次中断接收到的一个字节。
 * @return 无。
 * @note 只能在 USART2 接收中断/回调中调用；这里只入环形缓冲区并通知任务，不解析协议。
 */
void UART2_Comm_RxByteFromISR(uint8_t byte)
{
    (void)UART2_RingBuffer_PushFromISR(byte);
    UART2_Comm_NotifyFromISR(UART2_COMM_NOTIFY_RX);
}

/**
 * @brief 通知通讯任务 USART2 DMA 发送已经完成。
 * @param 无。
 * @return 无。
 * @note 不能在 DMA 回调中直接复位；由任务根据当前状态决定普通发送完成还是 OTA ACK 完成。
 */
void UART2_Comm_TxCompleteFromISR(void)
{
    UART2_Comm_NotifyFromISR(UART2_COMM_NOTIFY_TX_COMPLETE);
}

/**
 * @brief 记录 USART2 接收错误并通知通讯任务恢复解析同步。
 * @param 无。
 * @return 无。
 * @note 不在中断中清空复杂状态，避免中断与任务同时操作解析变量。
 */
void UART2_Comm_RxErrorFromISR(void)
{
    s_rx_error_pending = 1U;
    UART2_Comm_NotifyFromISR(UART2_COMM_NOTIFY_RX_ERROR);
}

/**
 * @brief USART2 唯一通讯任务：解析接收数据、发送业务帧、等待 ACK 及处理 OTA 应答。
 * @param pvParameters 未使用，固定为 NULL。
 * @return 无；任务永久运行。
 * @note 此任务是 USART2 DMA 唯一发送者，避免多个业务任务同时调用 HAL_UART_Transmit_DMA。
 */
static void UART2_Comm_Task(void *pvParameters)
{
    uint32_t notify_value;

    (void)pvParameters;

    for (;;)
    {
        notify_value = 0U;
        /* 有中断事件立即被唤醒；无事件时每 10 ms 醒一次，用于检查 200 ms ACK 超时。 */
        (void)xTaskNotifyWait(0U,
                               0xFFFFFFFFUL,
                               &notify_value,
                               pdMS_TO_TICKS(10U));

        if ((notify_value & UART2_COMM_NOTIFY_TX_COMPLETE) != 0U)
        {
            /* DMA 已不再读取发送缓冲区，后续才允许发送下一帧或执行 OTA 复位。 */
            s_tx_dma_busy = 0U;

            /* OTA ACK的最后一个字节已由DMA发出，此时才允许复位。 */
            if (s_comm_mode == UART2_COMM_MODE_OTA_ACK_SENDING)
            {
                NVIC_SystemReset();
            }
        }

        if ((s_rx_error_pending != 0U) ||
            (s_last_overflow_count != UART2_RingBuffer_GetOverflowCount()))
        {
            /* 丢字节或 UART 错误会破坏帧边界；不猜测剩余内容，直接重新找帧头。 */
            printf("[UART2][RX] error/overflow, reset parser\r\n");
            s_rx_error_pending = 0U;
            s_last_overflow_count = UART2_RingBuffer_GetOverflowCount();
            UART2_Comm_ResetParser();
        }

        UART2_Comm_DrainRxBytes();

        if (s_comm_mode == UART2_COMM_MODE_OTA_ACK_PENDING)
        {
            /* OTA 控制帧优先级最高：不再从业务队列取新数据。 */
            UART2_Comm_TryStartOtaAck();
            continue;
        }

        if (s_comm_mode == UART2_COMM_MODE_NORMAL)
        {
            /* 正常模式才允许组帧、检查 ACK 超时并启动下一次业务 DMA 发送。 */
            UART2_Comm_TryQueueVersionReport();
            UART2_Comm_QueueBusinessPackets();
            UART2_Comm_CheckBusinessTimeout();
            UART2_Comm_TryStartBusinessTx();
        }
    }
}

/**
 * @brief 从中断上下文向通讯任务发送事件位。
 * @param notify_bits 接收字节、发送完成或接收错误对应的通知位。
 * @return 无。
 * @note USART2/DMA 中断优先级为 5，满足调用 xTaskNotifyFromISR 的 FreeRTOS 优先级要求。
 */
static void UART2_Comm_NotifyFromISR(uint32_t notify_bits)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (s_uart2_comm_task_handle != NULL)
    {
        (void)xTaskNotifyFromISR(s_uart2_comm_task_handle,
                                 notify_bits,
                                 eSetBits,
                                 &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}

/**
 * @brief 丢弃当前正在收集的半帧，回到“寻找帧头”状态。
 * @param 无。
 * @return 无。
 * @note 发生接收错误或环形缓冲区溢出后，半帧已不可信，必须重新同步。
 */
static void UART2_Comm_ResetParser(void)
{
    s_rx_frame_length = 0U;
    s_rx_expected_length = 0U;
}

/**
 * @brief 取空环形缓冲区中当前已有的字节，并逐个送入协议解析器。
 * @param 无。
 * @return 无。
 * @note 只在通讯任务中调用，保证环形缓冲区只有一个消费者。
 */
static void UART2_Comm_DrainRxBytes(void)
{
    uint8_t byte;

    while (UART2_RingBuffer_Pop(&byte))
    {
        UART2_Comm_FeedRxByte(byte);
    }
}

/**
 * @brief 根据帧头识别协议长度，并累计接收字节直到组成完整帧。
 * @param byte 从环形缓冲区取出的一个原始字节。
 * @return 无。
 * @note 0x55 帧固定收集 8 字节；0x66 OTA 控制帧固定收集 3 字节。
 */
static void UART2_Comm_FeedRxByte(uint8_t byte)
{
    if (s_rx_expected_length == 0U)
    {
        /* 空闲时只识别两种帧头；无关字节作为噪声跳过。 */
        if (byte == UART2_COMM_BUSINESS_HEAD)
        {
            s_rx_frame[0] = byte;
            s_rx_frame_length = 1U;
            s_rx_expected_length = sizeof(UART_Packet_t);
        }
        else if (byte == UART2_COMM_OTA_HEAD)
        {
            s_rx_frame[0] = byte;
            s_rx_frame_length = 1U;
            s_rx_expected_length = sizeof(s_ota_ack_frame);
        }
        return;
    }

    /* 已找到帧头后按预期长度收集字节，收满前不执行任何业务动作。 */
    s_rx_frame[s_rx_frame_length++] = byte;
    if (s_rx_frame_length < s_rx_expected_length)
    {
        return;
    }

    UART2_Comm_ProcessCompletedFrame();
    UART2_Comm_ResetParser();

    /* 当前字节本身也可能是下一帧的帧头，避免丢失同步机会。 */
    if ((byte == UART2_COMM_BUSINESS_HEAD) || (byte == UART2_COMM_OTA_HEAD))
    {
        UART2_Comm_FeedRxByte(byte);
    }
}

/**
 * @brief 校验一帧已收满的数据，并分派为业务 ACK 或 OTA 升级通知。
 * @param 无。
 * @return 无。
 * @note 业务 ACK 除帧头、类型、结果、帧尾外，还要求其余 4 个数据字节均为 0。
 */
static void UART2_Comm_ProcessCompletedFrame(void)
{
    if ((s_rx_expected_length == sizeof(UART_Packet_t)) &&
        (s_rx_frame[sizeof(UART_Packet_t) - 1U] == UART2_COMM_BUSINESS_TAIL) &&
        (s_rx_frame[1] == UART2_COMM_BUSINESS_ACK_TYPE) &&
        (s_rx_frame[3] == 0U) &&
        (s_rx_frame[4] == 0U) &&
        (s_rx_frame[5] == 0U) &&
        (s_rx_frame[6] == 0U))
    {
        UART2_Comm_LogFrame("[UART2][RX][ACK]", s_rx_frame, s_rx_expected_length);
        UART2_Comm_HandleBusinessAck(s_rx_frame[2]);
    }
    else if ((s_rx_expected_length == sizeof(s_ota_ack_frame)) &&
             (s_rx_frame[1] == UART2_COMM_OTA_NOTIFY_CMD) &&
             (s_rx_frame[2] == UART2_COMM_OTA_TAIL))
    {
        /* 收到重复通知时只保持待发送状态，不重复启动DMA。 */
        if (s_comm_mode == UART2_COMM_MODE_NORMAL)
        {
            /* OTA 通知只改变任务状态；实际 ACK 与复位仍由后续任务流程完成。 */
            UART2_Comm_LogFrame("[UART2][RX][OTA]", s_rx_frame, s_rx_expected_length);
            printf("收到上位机数据帧：66 01 BB，升级通知，准备回复ACK并重启到Bootloader\r\n");
            s_comm_mode = UART2_COMM_MODE_OTA_ACK_PENDING;
        }
    }
    else if ((s_rx_expected_length == sizeof(s_ota_ack_frame)) &&
             (s_rx_frame[1] == UART2_COMM_VERSION_REQUEST_CMD) &&
             (s_rx_frame[2] == UART2_COMM_OTA_TAIL))
    {
        UART2_Comm_LogFrame("[UART2][RX][VERSION-REQ]",
                            s_rx_frame,
                            s_rx_expected_length);
        UART2_Comm_HandleVersionRequest();
    }
    else
    {
        /* 收满但不符合 ACK 或 OTA 格式的数据也记录下来，方便定位协议版本或线路异常。 */
        UART2_Comm_LogFrame("[UART2][RX][DROP]", s_rx_frame, s_rx_expected_length);
    }
}

/**
 * @brief 处理 ESP32 对当前业务帧返回的 ACK 结果。
 * @param result ACK 的结果字节：0 成功；非 0 为失败。
 * @return 无。
 * @note 非 0 结果通过把截止时间置为已超时，使任务下一轮立即进入重发分支。
 */
static void UART2_Comm_HandleBusinessAck(uint8_t result)
{
    if ((s_comm_mode != UART2_COMM_MODE_NORMAL) ||
        (s_waiting_business_ack == 0U))
    {
        return;
    }

    if (result == 0U)
    {
        //printf("[UART2][ACK] success: type=0x%02X value=%u\r\n",
        //       (unsigned int)s_active_business_packet.type,
        //       (unsigned int)s_active_business_packet.data[0]);
        if (s_active_business_packet.type == UART2_COMM_VERSION_TYPE)
        {
            s_version_report_acked = 1U;
            printf("[UART2][VERSION] ESP32已确认当前APP版本 %u.%u.%u\r\n",
                   (unsigned int)s_active_business_packet.data[0],
                   (unsigned int)s_active_business_packet.data[1],
                   (unsigned int)s_active_business_packet.data[2]);
        }

        /* ESP32 已成功缓存本帧，当前帧的可靠传输闭环结束。 */
        s_waiting_business_ack = 0U;
        s_active_business_valid = 0U;
        s_retry_count = 0U;
    }
    else
    {
        /* 非成功ACK不等待满200ms，下一轮立即走重发分支。 */
        /* result=01/02 均视为本帧失败，立即触发重发，而不是无意义地等待 200 ms。 */
        printf("[UART2][ACK] failed: result=0x%02X, retry now\r\n",
               (unsigned int)result);
        s_ack_deadline = HAL_GetTick() - UART2_COMM_ACK_TIMEOUT_MS;
    }
}

/**
 * @brief 处理ESP32发来的当前APP版本查询。
 * @param 无。
 * @return 无。
 * @note 本函数运行在UART2通讯任务中，只重新开放现有版本帧的发送资格，不在接收中断中执行DMA发送。
 */
static void UART2_Comm_HandleVersionRequest(void)
{
    if (s_comm_mode != UART2_COMM_MODE_NORMAL)
    {
        printf("[UART2][VERSION] OTA切换期间忽略版本查询\r\n");
        return;
    }

    if (s_version_report_valid == 0U)
    {
        printf("[UART2][VERSION] 收到查询，但AT24C02中没有有效APP版本\r\n");
        return;
    }

    /* 旧ACK只属于ESP32上一次运行会话；收到新查询后必须允许重新上报。 */
    s_version_report_acked = 0U;
    s_version_report_next_tick = HAL_GetTick();
    printf("[UART2][VERSION] 收到ESP32查询，重新上报当前APP版本\r\n");
}

/**
 * @brief 将原有 xUART2_TxQueue 的业务消息转换为统一的 0x55 八字节帧。
 * @param 无。
 * @return 无。
 * @note DHT11 一条消息拆成湿度(type=1)和温度(type=2)两帧，保持原任务调用方式不变。
 */
static void UART2_Comm_QueueBusinessPackets(void)
{
    UART_Msg_t msg;
    UART_Packet_t packet;

    if (xQueueReceive(xUART2_TxQueue, &msg, 0U) != pdTRUE)
    {
        return;
    }

    switch (msg.type)
    {
        case MSG_TYPE_DHT11:
            UART2_Comm_InitPacket(&packet, 0x01U);
            packet.data[0] = msg.data.dht11_data.humi;
            UART2_Comm_QueuePacket(&packet);

            UART2_Comm_InitPacket(&packet, 0x02U);
            packet.data[0] = msg.data.dht11_data.temp;
            UART2_Comm_QueuePacket(&packet);
            break;

        case MSG_TYPE_LED:
            UART2_Comm_InitPacket(&packet, 0x03U);
            packet.data[0] = (uint8_t)msg.data.led_cmd;
            UART2_Comm_QueuePacket(&packet);
            break;

        case MSG_TYPE_BUZZER:
            UART2_Comm_InitPacket(&packet, 0x04U);
            packet.data[0] = (uint8_t)msg.data.buzzer_cmd;
            UART2_Comm_QueuePacket(&packet);
            break;

        default:
            break;
    }
}

/**
 * @brief 在 USART2 空闲且没有待确认帧时，启动一帧业务 DMA 发送。
 * @param 无。
 * @return 无。
 * @note 发送数据使用 s_active_business_packet，不能使用函数局部变量作为 DMA 源。
 */
static void UART2_Comm_TryStartBusinessTx(void)
{
    if ((s_tx_dma_busy != 0U) || (s_waiting_business_ack != 0U))
    {
        /* 一帧业务数据未得到 ACK 前不发送下一帧，当前协议没有 seq，必须串行发送。 */
        return;
    }

    if (s_active_business_valid == 0U)
    {
        if (xQueueReceive(s_uart2_packet_queue,
                          &s_active_business_packet,
                          0U) != pdTRUE)
        {
            return;
        }

        s_active_business_valid = 1U;
        s_retry_count = 0U;

        if (s_active_business_packet.type == UART2_COMM_VERSION_TYPE)
        {
            s_version_report_queued = 0U;
        }
    }

    if (UART2_Comm_StartDmaTx((uint8_t *)&s_active_business_packet,
                              sizeof(s_active_business_packet)) == pdPASS)
    {
        UART2_Comm_LogFrame("[UART2][TX][BUS]",
                            (const uint8_t *)&s_active_business_packet,
                            sizeof(s_active_business_packet));
       //printf("[UART2][TX] send: type=0x%02X value=%u, attempt=1\r\n",
       //       (unsigned int)s_active_business_packet.type,
       //       (unsigned int)s_active_business_packet.data[0]);
        s_waiting_business_ack = 1U;
        s_ack_deadline = HAL_GetTick() + UART2_COMM_ACK_TIMEOUT_MS;
    }
}

/**
 * @brief 检查当前业务帧是否等待 ACK 超时，并按重试上限决定重发或丢弃。
 * @param 无。
 * @return 无。
 * @note ACK 超时为 200 ms；首次发送之外最多重发 3 次，因此总发送次数最多为 4 次。
 */
static void UART2_Comm_CheckBusinessTimeout(void)
{
    if ((s_waiting_business_ack == 0U) || (s_tx_dma_busy != 0U))
    {
        return;
    }

    if ((int32_t)(HAL_GetTick() - s_ack_deadline) < 0)
    {
        return;
    }

    if (s_retry_count >= UART2_COMM_MAX_RETRY_COUNT)
    {
        printf("[UART2][DROP] type=0x%02X value=%u, no ACK after 4 sends\r\n",
               (unsigned int)s_active_business_packet.type,
               (unsigned int)s_active_business_packet.data[0]);
        if (s_active_business_packet.type == UART2_COMM_VERSION_TYPE)
        {
            /* ESP32可能尚未启动；不阻塞业务帧，5秒后再开启新一轮版本上报。 */
            s_version_report_next_tick = HAL_GetTick() + UART2_COMM_VERSION_RETRY_MS;
            printf("[UART2][VERSION] 本轮无ACK，%lu ms后重新上报\r\n",
                   (unsigned long)UART2_COMM_VERSION_RETRY_MS);
        }

        /* 已完成首次发送和 3 次重发仍失败：放弃本帧，避免永远卡住后续业务。 */
        s_waiting_business_ack = 0U;
        s_active_business_valid = 0U;
        s_retry_count = 0U;
        s_ack_drop_count++;
        return;
    }

    if (UART2_Comm_StartDmaTx((uint8_t *)&s_active_business_packet,
                              sizeof(s_active_business_packet)) == pdPASS)
    {
        s_retry_count++;
        UART2_Comm_LogFrame("[UART2][TX][BUS][RETRY]",
                            (const uint8_t *)&s_active_business_packet,
                            sizeof(s_active_business_packet));
        printf("[UART2][TX] retry: type=0x%02X value=%u, retry=%u/%u\r\n",
               (unsigned int)s_active_business_packet.type,
               (unsigned int)s_active_business_packet.data[0],
               (unsigned int)s_retry_count,
               (unsigned int)UART2_COMM_MAX_RETRY_COUNT);
        s_ack_deadline = HAL_GetTick() + UART2_COMM_ACK_TIMEOUT_MS;
    }
}

/**
 * @brief 从AT24C02读取并准备当前APP版本上报帧。
 * @param 无。
 * @return 无。
 * @note Bootloader的OTA_INFOCB版本字段固定从0x2C开始，占8字节；非法内容不使用硬编码版本替代。
 */
static void UART2_Comm_PrepareVersionReport(void)
{
    uint8_t raw_version[UART2_COMM_EEPROM_VERSION_LENGTH];
    uint8_t major;
    uint8_t minor;
    uint8_t patch;

    memset(raw_version, 0, sizeof(raw_version));
    at24cxx_init();
    at24cxx_read(UART2_COMM_EEPROM_VERSION_ADDR,
                 raw_version,
                 UART2_COMM_EEPROM_VERSION_LENGTH);

    if (UART2_Comm_ParseVersion(raw_version, &major, &minor, &patch) == 0U)
    {
        printf("[UART2][VERSION] AT24C02版本字段无效，不上报伪造版本\r\n");
        return;
    }

    UART2_Comm_InitPacket(&s_version_report_packet, UART2_COMM_VERSION_TYPE);
    s_version_report_packet.data[0] = major;
    s_version_report_packet.data[1] = minor;
    s_version_report_packet.data[2] = patch;
    s_version_report_valid = 1U;
    s_version_report_next_tick = HAL_GetTick();

    printf("[UART2][VERSION] 从AT24C02读取当前APP版本 %u.%u.%u\r\n",
           (unsigned int)major,
           (unsigned int)minor,
           (unsigned int)patch);
}

/**
 * @brief 在版本尚未被ESP32确认时，按5秒周期重新放入最高优先级发送队列。
 * @param 无。
 * @return 无。
 * @note 每轮仍沿用业务帧的200ms超时和最多4次发送，周期之间不阻塞其他业务数据。
 */
static void UART2_Comm_TryQueueVersionReport(void)
{
    uint32_t now;

    if ((s_version_report_valid == 0U) ||
        (s_version_report_acked != 0U) ||
        (s_version_report_queued != 0U) ||
        ((s_active_business_valid != 0U) &&
         (s_active_business_packet.type == UART2_COMM_VERSION_TYPE)))
    {
        return;
    }

    now = HAL_GetTick();
    if ((int32_t)(now - s_version_report_next_tick) < 0)
    {
        return;
    }

    if (xQueueSendToFront(s_uart2_packet_queue,
                          &s_version_report_packet,
                          0U) == pdPASS)
    {
        s_version_report_queued = 1U;
    }
    else
    {
        s_version_report_next_tick = now + UART2_COMM_VERSION_RETRY_MS;
    }
}

/**
 * @brief 将EEPROM中的ASCII版本解析为三个数值字段。
 * @param raw_version 固定8字节版本字段，可带可选的v/V前缀。
 * @param major 输出主版本号，范围0到255。
 * @param minor 输出次版本号，范围0到255。
 * @param patch 输出修订号，范围0到255。
 * @return 1表示格式有效，0表示字段为空、格式错误或数值越界。
 * @note 仅接受X.X.X，不接受额外后缀，避免ESP32与OneNET比较出歧义。
 */
static uint8_t UART2_Comm_ParseVersion(const uint8_t *raw_version,
                                       uint8_t *major,
                                       uint8_t *minor,
                                       uint8_t *patch)
{
    char version[UART2_COMM_EEPROM_VERSION_LENGTH + 1U];
    uint16_t values[3] = {0U, 0U, 0U};
    uint8_t raw_index;
    uint8_t text_index = 0U;
    uint8_t part;
    uint8_t digit_count;

    if ((raw_version == NULL) || (major == NULL) ||
        (minor == NULL) || (patch == NULL))
    {
        return 0U;
    }

    memset(version, 0, sizeof(version));
    for (raw_index = 0U; raw_index < UART2_COMM_EEPROM_VERSION_LENGTH; raw_index++)
    {
        if ((raw_version[raw_index] == 0x00U) ||
            (raw_version[raw_index] == 0xFFU))
        {
            break;
        }
        version[text_index++] = (char)raw_version[raw_index];
    }

    text_index = 0U;
    if ((version[text_index] == 'v') || (version[text_index] == 'V'))
    {
        text_index++;
    }

    for (part = 0U; part < 3U; part++)
    {
        digit_count = 0U;
        while ((version[text_index] >= '0') && (version[text_index] <= '9'))
        {
            values[part] = (uint16_t)(values[part] * 10U +
                                      (uint16_t)(version[text_index] - '0'));
            if (values[part] > 255U)
            {
                return 0U;
            }
            text_index++;
            digit_count++;
        }

        if (digit_count == 0U)
        {
            return 0U;
        }

        if (part < 2U)
        {
            if (version[text_index] != '.')
            {
                return 0U;
            }
            text_index++;
        }
    }

    if (version[text_index] != '\0')
    {
        return 0U;
    }

    *major = (uint8_t)values[0];
    *minor = (uint8_t)values[1];
    *patch = (uint8_t)values[2];
    return 1U;
}

/**
 * @brief 在 USART2 空闲后优先发送 OTA 确认帧 66 02 BB。
 * @param 无。
 * @return 无。
 * @note OTA 即将复位，当前业务帧及内部待发业务帧会被放弃；复位只发生在该 ACK 的 DMA 完成后。
 */
static void UART2_Comm_TryStartOtaAck(void)
{
    if (s_tx_dma_busy != 0U)
    {
        /* 不中止已经开始的 DMA；等待这一帧物理发送完成后再发送 OTA ACK。 */
        return;
    }

    /* OTA即将复位，放弃当前业务帧及尚未发送的业务数据。 */
    s_waiting_business_ack = 0U;
    s_active_business_valid = 0U;
    s_retry_count = 0U;
    (void)xQueueReset(s_uart2_packet_queue);

    if (UART2_Comm_StartDmaTx(s_ota_ack_frame,
                              sizeof(s_ota_ack_frame)) == pdPASS)
    {
        UART2_Comm_LogFrame("[UART2][TX][OTA]", s_ota_ack_frame, sizeof(s_ota_ack_frame));
        printf("[UART2][OTA] ACK 66 02 BB DMA started\r\n");
        s_comm_mode = UART2_COMM_MODE_OTA_ACK_SENDING;
    }
}

/**
 * @brief 统一启动 USART2 DMA 发送，并记录 DMA 忙状态。
 * @param data DMA 源数据地址；调用者必须保证发送完成前该地址始终有效。
 * @param length 要发送的字节数。
 * @return pdPASS：DMA 已启动；pdFAIL：HAL 当前忙或启动失败。
 * @note 仅允许 UART2_Comm_Task 调用，确保 USART2 DMA 只有一个发送者。
 */
static BaseType_t UART2_Comm_StartDmaTx(uint8_t *data, uint16_t length)
{
    if (HAL_UART_Transmit_DMA(&huart2, data, length) != HAL_OK)
    {
        /* HAL 忙或启动失败时保持原状态，任务下一轮会按当前状态再次尝试。 */
        return pdFAIL;
    }

    s_tx_dma_busy = 1U;
    return pdPASS;
}

/**
 * @brief 将一帧已构造的业务数据放入通讯模块内部待发送队列。
 * @param packet 指向完整八字节业务帧的地址。
 * @return 无。
 * @note 内部队列已满时丢弃该帧并累计失败计数，不能阻塞传感器相关任务。
 */
static void UART2_Comm_QueuePacket(const UART_Packet_t *packet)
{
    if (xQueueSendToBack(s_uart2_packet_queue, packet, 0U) != pdPASS)
    {
        s_ack_drop_count++;
    }
}

/**
 * @brief 初始化统一物模型帧的固定字段。
 * @param packet 待初始化的帧地址。
 * @param type 物模型数据类型，例如湿度、温度、LED、蜂鸣器。
 * @return 无。
 * @note data[5] 先清零，调用者只需填写实际需要的数据位。
 */
static void UART2_Comm_InitPacket(UART_Packet_t *packet, uint8_t type)
{
    packet->header = UART2_COMM_BUSINESS_HEAD;
    packet->type = type;
    memset(packet->data, 0, sizeof(packet->data));
    packet->tail = UART2_COMM_BUSINESS_TAIL;
}

/**
 * @brief 通过 STM32 调试串口打印一帧 UART2 协议数据。
 * @param tag 日志分类标签，例如 [UART2][TX][BUS]。
 * @param data 待打印数据的首地址。
 * @param length 待打印数据长度，单位：字节。
 * @return 无。
 * @note 仅能在 UART2 通讯任务上下文调用，禁止在 USART2/DMA 中断回调中调用 printf。
 */
static void UART2_Comm_LogFrame(const char *tag, const uint8_t *data, uint16_t length)
{
    uint16_t index;

    printf("%s len=%u :", tag, (unsigned int)length);
    for (index = 0U; index < length; index++)
    {
        printf(" %02X", (unsigned int)data[index]);
    }
    printf("\r\n");
}
