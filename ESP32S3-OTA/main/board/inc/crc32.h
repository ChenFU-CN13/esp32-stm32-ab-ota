#ifndef _CRC32_H_
#define _CRC32_H_

#include <stdint.h>
#include <stddef.h>

/**
 * @brief 计算CRC32校验值（一次性，对整块数据）
 * @param data 待计算的数据指针
 * @param len 数据长度
 * @return CRC32校验值
 * @note 标准CRC-32：多项式0xEDB88320，初值0xFFFFFFFF，最终异或0xFFFFFFFF，与STM32端OTA_CalcCRC32兼容
 */
uint32_t calculate_crc32(const uint8_t *data, size_t len);

/**
 * @brief 流式CRC32：用于对分块下载/读取的数据累加计算整体CRC32
 *        三步用法：crc = crc32_stream_init(); crc = crc32_stream_update(crc, chunk, n); ... crc = crc32_stream_final(crc);
 *        结果与 calculate_crc32() 对同一份数据完全一致。
 */
uint32_t crc32_stream_init(void);                                              /* 返回初值 0xFFFFFFFF */
uint32_t crc32_stream_update(uint32_t crc, const uint8_t *data, size_t len);   /* 累加一段数据，不做最终异或 */
uint32_t crc32_stream_final(uint32_t crc);                                     /* 做最终异或，返回最终CRC32 */

#endif
