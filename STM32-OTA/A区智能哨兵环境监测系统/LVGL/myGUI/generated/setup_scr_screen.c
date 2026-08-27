/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"



void setup_scr_screen(lv_ui *ui)
{
    //Write codes screen
    ui->screen = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen, 800, 480);
    lv_obj_set_scrollbar_mode(ui->screen, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_cont_1
    ui->screen_cont_1 = lv_obj_create(ui->screen);
    lv_obj_set_pos(ui->screen_cont_1, 0, 0);
    lv_obj_set_size(ui->screen_cont_1, 800, 480);
    lv_obj_set_scrollbar_mode(ui->screen_cont_1, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_cont_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_cont_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_cont_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_cont_1, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_cont_1, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_cont_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_cont_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_cont_1, LV_GRAD_DIR_HOR, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui->screen_cont_1, lv_color_hex(0xdfecff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui->screen_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui->screen_cont_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_cont_2
    ui->screen_cont_2 = lv_obj_create(ui->screen_cont_1);
    lv_obj_set_pos(ui->screen_cont_2, 204, 15);
    lv_obj_set_size(ui->screen_cont_2, 407, 53);
    lv_obj_set_scrollbar_mode(ui->screen_cont_2, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_cont_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_cont_2, 208, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_cont_2, lv_color_hex(0x3b8aef), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_cont_2, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_cont_2, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_cont_2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_cont_2, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_cont_2, 20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_title
    ui->screen_title = lv_label_create(ui->screen_cont_2);
    lv_label_set_text(ui->screen_title, "智能环境哨兵远程监测");
    lv_label_set_long_mode(ui->screen_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_title, 31, 6);
    lv_obj_set_size(ui->screen_title, 334, 36);

    //Write style for screen_title, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_title, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_title, &lv_font_SourceHanSerifSC_Regular_30, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_title, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_title, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_buzzer_LOG
    ui->screen_buzzer_LOG = lv_img_create(ui->screen_cont_1);
    lv_obj_add_flag(ui->screen_buzzer_LOG, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_buzzer_LOG, &_baojingqi_alpha_40x40);
    lv_img_set_pivot(ui->screen_buzzer_LOG, 50,50);
    lv_img_set_angle(ui->screen_buzzer_LOG, 0);
    lv_obj_set_pos(ui->screen_buzzer_LOG, 456, 144);
    lv_obj_set_size(ui->screen_buzzer_LOG, 40, 40);

    //Write style for screen_buzzer_LOG, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_buzzer_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_buzzer_LOG, lv_color_hex(0x0f0e0e), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_buzzer_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_buzzer_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_buzzer_LOG, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_sw_light
    ui->screen_sw_light = lv_switch_create(ui->screen_cont_1);
    lv_obj_set_pos(ui->screen_sw_light, 255, 310);
    lv_obj_set_size(ui->screen_sw_light, 60, 25);

    //Write style for screen_sw_light, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_sw_light, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_sw_light, lv_color_hex(0xe6e2e6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_sw_light, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_sw_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_sw_light, 20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_sw_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for screen_sw_light, Part: LV_PART_INDICATOR, State: LV_STATE_CHECKED.
    lv_obj_set_style_bg_opa(ui->screen_sw_light, 255, LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(ui->screen_sw_light, lv_color_hex(0x2195f6), LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_bg_grad_dir(ui->screen_sw_light, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_border_width(ui->screen_sw_light, 0, LV_PART_INDICATOR|LV_STATE_CHECKED);

    //Write style for screen_sw_light, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_sw_light, 255, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_sw_light, lv_color_hex(0xffffff), LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_sw_light, LV_GRAD_DIR_NONE, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_sw_light, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_sw_light, 10, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes screen_sw_Alarm_light
    ui->screen_sw_Alarm_light = lv_switch_create(ui->screen_cont_1);
    lv_obj_set_pos(ui->screen_sw_Alarm_light, 620, 313);
    lv_obj_set_size(ui->screen_sw_Alarm_light, 60, 25);

    //Write style for screen_sw_Alarm_light, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_sw_Alarm_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_sw_Alarm_light, 20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_sw_Alarm_light, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_sw_Alarm_light, lv_color_hex(0xe6e2e6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_sw_Alarm_light, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_sw_Alarm_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for screen_sw_Alarm_light, Part: LV_PART_INDICATOR, State: LV_STATE_CHECKED.
    lv_obj_set_style_bg_opa(ui->screen_sw_Alarm_light, 255, LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(ui->screen_sw_Alarm_light, lv_color_hex(0x2195f6), LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_bg_grad_dir(ui->screen_sw_Alarm_light, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_border_width(ui->screen_sw_Alarm_light, 0, LV_PART_INDICATOR|LV_STATE_CHECKED);

    //Write style for screen_sw_Alarm_light, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_sw_Alarm_light, 255, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_sw_Alarm_light, lv_color_hex(0xffffff), LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_sw_Alarm_light, LV_GRAD_DIR_NONE, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_sw_Alarm_light, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_sw_Alarm_light, 10, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes screen_sw_buzzer
    ui->screen_sw_buzzer = lv_switch_create(ui->screen_cont_1);
    lv_obj_set_pos(ui->screen_sw_buzzer, 618, 152);
    lv_obj_set_size(ui->screen_sw_buzzer, 60, 25);

    //Write style for screen_sw_buzzer, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_sw_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_sw_buzzer, 20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_sw_buzzer, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_sw_buzzer, lv_color_hex(0xe6e2e6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_sw_buzzer, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_sw_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for screen_sw_buzzer, Part: LV_PART_INDICATOR, State: LV_STATE_CHECKED.
    lv_obj_set_style_bg_opa(ui->screen_sw_buzzer, 255, LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(ui->screen_sw_buzzer, lv_color_hex(0x2195f6), LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_bg_grad_dir(ui->screen_sw_buzzer, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_CHECKED);
    lv_obj_set_style_border_width(ui->screen_sw_buzzer, 0, LV_PART_INDICATOR|LV_STATE_CHECKED);

    //Write style for screen_sw_buzzer, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_sw_buzzer, 255, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_sw_buzzer, lv_color_hex(0xffffff), LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_sw_buzzer, LV_GRAD_DIR_NONE, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_sw_buzzer, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_sw_buzzer, 10, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes screen_bar_fan
    ui->screen_bar_fan = lv_bar_create(ui->screen_cont_1);
    lv_obj_set_style_anim_time(ui->screen_bar_fan, 1000, 0);
    lv_bar_set_mode(ui->screen_bar_fan, LV_BAR_MODE_NORMAL);
    lv_bar_set_range(ui->screen_bar_fan, 0, 1000);
    lv_bar_set_value(ui->screen_bar_fan, 50, LV_ANIM_OFF);
    lv_obj_set_pos(ui->screen_bar_fan, 301, 155);
    lv_obj_set_size(ui->screen_bar_fan, 100, 20);

    //Write style for screen_bar_fan, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_bar_fan, 63, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_bar_fan, lv_color_hex(0x25333f), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_bar_fan, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_bar_fan, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_bar_fan, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for screen_bar_fan, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_bar_fan, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_bar_fan, lv_color_hex(0x2195f6), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_bar_fan, LV_GRAD_DIR_NONE, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_bar_fan, 10, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write codes screen_label_temperature
    ui->screen_label_temperature = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_label_temperature, "温度:");
    lv_label_set_long_mode(ui->screen_label_temperature, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_pos(ui->screen_label_temperature, 140, 226);
    lv_obj_set_size(ui->screen_label_temperature, 60, 23);

    //Write style for screen_label_temperature, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_label_temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_label_temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_label_temperature, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_label_temperature, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_label_temperature, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_label_temperature, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_label_temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_label_temperature, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_label_temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_label_temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_label_temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_label_temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_label_temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_label_temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_humidity_num
    ui->screen_humidity_num = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_humidity_num, "");
    lv_label_set_long_mode(ui->screen_humidity_num, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_humidity_num, 618, 220);
    lv_obj_set_size(ui->screen_humidity_num, 61, 29);

    //Write style for screen_humidity_num, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_humidity_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_humidity_num, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_humidity_num, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_humidity_num, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_humidity_num, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_humidity_num, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_humidity_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_humidity_num, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_humidity_num, 207, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_humidity_num, lv_color_hex(0x3b8aef), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_humidity_num, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_humidity_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_humidity_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_humidity_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_humidity_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_humidity_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_temperature_num
    ui->screen_temperature_num = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_temperature_num, "");
    lv_label_set_long_mode(ui->screen_temperature_num, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_temperature_num, 255, 223);
    lv_obj_set_size(ui->screen_temperature_num, 65, 29);

    //Write style for screen_temperature_num, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_temperature_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_temperature_num, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_temperature_num, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_temperature_num, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_temperature_num, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_temperature_num, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_temperature_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_temperature_num, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_temperature_num, 213, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_temperature_num, lv_color_hex(0x3b8aef), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_temperature_num, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_temperature_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_temperature_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_temperature_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_temperature_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_temperature_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_sunlight_num
    ui->screen_sunlight_num = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_sunlight_num, "");
    lv_label_set_long_mode(ui->screen_sunlight_num, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_sunlight_num, 253, 396);
    lv_obj_set_size(ui->screen_sunlight_num, 65, 29);

    //Write style for screen_sunlight_num, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_sunlight_num, 212, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_sunlight_num, lv_color_hex(0x3b8aef), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_sunlight_num, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_sunlight_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_sunlight_num, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_sunlight_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_sunlight_num, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_sunlight_num, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_sunlight_num, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_sunlight_num, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_sunlight_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_sunlight_num, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_sunlight_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_sunlight_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_sunlight_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_sunlight_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_fan_num
    ui->screen_fan_num = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_fan_num, "");
    lv_label_set_long_mode(ui->screen_fan_num, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_fan_num, 204, 149);
    lv_obj_set_size(ui->screen_fan_num, 65, 29);

    //Write style for screen_fan_num, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_fan_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_fan_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_fan_num, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_fan_num, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_fan_num, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_fan_num, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_fan_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_fan_num, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_fan_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_fan_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_fan_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_fan_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_fan_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_fan_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_smoke_num
    ui->screen_smoke_num = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_smoke_num, "");
    lv_label_set_long_mode(ui->screen_smoke_num, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_smoke_num, 620, 396);
    lv_obj_set_size(ui->screen_smoke_num, 65, 29);

    //Write style for screen_smoke_num, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_smoke_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_smoke_num, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_smoke_num, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_smoke_num, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_smoke_num, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_smoke_num, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_smoke_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_smoke_num, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_smoke_num, 206, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_smoke_num, lv_color_hex(0x3b8aef), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_smoke_num, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_smoke_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_smoke_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_smoke_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_smoke_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_smoke_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Alarm_light
    ui->screen_Alarm_light = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_Alarm_light, "报警灯：");
    lv_label_set_long_mode(ui->screen_Alarm_light, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_Alarm_light, 514, 315);
    lv_obj_set_size(ui->screen_Alarm_light, 96, 23);

    //Write style for screen_Alarm_light, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_radius(ui->screen_Alarm_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_Alarm_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_Alarm_light, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_Alarm_light, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_Alarm_light, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_Alarm_light, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_Alarm_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_Alarm_light, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_Alarm_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_Alarm_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_Alarm_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_Alarm_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_Alarm_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_Alarm_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_iot_flag
    ui->screen_iot_flag = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_iot_flag, "NC");
    lv_label_set_long_mode(ui->screen_iot_flag, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_iot_flag, 457, 84);
    lv_obj_set_size(ui->screen_iot_flag, 114, 21);

    //Write style for screen_iot_flag, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_radius(ui->screen_iot_flag, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_iot_flag, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_iot_flag, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_iot_flag, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_iot_flag, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_iot_flag, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_iot_flag, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_iot_flag, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_iot_flag, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_iot_flag, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_iot_flag, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_iot_flag, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_iot_flag, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_iot_flag, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_WiFi_LOG
    ui->screen_WiFi_LOG = lv_img_create(ui->screen_cont_1);
    lv_obj_add_flag(ui->screen_WiFi_LOG, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_WiFi_LOG, &_WiFi_alpha_25x25);
    lv_img_set_pivot(ui->screen_WiFi_LOG, 50,50);
    lv_img_set_angle(ui->screen_WiFi_LOG, 0);
    lv_obj_set_pos(ui->screen_WiFi_LOG, 269, 81);
    lv_obj_set_size(ui->screen_WiFi_LOG, 25, 25);

    //Write style for screen_WiFi_LOG, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_WiFi_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_WiFi_LOG, lv_color_hex(0x71abee), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_WiFi_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_WiFi_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_WiFi_LOG, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_Alarm_light_LOG
    ui->screen_Alarm_light_LOG = lv_img_create(ui->screen_cont_1);
    lv_obj_add_flag(ui->screen_Alarm_light_LOG, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_Alarm_light_LOG, &_9_alpha_40x40);
    lv_img_set_pivot(ui->screen_Alarm_light_LOG, 50,50);
    lv_img_set_angle(ui->screen_Alarm_light_LOG, 0);
    lv_obj_set_pos(ui->screen_Alarm_light_LOG, 454, 302);
    lv_obj_set_size(ui->screen_Alarm_light_LOG, 40, 40);

    //Write style for screen_Alarm_light_LOG, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_Alarm_light_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_Alarm_light_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_Alarm_light_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_Alarm_light_LOG, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_LOG
    ui->screen_setting_LOG = lv_img_create(ui->screen_cont_1);
    lv_img_set_src(ui->screen_setting_LOG, &_meun1_5_alpha_60x60);
    lv_img_set_pivot(ui->screen_setting_LOG, 50,50);
    lv_img_set_angle(ui->screen_setting_LOG, 0);
    lv_obj_set_pos(ui->screen_setting_LOG, 33, 15);
    lv_obj_set_size(ui->screen_setting_LOG, 60, 60);
    lv_obj_add_flag(ui->screen_setting_LOG, LV_OBJ_FLAG_CLICKABLE);

    //Write style for screen_setting_LOG, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_setting_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_setting_LOG, lv_color_hex(0x5495f1), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_setting_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_setting_LOG, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_smoke_LOG
    ui->screen_smoke_LOG = lv_img_create(ui->screen_cont_1);
    lv_obj_add_flag(ui->screen_smoke_LOG, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_smoke_LOG, &_7_alpha_40x40);
    lv_img_set_pivot(ui->screen_smoke_LOG, 50,50);
    lv_img_set_angle(ui->screen_smoke_LOG, 0);
    lv_obj_set_pos(ui->screen_smoke_LOG, 458, 387);
    lv_obj_set_size(ui->screen_smoke_LOG, 40, 40);

    //Write style for screen_smoke_LOG, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_smoke_LOG, 154, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_smoke_LOG, lv_color_hex(0x824881), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_smoke_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_smoke_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_smoke_LOG, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_electric_light_LOG
    ui->screen_electric_light_LOG = lv_img_create(ui->screen_cont_1);
    lv_obj_add_flag(ui->screen_electric_light_LOG, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_electric_light_LOG, &_1_alpha_40x40);
    lv_img_set_pivot(ui->screen_electric_light_LOG, 50,50);
    lv_img_set_angle(ui->screen_electric_light_LOG, 0);
    lv_obj_set_pos(ui->screen_electric_light_LOG, 96, 302);
    lv_obj_set_size(ui->screen_electric_light_LOG, 40, 40);

    //Write style for screen_electric_light_LOG, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_electric_light_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_electric_light_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_electric_light_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_electric_light_LOG, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_weather_LOG
    ui->screen_weather_LOG = lv_img_create(ui->screen_cont_1);
    lv_obj_add_flag(ui->screen_weather_LOG, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_weather_LOG, &_log_alpha_123x122);
    lv_img_set_pivot(ui->screen_weather_LOG, 50,50);
    lv_img_set_angle(ui->screen_weather_LOG, 0);
    lv_obj_set_pos(ui->screen_weather_LOG, 662, 9);
    lv_obj_set_size(ui->screen_weather_LOG, 123, 122);

    //Write style for screen_weather_LOG, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_weather_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_weather_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_weather_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_weather_LOG, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_humidity_LOG
    ui->screen_humidity_LOG = lv_img_create(ui->screen_cont_1);
    lv_obj_add_flag(ui->screen_humidity_LOG, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_humidity_LOG, &_hum_alpha_40x40);
    lv_img_set_pivot(ui->screen_humidity_LOG, 50,50);
    lv_img_set_angle(ui->screen_humidity_LOG, 0);
    lv_obj_set_pos(ui->screen_humidity_LOG, 461, 213);
    lv_obj_set_size(ui->screen_humidity_LOG, 40, 40);

    //Write style for screen_humidity_LOG, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_humidity_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_humidity_LOG, lv_color_hex(0x417de9), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_humidity_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_humidity_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_humidity_LOG, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_temperature_LOG
    ui->screen_temperature_LOG = lv_img_create(ui->screen_cont_1);
    lv_obj_add_flag(ui->screen_temperature_LOG, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_temperature_LOG, &_temp1_alpha_40x40);
    lv_img_set_pivot(ui->screen_temperature_LOG, 50,50);
    lv_img_set_angle(ui->screen_temperature_LOG, 0);
    lv_obj_set_pos(ui->screen_temperature_LOG, 96, 213);
    lv_obj_set_size(ui->screen_temperature_LOG, 40, 40);

    //Write style for screen_temperature_LOG, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_temperature_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_temperature_LOG, lv_color_hex(0x5f6bed), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_temperature_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_temperature_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_temperature_LOG, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_sunlight_LOG
    ui->screen_sunlight_LOG = lv_img_create(ui->screen_cont_1);
    lv_obj_add_flag(ui->screen_sunlight_LOG, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_sunlight_LOG, &_6_alpha_40x40);
    lv_img_set_pivot(ui->screen_sunlight_LOG, 50,50);
    lv_img_set_angle(ui->screen_sunlight_LOG, 0);
    lv_obj_set_pos(ui->screen_sunlight_LOG, 99, 389);
    lv_obj_set_size(ui->screen_sunlight_LOG, 40, 40);

    //Write style for screen_sunlight_LOG, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_radius(ui->screen_sunlight_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_sunlight_LOG, true, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui->screen_sunlight_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_sunlight_LOG, lv_color_hex(0xff7419), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_sunlight_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_fan_LOG
    ui->screen_fan_LOG = lv_img_create(ui->screen_cont_1);
    lv_obj_add_flag(ui->screen_fan_LOG, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_fan_LOG, &_fan_alpha_40x40);
    lv_img_set_pivot(ui->screen_fan_LOG, 50,50);
    lv_img_set_angle(ui->screen_fan_LOG, 0);
    lv_obj_set_pos(ui->screen_fan_LOG, 99, 142);
    lv_obj_set_size(ui->screen_fan_LOG, 40, 40);

    //Write style for screen_fan_LOG, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_radius(ui->screen_fan_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_fan_LOG, true, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui->screen_fan_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_fan_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_label_sunlight
    ui->screen_label_sunlight = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_label_sunlight, "光照指数：");
    lv_label_set_long_mode(ui->screen_label_sunlight, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_label_sunlight, 143, 402);
    lv_obj_set_size(ui->screen_label_sunlight, 114, 23);

    //Write style for screen_label_sunlight, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_label_sunlight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_label_sunlight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_label_sunlight, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_label_sunlight, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_label_sunlight, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_label_sunlight, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_label_sunlight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_label_sunlight, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_label_sunlight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_label_sunlight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_label_sunlight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_label_sunlight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_label_sunlight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_label_sunlight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_label_light
    ui->screen_label_light = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_label_light, "照明灯：");
    lv_label_set_long_mode(ui->screen_label_light, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_label_light, 146, 312);
    lv_obj_set_size(ui->screen_label_light, 91, 23);

    //Write style for screen_label_light, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_label_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_label_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_label_light, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_label_light, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_label_light, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_label_light, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_label_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_label_light, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_label_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_label_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_label_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_label_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_label_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_label_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_label_humidity
    ui->screen_label_humidity = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_label_humidity, "湿度:");
    lv_label_set_long_mode(ui->screen_label_humidity, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_label_humidity, 508, 228);
    lv_obj_set_size(ui->screen_label_humidity, 66, 23);

    //Write style for screen_label_humidity, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_label_humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_label_humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_label_humidity, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_label_humidity, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_label_humidity, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_label_humidity, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_label_humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_label_humidity, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_label_humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_label_humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_label_humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_label_humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_label_humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_label_humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_label_IOT
    ui->screen_label_IOT = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_label_IOT, "IOT平台连接：");
    lv_label_set_long_mode(ui->screen_label_IOT, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_label_IOT, 298, 84);
    lv_obj_set_size(ui->screen_label_IOT, 190, 21);

    //Write style for screen_label_IOT, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_label_IOT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_label_IOT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_label_IOT, lv_color_hex(0x161618), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_label_IOT, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_label_IOT, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_label_IOT, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_label_IOT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_label_IOT, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_label_IOT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_label_IOT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_label_IOT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_label_IOT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_label_IOT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_label_IOT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_label_smoke
    ui->screen_label_smoke = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_label_smoke, "烟雾指数：");
    lv_label_set_long_mode(ui->screen_label_smoke, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_label_smoke, 510, 401);
    lv_obj_set_size(ui->screen_label_smoke, 117, 23);

    //Write style for screen_label_smoke, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_label_smoke, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_label_smoke, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_label_smoke, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_label_smoke, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_label_smoke, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_label_smoke, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_label_smoke, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_label_smoke, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_label_smoke, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_label_smoke, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_label_smoke, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_label_smoke, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_label_smoke, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_label_smoke, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_label_setting
    ui->screen_label_setting = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_label_setting, "用户设置");
    lv_label_set_long_mode(ui->screen_label_setting, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_pos(ui->screen_label_setting, 99, 41);
    lv_obj_set_size(ui->screen_label_setting, 60, 23);

    //Write style for screen_label_setting, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_label_setting, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_label_setting, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_label_setting, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_label_setting, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_label_setting, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_label_setting, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_label_setting, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_label_setting, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_label_setting, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_label_setting, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_label_setting, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_label_setting, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_label_setting, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_label_setting, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_label_fan
    ui->screen_label_fan = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_label_fan, "风扇:");
    lv_label_set_long_mode(ui->screen_label_fan, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_pos(ui->screen_label_fan, 142, 152);
    lv_obj_set_size(ui->screen_label_fan, 60, 23);

    //Write style for screen_label_fan, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_radius(ui->screen_label_fan, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_label_fan, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_label_fan, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_label_fan, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_label_fan, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_label_fan, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_label_fan, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_label_fan, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_label_fan, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_label_fan, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_label_fan, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_label_fan, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_label_fan, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_label_fan, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_label_buzzer
    ui->screen_label_buzzer = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_label_buzzer, "蜂鸣器:");
    lv_label_set_long_mode(ui->screen_label_buzzer, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_label_buzzer, 512, 154);
    lv_obj_set_size(ui->screen_label_buzzer, 78, 23);

    //Write style for screen_label_buzzer, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_label_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_label_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_label_buzzer, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_label_buzzer, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_label_buzzer, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_label_buzzer, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_label_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_label_buzzer, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_label_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_label_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_label_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_label_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_label_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_label_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of screen.


    //Update current screen layout.
    lv_obj_update_layout(ui->screen);

    //Init events for screen.
    events_init_screen(ui);
}
