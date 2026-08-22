#include "crc32.h"
#include <stdint.h>
#include <stddef.h>

/**
 * @brief 计算标准CRC32校验值（一次性，对整块数据）
 * @param data 待计算的数据指针
 * @param len 数据长度
 * @return CRC32校验值（32位无符号整数）
 *
 * @note 标准CRC-32算法参数：
 *       - 多项式: 0xEDB88320 (反向表示)
 *       - 初始值: 0xFFFFFFFF
 *       - 最终异或: 0xFFFFFFFF
 *       - 与STM32端OTA_CalcCRC32兼容
 *       逐帧CRC与固件整体CRC使用同一算法，保证可比性。
 */
uint32_t calculate_crc32(const uint8_t *data, size_t len)
{
    return crc32_stream_final(crc32_stream_update(crc32_stream_init(), data, len));
}

/**
 * @brief 流式CRC32初值
 * @return 0xFFFFFFFF（标准CRC-32初值）
 */
uint32_t crc32_stream_init(void)
{
    return 0xFFFFFFFFU;
}

/**
 * @brief 流式CRC32累加一段数据（不做最终异或，可跨chunk连续调用）
 * @param crc 上一次的累加值（首次用 crc32_stream_init() 的返回值）
 * @param data 本段数据指针
 * @param len 本段数据长度
 * @return 累加后的中间值（未做最终异或）
 */
uint32_t crc32_stream_update(uint32_t crc, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1)
            {
                crc = (crc >> 1) ^ 0xEDB88320U;  /* 反向多项式 */
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief 流式CRC32收尾：做最终异或，得到最终CRC32
 * @param crc crc32_stream_update() 的最后返回值
 * @return 最终CRC32校验值
 */
uint32_t crc32_stream_final(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFU;  /* 最终异或 */
}
