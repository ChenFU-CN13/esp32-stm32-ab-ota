/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef GUI_GUIDER_H
#define GUI_GUIDER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

typedef struct
{
  
	lv_obj_t *screen;
	bool screen_del;
	lv_obj_t *screen_cont_1;
	lv_obj_t *screen_cont_2;
	lv_obj_t *screen_title;
	lv_obj_t *screen_label_temperature;
	lv_obj_t *screen_label_humidity;
	lv_obj_t *screen_humidity;
	lv_obj_t *screen_temperature;
	lv_obj_t *screen_label_light;
	lv_obj_t *screen_label_buzzer;
	lv_obj_t *screen_sw_buzzer;
	lv_obj_t *screen_sw_light;
	lv_obj_t *screen_label_IOT;
	lv_obj_t *screen_IOT_FLAG;
	lv_obj_t *screen_WiFi_LOG;
	lv_obj_t *screen_buzzer_LOG;
	lv_obj_t *screen_setting_LOG;
	lv_obj_t *screen_smoke_LOG;
	lv_obj_t *screen_electric_light_LOG;
	lv_obj_t *screen_weather_LOG;
	lv_obj_t *screen_humidity_LOG;
	lv_obj_t *screen_temperature_LOG;
	lv_obj_t *screen_sunlight_LOG;
	lv_obj_t *screen_label_sunlight;
	lv_obj_t *screen_label_smoke;
	lv_obj_t *screen_label_setting;
	lv_obj_t *screen_sunlight_num;
	lv_obj_t *screen_smoke_num;
	lv_obj_t *screen_setting;
	bool screen_setting_del;
	lv_obj_t *screen_setting_cont_1;
	lv_obj_t *screen_setting_add_1;
	lv_obj_t *screen_setting_minus_1;
	lv_obj_t *screen_setting_label__threshold_temp;
	lv_obj_t *screen_setting_num_temp;
	lv_obj_t *screen_setting_add_2;
	lv_obj_t *screen_setting_minus_2;
	lv_obj_t *screen_setting_label__threshold_hum;
	lv_obj_t *screen_setting_num_hum;
	lv_obj_t *screen_setting_minus_3;
	lv_obj_t *screen_setting_add_3;
	lv_obj_t *screen_setting_label__threshold_sunlight;
	lv_obj_t *screen_setting_num_sunlight;
	lv_obj_t *screen_setting_add_4;
	lv_obj_t *screen_setting_minus_4;
	lv_obj_t *screen_setting_label__threshold_smoke;
	lv_obj_t *screen_setting_num_smoke;
	lv_obj_t *screen_setting_btn_save;
	lv_obj_t *screen_setting_btn_save_label;
	lv_obj_t *screen_setting_btn_reset;
	lv_obj_t *screen_setting_btn_reset_label;
	lv_obj_t *screen_setting_btn_Version;
	lv_obj_t *screen_setting_btn_Version_label;
	lv_obj_t *screen_setting_win_Version;
	lv_obj_t *screen_setting_win_Version_item0;
	lv_obj_t *screen_setting_exit_LOG;
	lv_obj_t *screen_setting_btn_default;
	lv_obj_t *screen_setting_btn_default_label;
	lv_obj_t *screen_setting_cont_2;
	lv_obj_t *screen_setting_label_setting_title;
}lv_ui;

typedef void (*ui_setup_scr_t)(lv_ui * ui);

void ui_init_style(lv_style_t * style);

void ui_load_scr_animation(lv_ui *ui, lv_obj_t ** new_scr, bool new_scr_del, bool * old_scr_del, ui_setup_scr_t setup_scr,
                           lv_scr_load_anim_t anim_type, uint32_t time, uint32_t delay, bool is_clean, bool auto_del);

void ui_animation(void * var, int32_t duration, int32_t delay, int32_t start_value, int32_t end_value, lv_anim_path_cb_t path_cb,
                       uint16_t repeat_cnt, uint32_t repeat_delay, uint32_t playback_time, uint32_t playback_delay,
                       lv_anim_exec_xcb_t exec_cb, lv_anim_start_cb_t start_cb, lv_anim_ready_cb_t ready_cb, lv_anim_deleted_cb_t deleted_cb);


void init_scr_del_flag(lv_ui *ui);

void setup_ui(lv_ui *ui);


extern lv_ui guider_ui;


void setup_scr_screen(lv_ui *ui);
void setup_scr_screen_setting(lv_ui *ui);
LV_IMG_DECLARE(_WiFi_alpha_40x40);
LV_IMG_DECLARE(_9_alpha_40x40);
LV_IMG_DECLARE(_meun1_5_alpha_60x60);
LV_IMG_DECLARE(_7_alpha_40x40);
LV_IMG_DECLARE(_1_alpha_40x40);
LV_IMG_DECLARE(_log_alpha_123x122);
LV_IMG_DECLARE(_hum_alpha_40x40);
LV_IMG_DECLARE(_temp1_alpha_40x40);
LV_IMG_DECLARE(_addmeteo_alpha_40x40);
LV_IMG_DECLARE(_jia_alpha_35x35);
LV_IMG_DECLARE(_jian_alpha_35x35);
LV_IMG_DECLARE(_jia_alpha_35x35);
LV_IMG_DECLARE(_jian_alpha_35x35);
LV_IMG_DECLARE(_jian_alpha_35x35);
LV_IMG_DECLARE(_jia_alpha_35x35);
LV_IMG_DECLARE(_jia_alpha_35x35);
LV_IMG_DECLARE(_jian_alpha_35x35);
LV_IMG_DECLARE(_t1_alpha_50x50);

LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_30)
LV_FONT_DECLARE(lv_font_montserratMedium_16)
LV_FONT_DECLARE(lv_font_SourceHanSerifSC_Regular_20)
LV_FONT_DECLARE(lv_font_montserratMedium_12)


#ifdef __cplusplus
}
#endif
#endif
