#include "MyFLASH.h"
 
 
/**
  * 函    数：FLASH读取一个32位的字
  * 参    数：Address 要读取数据的字地址
  * 返 回 值：指定地址下的数据
  */
uint32_t MyFLASH_ReadWord(uint32_t Address)
{
	return *((__IO uint32_t *)(Address));	//使用指针访问指定地址下的数据并返回
}
 
/**
  * 函    数：FLASH读取一个16位的半字
  * 参    数：Address 要读取数据的半字地址
  * 返 回 值：指定地址下的数据
  */
uint16_t MyFLASH_ReadHalfWord(uint32_t Address)
{
	return *((__IO uint16_t *)(Address));	//使用指针访问指定地址下的数据并返回
}
 
/**
  * 函    数：FLASH读取一个8位的字节
  * 参    数：Address 要读取数据的字节地址
  * 返 回 值：指定地址下的数据
  */
uint8_t MyFLASH_ReadByte(uint32_t Address)
{
	return *((__IO uint8_t *)(Address));	//使用指针访问指定地址下的数据并返回
}
/**
  * @brief  擦除指定 Flash 扇区
  * @param  start_sector   起始扇区号（STM32F407: 0~11）
  * @param  num_sectors    擦除扇区数量（最多 12-start_sector）
  * @retval HAL_StatusTypeDef 操作状态（HAL_OK 表示成功）
  */
HAL_StatusTypeDef MyFlash_EraseSectors(uint8_t start_sector, uint8_t num_sectors)
{
    HAL_StatusTypeDef status;
    FLASH_EraseInitTypeDef erase_init;
    uint32_t sector_error = 0;
 
    // 1. 参数合法性校验
    if (start_sector > 11 || num_sectors == 0 || (start_sector + num_sectors) > 12) {
        return HAL_ERROR; // 参数非法
    }
 
    // 2. 解锁 Flash 操作权限
    status = HAL_FLASH_Unlock();
    if (status != HAL_OK) {
        return status;
    }
 
    // 3. 配置擦除参数
    erase_init.TypeErase    = FLASH_TYPEERASE_SECTORS;  // 按扇区擦除
    erase_init.Banks        = FLASH_BANK_1;            // F407 单 Bank
    erase_init.Sector       = start_sector;            // 起始扇区
    erase_init.NbSectors    = num_sectors;             // 擦除扇区数量
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;   // 电压范围 2.7V~3.6V
 
    // 4. 执行擦除
    status = HAL_FLASHEx_Erase(&erase_init, &sector_error);
    if (status != HAL_OK) {
        // 可在此记录出错扇区（sector_error）
        // U1_Printf("擦除失败，错误扇区：%lu\r\n", sector_error);
    }
 
    // 5. 重新锁定 Flash
    HAL_FLASH_Lock();
 
    return status;
}
/**
  * @brief  Flash 字（32位）写入
  * @param  address  目标地址（需4字节对齐，如0x0800xxxx）
  * @param  data     待写入的32位数据
  * @retval HAL_StatusTypeDef 操作状态
  */
HAL_StatusTypeDef MyFlash_WriteWord(uint32_t address, uint32_t data)
{
    HAL_StatusTypeDef status;
 
    // 检查地址是否4字节对齐
    if (address % 4 != 0) {
        return HAL_ERROR;
    }
 
    // 解锁Flash
    status = HAL_FLASH_Unlock();
    if (status != HAL_OK) {
        return status;
    }
 
    __disable_irq(); // 禁用中断
 
    // 执行字编程
    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, data);
 
    __enable_irq();  // 启用中断
    HAL_FLASH_Lock(); // 重新锁定
 
    return status;
}
/**
  * @brief  Flash 半字（16位）写入
  * @param  address  目标地址（需2字节对齐，如0x0800xxxx）
  * @param  data     待写入的16位数据
  * @retval HAL_StatusTypeDef 操作状态
  */
HAL_StatusTypeDef MyFlash_WriteHalfWord(uint32_t address, uint16_t data)
{
    uint32_t target_word_addr = address & ~0x03; // 对齐到4字节边界
    uint32_t existing_data = *(__IO uint32_t*)target_word_addr;
    uint32_t new_data;
 
    // 检查地址是否2字节对齐
    if (address % 2 != 0) {
        return HAL_ERROR;
    }
 
    // 合并数据：替换目标半字，保留其他部分
    if ((address % 4) < 2) {
        new_data = (existing_data & 0xFFFF0000) | data; // 低16位
    } else {
        new_data = (existing_data & 0x0000FFFF) | (data << 16); // 高16位
    }
 
    // 调用字写入函数
    return MyFlash_WriteWord(target_word_addr, new_data);
}
/**
  * @brief  Flash 字节（8位）写入
  * @param  address  目标地址（任意地址，如0x0800xxxx）
  * @param  data     待写入的8位数据
  * @retval HAL_StatusTypeDef 操作状态
  */
HAL_StatusTypeDef MyFlash_WriteByte(uint32_t address, uint8_t data)
{
    uint32_t target_word_addr = address & ~0x03; // 对齐到4字节边界
    uint32_t existing_data = *(__IO uint32_t*)target_word_addr;
    uint32_t shift = (address % 4) * 8; // 计算字节偏移
    uint32_t mask = 0xFF << shift;
    uint32_t new_data;
 
    // 合并数据：替换目标字节，保留其他部分
    new_data = (existing_data & ~mask) | (data << shift);
 
    // 调用字写入函数
    return MyFlash_WriteWord(target_word_addr, new_data);
}





