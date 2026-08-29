/*
* Copyright 2023 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef __CUSTOM_H_
#define __CUSTOM_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "gui_guider.h"

/*
 * 精简字体替代 GUI Guider 生成的大字库。
 * 页面创建完成后 custom.c 会应用最终字体，因此初始化阶段可直接复用这些子集。
 */
LV_FONT_DECLARE(lv_font_sentinel_cn_16);
LV_FONT_DECLARE(lv_font_sentinel_title_24);
LV_FONT_DECLARE(lv_font_sentinel_ui_18);
LV_FONT_DECLARE(lv_font_sentinel_value_32);
LV_FONT_DECLARE(lv_font_sentinel_unit_18);

#define lv_font_SourceHanSerifSC_Regular_16 lv_font_sentinel_cn_16
#define lv_font_SourceHanSerifSC_Regular_18 lv_font_sentinel_ui_18
#define lv_font_SourceHanSerifSC_Regular_19 lv_font_sentinel_ui_18
#define lv_font_SourceHanSerifSC_Regular_20 lv_font_sentinel_ui_18
#define lv_font_SourceHanSerifSC_Regular_30 lv_font_sentinel_title_24
#define lv_font_montserratMedium_16         lv_font_sentinel_ui_18

/* 两个旧图片控件只保留对象和点击事件，实际位图由最终主题隐藏。 */
#define _log_alpha_123x122    _WiFi_alpha_25x25
#define _meun1_5_alpha_60x60  _WiFi_alpha_25x25

void custom_init(lv_ui *ui);

/**
 * @brief  同步设备控制区三个开关下方的状态文字。
 * @param  buzzer_on     蜂鸣器手动开关状态。
 * @param  illumination_on 照明自动控制开关状态。
 * @param  alarm_light_on  报警灯手动开关状态。
 * @return 无。
 * @note   必须在 LVGL 任务上下文中调用。
 */
void custom_update_control_status(bool buzzer_on, bool illumination_on,
                                  bool alarm_light_on);

/**
 * @brief  刷新PF10照明灯的实际输出状态。
 * @param  light_on true=照明灯实际点亮，false=照明灯实际关闭。
 * @return 无。
 * @note   必须在 LVGL 任务上下文中调用。
 */
void custom_update_light_output_status(bool light_on);

#ifdef __cplusplus
}
#endif
#endif /* EVENT_CB_H_ */
