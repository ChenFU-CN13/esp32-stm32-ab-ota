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
         U1_Printf("擦除失败，错误扇区：%lu\r\n", sector_error);
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








/**
  * @brief  Flash 缓冲区批量写入（核心函数）
  * @param  address  目标起始地址（需4字节对齐）
  * @param  pBuffer  待写入的数据缓冲区
  * @param  length   写入长度（字节数，必须是4的倍数）
  * @retval HAL_StatusTypeDef 操作状态
  * @note   依赖已实现的 MyFlash_WriteWord/Byte/HalfWord 函数
  */
HAL_StatusTypeDef MyFlash_WriteBuffer(uint32_t address, uint8_t *pBuffer, uint32_t length)
{
    HAL_StatusTypeDef status = HAL_OK;
    uint32_t i = 0;

    // 1. 参数合法性校验
    if (pBuffer == NULL || length == 0) {
        return HAL_ERROR;
    }
    // 地址必须4字节对齐（STM32F4 Flash编程要求）
    if (address % 4 != 0) {
        return HAL_ERROR;
    }
    // 长度必须4字节对齐（和你代码中的校验逻辑一致）
    if (length % 4 != 0) {
        return HAL_ERROR;
    }

    // 2. 按字（32位）批量写入（效率最高）
    while (i < length) {
        // 拼接4个字节为1个32位字（小端模式，符合STM32存储习惯）
        uint32_t write_word = (pBuffer[i+3] << 24) | (pBuffer[i+2] << 16) | 
                              (pBuffer[i+1] << 8)  | pBuffer[i];
        
        // 调用已实现的字写入函数
        status = MyFlash_WriteWord(address + i, write_word);
        if (status != HAL_OK) {
            return status; // 写入失败直接返回
        }

        i += 4; // 每次写4字节，偏移+4
    }

    return status;
}












/**
  * @brief  实现16位的CRC校验
  * @param  uint8_t* data	待校验数据数组，
  * @param  uint16_t DataLength	 待校验的数据长度
  * @retval 16位的校验结果
  *	注意：1个1个字节地运算，对1个字节进行1位1位地运算，1个字节刚好8位
  */
uint16_t Xmodem_CRC16(uint8_t *data,uint16_t datalen)
{
	uint16_t CRC_Init = 0X0000;//初始值
	uint16_t CRC_Ipity = 0x1021;//多项式

	
	while(datalen--)
	{
		//在内部将进来的 字节 与初始值进行运算
		//第1个字节运算完后的结果，作为和 第2个字节 进行运算，往后以此类推
		CRC_Init = (*data << 8) ^ CRC_Init;		//左移8位，取最高位
		
		for(uint8_t i = 0;i<8;i++)
        {
			//以此运行1个字节的8位
			//进行判是否进行 异或多项式运算
			if(CRC_Init & 0x8000)
            {
				CRC_Init = (CRC_Init << 1)^CRC_Ipity;
            }
			else
			{
				CRC_Init = (CRC_Init << 1);
			}
			
        }
		data++;	//注意传入的data是个地址，并不是真正的值，所以这里++，就相当于移动下个字节计算
	}
	
	return CRC_Init;//最终累计的结果就是检验值
}
























































