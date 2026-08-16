#include "W25Q128.h"









/**
 * @brief   读取W25Q128芯片ID，验证芯片在线状态
 * @param   无
 * @retval  24位芯片ID：厂商ID(0xEF) + 设备ID(0x18) + 容量ID；读取失败返回0
 */
uint32_t W25Q128_ReadID(void)
{
	uint32_t id = 0;
	uint8_t buf[4]={0};

	HAL_StatusTypeDef status = HAL_ERROR;
	
	W25Q128_CS_LOW();
	
	status = HAL_SPI_Transmit(&hspi1,(uint8_t[]){W25Q128_CMD_READ_ID},1,W25Q128_TIMEOUT);
	
	if(status==HAL_OK)
    {
		status = HAL_SPI_Receive(&hspi1,buf,4,W25Q128_TIMEOUT);
		
			
		if(status==HAL_OK)
		{
			
			            // 打印原始字节，调试用
            U1_Printf("Raw ID: buf[0]=0x%X, buf[1]=0x%X, buf[2]=0x%X, buf[3]=0x%X\r\n",
                      buf[0], buf[1], buf[2], buf[3]);
			
			id = (uint32_t)buf[0] << 16 | buf[1] << 8 | buf[2];


		}

    }
	else
	{
	
		U1_Printf("W25Q128_ERROR!");
	}
	
	W25Q128_CS_HIGH();
	return id;

}







/**
 * @brief   读取W25Q128忙位状态
 * @param   无
 * @retval  1：芯片忙（擦除/写入中）；0：芯片空闲
 */
uint8_t W25Q128_ReadBusy(void)
{
	uint8_t status_reg = 0XFF;
	W25Q128_CS_LOW(); // 新增：选中芯片
	HAL_SPI_Transmit(&hspi1,(uint8_t[]){W25Q128_CMD_READ_STATUS},1,W25Q128_TIMEOUT);
	HAL_SPI_Receive(&hspi1,&status_reg,1,W25Q128_TIMEOUT);//返回状态寄存器1的8位数据，然后只取第0位值
	W25Q128_CS_HIGH(); // 新增：释放芯片
	return (status_reg & 0x01);
}




/**
 * @brief   等待W25Q128芯片空闲（循环检测忙位）
 * @param   无
 * @retval  HAL_OK：芯片进入空闲状态；HAL_TIMEOUT：等待超时
 */
HAL_StatusTypeDef W25Q128_WaitIdle(void)
{
	
	uint32_t TIMEOUT=0;
	//循环检测BUSY位，超3000ms（覆盖整片檫除的最大耗时1000ms）退出，防卡死

	while(W25Q128_ReadBusy())
	{
		HAL_Delay(1);
		TIMEOUT++;
		if(TIMEOUT > 5000)
		{
			U1_Printf("BUSY BIT wait timeout!\r\n");
			return HAL_TIMEOUT;
		}
	}
	return HAL_OK;

}


/**
 * @brief   执行W25Q128写使能操作（擦除/写入前必须调用）
 * @param   无
 * @retval  HAL_OK：操作成功；HAL_ERROR/HAL_TIMEOUT：操作失败/超时
 */
HAL_StatusTypeDef W25Q128_WriteEnable(void)
{
    HAL_StatusTypeDef status = HAL_ERROR;
	W25Q128_CS_LOW();
	status = HAL_SPI_Transmit(&hspi1,(uint8_t[]){W25Q128_CMD_WRITE_EN},1,W25Q128_TIMEOUT);
	W25Q128_CS_HIGH();
	return status;

}


/**
 * @brief   执行W25Q128睡眠模式
 * @param   无
 * @retval  HAL_OK：操作成功；HAL_ERROR/HAL_TIMEOUT：操作失败/超时
 */
HAL_StatusTypeDef W25Q128_PowerDown(void) {
	
    HAL_StatusTypeDef status = HAL_ERROR;
	
	W25Q128_CS_LOW();	//CS=0
	status = HAL_SPI_Transmit(&hspi1,(uint8_t[]){W25Q128_CMD_Power_Down},1,W25Q128_TIMEOUT);
	W25Q128_CS_HIGH();	//CS=1
	HAL_Delay(1); //等待TPD
	return status;
}


/**
 * @brief   执行W25Q128唤醒模式
 * @param   无
 * @retval  HAL_OK：操作成功；HAL_ERROR/HAL_TIMEOUT：操作失败/超时
 */
HAL_StatusTypeDef W25Q128_WakeUp(void) {
	
    HAL_StatusTypeDef status = HAL_ERROR;
	
	W25Q128_CS_LOW();	//CS=0
	status = HAL_SPI_Transmit(&hspi1,(uint8_t[]){W25Q128_CMD_WAKEP_UP},1,W25Q128_TIMEOUT);
	W25Q128_CS_HIGH();	//CS=1
	HAL_Delay(1); //等待TRES1
	return status;
}


/**
 * @brief   擦除W25Q128指定扇区（4KB）
 * @param   addr：扇区起始地址（必须4KB对齐，如0x000000、0x001000、0x002000）
 * @retval  HAL_OK：擦除成功；HAL_ERROR：地址越界；HAL_TIMEOUT：通信/等待超时
 */
HAL_StatusTypeDef W25Q128_SectorErase(uint32_t addr)
{
    HAL_StatusTypeDef status = HAL_ERROR;
	
	 uint8_t erase_cmd[4] = {0};		//新增数组归纳发送的指令+地址，避免发送指令耗时较长处理
	erase_cmd[0] = W25Q128_CMD_SECTOR_ERASE;
	erase_cmd[1] = (uint8_t)((addr >> 16) & 0xFF);
	erase_cmd[2] = (uint8_t)((addr >> 8) & 0xFF);
	erase_cmd[3] = (uint8_t)(addr & 0xFF);

	 
	 
	 
	 
	 
	// 校验1：地址越界检查
	if (addr >= W25Q128_TOTAL_SIZE)
    {
		return HAL_ERROR;
    }
	
	
    // 校验2：地址4KB对齐检查
	if(addr % W25Q128_SECTOR_SIZE !=0)
    {
		return HAL_ERROR;//非法
    
    }
	
		//先写使能
	    status = W25Q128_WriteEnable();
		if (status != HAL_OK)
		{
			return status;
		}
		
		
		W25Q128_CS_LOW();//发生檫除指令，这里 & 上FF是为了保证只发送最低位的字节
		status = HAL_SPI_Transmit(&hspi1,erase_cmd,4, W25Q128_TIMEOUT);
		W25Q128_CS_HIGH();
													  
		if(status!=HAL_OK)
		{
			return status;
		}	

		    // 等待擦除完成
		status = W25Q128_WaitIdle();

		
		return status;
		
		
		
		
		
}





/**
 * @brief   向W25Q128指定页写入数据（≤256字节）
 * @param   addr：页起始地址（必须256字节对齐）
 * @param   pData：待写入数据指针（非NULL）
 * @param   len：写入长度（≤256字节）
 * @retval  HAL_OK：写入成功；HAL_ERROR：地址越界/长度超限/指针为空；HAL_TIMEOUT：通信/等待超时
 */
HAL_StatusTypeDef W25Q128_PageWrite(uint32_t addr, uint8_t *pData, uint16_t len)
{
    HAL_StatusTypeDef status = HAL_ERROR;
	
	uint8_t PageWrite_cmd[4] = {0};		//新增数组归纳发送的指令+地址，避免发送指令耗时较长处理
	PageWrite_cmd[0] = W25Q128_CMD_WRITE;
	PageWrite_cmd[1] = (uint8_t)((addr >> 16) & 0xFF);
	PageWrite_cmd[2] = (uint8_t)((addr >> 8) & 0xFF);
	PageWrite_cmd[3] = (uint8_t)(addr & 0xFF);

	
	
    // 校验1：指针空检查
    if (pData == NULL)
    {
        return HAL_ERROR;
    }
    // 校验2：地址越界检查
    if (addr >= W25Q128_TOTAL_SIZE)
    {
        return HAL_ERROR;
    }
    // 校验3：地址256字节对齐检查
    if ((addr % W25Q128_PAGE_SIZE) != 0U)
    {
        return HAL_ERROR;
    }
    // 校验4：写入长度超限检查
    if (len > W25Q128_PAGE_SIZE)
    {
        return HAL_ERROR;
    }
    if (len == 0U)
    {
        return HAL_OK; // 长度为0，直接返回成功
    }

    // 执行写使能
    status = W25Q128_WriteEnable();
    if (status != HAL_OK)
    {
        return status;
    }

    W25Q128_CS_LOW();
    // 发送写入指令 + 3字节地址
    status = HAL_SPI_Transmit(&hspi1,PageWrite_cmd,4, W25Q128_TIMEOUT);
    if (status != HAL_OK)
    {
        W25Q128_CS_HIGH();
        return status;
    }
    // 发送写入数据
    status = HAL_SPI_Transmit(&hspi1, pData, len, W25Q128_TIMEOUT);
    W25Q128_CS_HIGH();

    if (status != HAL_OK)
    {
        return status;
    }

    // 等待写入完成
    status = W25Q128_WaitIdle();

    return status;
	
	
}





/**
 * @brief   W25Q128跨扇区批量写入（自动分页+自动扇区擦除）
 * @param   start_addr：写入起始地址（无需对齐，函数内部自动处理）
 * @param   pData：待写入数据缓冲区指针（非NULL）
 * @param   total_len：待写入数据总长度（任意长度，支持跨扇区/跨页）
 * @param   buf_max_len：pData缓冲区的最大长度（防溢出）
 * @retval  HAL_OK：写入成功；HAL_ERROR：参数错误；HAL_TIMEOUT：通信/擦除/写入超时
 */
HAL_StatusTypeDef W25Q128_MassWrite(uint32_t start_addr,
									uint8_t *pData,
									uint32_t total_len,
									uint32_t buf_max_len
									)
{
	HAL_StatusTypeDef status = HAL_ERROR;
	uint32_t curr_addr = start_addr;		// 当前写入地址
	uint8_t *curr_data = pData;				// 当前数据指针
    uint32_t remain_len = total_len;    	// 剩余未写入长度
	uint16_t  once_write_len = 0;			//单次页写入长度
	uint32_t curr_sector = 0;				//当前地址默认扇区
	uint32_t last_erase_sector = 0xFFFFFFFFU;//上次檫除的扇区（避免重复檫除）
	
	  /*判断写入长度是否大于数组长度，是否空指针，是否溢出存储芯片空间*/
	if(pData==NULL || total_len == 0 || total_len>buf_max_len ||
		(start_addr + total_len)>W25Q128_TOTAL_SIZE)
    {
		
        U1_Printf("MassWrite param error!\r\n");
        return HAL_ERROR;
    
    }
	
	
	
	while (remain_len > 0)
    {
		
		//1.计算 当前1次页写入的字节长度 = 1页长度 - （目标写入地址 % 1页长度）
		//注意：curr_addr % W25Q128_SECTOR_SIZE 计算页地址的偏移量
		once_write_len = W25Q128_PAGE_SIZE - (curr_addr % W25Q128_PAGE_SIZE);
		if(once_write_len > remain_len)
        {
			//说明此时剩余未写入的长度不足1页，直接取剩余长度
			once_write_len=remain_len;
        }
		
		
		
		//2.计算当前地址所在扇区，如果没有檫除则檫除（避免重复檫除同一扇区），计算公式原理同上
		//curr_addr % W25Q128_SECTOR_SIZE 计算扇区偏移量，比如地址0x3250 那么偏移量即是250，0x3250 - 0X250 =0X3000 即可得该扇区起始地址
		curr_sector = curr_addr - (curr_addr % W25Q128_SECTOR_SIZE);
		if(curr_sector != last_erase_sector)
        {
			status = W25Q128_SectorErase(curr_sector);
            if (status != HAL_OK)
            {
                U1_Printf("Sector %06X erase failed! Status: %d\r\n", curr_sector, status);
                return status;
            }
            last_erase_sector = curr_sector; // 更新已擦除扇区
            //U1_Printf("Sector %06X erase success!\r\n", curr_sector);
        }
		
		
		
		//3.执行当前页写入（地址已自动对齐）
		//注意：curr_addr % W25Q128_SECTOR_SIZE 计算页地址的偏移量
		status = W25Q128_PageWrite(curr_addr - (curr_addr % W25Q128_PAGE_SIZE),curr_data,once_write_len);
        if (status != HAL_OK)
        {
            U1_Printf("Page %06X write failed! Status: %d\r\n", curr_addr, status);
            return status;
        }
        //U1_Printf("Write %d bytes to page %06X success!\r\n", once_write_len, curr_addr);
		
		 // 4. 更新地址、数据指针、剩余长度
		curr_addr += once_write_len;
		curr_data += once_write_len;
		remain_len -= once_write_len;
		
    }
	
	
	
	
	//U1_Printf("==================== MassWrite total %d bytes success! ====================\r\n", total_len);
    return HAL_OK;
	
	
	
}






/**
 * @brief   从W25Q128指定地址读取任意长度数据（防缓冲区溢出）
 * @param   addr：读取起始地址
 * @param   pBuffer：存储读取数据的缓冲区指针（非NULL）
 * @param   bufferLen：期望读取的长度
 * @param   bufferMaxLen：外部缓冲区实际最大长度（防止溢出）
 * @retval  HAL_OK：读取成功；HAL_ERROR：地址越界/长度超限/指针为空；HAL_TIMEOUT：通信超时
 */
HAL_StatusTypeDef W25Q128_Read(uint32_t addr, uint8_t *pBuffer, uint32_t bufferLen, uint32_t bufferMaxLen)
{

    HAL_StatusTypeDef status = HAL_ERROR;
	uint8_t Read_cmd[4] = {0};		//新增数组归纳发送的指令+地址，避免发送指令耗时较长处理
	Read_cmd[0] = W25Q128_CMD_READ;
	Read_cmd[1] = (uint8_t)((addr >> 16) & 0xFF);
	Read_cmd[2] = (uint8_t)((addr >> 8) & 0xFF);
	Read_cmd[3] = (uint8_t)(addr & 0xFF);
	
    // 校验1：指针空检查
    if (pBuffer == NULL)
    {
        return HAL_ERROR;
    }
    // 校验2：地址越界检查
    if (addr >= W25Q128_TOTAL_SIZE)
    {
        return HAL_ERROR;
    }
    // 校验3：读取长度超限检查（防芯片越界+防缓冲区溢出）
    if (bufferLen == 0U || bufferLen > bufferMaxLen || (addr + bufferLen) > W25Q128_TOTAL_SIZE)
    {
        return HAL_ERROR;
    }
	
	
	
	 W25Q128_CS_LOW();
    // 发送读指令 + 3字节地址
    status = HAL_SPI_Transmit(&hspi1,Read_cmd,4, W25Q128_TIMEOUT);
    if (status != HAL_OK)
    {
        W25Q128_CS_HIGH();
        return status;
    }
	//接收数据
	status = HAL_SPI_Receive(&hspi1,pBuffer,bufferLen,W25Q128_TIMEOUT);
    W25Q128_CS_HIGH();
    return status;


}





/**
 * @brief   从W25Q128指定地址读取任意长度数据（防缓冲区溢出），高速连续读取多个字节，任意全局地址， 速度大约是常规读取的2倍
 * @param   addr：读取起始地址
 * @param   pBuffer：存储读取数据的缓冲区指针（非NULL）
 * @param   bufferLen：期望读取的长度
 * @param   bufferMaxLen：外部缓冲区实际最大长度（防止溢出）
 * @retval  HAL_OK：读取成功；HAL_ERROR：地址越界/长度超限/指针为空；HAL_TIMEOUT：通信超时
 */
HAL_StatusTypeDef W25Q128_Flash_ReadBytes(uint32_t addr, uint8_t *pBuffer, uint32_t bufferLen, uint32_t bufferMaxLen)
{

    HAL_StatusTypeDef status = HAL_ERROR;
	uint8_t Read_cmd[5] = {0};		//新增数组归纳发送的指令+地址，避免发送指令耗时较长处理
	Read_cmd[0] = W25Q128_CMD_FLASH_READ;
	Read_cmd[1] = (uint8_t)((addr >> 16) & 0xFF);
	Read_cmd[2] = (uint8_t)((addr >> 8) & 0xFF);
	Read_cmd[3] = (uint8_t)(addr & 0xFF);
	Read_cmd[4] = 0xFF;
	
    // 校验1：指针空检查
    if (pBuffer == NULL)
    {
        return HAL_ERROR;
    }
    // 校验2：地址越界检查
    if (addr >= W25Q128_TOTAL_SIZE)
    {
        return HAL_ERROR;
    }
    // 校验3：读取长度超限检查（防芯片越界+防缓冲区溢出）
    if (bufferLen == 0U || bufferLen > bufferMaxLen || (addr + bufferLen) > W25Q128_TOTAL_SIZE)
    {
        return HAL_ERROR;
    }
	
	
	
	 W25Q128_CS_LOW();
    // 发送读指令 + 3字节地址
    status = HAL_SPI_Transmit(&hspi1,Read_cmd,5, W25Q128_TIMEOUT);
    if (status != HAL_OK)
    {
        W25Q128_CS_HIGH();
        return status;
    }
	//接收数据
	status = HAL_SPI_Receive(&hspi1,pBuffer,bufferLen,W25Q128_TIMEOUT);
    W25Q128_CS_HIGH();
    return status;


}









/**
 * @brief   擦除W25Q128整片存储空间（慎用，耗时约100ms）
 * @param   无
 * @retval  HAL_OK：擦除成功；HAL_TIMEOUT：等待超时；HAL_ERROR：操作失败
 */
HAL_StatusTypeDef W25Q128_ChipErase(void)
{

	    HAL_StatusTypeDef status = HAL_ERROR;
	
		// 执行写使能
		status = W25Q128_WriteEnable();
		if (status != HAL_OK)
		{
			return status;
		}

			 W25Q128_CS_LOW();
			 status = HAL_SPI_Transmit(&hspi1,(uint8_t[]){W25Q128_CMD_CHIP_ERASE},1,W25Q128_TIMEOUT);
			 W25Q128_CS_HIGH();
			 
			if (status != HAL_OK)
			{
				return status;
			}
			
			
    // 等待整片擦除完成（耗时约100ms）
    status = W25Q128_WaitIdle();
    return status;
			
}


/**
 * @brief   擦除W25Q128指定64KB块（耗时1000ms）
 * @param   addr：64KB块起始地址（必须64KB对齐，如0x000000、0x010000）
 * @retval  HAL_OK：擦除成功；HAL_ERROR：地址越界/未对齐；HAL_TIMEOUT：通信/等待超时
 */
HAL_StatusTypeDef W25Q128_64KBBlockErase(uint32_t addr)
{
    HAL_StatusTypeDef status = HAL_ERROR;
	uint8_t BBlockEras_cmd[4] = {0};		//新增数组归纳发送的指令+地址，避免发送指令耗时较长处理
	BBlockEras_cmd[0] = W25Q128_CMD_64KB_BLOCK_ERASE;
	BBlockEras_cmd[1] = (uint8_t)((addr >> 16) & 0xFF);
	BBlockEras_cmd[2] = (uint8_t)((addr >> 8) & 0xFF);
	BBlockEras_cmd[3] = (uint8_t)(addr & 0xFF);

    // 校验1：地址越界检查
	if(addr > W25Q128_TOTAL_SIZE)
    {
		return HAL_ERROR;
    }
	
	// 校验2：地址64KB对齐检查（核心！未对齐擦除会失败）
	if(addr%W25Q128_64KB_BLOCK_SIZE!=0)
    {
		return HAL_ERROR;
    }
	
	    // 执行写使能（擦除前必须使能）
		status = W25Q128_WriteEnable();
		if (status != HAL_OK)
		{
			return status;
		}
	
		
		
			    W25Q128_CS_LOW();
    // 发送64KB块擦除指令 + 3字节地址（addr[23:16], addr[15:8], addr[7:0]）
				status = HAL_SPI_Transmit(&hspi1,BBlockEras_cmd,4,W25Q128_TIMEOUT);
				W25Q128_CS_HIGH();
				if (status != HAL_OK)
				{
					return status;
				}
				

				// 等待块擦除完成（64KB擦除耗时约15ms）
				status = W25Q128_WaitIdle();

				return status;
}


/**
 * @brief   擦除W25Q128指定32KB块（耗时800ms）
 * @param   addr：32KB块起始地址（必须32KB对齐，如0x008000、0x008000）
 * @retval  HAL_OK：擦除成功；HAL_ERROR：地址越界/未对齐；HAL_TIMEOUT：通信/等待超时
 */
HAL_StatusTypeDef W25Q128_32KBBlockErase(uint32_t addr)
{
    HAL_StatusTypeDef status = HAL_ERROR;
	uint8_t BBlockEras_cmd[4] = {0};		//新增数组归纳发送的指令+地址，避免发送指令耗时较长处理
	BBlockEras_cmd[0] = W25Q128_CMD_32KB_BLOCK_ERASE;
	BBlockEras_cmd[1] = (uint8_t)((addr >> 16) & 0xFF);
	BBlockEras_cmd[2] = (uint8_t)((addr >> 8) & 0xFF);
	BBlockEras_cmd[3] = (uint8_t)(addr & 0xFF);

    // 校验1：地址越界检查
	if(addr > W25Q128_TOTAL_SIZE)
    {
		return HAL_ERROR;
    }
	
	// 校验2：地址32KB对齐检查（核心！未对齐擦除会失败）
	if(addr%W25Q128_32KB_BLOCK_SIZE!=0)
    {
		return HAL_ERROR;
    }
	
	    // 执行写使能（擦除前必须使能）
		status = W25Q128_WriteEnable();
		if (status != HAL_OK)
		{
			return status;
		}
	
		
		
			    W25Q128_CS_LOW();
				status = HAL_SPI_Transmit(&hspi1,BBlockEras_cmd,4,W25Q128_TIMEOUT);
				W25Q128_CS_HIGH();
				if (status != HAL_OK)
				{
					return status;
				}
				

				// 等待块擦除完成
				status = W25Q128_WaitIdle();

				return status;
}











//========================================2. 计算地址的辅助功能函数============================


/**
 * @brief   根据Block  绝对编号获取地址,共256个Block
 * @param   BlockNo：0-255
 * @retval  24位的绝对存储地址
 */
uint32_t Flash_Addr_byBlock(uint8_t BlockNo)
{
	
	uint32_t address=BlockNo;
	
	address = address<<16;	//左移16位，等于乘以0x10000
	return address;
}


/**
 * @brief   Sector  绝对编号获取地址,共4096个Sector
 * @param   SectorNo：0-4095
 * @retval  24位的绝对存储地址
 */
uint32_t Flash_Addr_bySector(uint16_t SectorNo) {
	if (SectorNo > 4095)	//不能超过4095
		SectorNo = 0;

 
	uint32_t addr = SectorNo;
	addr = addr << 12;		//左移12位,等于乘以0x1000
	return addr;
}



/**
 * @brief   Page  绝对编号获取地址,共65536个Page
 * @param   PageNo：0-65535
 * @retval  24位的绝对存储地址
 */
uint32_t Flash_Addr_byPage(uint16_t PageNo) {
 
	uint32_t addr = PageNo;
	addr = addr << 8;		//左移8位，等于乘以0x100
	return addr;
}



/**
 * @brief  根据Block编号，和内部Sector编号计算地址，一个Block有16个Sector
* @param   BlockNo：0-255，SubSectorNo:0-15
 * @retval  24位的绝对存储地址
 */
uint32_t Flash_Addr_byBlockSector(uint8_t BlockNo, uint8_t SubSectorNo) {
	
	uint32_t addr,offset = 0;
	
	if(SubSectorNo > 15)
    {
		SubSectorNo = 0;
    }
	
	addr = BlockNo;
	addr = addr << 16;//先计算Block的起始地址

	
	offset = SubSectorNo;
	offset = offset << 12;	//计算Sector的偏移地址
	
	addr |= offset;		// |运算不影响结果，因为块地址有效位在第16位以上，扇区地址有效位在12~15位以上
	
	return addr;
}


/**
 * @brief  根据Block编号，内部Sector编号，内部Page编号获取地址,一个Sector有16个Page 
* @param   BlockNo：0-255，SubSectorNo:0-15,SubPageNo:0-15
 * @retval  24位的绝对存储地址
 */
uint32_t Flash_Addr_byBlockSectorPage(uint8_t BlockNo, uint8_t SubSectorNo,
		uint8_t SubPageNo) {
	if (SubSectorNo > 15)	//不能超过15
		SubSectorNo = 0;
 
	if (SubPageNo > 15)	//不能超过15
		SubPageNo = 0;
 

	uint32_t addr = BlockNo;
	addr = addr << 16;		//先计算Block的起始地址
 

	uint32_t offset = SubSectorNo;	//计算Sector的偏移地址
	offset = offset << 12;	//计算Sector的偏移地址
	addr |= offset;
 

	offset = SubPageNo;
	offset = offset << 8;	//计算Page的偏移地址
 
	addr |= offset;		//Page的起始地址
	return addr;
}
		



/**
* @brief  将以上函数返回的24位存储地址分割为3个字节，便于发送SPI通信
* @param   globalAddr:全局24位地址
 * @retval   指针值返回 addrHigh高字节，addrMid中间字节，addrLow低字节
 */
void Flash_SpliteAddr(uint32_t globalAddr, 
						uint8_t *addrHigh, 
						uint8_t *addrMid,
						uint8_t *addrLow)
{
	*addrHigh = (uint8_t)globalAddr >> 16;	
	*addrMid = (uint8_t)(globalAddr >> 8) & 0XFF;
	*addrLow = (uint8_t)globalAddr & 0XFF;
}




















//================================================================================================





















