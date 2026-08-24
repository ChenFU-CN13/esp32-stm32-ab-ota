#include "AT24C02.h"


//超时时间
#define EP24C_TIMEOUT		200
#define EP24C_MEMADD_SIZE 	I2C_MEMADD_SIZE_8BIT
/**
 * @brief   检查设备是否准备好
 * @param   None
 * @retval  HAL_OK or ERROR
 */
HAL_StatusTypeDef EP24C_IsDeviceReady(void) {
	uint32_t Trials = 10;
	return HAL_I2C_IsDeviceReady(&I2C_HANDLE, DEV_ADDR_24CXX, Trials, EP24C_TIMEOUT);//测试I2C设备是否在总线，随便发数据等应答
}



/**
 * @brief   指定地址写入一个字节
 * @param   memAddress指定的地址 byteData一字节数据
 * @retval  HAL_OK or ERROR
 */
HAL_StatusTypeDef EP24C_WriteOneByte(uint16_t memAddress, uint8_t byteData) {
	return HAL_I2C_Mem_Write(&I2C_HANDLE, DEV_ADDR_24CXX, memAddress,
			EP24C_MEMADD_SIZE, &byteData, 1, EP24C_TIMEOUT);
}



/**
 * @brief   指定地址读取一个字节
 * @param   memAddress指定的地址 byteData存储数据的地址指针
 * @retval  HAL_OK or ERROR
 */
HAL_StatusTypeDef EP24C_ReadOneByte(uint16_t memAddress, uint8_t *byteData) {
	return HAL_I2C_Mem_Read(&I2C_HANDLE, DEV_ADDR_24CXX, memAddress,
				EP24C_MEMADD_SIZE, byteData, 1, EP24C_TIMEOUT);
}



/**
 * @brief   指定地址读取多个字节,新加入外部数组长度小于读取长度检查，防止溢出跑飞
 * @param   memAddress指定的地址  *pBuffer读取数据存储的指针 bufferLen读取的长度  buffer_Len_Max外部存储数组长度
 * @retval  HAL_OK or ERROR
 */
HAL_StatusTypeDef EP24C_ReadBytes(uint16_t memAddress, 
								uint8_t *pBuffer, 
								uint16_t bufferLen,
								uint16_t buffer_Len_Max) 
{
	
	//双重检验，防止外部存储数据组 溢出和存储芯片 越界
	if(bufferLen > MEM_SIZE_24CXX || bufferLen > buffer_Len_Max)
	{
		return HAL_ERROR;
	}
	
	return HAL_I2C_Mem_Read(&I2C_HANDLE,DEV_ADDR_24CXX,memAddress,
		EP24C_MEMADD_SIZE,pBuffer,bufferLen,EP24C_TIMEOUT);
}


/**
 * @brief   指定地址写入一页 不会超过一页
 * @param   memAddress指定的地址 *pBuffer写入的数据 bufferLen写入的长度
 * @retval  HAL_OK or ERROR
 */
HAL_StatusTypeDef EP24C_WriteInOnePage(uint16_t memAddress, uint8_t *pBuffer, uint16_t bufferLen) {
	if (bufferLen > PAGE_SIZE_24CXX) {		//先判断是否超过1页存储空间（1页即8个字节）
		return HAL_ERROR;
	}
	return HAL_I2C_Mem_Write(&I2C_HANDLE, DEV_ADDR_24CXX, memAddress,
				EP24C_MEMADD_SIZE, pBuffer, bufferLen, EP24C_TIMEOUT);
}



/**
 * @brief   指定地址写入超一页地址
 * @param   memAddress指定的地址 
			*pBuffer写入的数据的地址 
			bufferLen写入的长度（单位是字节）
 * @retval  HAL_OK or ERROR
 */
HAL_StatusTypeDef EP24C_WriteLongData(uint16_t memAddress, uint8_t *pBuffer, uint16_t bufferLen) {
	if (bufferLen > MEM_SIZE_24CXX) {		//首先判断写入的数据长度是否超过32个页的长度
		return HAL_ERROR;
	}
	HAL_StatusTypeDef result = HAL_ERROR;		//先设置默认返回状态为失败，经过后续如果每次正常写入成功则可以覆盖为成功状态
	if (bufferLen <= PAGE_SIZE_24CXX) {			//如果没超过1页，就直接和上面一样直接写入
		return HAL_I2C_Mem_Write(&I2C_HANDLE, DEV_ADDR_24CXX, memAddress,
						EP24C_MEMADD_SIZE, pBuffer, bufferLen, EP24C_TIMEOUT);
	}
 
	uint8_t *pt = pBuffer;						//获取外部数据包首地址
	uint16_t pageCount = bufferLen / PAGE_SIZE_24CXX;	//计算一共需要写入多少整页的数据，余下凑不成整页的数据另外计算
	for (uint16_t i = 0; i < pageCount; ++i) {
		result = HAL_I2C_Mem_Write(&I2C_HANDLE, DEV_ADDR_24CXX, memAddress,
								EP24C_MEMADD_SIZE, pt, PAGE_SIZE_24CXX, EP24C_TIMEOUT);
		pt += PAGE_SIZE_24CXX;					//移动下1轮的外部数据包
		memAddress += PAGE_SIZE_24CXX;			//移动下1轮存储外部数据的空间地址
		HAL_Delay(5);
 
		if (result != HAL_OK) {					//如果有1轮无法写入成功，那么就需要跳出，并返回报错
			return result;
		}
	}
 
	uint8_t leftBytes = bufferLen % PAGE_SIZE_24CXX;	//处理剩余的数据，可能剩余0-7个字节未处理
	if (leftBytes > 0) {
		result = HAL_I2C_Mem_Write(&I2C_HANDLE, DEV_ADDR_24CXX, memAddress,		//这里的memAddress，pt不用额外再操作地址了，上次退出循环前已++
										EP24C_MEMADD_SIZE, pt, leftBytes, EP24C_TIMEOUT);
	}
	return result;
}

/**
 * @brief   安全写入任意长度数据，自动按AT24C02实际页边界拆分
 * @param   memAddress 起始地址
 * @param   pBuffer    源数据，不能为空
 * @param   bufferLen  写入长度，允许为0
 * @retval  HAL_OK-成功；HAL_ERROR-参数或越界错误；其余为I2C错误
 *
 * @note    旧EP24C_WriteLongData()每次固定推进8字节，只适用于页对齐地址。
 *          本函数按“当前页剩余字节数”切分，因此可安全写入任意起始地址。
 */
HAL_StatusTypeDef EP24C_WriteBytes(uint16_t memAddress, const uint8_t *pBuffer, uint16_t bufferLen)
{
	HAL_StatusTypeDef status;
	uint16_t page_remaining;
	uint16_t write_len;

	if ((pBuffer == NULL && bufferLen != 0U) || memAddress >= MEM_SIZE_24CXX)
	{
		return HAL_ERROR;
	}

	if (bufferLen == 0U)
	{
		return HAL_OK;
	}

	if (bufferLen > (MEM_SIZE_24CXX - memAddress))
	{
		return HAL_ERROR;
	}

	while (bufferLen > 0U)
	{
		page_remaining = PAGE_SIZE_24CXX - (memAddress % PAGE_SIZE_24CXX);
		write_len = (bufferLen < page_remaining) ? bufferLen : page_remaining;

		status = HAL_I2C_Mem_Write(&I2C_HANDLE, DEV_ADDR_24CXX, memAddress,
				EP24C_MEMADD_SIZE, (uint8_t *)pBuffer, write_len, EP24C_TIMEOUT);
		if (status != HAL_OK)
		{
			return status;
		}

		/* AT24C02内部写周期最大值由现有驱动统一采用5ms。 */
		HAL_Delay(5);
		memAddress += write_len;
		pBuffer += write_len;
		bufferLen -= write_len;
	}

	return HAL_OK;
}





void test_AT24C02(void)
{
	uint8_t i = 0;
	uint8_t STROUT[100];

	if(EP24C_IsDeviceReady()==HAL_OK)
    {
		U1_Printf("the device is ok\r\n");
    }
	else
	{
		U1_Printf("the device is EEROR\r\n");
	}

	for(i=0;i<100;i++)
    {
		EP24C_WriteOneByte(i,i);
		HAL_Delay(5);
    }
	
	EP24C_ReadBytes(0,STROUT,100,100);
	
	for(i=0;i<100;i++)
    {
		U1_Printf("MemAddress%d = %d \r\n",i,STROUT[i]);
    }
}





























