#include "main.h"
#include "uart2_ringbuf.h"

/* 缓冲区长度必须是 2 的幂，便于用位与实现回绕。 */
#if ((UART2_RINGBUF_SIZE == 0U) || ((UART2_RINGBUF_SIZE & (UART2_RINGBUF_SIZE - 1U)) != 0U))
#error "UART2_RINGBUF_SIZE必须为2的幂"
#endif


/*缓冲区结构体，本次采用累计器方式：写计数和读计数，将计数 和 指针操作读写分离，通过掩码的方式操作下标进行读写*/
typedef struct
{
    volatile uint32_t wr;   //写计数
    volatile uint32_t rd;   //读计数
    volatile uint32_t overflow_count;   //丢字节计数器
     uint8_t data[UART2_RINGBUF_SIZE];  //环形缓冲区
} UART2_RingBuffer_t;

static UART2_RingBuffer_t s_uart2_ring_buffer;





/**
 * @brief 清空接收缓冲区的读写计数和溢出计数。
 * @param 无。
 * @return 无。
 * @note 必须在开启USART2接收中断之前调用。
 *       正在收发时不能随意清零，否则会破坏读写双方的计数关系。
 */
void UART2_RingBuffer_Init(void)
{
    s_uart2_ring_buffer.wr = 0;
    s_uart2_ring_buffer.rd = 0;
    s_uart2_ring_buffer.overflow_count = 0;

    /*不用先清空data数组，因为wr=rd表示没有有效数据，旧内容不会被读取*/

}

/**
 * @brief 在USART2接收中断中保存一个字节。
 * @param byte 本次接收到的字节。
 * @return 1：保存成功；0：缓冲区已满，丢弃该新字节。
 * @note 只能由唯一接收中断调用，任务不能同时向这里写数据。
 *       写满时不覆盖未读数据，保留原工程的溢出处理行为。
 */
uint8_t UART2_RingBuffer_PushFromISR(uint8_t byte)
{
    uint32_t wr;
    uint32_t rd;
    uint32_t used;

    /*1.先保存当前操作使用的计数值*/
    wr = s_uart2_ring_buffer.wr;
    rd = s_uart2_ring_buffer.rd;

    /*读取已用数据空间*/
    used = (uint32_t)(wr-rd);

    if (used >= UART2_RINGBUF_SIZE)
    {
        /* 满了就丢弃一些数据，保持wr计数值，不覆盖就数据 */
        s_uart2_ring_buffer.overflow_count++;
        return 0;
    }

    __DMB();/*使用__DMB保证下面这条代码执行完，然后再往下执行代码*/

    /*2.WR累计，使用掩码操作数组的下标，并写入数据*/
    s_uart2_ring_buffer.data[wr & UART2_RINGBUF_MASK] = byte;//掩码

    
    __DMB();/*使用__DMB保证下面这条代码执行完，然后再往下执行代码*/
    s_uart2_ring_buffer.wr = wr+1;  //更新读计数
    return 1;
}



 /* @brief 在通信任务中取出一个最早到达的字节。
 * @param byte 保存读出字节的地址，不能为空。
 * @return 1：读取成功；0：缓冲区为空或参数无效。
 * @note 只能由UART2_Comm_Task调用，不能让多个任务同时读取。
 *       读完数据之后才更新rd，避免写入者提前复用该位置。
 */
uint8_t UART2_RingBuffer_Pop(uint8_t *byte)
{
    uint32_t rd;    //2个中间变量
    uint32_t wr;

    rd = s_uart2_ring_buffer.rd;    //记录旧数据，后续操作使用中间变量，防止在中断或者任务中修改
    wr = s_uart2_ring_buffer.wr;

    if (byte == NULL) return 0U; /* 检查输出指针是否合法。 */

    if (rd == wr) return 0U;     /* 比较读写计数，判断缓冲区是否为空。 */

    __DMB();/*使用__DMB保证下面这条代码执行完，然后再往下执行代码*/

    *byte = s_uart2_ring_buffer.data[rd & UART2_RINGBUF_MASK];  //取出1个子节

    __DMB();/*使用__DMB保证下面这条代码执行完，然后再往下执行代码*/

    s_uart2_ring_buffer.rd = rd + 1;
    return 1;
}





/**
 * @brief 获取因缓冲区写满而丢弃的新字节数量。
 * @param 无。
 * @return 溢出累计值，以uint32_t自然回绕。
 * @note 通信任务发现该值变化后，会放弃当前半帧并重新找帧头。
 *       保留这个接口，才能继续使用原工程的丢字节恢复逻辑。
 */
uint32_t UART2_RingBuffer_GetOverflowCount(void)
{
    return s_uart2_ring_buffer.overflow_count;
}

