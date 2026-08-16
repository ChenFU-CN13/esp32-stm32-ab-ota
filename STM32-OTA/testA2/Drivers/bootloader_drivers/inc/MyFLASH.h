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


#endif








