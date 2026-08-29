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



void setup_scr_screen_setting(lv_ui *ui)
{
    //Write codes screen_setting
    ui->screen_setting = lv_obj_create(NULL);
    lv_obj_set_size(ui->screen_setting, 800, 480);
    lv_obj_set_scrollbar_mode(ui->screen_setting, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_setting, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_setting, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_cont_1
    ui->screen_setting_cont_1 = lv_obj_create(ui->screen_setting);
    lv_obj_set_pos(ui->screen_setting_cont_1, 0, 0);
    lv_obj_set_size(ui->screen_setting_cont_1, 800, 480);
    lv_obj_set_scrollbar_mode(ui->screen_setting_cont_1, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_setting_cont_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_setting_cont_1, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_setting_cont_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_setting_cont_1, lv_color_hex(0x2195f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_setting_cont_1, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_setting_cont_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_setting_cont_1, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_setting_cont_1, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_setting_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_setting_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_setting_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_setting_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_setting_cont_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_add_1
    ui->screen_setting_add_1 = lv_img_create(ui->screen_setting_cont_1);
    lv_obj_add_flag(ui->screen_setting_add_1, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_setting_add_1, &_jia_alpha_35x35);
    lv_img_set_pivot(ui->screen_setting_add_1, 50,50);
    lv_img_set_angle(ui->screen_setting_add_1, 0);
    lv_obj_set_pos(ui->screen_setting_add_1, 213, 114);
    lv_obj_set_size(ui->screen_setting_add_1, 35, 35);

    //Write style for screen_setting_add_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_setting_add_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_setting_add_1, lv_color_hex(0x3785ff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_setting_add_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_add_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_setting_add_1, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_minus_1
    ui->screen_setting_minus_1 = lv_img_create(ui->screen_setting_cont_1);
    lv_obj_add_flag(ui->screen_setting_minus_1, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_setting_minus_1, &_jian_alpha_35x35);
    lv_img_set_pivot(ui->screen_setting_minus_1, 50,50);
    lv_img_set_angle(ui->screen_setting_minus_1, 0);
    lv_obj_set_pos(ui->screen_setting_minus_1, 530, 113);
    lv_obj_set_size(ui->screen_setting_minus_1, 35, 35);

    //Write style for screen_setting_minus_1, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_setting_minus_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_setting_minus_1, lv_color_hex(0x3785ff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_setting_minus_1, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_minus_1, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_setting_minus_1, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_label__threshold_temp
    ui->screen_setting_label__threshold_temp = lv_label_create(ui->screen_setting_cont_1);
    lv_label_set_text(ui->screen_setting_label__threshold_temp, "温度阈值");
    lv_label_set_long_mode(ui->screen_setting_label__threshold_temp, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_setting_label__threshold_temp, 258, 110);
    lv_obj_set_size(ui->screen_setting_label__threshold_temp, 124, 42);

    //Write style for screen_setting_label__threshold_temp, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_setting_label__threshold_temp, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_setting_label__threshold_temp, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_setting_label__threshold_temp, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_setting_label__threshold_temp, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_label__threshold_temp, 20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_setting_label__threshold_temp, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_label__threshold_temp, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_setting_label__threshold_temp, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_setting_label__threshold_temp, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_setting_label__threshold_temp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_setting_label__threshold_temp, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_setting_label__threshold_temp, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_setting_label__threshold_temp, lv_color_hex(0x007fff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_setting_label__threshold_temp, LV_GRAD_DIR_HOR, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui->screen_setting_label__threshold_temp, lv_color_hex(0x78b9ef), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui->screen_setting_label__threshold_temp, 78, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui->screen_setting_label__threshold_temp, 206, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_setting_label__threshold_temp, 9, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_setting_label__threshold_temp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_setting_label__threshold_temp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_setting_label__threshold_temp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_setting_label__threshold_temp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_num_temp
    ui->screen_setting_num_temp = lv_label_create(ui->screen_setting_cont_1);
    lv_label_set_text(ui->screen_setting_num_temp, "00");
    lv_label_set_long_mode(ui->screen_setting_num_temp, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_setting_num_temp, 391, 110);
    lv_obj_set_size(ui->screen_setting_num_temp, 124, 42);

    //Write style for screen_setting_num_temp, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_setting_num_temp, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_setting_num_temp, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_setting_num_temp, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_setting_num_temp, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_num_temp, 20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_setting_num_temp, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_num_temp, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_setting_num_temp, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_setting_num_temp, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_setting_num_temp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_setting_num_temp, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_setting_num_temp, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_setting_num_temp, lv_color_hex(0xdfecf9), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_setting_num_temp, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_setting_num_temp, 9, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_setting_num_temp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_setting_num_temp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_setting_num_temp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_setting_num_temp, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_add_2
    ui->screen_setting_add_2 = lv_img_create(ui->screen_setting_cont_1);
    lv_obj_add_flag(ui->screen_setting_add_2, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_setting_add_2, &_jia_alpha_35x35);
    lv_img_set_pivot(ui->screen_setting_add_2, 50,50);
    lv_img_set_angle(ui->screen_setting_add_2, 0);
    lv_obj_set_pos(ui->screen_setting_add_2, 213, 188);
    lv_obj_set_size(ui->screen_setting_add_2, 35, 35);

    //Write style for screen_setting_add_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_setting_add_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_setting_add_2, lv_color_hex(0x3785ff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_setting_add_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_add_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_setting_add_2, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_minus_2
    ui->screen_setting_minus_2 = lv_img_create(ui->screen_setting_cont_1);
    lv_obj_add_flag(ui->screen_setting_minus_2, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_setting_minus_2, &_jian_alpha_35x35);
    lv_img_set_pivot(ui->screen_setting_minus_2, 50,50);
    lv_img_set_angle(ui->screen_setting_minus_2, 0);
    lv_obj_set_pos(ui->screen_setting_minus_2, 530, 187);
    lv_obj_set_size(ui->screen_setting_minus_2, 35, 35);

    //Write style for screen_setting_minus_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_setting_minus_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_setting_minus_2, lv_color_hex(0x3785ff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_setting_minus_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_minus_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_setting_minus_2, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_label__threshold_hum
    ui->screen_setting_label__threshold_hum = lv_label_create(ui->screen_setting_cont_1);
    lv_label_set_text(ui->screen_setting_label__threshold_hum, "湿度阈值");
    lv_label_set_long_mode(ui->screen_setting_label__threshold_hum, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_setting_label__threshold_hum, 256, 182);
    lv_obj_set_size(ui->screen_setting_label__threshold_hum, 124, 42);

    //Write style for screen_setting_label__threshold_hum, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_setting_label__threshold_hum, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_setting_label__threshold_hum, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_setting_label__threshold_hum, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_setting_label__threshold_hum, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_label__threshold_hum, 20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_setting_label__threshold_hum, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_label__threshold_hum, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_setting_label__threshold_hum, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_setting_label__threshold_hum, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_setting_label__threshold_hum, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_setting_label__threshold_hum, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_setting_label__threshold_hum, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_setting_label__threshold_hum, lv_color_hex(0x007fff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_setting_label__threshold_hum, LV_GRAD_DIR_HOR, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui->screen_setting_label__threshold_hum, lv_color_hex(0x78b9ef), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui->screen_setting_label__threshold_hum, 78, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui->screen_setting_label__threshold_hum, 206, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_setting_label__threshold_hum, 9, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_setting_label__threshold_hum, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_setting_label__threshold_hum, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_setting_label__threshold_hum, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_setting_label__threshold_hum, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_num_hum
    ui->screen_setting_num_hum = lv_label_create(ui->screen_setting_cont_1);
    lv_label_set_text(ui->screen_setting_num_hum, "00");
    lv_label_set_long_mode(ui->screen_setting_num_hum, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_setting_num_hum, 391, 182);
    lv_obj_set_size(ui->screen_setting_num_hum, 124, 42);

    //Write style for screen_setting_num_hum, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_setting_num_hum, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_setting_num_hum, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_setting_num_hum, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_setting_num_hum, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_num_hum, 20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_setting_num_hum, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_num_hum, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_setting_num_hum, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_setting_num_hum, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_setting_num_hum, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_setting_num_hum, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_setting_num_hum, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_setting_num_hum, lv_color_hex(0xdfecf9), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_setting_num_hum, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_setting_num_hum, 9, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_setting_num_hum, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_setting_num_hum, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_setting_num_hum, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_setting_num_hum, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_minus_3
    ui->screen_setting_minus_3 = lv_img_create(ui->screen_setting_cont_1);
    lv_obj_add_flag(ui->screen_setting_minus_3, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_setting_minus_3, &_jian_alpha_35x35);
    lv_img_set_pivot(ui->screen_setting_minus_3, 50,50);
    lv_img_set_angle(ui->screen_setting_minus_3, 0);
    lv_obj_set_pos(ui->screen_setting_minus_3, 530, 258);
    lv_obj_set_size(ui->screen_setting_minus_3, 35, 35);

    //Write style for screen_setting_minus_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_setting_minus_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_setting_minus_3, lv_color_hex(0x3785ff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_setting_minus_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_minus_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_setting_minus_3, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_add_3
    ui->screen_setting_add_3 = lv_img_create(ui->screen_setting_cont_1);
    lv_obj_add_flag(ui->screen_setting_add_3, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_setting_add_3, &_jia_alpha_35x35);
    lv_img_set_pivot(ui->screen_setting_add_3, 50,50);
    lv_img_set_angle(ui->screen_setting_add_3, 0);
    lv_obj_set_pos(ui->screen_setting_add_3, 213, 258);
    lv_obj_set_size(ui->screen_setting_add_3, 35, 35);

    //Write style for screen_setting_add_3, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_setting_add_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_setting_add_3, lv_color_hex(0x3785ff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_setting_add_3, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_add_3, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_setting_add_3, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_label__threshold_sunlight
    ui->screen_setting_label__threshold_sunlight = lv_label_create(ui->screen_setting_cont_1);
    lv_label_set_text(ui->screen_setting_label__threshold_sunlight, "光线阈值");
    lv_label_set_long_mode(ui->screen_setting_label__threshold_sunlight, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_setting_label__threshold_sunlight, 258, 258);
    lv_obj_set_size(ui->screen_setting_label__threshold_sunlight, 124, 42);

    //Write style for screen_setting_label__threshold_sunlight, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_setting_label__threshold_sunlight, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_setting_label__threshold_sunlight, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_setting_label__threshold_sunlight, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_setting_label__threshold_sunlight, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_label__threshold_sunlight, 20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_setting_label__threshold_sunlight, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_label__threshold_sunlight, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_setting_label__threshold_sunlight, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_setting_label__threshold_sunlight, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_setting_label__threshold_sunlight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_setting_label__threshold_sunlight, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_setting_label__threshold_sunlight, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_setting_label__threshold_sunlight, lv_color_hex(0x007fff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_setting_label__threshold_sunlight, LV_GRAD_DIR_HOR, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui->screen_setting_label__threshold_sunlight, lv_color_hex(0x78b9ef), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui->screen_setting_label__threshold_sunlight, 78, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui->screen_setting_label__threshold_sunlight, 206, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_setting_label__threshold_sunlight, 9, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_setting_label__threshold_sunlight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_setting_label__threshold_sunlight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_setting_label__threshold_sunlight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_setting_label__threshold_sunlight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_num_sunlight
    ui->screen_setting_num_sunlight = lv_label_create(ui->screen_setting_cont_1);
    lv_label_set_text(ui->screen_setting_num_sunlight, "00");
    lv_label_set_long_mode(ui->screen_setting_num_sunlight, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_setting_num_sunlight, 393, 258);
    lv_obj_set_size(ui->screen_setting_num_sunlight, 124, 42);

    //Write style for screen_setting_num_sunlight, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_setting_num_sunlight, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_setting_num_sunlight, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_setting_num_sunlight, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_setting_num_sunlight, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_num_sunlight, 20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_setting_num_sunlight, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_num_sunlight, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_setting_num_sunlight, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_setting_num_sunlight, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_setting_num_sunlight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_setting_num_sunlight, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_setting_num_sunlight, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_setting_num_sunlight, lv_color_hex(0xdfecf9), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_setting_num_sunlight, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_setting_num_sunlight, 9, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_setting_num_sunlight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_setting_num_sunlight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_setting_num_sunlight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_setting_num_sunlight, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_add_4
    ui->screen_setting_add_4 = lv_img_create(ui->screen_setting_cont_1);
    lv_obj_add_flag(ui->screen_setting_add_4, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_setting_add_4, &_jia_alpha_35x35);
    lv_img_set_pivot(ui->screen_setting_add_4, 50,50);
    lv_img_set_angle(ui->screen_setting_add_4, 0);
    lv_obj_set_pos(ui->screen_setting_add_4, 213, 330);
    lv_obj_set_size(ui->screen_setting_add_4, 35, 35);

    //Write style for screen_setting_add_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_setting_add_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_setting_add_4, lv_color_hex(0x3785ff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_setting_add_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_add_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_setting_add_4, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_minus_4
    ui->screen_setting_minus_4 = lv_img_create(ui->screen_setting_cont_1);
    lv_obj_add_flag(ui->screen_setting_minus_4, LV_OBJ_FLAG_CLICKABLE);
    lv_img_set_src(ui->screen_setting_minus_4, &_jian_alpha_35x35);
    lv_img_set_pivot(ui->screen_setting_minus_4, 50,50);
    lv_img_set_angle(ui->screen_setting_minus_4, 0);
    lv_obj_set_pos(ui->screen_setting_minus_4, 530, 330);
    lv_obj_set_size(ui->screen_setting_minus_4, 35, 35);

    //Write style for screen_setting_minus_4, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_setting_minus_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_setting_minus_4, lv_color_hex(0x3785ff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_setting_minus_4, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_minus_4, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_setting_minus_4, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_label__threshold_smoke
    ui->screen_setting_label__threshold_smoke = lv_label_create(ui->screen_setting_cont_1);
    lv_label_set_text(ui->screen_setting_label__threshold_smoke, "烟雾阈值");
    lv_label_set_long_mode(ui->screen_setting_label__threshold_smoke, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_setting_label__threshold_smoke, 258, 327);
    lv_obj_set_size(ui->screen_setting_label__threshold_smoke, 124, 42);

    //Write style for screen_setting_label__threshold_smoke, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_setting_label__threshold_smoke, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_setting_label__threshold_smoke, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_setting_label__threshold_smoke, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_setting_label__threshold_smoke, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_label__threshold_smoke, 20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_setting_label__threshold_smoke, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_label__threshold_smoke, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_setting_label__threshold_smoke, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_setting_label__threshold_smoke, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_setting_label__threshold_smoke, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_setting_label__threshold_smoke, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_setting_label__threshold_smoke, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_setting_label__threshold_smoke, lv_color_hex(0x007fff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_setting_label__threshold_smoke, LV_GRAD_DIR_HOR, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui->screen_setting_label__threshold_smoke, lv_color_hex(0x78b9ef), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui->screen_setting_label__threshold_smoke, 78, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui->screen_setting_label__threshold_smoke, 206, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_setting_label__threshold_smoke, 9, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_setting_label__threshold_smoke, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_setting_label__threshold_smoke, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_setting_label__threshold_smoke, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_setting_label__threshold_smoke, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_num_smoke
    ui->screen_setting_num_smoke = lv_label_create(ui->screen_setting_cont_1);
    lv_label_set_text(ui->screen_setting_num_smoke, "00");
    lv_label_set_long_mode(ui->screen_setting_num_smoke, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_setting_num_smoke, 393, 327);
    lv_obj_set_size(ui->screen_setting_num_smoke, 124, 42);

    //Write style for screen_setting_num_smoke, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_setting_num_smoke, 3, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_setting_num_smoke, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_setting_num_smoke, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_setting_num_smoke, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_num_smoke, 20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_setting_num_smoke, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_num_smoke, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_setting_num_smoke, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_setting_num_smoke, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_setting_num_smoke, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_setting_num_smoke, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_setting_num_smoke, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_setting_num_smoke, lv_color_hex(0xdfecf9), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_setting_num_smoke, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_setting_num_smoke, 9, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_setting_num_smoke, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_setting_num_smoke, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_setting_num_smoke, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_setting_num_smoke, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_btn_save
    ui->screen_setting_btn_save = lv_btn_create(ui->screen_setting_cont_1);
    ui->screen_setting_btn_save_label = lv_label_create(ui->screen_setting_btn_save);
    lv_label_set_text(ui->screen_setting_btn_save_label, "保存设置");
    lv_label_set_long_mode(ui->screen_setting_btn_save_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_setting_btn_save_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_setting_btn_save, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_setting_btn_save_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_setting_btn_save, 139, 393);
    lv_obj_set_size(ui->screen_setting_btn_save, 117, 50);
    lv_obj_add_flag(ui->screen_setting_btn_save, LV_OBJ_FLAG_CLICKABLE);

    //Write style for screen_setting_btn_save, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_setting_btn_save, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_setting_btn_save, lv_color_hex(0x2698f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_setting_btn_save, LV_GRAD_DIR_HOR, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui->screen_setting_btn_save, lv_color_hex(0x8fbfe6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui->screen_setting_btn_save, 20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui->screen_setting_btn_save, 193, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_setting_btn_save, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_btn_save, 30, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_setting_btn_save, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_setting_btn_save, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_btn_save, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_setting_btn_save, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_setting_btn_save, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_btn_reset
    ui->screen_setting_btn_reset = lv_btn_create(ui->screen_setting_cont_1);
    ui->screen_setting_btn_reset_label = lv_label_create(ui->screen_setting_btn_reset);
    lv_label_set_text(ui->screen_setting_btn_reset_label, "复位报警");
    lv_label_set_long_mode(ui->screen_setting_btn_reset_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_setting_btn_reset_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_setting_btn_reset, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_setting_btn_reset_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_setting_btn_reset, 546, 393);
    lv_obj_set_size(ui->screen_setting_btn_reset, 117, 50);
    lv_obj_add_flag(ui->screen_setting_btn_reset, LV_OBJ_FLAG_CLICKABLE);

    //Write style for screen_setting_btn_reset, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_setting_btn_reset, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_setting_btn_reset, lv_color_hex(0x2698f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_setting_btn_reset, LV_GRAD_DIR_HOR, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui->screen_setting_btn_reset, lv_color_hex(0x8fbfe6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui->screen_setting_btn_reset, 20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui->screen_setting_btn_reset, 193, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_setting_btn_reset, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_btn_reset, 30, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_setting_btn_reset, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_setting_btn_reset, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_btn_reset, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_setting_btn_reset, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_setting_btn_reset, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_btn_Version
    ui->screen_setting_btn_Version = lv_btn_create(ui->screen_setting_cont_1);
    ui->screen_setting_btn_Version_label = lv_label_create(ui->screen_setting_btn_Version);
    lv_label_set_text(ui->screen_setting_btn_Version_label, "版本信息查询");
    lv_label_set_long_mode(ui->screen_setting_btn_Version_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_setting_btn_Version_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_setting_btn_Version, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_setting_btn_Version_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_setting_btn_Version, 613, 31);
    lv_obj_set_size(ui->screen_setting_btn_Version, 153, 50);
    lv_obj_add_flag(ui->screen_setting_btn_Version, LV_OBJ_FLAG_CLICKABLE);

    //Write style for screen_setting_btn_Version, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_setting_btn_Version, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_setting_btn_Version, lv_color_hex(0x2698f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_setting_btn_Version, LV_GRAD_DIR_HOR, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui->screen_setting_btn_Version, lv_color_hex(0x8fbfe6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui->screen_setting_btn_Version, 20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui->screen_setting_btn_Version, 193, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_setting_btn_Version, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_btn_Version, 30, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_setting_btn_Version, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_setting_btn_Version, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_btn_Version, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_setting_btn_Version, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_setting_btn_Version, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_win_Version
    ui->screen_setting_win_Version = lv_win_create(ui->screen_setting_cont_1, 40);
    lv_obj_t * screen_setting_win_Version_title = lv_win_add_title(ui->screen_setting_win_Version, "Version Information");
    ui->screen_setting_win_Version_item0 = lv_win_add_btn(ui->screen_setting_win_Version, LV_SYMBOL_CLOSE, 40);
    lv_obj_t *screen_setting_win_Version_label = lv_label_create(lv_win_get_content(ui->screen_setting_win_Version));
    lv_label_set_text(screen_setting_win_Version_label, "v2.0.0");
    lv_obj_set_scrollbar_mode(lv_win_get_content(ui->screen_setting_win_Version), LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_pos(ui->screen_setting_win_Version, 246, 144);
    lv_obj_set_size(ui->screen_setting_win_Version, 282, 146);
    lv_obj_set_scrollbar_mode(ui->screen_setting_win_Version, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(ui->screen_setting_win_Version, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->screen_setting_win_Version, LV_OBJ_FLAG_HIDDEN);

    //Write style for screen_setting_win_Version, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_setting_win_Version, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_setting_win_Version, lv_color_hex(0xeeeef6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_setting_win_Version, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(ui->screen_setting_win_Version, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_setting_win_Version, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_DEFAULT for &style_screen_setting_win_Version_extra_content_main_default
    static lv_style_t style_screen_setting_win_Version_extra_content_main_default;
    ui_init_style(&style_screen_setting_win_Version_extra_content_main_default);

    lv_style_set_bg_opa(&style_screen_setting_win_Version_extra_content_main_default, 255);
    lv_style_set_bg_color(&style_screen_setting_win_Version_extra_content_main_default, lv_color_hex(0xeeeef6));
    lv_style_set_bg_grad_dir(&style_screen_setting_win_Version_extra_content_main_default, LV_GRAD_DIR_NONE);
    lv_style_set_text_color(&style_screen_setting_win_Version_extra_content_main_default, lv_color_hex(0x393c41));
    lv_style_set_text_font(&style_screen_setting_win_Version_extra_content_main_default, &lv_font_montserratMedium_12);
    lv_style_set_text_opa(&style_screen_setting_win_Version_extra_content_main_default, 255);
    lv_style_set_text_letter_space(&style_screen_setting_win_Version_extra_content_main_default, 0);
    lv_style_set_text_line_space(&style_screen_setting_win_Version_extra_content_main_default, 2);
    lv_obj_add_style(lv_win_get_content(ui->screen_setting_win_Version), &style_screen_setting_win_Version_extra_content_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_DEFAULT for &style_screen_setting_win_Version_extra_header_main_default
    static lv_style_t style_screen_setting_win_Version_extra_header_main_default;
    ui_init_style(&style_screen_setting_win_Version_extra_header_main_default);

    lv_style_set_bg_opa(&style_screen_setting_win_Version_extra_header_main_default, 255);
    lv_style_set_bg_color(&style_screen_setting_win_Version_extra_header_main_default, lv_color_hex(0xe6e6e6));
    lv_style_set_bg_grad_dir(&style_screen_setting_win_Version_extra_header_main_default, LV_GRAD_DIR_NONE);
    lv_style_set_text_color(&style_screen_setting_win_Version_extra_header_main_default, lv_color_hex(0x393c41));
    lv_style_set_text_font(&style_screen_setting_win_Version_extra_header_main_default, &lv_font_montserratMedium_12);
    lv_style_set_text_opa(&style_screen_setting_win_Version_extra_header_main_default, 255);
    lv_style_set_text_letter_space(&style_screen_setting_win_Version_extra_header_main_default, 0);
    lv_style_set_text_line_space(&style_screen_setting_win_Version_extra_header_main_default, 2);
    lv_style_set_pad_top(&style_screen_setting_win_Version_extra_header_main_default, 5);
    lv_style_set_pad_right(&style_screen_setting_win_Version_extra_header_main_default, 5);
    lv_style_set_pad_bottom(&style_screen_setting_win_Version_extra_header_main_default, 5);
    lv_style_set_pad_left(&style_screen_setting_win_Version_extra_header_main_default, 5);
    lv_style_set_pad_column(&style_screen_setting_win_Version_extra_header_main_default, 5);
    lv_obj_add_style(lv_win_get_header(ui->screen_setting_win_Version), &style_screen_setting_win_Version_extra_header_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style state: LV_STATE_DEFAULT for &style_screen_setting_win_Version_extra_btns_main_default
    static lv_style_t style_screen_setting_win_Version_extra_btns_main_default;
    ui_init_style(&style_screen_setting_win_Version_extra_btns_main_default);

    lv_style_set_radius(&style_screen_setting_win_Version_extra_btns_main_default, 8);
    lv_style_set_border_width(&style_screen_setting_win_Version_extra_btns_main_default, 0);
    lv_style_set_bg_opa(&style_screen_setting_win_Version_extra_btns_main_default, 255);
    lv_style_set_bg_color(&style_screen_setting_win_Version_extra_btns_main_default, lv_color_hex(0x2195f6));
    lv_style_set_bg_grad_dir(&style_screen_setting_win_Version_extra_btns_main_default, LV_GRAD_DIR_NONE);
    lv_style_set_shadow_width(&style_screen_setting_win_Version_extra_btns_main_default, 0);
    lv_obj_add_style(ui->screen_setting_win_Version_item0, &style_screen_setting_win_Version_extra_btns_main_default, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_exit_LOG
    ui->screen_setting_exit_LOG = lv_img_create(ui->screen_setting_cont_1);
    lv_img_set_src(ui->screen_setting_exit_LOG, &_t1_alpha_50x50);
    lv_img_set_pivot(ui->screen_setting_exit_LOG, 50,50);
    lv_img_set_angle(ui->screen_setting_exit_LOG, 0);
    lv_obj_set_pos(ui->screen_setting_exit_LOG, 34, 30);
    lv_obj_set_size(ui->screen_setting_exit_LOG, 50, 50);
    lv_obj_add_flag(ui->screen_setting_exit_LOG, LV_OBJ_FLAG_CLICKABLE);

    //Write style for screen_setting_exit_LOG, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_img_recolor_opa(ui->screen_setting_exit_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui->screen_setting_exit_LOG, lv_color_hex(0x5990f3), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(ui->screen_setting_exit_LOG, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_exit_LOG, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_clip_corner(ui->screen_setting_exit_LOG, true, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_btn_default
    ui->screen_setting_btn_default = lv_btn_create(ui->screen_setting_cont_1);
    ui->screen_setting_btn_default_label = lv_label_create(ui->screen_setting_btn_default);
    lv_label_set_text(ui->screen_setting_btn_default_label, "恢复默认");
    lv_label_set_long_mode(ui->screen_setting_btn_default_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->screen_setting_btn_default_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->screen_setting_btn_default, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->screen_setting_btn_default_label, LV_PCT(100));
    lv_obj_set_pos(ui->screen_setting_btn_default, 336, 393);
    lv_obj_set_size(ui->screen_setting_btn_default, 117, 50);
    lv_obj_add_flag(ui->screen_setting_btn_default, LV_OBJ_FLAG_CLICKABLE);

    //Write style for screen_setting_btn_default, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->screen_setting_btn_default, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_setting_btn_default, lv_color_hex(0x2698f6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_setting_btn_default, LV_GRAD_DIR_HOR, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui->screen_setting_btn_default, lv_color_hex(0x8fbfe6), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui->screen_setting_btn_default, 20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui->screen_setting_btn_default, 193, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_setting_btn_default, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_btn_default, 30, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_setting_btn_default, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_setting_btn_default, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_btn_default, &lv_font_SourceHanSerifSC_Regular_20, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_setting_btn_default, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_setting_btn_default, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_cont_2
    ui->screen_setting_cont_2 = lv_obj_create(ui->screen_setting_cont_1);
    lv_obj_set_pos(ui->screen_setting_cont_2, 319, 31);
    lv_obj_set_size(ui->screen_setting_cont_2, 140, 50);
    lv_obj_set_scrollbar_mode(ui->screen_setting_cont_2, LV_SCROLLBAR_MODE_OFF);

    //Write style for screen_setting_cont_2, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_setting_cont_2, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui->screen_setting_cont_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui->screen_setting_cont_2, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui->screen_setting_cont_2, LV_BORDER_SIDE_FULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_cont_2, 30, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_setting_cont_2, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_setting_cont_2, lv_color_hex(0x3785ff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->screen_setting_cont_2, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_setting_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_setting_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_setting_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_setting_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_setting_cont_2, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes screen_setting_label_setting_title
    ui->screen_setting_label_setting_title = lv_label_create(ui->screen_setting_cont_2);
    lv_label_set_text(ui->screen_setting_label_setting_title, "设 置");
    lv_label_set_long_mode(ui->screen_setting_label_setting_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(ui->screen_setting_label_setting_title, 26, 5);
    lv_obj_set_size(ui->screen_setting_label_setting_title, 84, 36);

    //Write style for screen_setting_label_setting_title, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->screen_setting_label_setting_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_setting_label_setting_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_setting_label_setting_title, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_label_setting_title, &lv_font_SourceHanSerifSC_Regular_30, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->screen_setting_label_setting_title, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->screen_setting_label_setting_title, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->screen_setting_label_setting_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_setting_label_setting_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_setting_label_setting_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->screen_setting_label_setting_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->screen_setting_label_setting_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->screen_setting_label_setting_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->screen_setting_label_setting_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_setting_label_setting_title, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of screen_setting.


    //Update current screen layout.
    lv_obj_update_layout(ui->screen_setting);

    //Init events for screen.
    events_init_screen_setting(ui);
}
