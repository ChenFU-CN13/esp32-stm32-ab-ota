#ifndef W25Q128_H
#define W25Q128_H

#include "main.h"
#include "spi.h"


#define W25Q128_CS_LOW()	  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);//选中芯片
#define W25Q128_CS_HIGH()	  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);//屏蔽芯片

#define W25Q128_PAGE_SIZE    256    // 页大小：256字节
#define W25Q128_SECTOR_SIZE  4096   // 扇区大小：4KB,即是
#define W25Q128_TOTAL_SIZE   16*1024*1024 // 总容量：16MB=0x01000000
#define W25Q128_TIMEOUT       100U    // SPI通信超时时间（ms）

// 新增块大小宏定义（加到存储规格宏后面）
#define W25Q128_32KB_BLOCK_SIZE       32768U  // 32KB块大小（32*1024）=0x008000
#define W25Q128_64KB_BLOCK_SIZE       65536U  // 64KB块大小（64*1024）=0x010000

// W25Q128 指令宏定义
#define W25Q128_CMD_READ        0x03U  // 常规读指令
#define W25Q128_CMD_FLASH_READ  0x0BU  // 快速读指令
#define W25Q128_CMD_WRITE       0x02U  // 页编程（写入）指令
#define W25Q128_CMD_SECTOR_ERASE 0x20U // 扇区擦除（4KB）指令
#define W25Q128_CMD_WRITE_EN    0x06U  // 写使能指令
#define W25Q128_CMD_READ_STATUS 0x05U  // 读状态寄存器1指令
#define W25Q128_CMD_READ_ID     0x9FU  // 读芯片ID指令
#define W25Q128_CMD_CHIP_ERASE  0xC7U  // 整片擦除指令
#define W25Q128_CMD_Power_Down  0xB9U  // 睡眠指令
#define W25Q128_CMD_WAKEP_UP    0xABU  // 唤醒指令

// 新增块擦除指令宏定义（加到原有指令宏后面）
#define W25Q128_CMD_32KB_BLOCK_ERASE  0x52U  // 32KB块擦除指令
#define W25Q128_CMD_64KB_BLOCK_ERASE  0xD8U  // 64KB块擦除指令

/**
 * @brief   读取W25Q128芯片ID，验证芯片在线状态
 * @param   无
 * @retval  32位芯片ID：厂商ID(0xEF) + 设备ID(0x18) + 容量ID；读取失败返回0
 */
uint32_t W25Q128_ReadID(void);

/**
 * @brief   读取W25Q128忙位状态
 * @param   无
 * @retval  1：芯片忙（擦除/写入中）；0：芯片空闲
 */
uint8_t W25Q128_ReadBusy(void);

/**
 * @brief   等待W25Q128芯片空闲（循环检测忙位）
 * @param   无
 * @retval  HAL_OK：芯片进入空闲状态；HAL_TIMEOUT：等待超时
 */
HAL_StatusTypeDef W25Q128_WaitIdle(void);

/**
 * @brief   执行W25Q128写使能操作（擦除/写入前必须调用）
 * @param   无
 * @retval  HAL_OK：操作成功；HAL_ERROR/HAL_TIMEOUT：操作失败/超时
 */
HAL_StatusTypeDef W25Q128_WriteEnable(void);



/**
 * @brief   执行W25Q128睡眠模式
 * @param   无
 * @retval  HAL_OK：操作成功；HAL_ERROR/HAL_TIMEOUT：操作失败/超时
 */
HAL_StatusTypeDef W25Q128_PowerDown(void);



/**
 * @brief   执行W25Q128唤醒模式
 * @param   无
 * @retval  HAL_OK：操作成功；HAL_ERROR/HAL_TIMEOUT：操作失败/超时
 */
HAL_StatusTypeDef W25Q128_WakeUp(void);




/**
 * @brief   擦除W25Q128指定扇区（4KB）
 * @param   addr：扇区起始地址（必须4KB对齐，如0x000000、0x001000）
 * @retval  HAL_OK：擦除成功；HAL_ERROR：地址越界；HAL_TIMEOUT：通信/等待超时
 */
HAL_StatusTypeDef W25Q128_SectorErase(uint32_t addr);







/**
 * @brief   向W25Q128指定页写入数据（≤256字节）
 * @param   addr：页起始地址（必须256字节对齐）
 * @param   pData：待写入数据指针（非NULL）
 * @param   len：写入长度（≤256字节）
 * @retval  HAL_OK：写入成功；HAL_ERROR：地址越界/长度超限/指针为空；HAL_TIMEOUT：通信/等待超时
 */
HAL_StatusTypeDef W25Q128_PageWrite(uint32_t addr, uint8_t *pData, uint16_t len);





/**
 * @brief   W25Q128跨扇区批量写入（自动分页+自动扇区擦除）
 * @param   start_addr：写入起始地址（无需对齐，函数内部自动处理）
 * @param   pData：待写入数据缓冲区指针（非NULL）
 * @param   total_len：待写入数据总长度（任意长度，支持跨扇区/跨页）
 * @param   buf_max_len：pData缓冲区的最大长度（防溢出）
 * @retval  HAL_OK：写入成功；HAL_ERROR：参数错误；HAL_TIMEOUT：通信/擦除/写入超时
 */
HAL_StatusTypeDef W25Q128_MassWrite(uint32_t start_addr, uint8_t *pData, uint32_t total_len, uint32_t buf_max_len);





/**
 * @brief   从W25Q128指定地址读取任意长度数据（防缓冲区溢出）
 * @param   addr：读取起始地址
 * @param   pBuffer：存储读取数据的缓冲区指针（非NULL）
 * @param   bufferLen：期望读取的长度
 * @param   bufferMaxLen：外部缓冲区实际最大长度（防止溢出）
 * @retval  HAL_OK：读取成功；HAL_ERROR：地址越界/长度超限/指针为空；HAL_TIMEOUT：通信超时
 */
HAL_StatusTypeDef W25Q128_Read(uint32_t addr, uint8_t *pBuffer, uint32_t bufferLen, uint32_t bufferMaxLen);




/**
 * @brief   从W25Q128指定地址读取任意长度数据（防缓冲区溢出），高速连续读取多个字节，任意全局地址， 速度大约是常规读取的2倍
 * @param   addr：读取起始地址
 * @param   pBuffer：存储读取数据的缓冲区指针（非NULL）
 * @param   bufferLen：期望读取的长度
 * @param   bufferMaxLen：外部缓冲区实际最大长度（防止溢出）
 * @retval  HAL_OK：读取成功；HAL_ERROR：地址越界/长度超限/指针为空；HAL_TIMEOUT：通信超时
 */
HAL_StatusTypeDef W25Q128_Flash_ReadBytes(uint32_t addr, uint8_t *pBuffer, uint32_t bufferLen, uint32_t bufferMaxLen);






/**
 * @brief   擦除W25Q128整片存储空间（慎用，耗时约100ms）
 * @param   无
 * @retval  HAL_OK：擦除成功；HAL_TIMEOUT：等待超时；HAL_ERROR：操作失败
 */
HAL_StatusTypeDef W25Q128_ChipErase(void);




// 新增块擦除函数声明（加到SectorErase声明后面）
/**
 * @brief   擦除W25Q128指定32KB块
 * @param   addr：32KB块起始地址（必须32KB对齐，如0x000000、0x008000）
 * @retval  HAL_OK：擦除成功；HAL_ERROR：地址越界/未对齐；HAL_TIMEOUT：通信/等待超时
 */
HAL_StatusTypeDef W25Q128_32KBBlockErase(uint32_t addr);




/**
 * @brief   擦除W25Q128指定64KB块
 * @param   addr：64KB块起始地址（必须64KB对齐，如0x000000、0x010000）
 * @retval  HAL_OK：擦除成功；HAL_ERROR：地址越界/未对齐；HAL_TIMEOUT：通信/等待超时
 */
HAL_StatusTypeDef W25Q128_64KBBlockErase(uint32_t addr);









//========================================2. 计算地址的辅助功能函数============================

//根据Block  绝对编号获取地址,共256个Block
uint32_t Flash_Addr_byBlock(uint8_t BlockNo);


//根据Sector 绝对编号获取地址,共4096个Sector
uint32_t Flash_Addr_bySector(uint16_t SectorNo);



//根据Page  绝对编号获取地址，共65536个Page
uint32_t Flash_Addr_byPage(uint16_t PageNo);
 
 
 
 
//根据Block编号，和内部Sector编号计算地址，一个Block有16个Sector,
uint32_t Flash_Addr_byBlockSector(uint8_t BlockNo, uint8_t SubSectorNo);



//根据Block编号，内部Sector编号，内部Page编号计算地址
uint32_t Flash_Addr_byBlockSectorPage(uint8_t BlockNo, uint8_t SubSectorNo,
		uint8_t SubPageNo);
		
		
		
//将24位地址分解为3个字节
void Flash_SpliteAddr(uint32_t globalAddr, uint8_t *addrHigh, uint8_t *addrMid,
		uint8_t *addrLow);


//================================================================================================






#endif


