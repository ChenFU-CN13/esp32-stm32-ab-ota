/*
 * Copyright 2023 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms.
 */

#include "lvgl.h"
#include "custom.h"

#define UI_COLOR_BG            lv_color_hex(0xEEF4FF)
#define UI_COLOR_CARD          lv_color_hex(0xFFFFFF)
#define UI_COLOR_PRIMARY       lv_color_hex(0x2F80ED)
#define UI_COLOR_TEXT          lv_color_hex(0x142033)
#define UI_COLOR_MUTED         lv_color_hex(0x718096)
#define UI_COLOR_DIVIDER       lv_color_hex(0xDCE5F2)
#define UI_COLOR_TRACK         lv_color_hex(0xD9E0EA)
#define UI_COLOR_ORANGE        lv_color_hex(0xFF8A18)
#define UI_COLOR_PURPLE        lv_color_hex(0x8A4DE8)
#define UI_COLOR_RED           lv_color_hex(0xE24B52)
#define UI_COLOR_GREEN         lv_color_hex(0x18B86B)
#define UI_COLOR_DARK_MUTED    lv_color_hex(0x475569)
#define UI_COLOR_SETTING_BLUE  lv_color_hex(0x3785FF)

static lv_obj_t *s_styled_main_screen;
static lv_obj_t *s_buzzer_status_label;
static lv_obj_t *s_illumination_status_label;
static lv_obj_t *s_light_output_status_label;
static lv_obj_t *s_alarm_light_status_label;
static bool s_styles_ready;
static lv_style_t s_card_style;
static lv_style_t s_name_style;
static lv_style_t s_small_text_style;
static lv_style_t s_divider_style;
static lv_style_t s_switch_track_style;
static lv_style_t s_switch_checked_style;
static lv_style_t s_switch_knob_style;

static void ui_style_setting_page(lv_ui *ui);

/**
 * @brief  设置页创建完成后应用最终主题。
 * @param  e “用户设置”入口的 LVGL 点击事件。
 * @return 无。
 * @note   生成代码先创建设置页，本回调随后执行，因此不会被 GUI Guider 重新生成覆盖。
 */
static void ui_setting_page_event_handler(lv_event_t *e)
{
    lv_ui *ui;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    ui = (lv_ui *)lv_event_get_user_data(e);
    ui_style_setting_page(ui);
}

/**
 * @brief  将设置页控件背景统一为标题纯蓝色。
 * @param  obj 需要去除渐变的 LVGL 控件。
 * @return 无。
 * @note   仅覆盖默认状态的背景色与渐变方向，不改变控件尺寸和事件。
 */
static void ui_style_setting_solid_blue(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, UI_COLOR_SETTING_BLUE,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_NONE,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
}

/**
 * @brief  初始化主页面共用样式。
 * @param  无。
 * @return 无。
 * @note   多个控件共享同一份样式，减少 LVGL 48 KB 内存池中的重复属性。
 */
static void ui_theme_styles_init(void)
{
    if (s_styles_ready) {
        return;
    }

    lv_style_init(&s_card_style);
    lv_style_set_bg_color(&s_card_style, UI_COLOR_CARD);
    lv_style_set_bg_opa(&s_card_style, LV_OPA_COVER);
    lv_style_set_radius(&s_card_style, 14);
    lv_style_set_shadow_color(&s_card_style, lv_color_hex(0xAFC4E4));
    lv_style_set_shadow_opa(&s_card_style, LV_OPA_20);
    lv_style_set_shadow_width(&s_card_style, 8);
    lv_style_set_shadow_ofs_y(&s_card_style, 2);

    lv_style_init(&s_name_style);
    lv_style_set_text_color(&s_name_style, UI_COLOR_TEXT);
    lv_style_set_text_font(&s_name_style, &lv_font_sentinel_ui_18);
    lv_style_set_text_align(&s_name_style, LV_TEXT_ALIGN_LEFT);
    lv_style_set_bg_opa(&s_name_style, LV_OPA_TRANSP);
    lv_style_set_pad_all(&s_name_style, 0);

    lv_style_init(&s_small_text_style);
    lv_style_set_text_font(&s_small_text_style, &lv_font_sentinel_cn_16);
    lv_style_set_bg_opa(&s_small_text_style, LV_OPA_TRANSP);
    lv_style_set_pad_all(&s_small_text_style, 0);

    lv_style_init(&s_divider_style);
    lv_style_set_bg_color(&s_divider_style, UI_COLOR_DIVIDER);
    lv_style_set_bg_opa(&s_divider_style, LV_OPA_COVER);

    lv_style_init(&s_switch_track_style);
    lv_style_set_bg_color(&s_switch_track_style, UI_COLOR_TRACK);
    lv_style_set_bg_opa(&s_switch_track_style, LV_OPA_COVER);
    lv_style_set_radius(&s_switch_track_style, 16);

    lv_style_init(&s_switch_checked_style);
    lv_style_set_bg_color(&s_switch_checked_style, UI_COLOR_PRIMARY);
    lv_style_set_bg_opa(&s_switch_checked_style, LV_OPA_COVER);
    lv_style_set_radius(&s_switch_checked_style, 16);

    lv_style_init(&s_switch_knob_style);
    lv_style_set_bg_color(&s_switch_knob_style, UI_COLOR_CARD);
    lv_style_set_bg_opa(&s_switch_knob_style, LV_OPA_COVER);
    lv_style_set_radius(&s_switch_knob_style, LV_RADIUS_CIRCLE);
    lv_style_set_pad_all(&s_switch_knob_style, 1);
    lv_style_set_border_color(&s_switch_knob_style, lv_color_hex(0xCCD5E2));
    lv_style_set_border_width(&s_switch_knob_style, 1);
    lv_style_set_shadow_color(&s_switch_knob_style, lv_color_hex(0x8FA1B8));
    lv_style_set_shadow_opa(&s_switch_knob_style, LV_OPA_20);
    lv_style_set_shadow_width(&s_switch_knob_style, 4);

    s_styles_ready = true;
}

/**
 * @brief  创建轻量白色卡片背景。
 * @param  parent 父对象。
 * @param  x      横坐标。
 * @param  y      纵坐标。
 * @param  w      宽度。
 * @param  h      高度。
 * @return 创建后的卡片对象。
 * @note   仅使用纯色、圆角和小范围阴影，避免大图片及大面积透明图层占用资源。
 */
static lv_obj_t *ui_create_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *card = lv_obj_create(parent);

    lv_obj_remove_style_all(card);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(card, &s_card_style, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_move_background(card);

    return card;
}

/**
 * @brief  创建图标后的浅色圆形底座。
 * @param  parent 父对象。
 * @param  x      横坐标。
 * @param  y      纵坐标。
 * @param  size   圆形直径。
 * @param  color  圆形背景颜色。
 * @return 创建后的圆形对象。
 */
static lv_obj_t *ui_create_icon_disc(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                     lv_coord_t size, lv_color_t color)
{
    lv_obj_t *disc = lv_obj_create(parent);

    lv_obj_remove_style_all(disc);
    lv_obj_set_pos(disc, x, y);
    lv_obj_set_size(disc, size, size);
    lv_obj_clear_flag(disc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(disc, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(disc, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);

    return disc;
}

/**
 * @brief  创建设备控制区的竖向分隔线。
 * @param  parent 父对象。
 * @param  x      横坐标。
 * @param  y      纵坐标。
 * @param  h      高度。
 * @return 创建后的分隔线对象。
 */
static lv_obj_t *ui_create_divider(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                   lv_coord_t h)
{
    lv_obj_t *divider = lv_obj_create(parent);

    lv_obj_remove_style_all(divider);
    lv_obj_set_pos(divider, x, y);
    lv_obj_set_size(divider, 1, h);
    lv_obj_clear_flag(divider, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(divider, &s_divider_style, LV_PART_MAIN | LV_STATE_DEFAULT);

    return divider;
}

/**
 * @brief  创建仅用于显示的英文辅助标签。
 * @param  parent 父对象。
 * @param  text   显示文本。
 * @param  x      横坐标。
 * @param  y      纵坐标。
 * @param  w      宽度。
 * @param  color  文字颜色。
 * @return 创建后的标签对象。
 * @note   使用仅含本页面文字的 16 px 中文子集字体，控制 Flash 占用。
 */
static lv_obj_t *ui_create_hint(lv_obj_t *parent, const char *text,
                                lv_coord_t x, lv_coord_t y, lv_coord_t w,
                                lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_remove_style_all(label);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, w, 20);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_add_style(label, &s_small_text_style, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN | LV_STATE_DEFAULT);

    return label;
}

/**
 * @brief  更新单个设备开关的状态标签。
 * @param  label   状态标签对象。
 * @param  enabled  当前状态是否开启。
 * @param  on_text  开启时显示的文字。
 * @param  off_text 关闭时显示的文字。
 * @return 无。
 * @note   标签失效时直接忽略，避免页面生命周期变化导致访问无效对象。
 */
static void ui_update_control_status_label(lv_obj_t *label, bool enabled,
                                           const char *on_text,
                                           const char *off_text)
{
    if (label == NULL || !lv_obj_is_valid(label)) {
        return;
    }

    lv_label_set_text(label, enabled ? on_text : off_text);
    lv_obj_set_style_text_color(label,
                                enabled ? UI_COLOR_PRIMARY : UI_COLOR_MUTED,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
}

/**
 * @brief  同步设备控制区三个开关下方的状态文字。
 * @param  buzzer_on       蜂鸣器手动开关状态。
 * @param  illumination_on 照明自动控制开关状态。
 * @param  alarm_light_on  报警灯手动开关状态。
 * @return 无。
 * @note   状态文字跟随用户开关意图，不跟随烟雾报警期间的临时硬件覆盖输出。
 */
void custom_update_control_status(bool buzzer_on, bool illumination_on,
                                  bool alarm_light_on)
{
    ui_update_control_status_label(s_buzzer_status_label, buzzer_on,
                                   "已开启", "已关闭");
    ui_update_control_status_label(s_illumination_status_label, illumination_on,
                                   "控制开启", "控制关闭");
    ui_update_control_status_label(s_alarm_light_status_label, alarm_light_on,
                                   "已开启", "已关闭");
}

/**
 * @brief  刷新PF10照明灯的实际输出状态。
 * @param  light_on true=照明灯实际点亮，false=照明灯实际关闭。
 * @return 无。
 * @note   该状态来自报警引擎最终输出，不等同于自动控制开关状态。
 */
void custom_update_light_output_status(bool light_on)
{
    ui_update_control_status_label(s_light_output_status_label, light_on,
                                   "已开启", "已关闭");

    if (s_light_output_status_label != NULL &&
        lv_obj_is_valid(s_light_output_status_label) && light_on) {
        lv_obj_set_style_text_color(s_light_output_status_label,
                                    UI_COLOR_ORANGE,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

/**
 * @brief  设置普通中文标签的统一样式和位置。
 * @param  label 标签对象。
 * @param  x     横坐标。
 * @param  y     纵坐标。
 * @param  w     宽度。
 * @return 无。
 */
static void ui_style_name_label(lv_obj_t *label, lv_coord_t x, lv_coord_t y, lv_coord_t w)
{
    /* 释放 GUI Guider 生成的冗余局部样式，避免同一控件保存两套属性。 */
    lv_obj_remove_style_all(label);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, w, 25);
    lv_obj_add_style(label, &s_name_style, LV_PART_MAIN | LV_STATE_DEFAULT);
}

/**
 * @brief  创建“数值 + 单位”的自适应横向容器。
 * @param  parent      父对象。
 * @param  value_label GUI Guider 原数值标签，保留其对象句柄。
 * @param  unit_text   单位文本。
 * @param  x           横坐标。
 * @param  y           纵坐标。
 * @param  w           容器宽度。
 * @param  color       数值强调色。
 * @return 创建后的横向容器。
 * @note   数值标签采用内容宽度，单位会随数值长度自动移动，便于后续实时数据更新。
 */
static lv_obj_t *ui_create_value_row(lv_obj_t *parent, lv_obj_t *value_label,
                                     const char *unit_text, lv_coord_t x,
                                     lv_coord_t y, lv_coord_t w, lv_color_t color)
{
    lv_obj_t *row;
    lv_obj_t *unit_label;

    row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_pos(row, x, y);
    lv_obj_set_size(row, w, 32);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_parent(value_label, row);
    lv_obj_remove_style_all(value_label);
    lv_obj_set_size(value_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_label_set_long_mode(value_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(value_label, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(value_label, &lv_font_sentinel_value_32,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(value_label, LV_OPA_TRANSP,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(value_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    unit_label = lv_label_create(row);
    lv_obj_remove_style_all(unit_label);
    lv_label_set_text(unit_label, unit_text);
    lv_obj_set_size(unit_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(unit_label, UI_COLOR_MUTED,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(unit_label, &lv_font_sentinel_unit_18,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(unit_label, LV_OPA_TRANSP,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(unit_label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    return row;
}

/**
 * @brief  统一图片图标的位置及重着色。
 * @param  image 图片对象。
 * @param  x     横坐标。
 * @param  y     纵坐标。
 * @param  color 图标颜色。
 * @return 无。
 */
static void ui_style_icon(lv_obj_t *image, lv_coord_t x, lv_coord_t y, lv_color_t color)
{
    lv_obj_remove_style_all(image);
    lv_obj_set_pos(image, x, y);
    lv_obj_set_size(image, 40, 40);
    lv_obj_set_style_img_recolor(image, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(image, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_opa(image, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_move_foreground(image);
}

/**
 * @brief  统一开关的尺寸、颜色和圆角。
 * @param  sw 开关对象。
 * @param  x  横坐标。
 * @param  y  纵坐标。
 * @return 无。
 */
static void ui_style_switch(lv_obj_t *sw, lv_coord_t x, lv_coord_t y)
{
    lv_obj_remove_style_all(sw);
    lv_obj_set_pos(sw, x, y);
    lv_obj_set_size(sw, 54, 24);
    lv_obj_add_style(sw, &s_switch_track_style, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(sw, &s_switch_checked_style, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(sw, &s_switch_knob_style, LV_PART_KNOB | LV_STATE_DEFAULT);
}

/**
 * @brief  美化主监测页并保留全部原控件句柄。
 * @param  ui GUI Guider UI 对象集合。
 * @return 无。
 * @note   所有传感器值、开关和跳转控件继续使用原名称，便于最终移植到 APP。
 */
static void ui_style_main_screen(lv_ui *ui)
{
    lv_obj_t *root;
    lv_obj_t *footer_gear;
    lv_obj_t *section_accent;
    lv_obj_t *section_title;
    lv_obj_t *fan_slider_knob;
    lv_obj_t *fan_low_label;
    lv_obj_t *fan_high_label;

    if (ui == NULL || ui->screen == NULL || s_styled_main_screen == ui->screen) {
        return;
    }
    s_styled_main_screen = ui->screen;
    root = ui->screen_cont_1;

    lv_obj_remove_style_all(ui->screen);
    lv_obj_remove_style_all(root);

    /*
     * @brief 恢复主屏幕和根容器的几何尺寸。
     * @note  LVGL 的对象宽高同样保存在本地样式中，remove_style_all() 会一并清除；
     *        若不重新设置，根容器会退回默认尺寸并裁剪右侧和下方的全部子控件。
     */
    lv_obj_set_pos(ui->screen, 0, 0);
    lv_obj_set_size(ui->screen, 800, 480);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_size(root, 800, 480);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(ui->screen, UI_COLOR_BG, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(root, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(root, UI_COLOR_BG, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(root, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 卡片布局：头部、四个传感器卡片、控制区和底栏。 */
    ui_create_card(root, 12, 8, 776, 54);
    ui_create_card(root, 12, 72, 382, 112);
    ui_create_card(root, 406, 72, 382, 112);
    ui_create_card(root, 12, 194, 382, 112);
    ui_create_card(root, 406, 194, 382, 112);
    ui_create_card(root, 12, 316, 776, 118);
    ui_create_card(root, 12, 442, 776, 34);

    /* 头部。 */
    lv_obj_remove_style_all(ui->screen_cont_2);
    lv_obj_set_pos(ui->screen_cont_2, 25, 15);
    lv_obj_set_size(ui->screen_cont_2, 245, 38);
    lv_obj_set_style_bg_opa(ui->screen_cont_2, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->screen_cont_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->screen_cont_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_cont_2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style_all(ui->screen_title);
    lv_label_set_text(ui->screen_title, "智能环境监测");
    lv_obj_set_pos(ui->screen_title, 0, 3);
    lv_obj_set_size(ui->screen_title, 220, 31);
    lv_obj_set_style_text_color(ui->screen_title, UI_COLOR_TEXT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_title, &lv_font_sentinel_title_24,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_title, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_create_divider(root, 180, 20, 29);
    ui_create_hint(root, "环境哨兵 · 远程监测系统", 194, 25, 255, UI_COLOR_MUTED);

    lv_obj_set_pos(ui->screen_WiFi_LOG, 555, 22);
    lv_obj_set_size(ui->screen_WiFi_LOG, 25, 25);
    lv_obj_set_style_img_recolor(ui->screen_WiFi_LOG, UI_COLOR_PRIMARY,
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui->screen_WiFi_LOG, LV_OPA_COVER,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_move_foreground(ui->screen_WiFi_LOG);
    lv_label_set_text(ui->screen_label_IOT, "OneNET平台");
    lv_obj_remove_style_all(ui->screen_label_IOT);
    lv_obj_set_pos(ui->screen_label_IOT, 588, 25);
    lv_obj_set_size(ui->screen_label_IOT, 108, 22);
    lv_obj_add_style(ui->screen_label_IOT, &s_small_text_style,
                     LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_label_IOT, UI_COLOR_TEXT,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    ui_create_icon_disc(root, 699, 31, 8, UI_COLOR_GREEN);
    /* 尚未收到 ESP32 心跳前按离线显示，后续由 LVGL 任务切换 ON/NC。 */
    lv_label_set_text(ui->screen_iot_flag, "NC");
    lv_obj_remove_style_all(ui->screen_iot_flag);
    lv_obj_set_pos(ui->screen_iot_flag, 713, 25);
    lv_obj_set_size(ui->screen_iot_flag, 52, 22);
    lv_obj_add_style(ui->screen_iot_flag, &s_small_text_style,
                     LV_PART_MAIN | LV_STATE_DEFAULT);
    /* 状态只显示 ASCII；使用完整拉丁字体，避免精简中文字体缺少 C 而显示方框。 */
    lv_obj_set_style_text_font(ui->screen_iot_flag, &lv_font_montserrat_16,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_iot_flag, UI_COLOR_RED,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 四个传感器卡片。 */
    ui_create_icon_disc(root, 28, 88, 68, lv_color_hex(0xEAF3FF));
    ui_create_icon_disc(root, 422, 88, 68, lv_color_hex(0xEAF3FF));
    ui_create_icon_disc(root, 28, 210, 68, lv_color_hex(0xFFF3E7));
    ui_create_icon_disc(root, 422, 210, 68, lv_color_hex(0xF3ECFF));

    ui_style_icon(ui->screen_temperature_LOG, 42, 102, UI_COLOR_PRIMARY);
    ui_style_icon(ui->screen_humidity_LOG, 436, 102, UI_COLOR_PRIMARY);
    ui_style_icon(ui->screen_sunlight_LOG, 42, 224, UI_COLOR_ORANGE);
    ui_style_icon(ui->screen_smoke_LOG, 436, 224, UI_COLOR_PURPLE);

    lv_label_set_text(ui->screen_label_temperature, "温度");
    lv_label_set_text(ui->screen_label_humidity, "湿度");
    lv_label_set_text(ui->screen_label_sunlight, "光照");
    lv_label_set_text(ui->screen_label_smoke, "烟雾指数");
    ui_style_name_label(ui->screen_label_temperature, 116, 88, 120);
    ui_style_name_label(ui->screen_label_humidity, 510, 88, 120);
    ui_style_name_label(ui->screen_label_sunlight, 116, 210, 135);
    ui_style_name_label(ui->screen_label_smoke, 510, 210, 135);

    /* 温湿度和光照尚未取得有效样本前统一显示 NC。 */
    lv_label_set_text(ui->screen_temperature_num, "NC");
    lv_label_set_text(ui->screen_humidity_num, "NC");
    lv_label_set_text(ui->screen_sunlight_num, "NC");
    if (lv_label_get_text(ui->screen_smoke_num)[0] == '\0') {
        lv_label_set_text(ui->screen_smoke_num, "12");
    }
    ui_create_value_row(root, ui->screen_temperature_num, "°C",
                        116, 113, 210, UI_COLOR_TEXT);
    ui_create_value_row(root, ui->screen_humidity_num, "%",
                        510, 113, 190, UI_COLOR_PRIMARY);
    ui_create_value_row(root, ui->screen_sunlight_num, "lx",
                        116, 235, 210, UI_COLOR_ORANGE);
    ui_create_value_row(root, ui->screen_smoke_num, "%",
                        510, 235, 190, UI_COLOR_PURPLE);

    ui_create_hint(root, "舒适", 116, 158, 70, UI_COLOR_MUTED);
    ui_create_hint(root, "舒适", 510, 158, 70, UI_COLOR_MUTED);
    ui_create_hint(root, "适中", 116, 280, 70, UI_COLOR_MUTED);
    ui_create_hint(root, "安全", 510, 280, 70, UI_COLOR_MUTED);

    /* 控制区。 */
    section_accent = lv_obj_create(root);
    lv_obj_remove_style_all(section_accent);
    lv_obj_set_pos(section_accent, 30, 322);
    lv_obj_set_size(section_accent, 4, 18);
    lv_obj_set_style_bg_color(section_accent, UI_COLOR_PRIMARY,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(section_accent, LV_OPA_COVER,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(section_accent, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    section_title = ui_create_hint(root, "设备控制", 42, 320, 90, UI_COLOR_TEXT);
    lv_obj_set_style_text_font(section_title, &lv_font_sentinel_cn_16,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_size(section_title, 90, 20);

    ui_create_divider(root, 207, 344, 82);
    ui_create_divider(root, 401, 344, 82);
    ui_create_divider(root, 595, 344, 82);
    ui_create_icon_disc(root, 34, 344, 46, lv_color_hex(0xEAF3FF));
    ui_create_icon_disc(root, 220, 348, 54, lv_color_hex(0xEAF3FF));
    ui_create_icon_disc(root, 414, 348, 54, lv_color_hex(0xEAF3FF));
    ui_create_icon_disc(root, 608, 348, 54, lv_color_hex(0xFDECEE));

    /* 风扇图标缩小到约 30 px，与名称和系数统一布置在进度条上方。 */
    ui_style_icon(ui->screen_fan_LOG, 37, 347, UI_COLOR_PRIMARY);
    lv_obj_clear_flag(ui->screen_fan_LOG, LV_OBJ_FLAG_HIDDEN);
    lv_img_set_pivot(ui->screen_fan_LOG, 20, 20);
    lv_img_set_zoom(ui->screen_fan_LOG, 192);
    ui_style_icon(ui->screen_buzzer_LOG, 227, 355, UI_COLOR_PRIMARY);
    ui_style_icon(ui->screen_electric_light_LOG, 421, 355, UI_COLOR_PRIMARY);
    ui_style_icon(ui->screen_Alarm_light_LOG, 615, 355, UI_COLOR_RED);

    lv_label_set_text(ui->screen_label_fan, "风扇");
    lv_label_set_text(ui->screen_label_buzzer, "蜂鸣器");
    lv_label_set_text(ui->screen_label_light, "照明灯");
    lv_label_set_text(ui->screen_Alarm_light, "报警灯");
    ui_style_name_label(ui->screen_label_fan, 88, 347, 90);
    lv_obj_set_style_text_font(ui->screen_label_fan, &lv_font_sentinel_cn_16,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    ui_style_name_label(ui->screen_label_buzzer, 281, 345, 100);
    ui_style_name_label(ui->screen_label_light, 475, 345, 60);
    ui_style_name_label(ui->screen_Alarm_light, 669, 345, 100);

    if (lv_label_get_text(ui->screen_fan_num)[0] == '\0') {
        lv_label_set_text(ui->screen_fan_num, "50%");
    }
    lv_obj_remove_style_all(ui->screen_fan_num);
    lv_obj_set_pos(ui->screen_fan_num, 88, 370);
    lv_obj_set_size(ui->screen_fan_num, 60, 20);
    lv_obj_set_style_bg_opa(ui->screen_fan_num, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_fan_num, UI_COLOR_PRIMARY, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_fan_num, LV_FONT_DEFAULT,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_style_all(ui->screen_bar_fan);
    lv_obj_set_pos(ui->screen_bar_fan, 34, 398);
    lv_obj_set_size(ui->screen_bar_fan, 167, 7);
    lv_bar_set_range(ui->screen_bar_fan, 0, 1000);
    lv_bar_set_value(ui->screen_bar_fan, 500, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui->screen_bar_fan, UI_COLOR_TRACK, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_bar_fan, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_bar_fan, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->screen_bar_fan, UI_COLOR_PRIMARY,
                              LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->screen_bar_fan, LV_OPA_COVER,
                            LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->screen_bar_fan, 8, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    /* 50% 位置的白色滑块，使风扇控制更接近真实滑杆。 */
    fan_slider_knob = lv_obj_create(root);
    lv_obj_remove_style_all(fan_slider_knob);
    lv_obj_set_pos(fan_slider_knob, 110, 394);
    lv_obj_set_size(fan_slider_knob, 15, 15);
    lv_obj_set_style_bg_color(fan_slider_knob, UI_COLOR_CARD,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(fan_slider_knob, LV_OPA_COVER,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(fan_slider_knob, LV_RADIUS_CIRCLE,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(fan_slider_knob, lv_color_hex(0xCCD5E2),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(fan_slider_knob, 1,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_color(fan_slider_knob, lv_color_hex(0x8FA1B8),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_opa(fan_slider_knob, LV_OPA_20,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(fan_slider_knob, 4,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_move_foreground(fan_slider_knob);

    fan_low_label = ui_create_hint(root, "低", 34, 409, 24, UI_COLOR_MUTED);
    fan_high_label = ui_create_hint(root, "高", 180, 409, 24, UI_COLOR_MUTED);
    lv_obj_set_height(fan_low_label, 16);
    lv_obj_set_height(fan_high_label, 16);

    ui_style_switch(ui->screen_sw_buzzer, 283, 383);
    ui_style_switch(ui->screen_sw_light, 477, 383);
    ui_style_switch(ui->screen_sw_Alarm_light, 671, 383);
    s_buzzer_status_label = ui_create_hint(root, "已关闭", 286, 410, 60,
                                           UI_COLOR_MUTED);
    s_illumination_status_label = ui_create_hint(root, "控制关闭", 477, 410, 70,
                                                 UI_COLOR_MUTED);
    s_light_output_status_label = ui_create_hint(root, "已关闭", 535, 347, 60,
                                                 UI_COLOR_MUTED);
    s_alarm_light_status_label = ui_create_hint(root, "已关闭", 674, 410, 60,
                                                UI_COLOR_MUTED);
    custom_update_control_status(
        lv_obj_has_state(ui->screen_sw_buzzer, LV_STATE_CHECKED),
        lv_obj_has_state(ui->screen_sw_light, LV_STATE_CHECKED),
        lv_obj_has_state(ui->screen_sw_Alarm_light, LV_STATE_CHECKED));
    custom_update_light_output_status(false);

    /* 底部设置入口和更新时间。 */
    /* 原设置图片保留事件回调，只作为透明点击热区。 */
    lv_obj_remove_style_all(ui->screen_setting_LOG);
    lv_obj_set_pos(ui->screen_setting_LOG, 18, 442);
    lv_obj_set_size(ui->screen_setting_LOG, 145, 34);
    lv_obj_set_style_img_opa(ui->screen_setting_LOG, LV_OPA_TRANSP,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_move_foreground(ui->screen_setting_LOG);
    footer_gear = lv_label_create(root);
    lv_obj_remove_style_all(footer_gear);
    lv_label_set_text(footer_gear, LV_SYMBOL_SETTINGS);
    lv_obj_set_pos(footer_gear, 25, 451);
    lv_obj_set_size(footer_gear, 20, 18);
    lv_obj_set_style_text_color(footer_gear, UI_COLOR_DARK_MUTED,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(footer_gear, LV_FONT_DEFAULT,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ui->screen_label_setting, "用户设置");
    lv_obj_remove_style_all(ui->screen_label_setting);
    lv_obj_set_pos(ui->screen_label_setting, 52, 449);
    lv_obj_set_size(ui->screen_label_setting, 90, 22);
    lv_obj_set_style_text_font(ui->screen_label_setting, &lv_font_sentinel_ui_18,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->screen_label_setting, UI_COLOR_DARK_MUTED,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 原天气大图不再参与主界面布局，节省重绘面积。 */
    lv_obj_remove_style_all(ui->screen_weather_LOG);
    lv_obj_add_flag(ui->screen_weather_LOG, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief  自定义主题初始化入口。
 * @param  ui GUI Guider UI 对象集合。
 * @return 无。
 * @note   该文件不会被 GUI Guider 重新生成，适合作为最终 APP 移植的主题层。
 */
void custom_init(lv_ui *ui)
{
    if (ui == NULL) {
        return;
    }

    ui_theme_styles_init();
    ui_style_main_screen(ui);
    lv_obj_add_event_cb(ui->screen_setting_LOG, ui_setting_page_event_handler,
                        LV_EVENT_CLICKED, ui);
}

/**
 * @brief  应用设置页面最终字体和纯色主题。
 * @param  ui GUI Guider UI 对象集合。
 * @return 无。
 * @note   在生成页面之后覆盖样式，避免 GUI Guider 重新生成代码时丢失修改。
 */
static void ui_style_setting_page(lv_ui *ui)
{
    if (ui == NULL || ui->screen_setting == NULL) {
        return;
    }

    lv_obj_set_style_text_font(ui->screen_setting_label__threshold_temp,
                               &lv_font_sentinel_ui_18,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_label__threshold_hum,
                               &lv_font_sentinel_ui_18,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_label__threshold_sunlight,
                               &lv_font_sentinel_ui_18,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_label__threshold_smoke,
                               &lv_font_sentinel_ui_18,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_font(ui->screen_setting_num_temp,
                               &lv_font_sentinel_ui_18,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_num_hum,
                               &lv_font_sentinel_ui_18,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_num_sunlight,
                               &lv_font_sentinel_ui_18,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_num_smoke,
                               &lv_font_sentinel_ui_18,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_font(ui->screen_setting_btn_save_label,
                               &lv_font_sentinel_ui_18,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_btn_reset_label,
                               &lv_font_sentinel_ui_18,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_btn_Version_label,
                               &lv_font_sentinel_ui_18,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->screen_setting_btn_default_label,
                               &lv_font_sentinel_ui_18,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui->screen_setting_label_setting_title, "设置");
    lv_obj_set_style_text_font(ui->screen_setting_label_setting_title,
                               &lv_font_sentinel_title_24,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->screen_setting_label_setting_title,
                                LV_TEXT_ALIGN_CENTER,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_size(ui->screen_setting_label_setting_title,
                    LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(ui->screen_setting_label_setting_title, LV_ALIGN_CENTER, 0, 0);

    ui_style_setting_solid_blue(ui->screen_setting_label__threshold_temp);
    ui_style_setting_solid_blue(ui->screen_setting_label__threshold_hum);
    ui_style_setting_solid_blue(ui->screen_setting_label__threshold_sunlight);
    ui_style_setting_solid_blue(ui->screen_setting_label__threshold_smoke);
    ui_style_setting_solid_blue(ui->screen_setting_btn_save);
    ui_style_setting_solid_blue(ui->screen_setting_btn_reset);
    ui_style_setting_solid_blue(ui->screen_setting_btn_Version);
    ui_style_setting_solid_blue(ui->screen_setting_btn_default);
}
