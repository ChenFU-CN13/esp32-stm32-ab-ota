#ifndef MYFLASH_H
#define MYFLASH_H

#include "main.h"


//函数声明
uint32_t MyFLASH_ReadWord(uint32_t Address);
uint16_t MyFLASH_ReadHalfWord(uint32_t Address);
uint8_t MyFLASH_ReadByte(uint32_t Address);
HAL_StatusTypeDef MyFlash_EraseSectors(uint8_t start_sector, uint8_t num_sectors);
HAL_StatusTypeDef MyFlash_WriteWord(uint32_t address, uint32_t data);
HAL_StatusTypeDef MyFlash_WriteHalfWord(uint32_t address, uint16_t data);
HAL_StatusTypeDef MyFlash_WriteByte(uint32_t address, uint8_t data);






/**
  * @brief  Flash 缓冲区批量写入（核心函数）
  * @param  address  目标起始地址（需4字节对齐）
  * @param  pBuffer  待写入的数据缓冲区
  * @param  length   写入长度（字节数，必须是4的倍数）
  * @retval HAL_StatusTypeDef 操作状态
  * @note   依赖已实现的 MyFlash_WriteWord/Byte/HalfWord 函数
  */
HAL_StatusTypeDef MyFlash_WriteBuffer(uint32_t address, uint8_t *pBuffer, uint32_t length);



/**
  * @brief  实现16位的CRC校验
  * @param  uint8_t* data	待校验数据数组，
  * @param  uint16_t DataLength	 待校验的数据长度
  * @retval 16位的校验结果
  *	注意：1个1个字节地运算，对1个字节进行1位1位地运算，1个字节刚好8位
  在xmodem协议中，是校验128个字节的固件数据，不是整个数据包
  */
uint16_t Xmodem_CRC16(uint8_t *data,uint16_t datalen);

#endif








