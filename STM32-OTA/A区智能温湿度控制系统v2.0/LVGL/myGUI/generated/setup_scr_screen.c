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
    lv_obj_set_pos(ui->screen_cont_1, -1, 0);
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
    lv_obj_set_style_pad_bottom(ui->screen_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_cont_2
    ui->screen_cont_2 = lv_obj_create(ui->screen_cont_1);
    lv_obj_set_pos(ui->screen_cont_2, 204, 15);
    lv_obj_set_size(ui->screen_cont_2, 407, 53);
    lv_obj_set_scrollbar_mode(ui->screen_cont_2, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_cont_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_cont_2, 5, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_cont_2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_cont_2, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_cont_2, 20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_cont_2, 208, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_cont_2, lv_color_hex(0x3b8aef), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_cont_2, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_title
    ui->screen_title = lv_label_create(ui->screen_cont_2);
    lv_label_set_text(ui->screen_title, " 智能家庭环境检测系统 ");
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

    //Write codes screen_label_temperature
    ui->screen_label_temperature = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_label_temperature, "温度:");
    lv_label_set_long_mode(ui->screen_label_temperature, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_pos(ui->screen_label_temperature, 125, 207);
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

    //Write codes screen_label_humidity
    ui->screen_label_humidity = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_label_humidity, "湿度:");
    lv_label_set_long_mode(ui->screen_label_humidity, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_label_humidity, 447, 208);
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

    //Write codes screen_humidity
    ui->screen_humidity = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_humidity, "");
    lv_label_set_long_mode(ui->screen_humidity, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_humidity, 538, 199);
    lv_obj_set_size(ui->screen_humidity, 61, 29);

    //Write style for screen_humidity, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_humidity, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_humidity, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_humidity, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_humidity, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_humidity, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_humidity, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_temperature
    ui->screen_temperature = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_temperature, "");
    lv_label_set_long_mode(ui->screen_temperature, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_temperature, 200, 199);
    lv_obj_set_size(ui->screen_temperature, 65, 29);

    //Write style for screen_temperature, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_temperature, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_temperature, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_temperature, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_temperature, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_temperature, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_temperature, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_label_light
    ui->screen_label_light = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_label_light, "照明灯:");
    lv_label_set_long_mode(ui->screen_label_light, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_label_light, 125, 295);
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

    //Write codes screen_label_buzzer
    ui->screen_label_buzzer = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_label_buzzer, "报警器:");
    lv_label_set_long_mode(ui->screen_label_buzzer, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_label_buzzer, 446, 296);
    lv_obj_set_size(ui->screen_label_buzzer, 96, 23);

    //Write style for screen_label_buzzer, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_label_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_label_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_label_buzzer, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_label_buzzer, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_label_buzzer, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_label_buzzer, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_label_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_label_buzzer, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_label_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_label_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_label_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_label_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_label_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_label_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_sw_buzzer
    ui->screen_sw_buzzer = lv_switch_create(ui->screen_cont_1);
    lv_obj_set_pos(ui->screen_sw_buzzer, 549, 295);
    lv_obj_set_size(ui->screen_sw_buzzer, 60, 25);

    //Write style for screen_sw_buzzer, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_sw_buzzer, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_sw_buzzer, lv_color_hex(0xe6e2e6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_sw_buzzer, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_sw_buzzer, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_sw_buzzer, 20, LV_PART_MAIN|LV_STATE_DEFAULT);
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

    //Write codes screen_sw_light
    ui->screen_sw_light = lv_switch_create(ui->screen_cont_1);
    lv_obj_set_pos(ui->screen_sw_light, 226, 294);
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

    //Write codes screen_label_IOT
    ui->screen_label_IOT = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_label_IOT, "IOT平台连接状态:");
    lv_label_set_long_mode(ui->screen_label_IOT, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_label_IOT, 125, 134);
    lv_obj_set_size(ui->screen_label_IOT, 199, 21);

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

    //Write codes screen_IOT_FLAG
    ui->screen_IOT_FLAG = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_IOT_FLAG, "NC");
    lv_label_set_long_mode(ui->screen_IOT_FLAG, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_IOT_FLAG, 349, 134);
    lv_obj_set_size(ui->screen_IOT_FLAG, 114, 21);

    //Write style for screen_IOT_FLAG, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_IOT_FLAG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_IOT_FLAG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_IOT_FLAG, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_IOT_FLAG, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_IOT_FLAG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_IOT_FLAG, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_IOT_FLAG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_IOT_FLAG, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_IOT_FLAG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_IOT_FLAG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_IOT_FLAG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_IOT_FLAG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_IOT_FLAG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_IOT_FLAG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_WiFi_LOG
    ui->screen_WiFi_LOG = lv_img_create(ui->screen_cont_1);
    lv_obj_add_flag(ui->screen_WiFi_LOG, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_WiFi_LOG, &_WiFi_alpha_40x40);
    lv_img_set_pivot(ui->screen_WiFi_LOG, 50,50);
    lv_img_set_angle(ui->screen_WiFi_LOG, 0);
    lv_obj_set_pos(ui->screen_WiFi_LOG, 81, 121);
    lv_obj_set_size(ui->screen_WiFi_LOG, 40, 40);

    //Write style for screen_WiFi_LOG, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_WiFi_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_WiFi_LOG, lv_color_hex(0x0ac75d), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_WiFi_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_WiFi_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_WiFi_LOG, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_buzzer_LOG
    ui->screen_buzzer_LOG = lv_img_create(ui->screen_cont_1);
    lv_obj_add_flag(ui->screen_buzzer_LOG, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_buzzer_LOG, &_9_alpha_40x40);
    lv_img_set_pivot(ui->screen_buzzer_LOG, 50,50);
    lv_img_set_angle(ui->screen_buzzer_LOG, 0);
    lv_obj_set_pos(ui->screen_buzzer_LOG, 397, 284);
    lv_obj_set_size(ui->screen_buzzer_LOG, 40, 40);

    //Write style for screen_buzzer_LOG, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_buzzer_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_buzzer_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_buzzer_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_buzzer_LOG, true, LV_PART_MAIN|LV_STATE_DEFAULT);

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
    lv_obj_set_pos(ui->screen_smoke_LOG, 398, 368);
    lv_obj_set_size(ui->screen_smoke_LOG, 40, 40);

    //Write style for screen_smoke_LOG, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_smoke_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_smoke_LOG, lv_color_hex(0x2450e6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_smoke_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_smoke_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_smoke_LOG, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_electric_light_LOG
    ui->screen_electric_light_LOG = lv_img_create(ui->screen_cont_1);
    lv_obj_add_flag(ui->screen_electric_light_LOG, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_electric_light_LOG, &_1_alpha_40x40);
    lv_img_set_pivot(ui->screen_electric_light_LOG, 50,50);
    lv_img_set_angle(ui->screen_electric_light_LOG, 0);
    lv_obj_set_pos(ui->screen_electric_light_LOG, 81, 283);
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
    lv_obj_set_pos(ui->screen_weather_LOG, 662, 18);
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
    lv_obj_set_pos(ui->screen_humidity_LOG, 400, 194);
    lv_obj_set_size(ui->screen_humidity_LOG, 40, 40);

    //Write style for screen_humidity_LOG, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_humidity_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_humidity_LOG, lv_color_hex(0x407eed), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_humidity_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_humidity_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_humidity_LOG, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_temperature_LOG
    ui->screen_temperature_LOG = lv_img_create(ui->screen_cont_1);
    lv_obj_add_flag(ui->screen_temperature_LOG, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_temperature_LOG, &_temp1_alpha_40x40);
    lv_img_set_pivot(ui->screen_temperature_LOG, 50,50);
    lv_img_set_angle(ui->screen_temperature_LOG, 0);
    lv_obj_set_pos(ui->screen_temperature_LOG, 81, 194);
    lv_obj_set_size(ui->screen_temperature_LOG, 40, 40);

    //Write style for screen_temperature_LOG, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_temperature_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_temperature_LOG, lv_color_hex(0x407eed), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_temperature_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_temperature_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_temperature_LOG, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_sunlight_LOG
    ui->screen_sunlight_LOG = lv_img_create(ui->screen_cont_1);
    lv_obj_add_flag(ui->screen_sunlight_LOG, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_sunlight_LOG, &_addmeteo_alpha_40x40);
    lv_img_set_pivot(ui->screen_sunlight_LOG, 50,50);
    lv_img_set_angle(ui->screen_sunlight_LOG, 0);
    lv_obj_set_pos(ui->screen_sunlight_LOG, 81, 369);
    lv_obj_set_size(ui->screen_sunlight_LOG, 40, 40);

    //Write style for screen_sunlight_LOG, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_sunlight_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_sunlight_LOG, lv_color_hex(0xff7419), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_sunlight_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_sunlight_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_sunlight_LOG, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_label_sunlight
    ui->screen_label_sunlight = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_label_sunlight, "光照指数:");
    lv_label_set_long_mode(ui->screen_label_sunlight, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_label_sunlight, 126, 382);
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

    //Write codes screen_label_smoke
    ui->screen_label_smoke = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_label_smoke, "烟雾指数:");
    lv_label_set_long_mode(ui->screen_label_smoke, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_label_smoke, 450, 381);
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
    lv_label_set_text(ui->screen_label_setting, " 用户设置 ");
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

    //Write codes screen_sunlight_num
    ui->screen_sunlight_num = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_sunlight_num, "");
    lv_label_set_long_mode(ui->screen_sunlight_num, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_sunlight_num, 236, 377);
    lv_obj_set_size(ui->screen_sunlight_num, 65, 29);

    //Write style for screen_sunlight_num, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_sunlight_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_sunlight_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_sunlight_num, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_sunlight_num, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_sunlight_num, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_sunlight_num, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_sunlight_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_sunlight_num, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_sunlight_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_sunlight_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_sunlight_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_sunlight_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_sunlight_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_sunlight_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_smoke_num
    ui->screen_smoke_num = lv_label_create(ui->screen_cont_1);
    lv_label_set_text(ui->screen_smoke_num, "");
    lv_label_set_long_mode(ui->screen_smoke_num, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_smoke_num, 565, 377);
    lv_obj_set_size(ui->screen_smoke_num, 65, 29);

    //Write style for screen_smoke_num, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_smoke_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_smoke_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_smoke_num, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_smoke_num, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_smoke_num, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_smoke_num, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_smoke_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_smoke_num, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_smoke_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_smoke_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_smoke_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_smoke_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_smoke_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_smoke_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of screen.


    //Update current screen layout.
    lv_obj_update_layout(ui->screen);

    //Init events for screen.
    events_init_screen(ui);
}
