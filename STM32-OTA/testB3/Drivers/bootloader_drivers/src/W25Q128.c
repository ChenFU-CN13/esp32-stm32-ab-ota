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
 * @brief   向W25Q128指定页写入数据（按页号写入，256字节）
 * @param   wbuff: 写入数据指针（必须256字节）
 * @param   pageNB: 页编号，从0开始（0~65535）
 * @retval  无
 */
void W25Q128_PageWrite_ByPageNum(uint8_t *wbuff, uint16_t pageNB)
{
    uint8_t wdata[4]; // 保存命令和地址

    // 页写入数据指令 0x02
    wdata[0] = W25Q128_CMD_WRITE;
    // 需要写入数据的页地址（由页号计算得出）
    uint32_t page_addr = (uint32_t)pageNB * W25Q128_PAGE_SIZE;
    wdata[1] = (page_addr >> 16) & 0xFF; // 地址高字节
    wdata[2] = (page_addr >> 8)  & 0xFF; // 地址中字节
    wdata[3] =  page_addr        & 0xFF; // 地址低字节

    // 等待W25Q128空闲
    W25Q128_WaitIdle();
    // 写使能
    W25Q128_WriteEnable();

    // 打开CS
    W25Q128_CS_LOW();
    // 把wdata（指令+地址）发送给W25Q128
    HAL_SPI_Transmit(&hspi1, wdata, 4, W25Q128_TIMEOUT);
    // 接着发送256字节（一页）的数据
    HAL_SPI_Transmit(&hspi1, wbuff, W25Q128_PAGE_SIZE, W25Q128_TIMEOUT);
    // 关闭CS
    W25Q128_CS_HIGH();
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





//-----------------------------------------------------//

//-----------------------------------------------------//

/**
 * @brief   标准CRC32计算函数（和在线工具参数一致）
 * @param   data: 待计算数据缓冲区
 * @param   len:  数据长度
 * @retval  CRC32结果（初始值0xFFFFFFFF，输出反转，最终异或0xFFFFFFFF）
 */
uint32_t W25Q128_CalcCRC32(uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF; // 初始值
    for (uint32_t i = 0; i < len; i++)
    {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF; // 最终异或
}








/**
 * @brief   校验指定块数据的CRC32（核心函数）- 全量遍历+精准统计碎片化+打印末尾字节
 * @param   block_start_addr: 待校验块的起始地址（必须按块大小对齐）
 * @param   block_type:       块类型（32KB/64KB）
 * @param   check_len:        理论固件长度（必须传入准确的bin文件长度，如12676）
 * @param   target_crc32:     目标CRC32值
 * @retval  0: CRC匹配；1: CRC不匹配；2: 参数错误；3: 读取数据失败
 */
uint8_t W25Q128_VerifyBlockCRC32(uint32_t block_start_addr, W25Q128_BlockType block_type, uint32_t check_len, uint32_t target_crc32)
{
    // 1. 定义块大小，根据块类型赋值
    uint32_t block_size = 0;
    if (block_type == BLOCK_TYPE_32KB) {
        block_size = W25Q128_32KB_BLOCK_SIZE;
    } else if (block_type == BLOCK_TYPE_64KB) {
        block_size = W25Q128_64KB_BLOCK_SIZE;
    } else {
        U1_Printf("Error: 无效的块类型！\r\n");
        return 2;
    }

    // 2. 参数合法性检查
    if (block_start_addr % block_size != 0) { 
        U1_Printf("Error: 块起始地址未按%dKB对齐！\r\n", (block_size/1024));
        return 2; 
    }
    if (block_start_addr >= W25Q128_TOTAL_SIZE) { 
        U1_Printf("Error: 块起始地址越界！\r\n");
        return 2; 
    }
    if (check_len == 0) check_len = block_size;
    if (check_len > block_size || (block_start_addr + check_len) > W25Q128_TOTAL_SIZE) {
        U1_Printf("Error: 校验长度超限（块大小：%d，请求长度：%d）！\r\n", block_size, check_len);
        return 2;
    }

    // 3. 定义全局缓冲区（避免栈溢出）
    #define BLOCK_READ_BUF_SIZE 65536
    static uint8_t block_read_buf[BLOCK_READ_BUF_SIZE] __attribute__((section(".bss")));
    
    // 4. 从W25Q128读取指定块数据
    HAL_StatusTypeDef read_status = W25Q128_Flash_ReadBytes(block_start_addr, block_read_buf, check_len, BLOCK_READ_BUF_SIZE);
    if (read_status != HAL_OK) {
        U1_Printf("Error: 读取W25Q128块数据失败！状态码：%d\r\n", read_status);
        return 3;
    }

    // 5. 调试打印：前32字节 + 1265后100字节（保留原有需求）
    U1_Printf("===== 读取的块前32字节数据 =====\r\n");
    for (uint32_t i = 0; i < 32; i++) {
        U1_Printf("0x%02X ", block_read_buf[i]);
        if ((i+1) % 8 == 0) U1_Printf("\r\n");
    }
    U1_Printf("\r\n================================\r\n");

    uint32_t print_start_idx = 1265;
    uint32_t print_byte_count = 100;
    if (print_start_idx + print_byte_count <= check_len)
    {
        U1_Printf("===== 第1266-1365字节数据 =====\r\n");
        for (uint32_t i = 0; i < print_byte_count; i++) {
            uint32_t curr_idx = print_start_idx + i;
            U1_Printf("0x%02X ", block_read_buf[curr_idx]);
            if ((i+1) % 8 == 0) U1_Printf("\r\n");
        }
        U1_Printf("\r\n================================\r\n");
    }

    // ===================== 新增：打印最后N个字节（可自定义数量） =====================
    #define LAST_BYTES_COUNT 50  // 打印最后50个字节（可改为100/64等）
    if (check_len >= LAST_BYTES_COUNT)
    {
        uint32_t last_start_idx = check_len - LAST_BYTES_COUNT; // 最后N个字节的起始索引
        U1_Printf("===== 读取的块最后%d字节数据（索引%d~%d） =====\r\n", 
                  LAST_BYTES_COUNT, last_start_idx, check_len-1);
        for (uint32_t i = 0; i < LAST_BYTES_COUNT; i++) {
            uint32_t curr_idx = last_start_idx + i;
            U1_Printf("0x%02X ", block_read_buf[curr_idx]);
            if ((i+1) % 8 == 0) U1_Printf("\r\n"); // 每8个换行，方便对比
        }
        U1_Printf("\r\n================================\r\n");
    }
    else
    {
        U1_Printf("Warning: 数据长度不足%d字节，无法打印最后%d字节！\r\n", LAST_BYTES_COUNT, LAST_BYTES_COUNT);
    }
    // ===================== 末尾字节打印结束 =====================

    // ===================== 核心优化：全量遍历+统计所有碎片化段 =====================
    // 配置项（可按需调整）
    #define MIN_FF_SEQ_LEN 100  // 判定为“碎片化段”的最小连续FF长度
    uint32_t fragment_count = 0;       // 总碎片化次数（符合阈值的FF段数）
    uint32_t actual_valid_len = 0;     // 实际有效数据总长度（最后一段有效数据的索引+1）
    uint32_t consecutive_ff = 0;       // 当前连续FF计数器
    uint32_t curr_fragment_start = 0;  // 当前碎片化段的起始索引
    uint8_t is_in_fragment = 0;        // 是否正在遍历碎片化段

    U1_Printf("===== 开始全量检测固件碎片化（遍历%d字节） =====\r\n", check_len);

    // 遍历**全部**数据，不提前终止
    for (uint32_t i = 0; i < check_len; i++)
    {
        if (block_read_buf[i] == 0xFF)
        {
            consecutive_ff++;
            // 首次进入FF段：记录起始索引
            if (is_in_fragment == 0)
            {
                curr_fragment_start = i;
                is_in_fragment = 1;
            }
        }
        else
        {
            // 遇到有效数据：更新有效长度，并检查上一段FF是否为碎片化
            actual_valid_len = i + 1; // 有效数据长度更新到当前索引+1
            if (is_in_fragment == 1)
            {
                // 上一段FF长度≥阈值：记为1次碎片化
                if (consecutive_ff >= MIN_FF_SEQ_LEN)
                {
                    fragment_count++;
                    U1_Printf("碎片化%d次：索引%d~%d，连续%d个0xFF\r\n", 
                              fragment_count, curr_fragment_start, i-1, consecutive_ff);
                }
                // 重置FF段状态
                is_in_fragment = 0;
                consecutive_ff = 0;
            }
        }
    }

    // 处理数据末尾的FF段（循环结束后，可能还在FF段中）
    if (is_in_fragment == 1 && consecutive_ff >= MIN_FF_SEQ_LEN)
    {
        fragment_count++;
        U1_Printf("碎片化%d次：索引%d~%d，连续%d个0xFF\r\n", 
                  fragment_count, curr_fragment_start, check_len-1, consecutive_ff);
    }

    // 计算缺失字节数（有效长度 < 理论长度则为缺失）
    uint32_t missing_bytes = (actual_valid_len < check_len) ? (check_len - actual_valid_len) : 0;

    // 打印完整统计结果
    U1_Printf("================================\r\n");
    U1_Printf("【固件碎片化完整统计】：\r\n");
    U1_Printf("  - 理论固件长度：%d 字节\r\n", check_len);
    U1_Printf("  - Flash实际有效长度：%d 字节\r\n", actual_valid_len);
    U1_Printf("  - 缺失数据字节数：%d 字节\r\n", missing_bytes);
    U1_Printf("  - 碎片化阈值（连续0xFF数）：%d 字节\r\n", MIN_FF_SEQ_LEN);
    U1_Printf("  - 检测到碎片化次数：%d 次\r\n", fragment_count);
    if (missing_bytes > 0) {
        U1_Printf("  - 数据最终截断位置：0x%06X (Flash地址)\r\n", block_start_addr + actual_valid_len);
    }
    U1_Printf("================================\r\n");
    // ===================== 碎片化统计结束 =====================

    // 6. 计算读取数据的CRC32（按理论长度计算）
    uint32_t calc_crc32 = W25Q128_CalcCRC32(block_read_buf, check_len);
    U1_Printf("计算得到的CRC32: 0x%08X\r\n", calc_crc32);
    U1_Printf("目标CRC32（源bin）: 0x%08X\r\n", target_crc32);

    // 7. 对比CRC32并返回结果
    if (calc_crc32 == target_crc32)
    {
        U1_Printf(" CRC32校验通过！数据一致\r\n");
        return 0;
    }
    else
    {
        U1_Printf("CRC32校验失败！数据不一致\r\n");
        return 1;
    }
}








/**
 * @brief   测试函数：校验指定块（封装常用参数，方便调用）
 * @param   block_start_addr: 待校验块起始地址（如0x00050000）
 * @param   block_type:       块类型（32KB/64KB）
 * @param   fw_len:           固件长度（0=校验整个块，>0=自定义固件长度）
 * @retval  同W25Q128_VerifyBlockCRC32
 */
uint8_t W25Q128_TestBlockCRC32(uint32_t block_start_addr, W25Q128_BlockType block_type, uint32_t fw_len)
{
    // 你的源bin文件CRC32固定为0x9ED16969
    const uint32_t FW_TARGET_CRC32 = 0x9ED16969;
    return W25Q128_VerifyBlockCRC32(block_start_addr, block_type, fw_len, FW_TARGET_CRC32);
}

















//================================================================================================
//================================================================================================
//================================================================================================



/**
 * @brief   W25Q128 64KB块损坏检测测试函数（分段测试版，解决RAM不足+卡死问题）
 * @param   block64k_addr：64KB块起始地址（必须64KB对齐）
 * @param   test_mode：0-快速测试(仅前4KB) 1-完整测试(分段测64KB)
 * @retval  0:Pass 1:Param error 2:Erase fail 3:Write fail 4:Read fail 5:Data mismatch
 */
uint8_t W25Q128_Test64KBBlockHealth(uint32_t block64k_addr, uint8_t test_mode)
{
    // 1. 参数检查
    if (block64k_addr % W25Q128_64KB_BLOCK_SIZE != 0 || block64k_addr >= W25Q128_TOTAL_SIZE)
    {
        U1_Printf("Error: Invalid address 0x%06X!\r\n", block64k_addr);
        return 1;
    }

    // 2. 仅用4KB缓冲区（STM32F407轻松容纳）
    #define TEST_BUF_SIZE W25Q128_SECTOR_SIZE // 4KB buffer
    uint8_t write_buf[TEST_BUF_SIZE] = {0};
    uint8_t read_buf[TEST_BUF_SIZE] = {0};
    
    uint32_t total_test_len = (test_mode == 0) ? TEST_BUF_SIZE : W25Q128_64KB_BLOCK_SIZE;
    uint32_t curr_addr = block64k_addr;
    uint32_t error_count = 0;
    uint32_t first_error_addr = 0;
    uint8_t write_val = 0, read_val = 0;

    U1_Printf("%s test 64KB block 0x%06X, total length %d bytes...\r\n", 
              (test_mode==0)?"Quick":"Full", block64k_addr, total_test_len);

    // 3. 先擦除整个64KB块（增加擦除前唤醒+写使能校验）
    // 擦除前确保芯片唤醒
    W25Q128_WakeUp();
    HAL_Delay(2); // 等待唤醒完成
    
    // 手动写使能（防止自动写使能失败）
    HAL_StatusTypeDef en_status = W25Q128_WriteEnable();
    if (en_status != HAL_OK)
    {
        U1_Printf("Error: Write enable failed before erase! Status: %d\r\n", en_status);
        return 2;
    }
    
    U1_Printf("Erasing 64KB block 0x%06X...\r\n", block64k_addr);
    HAL_StatusTypeDef status = W25Q128_64KBBlockErase(block64k_addr);
    if (status != HAL_OK)
    {
        U1_Printf("Error: Block erase failed! Status: %d\r\n", status);
        return 2;
    }
    
    // 等待擦除完成（单独处理，增加超时提示）
    status = W25Q128_WaitIdle();
    if (status != HAL_OK)
    {
        U1_Printf("Error: Wait idle after erase timeout!\r\n");
        return 2;
    }
    U1_Printf("Block erase completed.\r\n");

    // 4. 分段写入+读取+对比（每次4KB）
    while (curr_addr < block64k_addr + total_test_len)
    {
        uint32_t curr_len = TEST_BUF_SIZE;
        // 最后一段可能不足4KB（仅快速测试时可能）
        if (curr_addr + curr_len > block64k_addr + total_test_len)
        {
            curr_len = block64k_addr + total_test_len - curr_addr;
        }

        // 生成当前段测试数据（固定模式，避免rand()导致数据不一致）
        for (uint32_t i = 0; i < curr_len; i++)
        {
            write_buf[i] = (uint8_t)((curr_addr + i) & 0xFF); // 固定递增模式，易验证
        }

        // 写入前再次写使能
        en_status = W25Q128_WriteEnable();
        if (en_status != HAL_OK)
        {
            U1_Printf("Error: Write enable failed at 0x%06X! Status: %d\r\n", curr_addr, en_status);
            return 3;
        }
        
        // 写入当前段数据
        U1_Printf("Writing %d bytes to 0x%06X...\r\n", curr_len, curr_addr);
        status = W25Q128_MassWrite(curr_addr, write_buf, curr_len, TEST_BUF_SIZE);
        if (status != HAL_OK)
        {
            U1_Printf("Error: Write failed at 0x%06X! Status: %d\r\n", curr_addr, status);
            return 3;
        }
        
        // 等待写入完成
        status = W25Q128_WaitIdle();
        if (status != HAL_OK)
        {
            U1_Printf("Error: Wait idle after write timeout!\r\n");
            return 3;
        }

        // 读取当前段数据
        memset(read_buf, 0, TEST_BUF_SIZE); // Clear read buffer
        U1_Printf("Reading %d bytes from 0x%06X...\r\n", curr_len, curr_addr);
        status = W25Q128_Flash_ReadBytes(curr_addr, read_buf, curr_len, TEST_BUF_SIZE);
        if (status != HAL_OK)
        {
            U1_Printf("Error: Read failed at 0x%06X! Status: %d\r\n", curr_addr, status);
            return 4;
        }

        // 对比当前段数据
        U1_Printf("Comparing data at 0x%06X...\r\n", curr_addr);
        for (uint32_t i = 0; i < curr_len; i++)
        {
            if (write_buf[i] != read_buf[i])
            {
                error_count++;
                if (error_count == 1)
                {
                    first_error_addr = curr_addr + i;
                    write_val = write_buf[i];
                    read_val = read_buf[i];
                }
                if (error_count <= 10) // Only print first 10 errors
                {
                    U1_Printf("Error at 0x%06X: Wrote 0x%02X, Read 0x%02X\r\n", 
                             curr_addr + i, write_buf[i], read_buf[i]);
                }
            }
        }

        curr_addr += curr_len; // Process next segment
        U1_Printf("Tested 0x%06X - 0x%06X, current error count: %d\r\n", 
                 curr_addr - curr_len, curr_addr - 1, error_count);
    }

    // 5. 输出最终结果
    if (error_count == 0)
    {
        U1_Printf(" Test Pass! 64KB block 0x%06X is healthy.\r\n", block64k_addr);
        return 0;
    }
    else
    {
        U1_Printf(" Test Failed! Total %d error bytes, first error at 0x%06X (W:0x%02X/R:0x%02X)\r\n", 
                 error_count, first_error_addr, write_val, read_val);
        return 5;
    }
}

/**
 * @brief   Batch test multiple 64KB blocks
 * @param   start_block: Start block number (0-255)
 * @param   block_count: Number of blocks to test
 * @retval  0:All pass  Non-0: Some failed
 */
uint8_t W25Q128_BatchTest64KBBlocks(uint8_t start_block, uint8_t block_count)
{
    uint8_t test_result = 0;
    for (uint8_t i = 0; i < block_count; i++)
    {
        uint8_t curr_block = start_block + i;
        if (curr_block >= 256) 
        {
            U1_Printf("Block %d out of range, stop batch test.\r\n", curr_block);
            break;
        }
        
        uint32_t block_addr = (uint32_t)curr_block * W25Q128_64KB_BLOCK_SIZE;
        U1_Printf("\n===== Testing 64KB block %d (0x%06X) =====\r\n", curr_block, block_addr);
        
        // Use quick test (0) for batch test to save time
        uint8_t result = W25Q128_Test64KBBlockHealth(block_addr, 0); 
        if (result != 0) 
        {
            test_result = result;
            U1_Printf("Block %d test failed! Error code: %d\r\n", curr_block, result);
        }
        else
        {
            U1_Printf("Block %d test passed.\r\n", curr_block);
        }
    }
    
    U1_Printf("\n===== Batch Test Complete =====\r\n");
    if (test_result == 0)
    {
        U1_Printf("All %d blocks tested passed!\r\n", block_count);
    }
    else
    {
        U1_Printf("Some blocks test failed! Error code: %d\r\n", test_result);
    }
    return test_result;
}

/**
 * @brief   Demo function for W25Q128 health test
 */
void W25Q128_HealthTest_Demo(void)
{
    U1_Printf("===== W25Q128 Health Test Start =====\r\n");
    
    // Read chip ID first
    uint32_t chip_id = W25Q128_ReadID();
    U1_Printf("Chip ID: 0x%06X (Expected: 0xEF4018)\r\n", chip_id);
    
    if (chip_id == 0) 
    {
        U1_Printf("Error: Chip not responding! Test aborted.\r\n");
        return;
    }
    else if (chip_id != 0xEF4018)
    {
        U1_Printf("Warning: Chip ID mismatch! Continue test...\r\n");
    }

    // Test single 64KB block (0x000000, quick test)
    U1_Printf("\n--- Testing single 64KB block ---\r\n");
    uint8_t single_result = W25Q128_Test64KBBlockHealth(0x050000, 1);
	
    U1_Printf("Single block test result: %d (0=Pass)\r\n", single_result);
    
    // Batch test first 2 blocks (optional)
    // U1_Printf("\n--- Batch testing multiple blocks ---\r\n");
    // uint8_t batch_result = W25Q128_BatchTest64KBBlocks(0, 2);
    
    U1_Printf("\n===== W25Q128 Health Test End =====\r\n");
}

//================================================================================================
//================================================================================================
//================================================================================================
