#include "main.h"
#include "uart2_ringbuf.h"

/* 缓冲区长度必须是 2 的幂，便于用位与实现回绕。 */
#if ((UART2_RINGBUF_SIZE == 0U) || ((UART2_RINGBUF_SIZE & (UART2_RINGBUF_SIZE - 1U)) != 0U))
#error "UART2_RINGBUF_SIZE必须为2的幂"
#endif

typedef struct
{
    uint8_t data[UART2_RINGBUF_SIZE];
    volatile uint8_t write_index;       /* 仅USART2接收中断写入 */
    volatile uint8_t read_index;        /* 仅UART2_Comm_Task写入 */
    volatile uint32_t overflow_count;   /* 缓冲满时累计丢弃的字节数 */
} UART2_RingBuffer_t;

static UART2_RingBuffer_t s_uart2_ring_buffer;

/**
 * @brief 初始化 USART2 单生产者、单消费者环形缓冲区。
 * @param 无。
 * @return 无。
 * @note 写索引仅由中断修改，读索引仅由通讯任务修改，因此不需要互斥锁。
 */
void UART2_RingBuffer_Init(void)
{
    s_uart2_ring_buffer.write_index = 0U;
    s_uart2_ring_buffer.read_index = 0U;
    s_uart2_ring_buffer.overflow_count = 0U;
}

/**
 * @brief 在 USART2 接收中断中写入一个字节。
 * @param byte 本次接收到的原始 UART 字节。
 * @return 1：写入成功；0：缓冲区已满，丢弃新字节。
 * @note 使用保留一个空槽的方式区分空与满，64 字节数组可暂存 63 字节。
 */
uint8_t UART2_RingBuffer_PushFromISR(uint8_t byte)
{
    uint8_t write_index = s_uart2_ring_buffer.write_index;
    uint8_t next_index = (uint8_t)((write_index + 1U) & (UART2_RINGBUF_SIZE - 1U));

    /* 留一个空槽区分“空”和“满”，满时丢弃新字节。 */
    if (next_index == s_uart2_ring_buffer.read_index)
    {
        s_uart2_ring_buffer.overflow_count++;
        return 0U;
    }

    /* 先写数据，再更新写索引；读任务看到新索引时，数据必须已准备好。 */
    s_uart2_ring_buffer.data[write_index] = byte;
    __DMB();    /* 先保证数据可见，再更新写指针。 */
    s_uart2_ring_buffer.write_index = next_index;

    return 1U;
}

/**
 * @brief 在通讯任务中读取一个字节。
 * @param byte 用于保存读取结果的地址，不能为空。
 * @return 1：读取成功；0：无数据或参数为空。
 * @note 该函数不在中断中调用，避免与唯一写入者产生多消费者/多生产者竞争。
 */
uint8_t UART2_RingBuffer_Pop(uint8_t *byte)
{
    uint8_t read_index;

    if (byte == NULL)
    {
        return 0U;
    }

    read_index = s_uart2_ring_buffer.read_index;
    if (read_index == s_uart2_ring_buffer.write_index)
    {
        return 0U;
    }

    *byte = s_uart2_ring_buffer.data[read_index];
    __DMB();    /* 先取走数据，再释放该缓冲槽。 */
    s_uart2_ring_buffer.read_index =
        (uint8_t)((read_index + 1U) & (UART2_RINGBUF_SIZE - 1U));

    return 1U;
}

/**
 * @brief 读取环形缓冲区溢出累计值。
 * @param 无。
 * @return 自初始化以来丢弃的新字节数量。
 * @note 返回值只用于通讯任务检测“是否发生过丢字节”，不用于精确流量统计。
 */
uint32_t UART2_RingBuffer_GetOverflowCount(void)
{
    return s_uart2_ring_buffer.overflow_count;
}
