/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "events_init.h"
#include <stdio.h>
#include "lvgl.h"

#if LV_USE_GUIDER_SIMULATOR && LV_USE_FREEMASTER
#include "freemaster_client.h"
#endif


static void screen_setting_LOG_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        ui_load_scr_animation(&guider_ui, &guider_ui.screen_setting, guider_ui.screen_setting_del, &guider_ui.screen_del, setup_scr_screen_setting, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false, false);
        break;
    }
    default:
        break;
    }
}

void events_init_screen (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_setting_LOG, screen_setting_LOG_event_handler, LV_EVENT_ALL, ui);
}

static void screen_setting_add_1_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        break;
    }
    case LV_EVENT_RELEASED:
    {
        break;
    }
    case LV_EVENT_LONG_PRESSED:
    {
        break;
    }
    default:
        break;
    }
}

static void screen_setting_minus_1_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        break;
    }
    case LV_EVENT_RELEASED:
    {
        break;
    }
    case LV_EVENT_PRESSED:
    {
        break;
    }
    default:
        break;
    }
}

static void screen_setting_add_2_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        break;
    }
    case LV_EVENT_RELEASED:
    {
        break;
    }
    case LV_EVENT_LONG_PRESSED:
    {
        break;
    }
    default:
        break;
    }
}

static void screen_setting_minus_2_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        break;
    }
    case LV_EVENT_RELEASED:
    {
        break;
    }
    case LV_EVENT_LONG_PRESSED:
    {
        break;
    }
    default:
        break;
    }
}

static void screen_setting_minus_3_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        break;
    }
    case LV_EVENT_RELEASED:
    {
        break;
    }
    case LV_EVENT_LONG_PRESSED:
    {
        break;
    }
    default:
        break;
    }
}

static void screen_setting_add_3_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        break;
    }
    case LV_EVENT_RELEASED:
    {
        break;
    }
    case LV_EVENT_LONG_PRESSED:
    {
        break;
    }
    default:
        break;
    }
}

static void screen_setting_add_4_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        break;
    }
    case LV_EVENT_RELEASED:
    {
        break;
    }
    case LV_EVENT_LONG_PRESSED:
    {
        break;
    }
    default:
        break;
    }
}

static void screen_setting_minus_4_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        break;
    }
    case LV_EVENT_RELEASED:
    {
        break;
    }
    case LV_EVENT_LONG_PRESSED:
    {
        break;
    }
    default:
        break;
    }
}

static void screen_setting_btn_save_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        break;
    }
    default:
        break;
    }
}

static void screen_setting_btn_reset_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        break;
    }
    default:
        break;
    }
}

static void screen_setting_btn_Version_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        lv_obj_clear_flag(guider_ui.screen_setting_win_Version, LV_OBJ_FLAG_HIDDEN);
        break;
    }
    default:
        break;
    }
}

static void screen_setting_win_Version_item0_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        lv_obj_add_flag(guider_ui.screen_setting_win_Version, LV_OBJ_FLAG_HIDDEN);
        break;
    }
    default:
        break;
    }
}

static void screen_setting_exit_LOG_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        ui_load_scr_animation(&guider_ui, &guider_ui.screen, guider_ui.screen_del, &guider_ui.screen_setting_del, setup_scr_screen, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, false, false);
        break;
    }
    default:
        break;
    }
}

static void screen_setting_btn_default_event_handler (lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_CLICKED:
    {
        break;
    }
    default:
        break;
    }
}

void events_init_screen_setting (lv_ui *ui)
{
    lv_obj_add_event_cb(ui->screen_setting_add_1, screen_setting_add_1_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_setting_minus_1, screen_setting_minus_1_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_setting_add_2, screen_setting_add_2_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_setting_minus_2, screen_setting_minus_2_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_setting_minus_3, screen_setting_minus_3_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_setting_add_3, screen_setting_add_3_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_setting_add_4, screen_setting_add_4_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_setting_minus_4, screen_setting_minus_4_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_setting_btn_save, screen_setting_btn_save_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_setting_btn_reset, screen_setting_btn_reset_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_setting_btn_Version, screen_setting_btn_Version_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_setting_win_Version_item0, screen_setting_win_Version_item0_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_setting_exit_LOG, screen_setting_exit_LOG_event_handler, LV_EVENT_ALL, ui);
    lv_obj_add_event_cb(ui->screen_setting_btn_default, screen_setting_btn_default_event_handler, LV_EVENT_ALL, ui);
}


void events_init(lv_ui *ui)
{

}
