#ifndef __CONFIG_MANAGER_H
#define __CONFIG_MANAGER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_CONFIG_MAGIC            0xA5A5A5A5UL
#define APP_CONFIG_EEPROM_ADDRESS   0x70U

typedef struct
{
    uint32_t magic;         //判断EEPROM里有没有有效配置，4字节
    int16_t temp_high_x10;  //温度上限，放大10倍保存，2字节
    int16_t humi_high;      //湿度上限，2字节
    uint16_t smoke_adc_thr; //烟雾ADC报警阈值，2字节
    uint16_t light_low;     //光照开灯下限，2字节
    uint16_t light_high;    //光照关灯上限，2字节
    uint16_t reserved;      //预留位置，以后扩展，2字节
} app_config_t;             //阈值配置结构体，一个结构体正好16个字节

/**
 * @brief 从 AT24C02 读取配置，必要时写入默认值。
 * @param 无。
 * @return 无。
 * @note EEPROM 地址固定为 0x70~0x7F，仅以 magic 判断初版数据有效性。
 */
void CFG_Init(void);

/**
 * @brief 获取配置对象只读指针。
 * @param 无。
 * @return 内部配置对象地址。
 * @note 指针长期有效；跨任务需要一致快照时优先使用 CFG_GetSnapshot。
 */
const app_config_t *CFG_Get(void);

/**
 * @brief 原子复制一份当前配置。
 * @param out_config 接收配置快照的有效指针。
 * @return 无。
 * @note 调度器启动前直接复制；启动后仅短暂关闭中断，不执行 EEPROM 操作。
 */
void CFG_GetSnapshot(app_config_t *out_config);

/**
 * @brief 更新 RAM 中的温度上限。
 * @param temp_x10 温度上限乘 10。
 * @return 无。
 * @note 不写 EEPROM，由保存按钮统一调用 CFG_Save。
 */
void CFG_SetTempHigh(int16_t temp_x10);

/**
 * @brief 更新 RAM 中的湿度上限。
 * @param humi 湿度上限百分数。
 * @return 无。
 * @note 不写 EEPROM，由保存按钮统一调用 CFG_Save。
 */
void CFG_SetHumiHigh(int16_t humi);

/**
 * @brief 更新 RAM 中的烟雾 ADC 阈值。
 * @param adc_thr 12 位 ADC 阈值。
 * @return 无。
 * @note 大于 4095 时按 4095 处理，不写 EEPROM。
 */
void CFG_SetSmokeThr(uint16_t adc_thr);

/**
 * @brief 更新 RAM 中的光照滞回范围。
 * @param low 低于此值开灯。
 * @param high 高于此值关灯。
 * @return 无。
 * @note high 必须大于 low，否则忽略本次修改；不写 EEPROM。
 */
void CFG_SetLightRange(uint16_t low, uint16_t high);

/**
 * @brief 恢复默认配置并立即执行一次写入与读回校验。
 * @param 无。
 * @return 0=保存成功，1=读回校验失败。
 * @note 用户点击“恢复默认”时只产生一次 16 字节保存操作。
 */
uint8_t CFG_SetDefault(void);

/**
 * @brief 将 RAM 配置保存到 AT24C02 并读回校验。
 * @param 无。
 * @return 0=读回一致，1=读回不一致。
 * @note 调用一次 at24cxx_write 写入 sizeof(app_config_t)=16 字节。
 */
uint8_t CFG_Save(void);

#ifdef __cplusplus
}
#endif

#endif
