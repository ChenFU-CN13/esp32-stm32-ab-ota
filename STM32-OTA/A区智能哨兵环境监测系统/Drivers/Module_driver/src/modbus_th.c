#include "modbus_th.h"
#include <string.h>

#define MBTH_TX_TIMEOUT_MS      100U
#define MBTH_RX_TIMEOUT_MS      200U

#define MBTH_MAX_REGS           4U
#define MBTH_RX_BUF_SIZE        13U


/**
 * @brief 把HAL串口错误转换为模块错误码
 */
static MBTH_Status_t MBTH_ConvertHalStatus(HAL_StatusTypeDef hal_status)
{
    if (hal_status == HAL_TIMEOUT)
    {
        return MBTH_ERR_TIMEOUT;
    }

    return MBTH_ERR_UART;
}




/**
 * @brief 清理串口中可能残留的旧数据
 */
static void MBTH_ClearRx(UART_HandleTypeDef *huart)
{

	uint8_t i;

    /*
     * STM32清除ORE需要依次读取SR和DR。
     * 当前HAL版本提供了这个宏。
     */
	__HAL_UART_CLEAR_OREFLAG(huart);

    /*
     * 限制最多清理16次，避免线路持续有干扰时卡死。
     */
    for (i = 0U; i < 16U; i++)
    {
        if (__HAL_UART_GET_FLAG(huart, UART_FLAG_RXNE) == RESET)	//如果程序还未读取数据，则不清理
        {
            break;
        }

        (void)__HAL_UART_FLUSH_DRREGISTER(huart);
    }





}





 /**
 * @brief 计算Modbus RTU CRC16
 * @param data 待计算的数据
 * @param len 数据长度
 * @return CRC16计算结果
 */
uint16_t MBTH_CRC16(const uint8_t *data, uint16_t len)
{
    uint16_t crc;
    uint16_t index;
    uint8_t bit;

    crc = 0xFFFFU;

    for (index = 0U; index < len; index++)
    {
        crc ^= data[index];

        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 0x0001U) != 0U)
            {
                crc = (crc >> 1U) ^ 0xA001U;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }

    return crc;

}



/**
 * @brief 读取Modbus寄存器
 */
MBTH_Status_t MBTH_ReadRegs(UART_HandleTypeDef *huart,
                            uint8_t slave_addr,
                            uint8_t function,
                            uint16_t start_reg,
                            uint16_t reg_count,
                            uint8_t *data,
                            uint8_t data_size)
{


	uint8_t tx_buf[8];
	uint8_t rx_buf[MBTH_RX_BUF_SIZE];

	uint16_t crc_calc;		//计算的CRC
	uint16_t crc_recv;		//收到的CRC

	uint8_t frame_len;		//帧长度
    uint8_t expected_bytes;	//期望的字节

	HAL_StatusTypeDef hal_status;	//串口句柄


    if ((huart == NULL) || (data == NULL))
    {
        return MBTH_ERR_PARAM;
    }

    if ((function != MBTH_FUNC_READ_HOLDING) &&
        (function != MBTH_FUNC_READ_INPUT))
    {
        return MBTH_ERR_PARAM;
    }

    if ((reg_count == 0U) || (reg_count > MBTH_MAX_REGS))
    {
        return MBTH_ERR_PARAM;
    }

    expected_bytes = (uint8_t)(reg_count * 2U);

    if (data_size < expected_bytes)
    {
        return MBTH_ERR_PARAM;
    }



    /*
     * Modbus读寄存器请求帧：
     * 地址 + 功能码 + 起始地址 + 寄存器数量 + CRC
     */
    tx_buf[0] = slave_addr;
	tx_buf[1] = function;
	tx_buf[2] = (uint8_t)(start_reg >> 8);				//高位
	tx_buf[3] = (uint8_t)(start_reg & 0XFF);			//低位
	tx_buf[4] = (uint8_t)(reg_count >> 8);				//高位
	tx_buf[5] = (uint8_t)(reg_count& 0XFF);				//低位


	crc_calc = MBTH_CRC16(tx_buf,6);


    /* Modbus CRC低字节先发送 */
	tx_buf[6] = (uint8_t)(crc_calc & 0XFF);					//低位
	tx_buf[7] = (uint8_t)(crc_calc >> 8);					//低位



    /*
     * 9600bps下3.5个字符时间约为3.65ms。
     * 这里等待5ms，保证帧间隔。并清除串口残留数据
		之后才发送数据帧请求数据
     */

    HAL_Delay(5U);

    MBTH_ClearRx(huart);

	hal_status = HAL_UART_Transmit(huart,tx_buf,sizeof(tx_buf),MBTH_TX_TIMEOUT_MS);



    if (hal_status != HAL_OK)
    {
        return MBTH_ConvertHalStatus(hal_status);
    }



    /*
     * 先接收响应头：
     * 从机地址 + 功能码 + 字节数/异常码，如果出现问题后立刻处理
		如果没有问题再往下接收数据
     */
    hal_status = HAL_UART_Receive(huart,
                                  rx_buf,
                                  3U,
                                  MBTH_RX_TIMEOUT_MS);

    if (hal_status != HAL_OK)
    {
        return MBTH_ConvertHalStatus(hal_status);
    }

    if (rx_buf[0] != slave_addr)
    {
        return MBTH_ERR_FRAME;
    }


    /*
     * Modbus异常响应：
     * 地址 + (功能码 | 0x80) + 异常码 + CRC，如果出现异常则反馈错误
     */
    if (rx_buf[1] == (uint8_t)(function | 0x80U))
    {
        hal_status = HAL_UART_Receive(huart,
                                      &rx_buf[3],
                                      2U,
                                      MBTH_RX_TIMEOUT_MS);
		/*这里在接收2个字节，是CRC字节，因为，标准Modbus异常反馈为5字节：地址+功能码+异常码+CRCL+CRCH*/

        if (hal_status != HAL_OK)
        {
            return MBTH_ConvertHalStatus(hal_status);
        }

        crc_calc = MBTH_CRC16(rx_buf, 3U);
        crc_recv = (uint16_t)rx_buf[3] |
                   ((uint16_t)rx_buf[4] << 8U);

        if (crc_calc != crc_recv)
        {
            return MBTH_ERR_CRC;
        }

        return MBTH_ERR_EXCEPTION;
    }

	/*下面两个if判断返回的功能码是否正确，以及期望字数是否符合，不合法则都记录错误*/
    if (rx_buf[1] != function)
    {
        return MBTH_ERR_FRAME;
    }

    if (rx_buf[2] != expected_bytes)
    {
        return MBTH_ERR_FRAME;
    }



	/*运行到这里说明没有异常
	* 继续接收：（前面已经接收到了头部数据，所以不是从头开始接收，还有6字节未接收完）
     * 寄存器数据 + 2字节CRC
     */
    hal_status = HAL_UART_Receive(huart,
                                  &rx_buf[3],
                                  (uint16_t)(expected_bytes + 2U),
                                  MBTH_RX_TIMEOUT_MS);
    if (hal_status != HAL_OK)
    {
        return MBTH_ConvertHalStatus(hal_status);
    }



    frame_len = (uint8_t)(3U + expected_bytes + 2U);

    crc_calc = MBTH_CRC16(rx_buf, (uint16_t)(frame_len - 2U));

    crc_recv = (uint16_t)rx_buf[frame_len - 2U] |
               ((uint16_t)rx_buf[frame_len - 1U] << 8U);

    if (crc_calc != crc_recv)
    {
        return MBTH_ERR_CRC;
    }

    memcpy(data, &rx_buf[3], expected_bytes);

    return MBTH_OK;


}






/**
 * @brief 读取16位定点格式温湿度
 */
MBTH_Status_t MBTH_ReadFixed(UART_HandleTypeDef *huart,
                             uint8_t slave_addr,
                             MBTH_FixedData_t *result)
{

    uint8_t data[4];		//接收缓存

    uint16_t raw_temp;		//温度
    uint16_t raw_humi;		//湿度

    MBTH_Status_t status;

    if (result == NULL)
    {
        return MBTH_ERR_PARAM;
    }


	status = MBTH_ReadRegs(huart,slave_addr,MBTH_FUNC_READ_INPUT,0X0000,2,data,sizeof(data));


	if (status != MBTH_OK)
    {
        return status;
    }


	/* Modbus寄存器数据高字节在前 */
    raw_temp = ((uint16_t)data[0] << 8U) | data[1];
    raw_humi = ((uint16_t)data[2] << 8U) | data[3];



    /*
     * 手册规定：
     * 小于10000表示正温度；
     * 大于等于10000表示负温度。
     */

    if (raw_temp >= 10000U)
    {
        result->temp_x10 = -(int16_t)(raw_temp - 10000U);
    }
    else
    {
        result->temp_x10 = (int16_t)raw_temp;
    }

    result->humi_x10 = raw_humi;

    return MBTH_OK;







}
























/**
 * @brief 读取32位浮点格式温湿度
 */
MBTH_Status_t MBTH_ReadFloat(UART_HandleTypeDef *huart,
                             uint8_t slave_addr,
                             MBTH_FloatData_t *result)
{
    uint8_t data[8];

    uint32_t temp_bits;
    uint32_t humi_bits;

    MBTH_Status_t status;

    if (result == NULL)
    {
        return MBTH_ERR_PARAM;
    }

    if (sizeof(float) != 4U)
    {
        return MBTH_ERR_FRAME;
    }

    /*
     * 输入寄存器：
     * 0x0002～0x0003：温度，32位浮点数
     * 0x0004～0x0005：湿度，32位浮点数
     */
    status = MBTH_ReadRegs(huart,
                           slave_addr,
                           MBTH_FUNC_READ_INPUT,
                           0x0002U,
                           4U,
                           data,
                           sizeof(data));

    if (status != MBTH_OK)
    {
        return status;
    }

    /*
     * 手册规定为32位大端模式，字节顺序为：
     * 高字节 -> 低字节，也就是ABCD顺序。
     */
    temp_bits = ((uint32_t)data[0] << 24U) |
                ((uint32_t)data[1] << 16U) |
                ((uint32_t)data[2] << 8U)  |
                ((uint32_t)data[3]);

    humi_bits = ((uint32_t)data[4] << 24U) |
                ((uint32_t)data[5] << 16U) |
                ((uint32_t)data[6] << 8U)  |
                ((uint32_t)data[7]);

    /*
     * 不直接把uint32_t指针强转为float指针，
     * 使用memcpy可以避免严格别名问题。
     */
    memcpy(&result->temperature, &temp_bits, sizeof(float));
    memcpy(&result->humidity, &humi_bits, sizeof(float));

    return MBTH_OK;
}
