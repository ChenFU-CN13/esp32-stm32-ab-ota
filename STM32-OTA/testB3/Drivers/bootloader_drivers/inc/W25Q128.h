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


// 标准 CRC32 校验表（多项式 0xEDB88320，初始值 0xFFFFFFFF，输出反转）
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};
// 头文件声明（加到W25Q128.h末尾）
typedef enum {
    BLOCK_TYPE_32KB = 0,  // 32KB块
    BLOCK_TYPE_64KB = 1   // 64KB块
} W25Q128_BlockType;


//// CRC32校验函数声明
uint32_t W25Q128_CalcCRC32(uint8_t *data, uint32_t len);
uint8_t W25Q128_VerifyBlockCRC32(uint32_t block_start_addr, W25Q128_BlockType block_type, uint32_t check_len, uint32_t target_crc32);
uint8_t W25Q128_TestBlockCRC32(uint32_t block_start_addr, W25Q128_BlockType block_type, uint32_t fw_len);
void W25Q128_HealthTest_Demo(void);




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
 * @brief   向W25Q128指定页写入数据（按页号写入，256字节）
 * @param   wbuff: 写入数据指针（必须256字节）
 * @param   pageNB: 页编号，从0开始（0~65535）
 * @retval  无
 */
void W25Q128_PageWrite_ByPageNum(uint8_t *wbuff, uint16_t pageNB);





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


