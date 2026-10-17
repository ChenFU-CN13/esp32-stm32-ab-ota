#ifndef __MODBUS_TH_H
#define __MODBUS_TH_H

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MBTH_DEFAULT_ADDR             0x01U			//默认从机地址

#define MBTH_FUNC_READ_HOLDING        0x03U			//读保持寄存器
#define MBTH_FUNC_READ_INPUT          0x04U			//读输入寄存器

/**
 * @brief Modbus温湿度模块操作结果
 */
typedef enum
{
    MBTH_OK = 0,
    MBTH_ERR_PARAM,
    MBTH_ERR_TIMEOUT,
    MBTH_ERR_UART,
    MBTH_ERR_FRAME,
    MBTH_ERR_CRC,
    MBTH_ERR_EXCEPTION
} MBTH_Status_t;

/**
 * @brief 16位定点格式温湿度数据
 */
typedef struct
{
    int16_t  temp_x10;       /* 温度扩大10倍，例如253表示25.3℃ */
    uint16_t humi_x10;       /* 湿度扩大10倍，例如508表示50.8%RH */
} MBTH_FixedData_t;

/**
 * @brief 32位浮点格式温湿度数据
 */
typedef struct
{
    float temperature;       /* 温度，单位℃ */
    float humidity;          /* 湿度，单位%RH */
} MBTH_FloatData_t;

/**
 * @brief 计算Modbus RTU CRC16
 * @param data 待计算的数据
 * @param len 数据长度
 * @return CRC16计算结果
 */
uint16_t MBTH_CRC16(const uint8_t *data, uint16_t len);

/**
 * @brief 读取Modbus寄存器
 * @param huart 使用的串口
 * @param slave_addr 从机地址
 * @param function 功能码，只允许0x03或0x04
 * @param start_reg 起始寄存器地址
 * @param reg_count 寄存器数量，当前最多读取4个
 * @param data 返回的数据区，只包含寄存器数据
 * @param data_size 数据区容量
 * @return 操作结果
 */
MBTH_Status_t MBTH_ReadRegs(UART_HandleTypeDef *huart,
                            uint8_t slave_addr,
                            uint8_t function,
                            uint16_t start_reg,
                            uint16_t reg_count,
                            uint8_t *data,
                            uint8_t data_size);

/**
 * @brief 读取16位定点格式温湿度
 * @param huart 使用的串口
 * @param slave_addr 从机地址
 * @param result 返回的温湿度数据
 * @return 操作结果
 */
MBTH_Status_t MBTH_ReadFixed(UART_HandleTypeDef *huart,
                             uint8_t slave_addr,
                             MBTH_FixedData_t *result);

/**
 * @brief 读取32位浮点格式温湿度
 * @param huart 使用的串口
 * @param slave_addr 从机地址
 * @param result 返回的温湿度数据
 * @return 操作结果
 */
MBTH_Status_t MBTH_ReadFloat(UART_HandleTypeDef *huart,
                             uint8_t slave_addr,
                             MBTH_FloatData_t *result);

#ifdef __cplusplus
}
#endif

#endif