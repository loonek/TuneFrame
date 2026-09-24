#include <Arduino.h>
#include <lvgl.h>

#include "ui_settings.h"
#include "ui_nav.h"
#include "i18n.h"
#include "settings_store.h"

LV_FONT_DECLARE(opensans_16);

// Returns to the home screen
static void back_cb(lv_event_t *e)
{
    lv_scr_load(g_scr_home);
}

// Applies and persists the language picked in the dropdown
static void lang_cb(lv_event_t *e)
{
    lv_obj_t *dd = lv_event_get_target(e);
    int index = lv_dropdown_get_selected(dd);
    set_language(index);
    settings_save_language(index);
}

// Creates a settings row: description on the left, returns the row so a control can be added on the right
static lv_obj_t *settings_row(lv_obj_t *list, const char *label)
{
    lv_obj_t *row = lv_obj_create(list);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(row, 8, 0);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_style_radius(row, 8, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x1A1A1A), 0);

    lv_obj_t *lbl = lv_label_create(row);
    lv_obj_set_style_text_font(lbl, &opensans_16, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_flex_grow(lbl, 1);   // grows to push the control to the right edge
    lv_label_set_text(lbl, label);
    return row;
}

// Adds the language dropdown to a settings row
static void add_language_dropdown(lv_obj_t *row)
{
    lv_obj_t *dd = lv_dropdown_create(row);
    lv_dropdown_set_options(dd, LANG_NAMES);
    lv_dropdown_set_selected(dd, current_language());
    lv_dropdown_set_symbol(dd, NULL);   // our font lacks the arrow glyph; drop it
    lv_obj_set_width(dd, 150);
    lv_obj_add_event_cb(dd, lang_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_style_text_font(dd, &opensans_16, 0);
    lv_obj_set_style_bg_color(dd, lv_color_hex(0x272727), 0);
    lv_obj_set_style_text_color(dd, lv_color_white(), 0);
    lv_obj_set_style_border_width(dd, 0, 0);
    lv_obj_set_style_radius(dd, 6, 0);

    lv_obj_t *ddlist = lv_dropdown_get_list(dd);
    if (ddlist)
    {
        lv_obj_set_style_text_font(ddlist, &opensans_16, 0);
        lv_obj_set_style_bg_color(ddlist, lv_color_hex(0x272727), 0);
        lv_obj_set_style_text_color(ddlist, lv_color_white(), 0);
        lv_obj_set_style_border_width(ddlist, 0, 0);
        lv_obj_set_style_bg_color(ddlist, lv_color_hex(0xFF0000), LV_PART_SELECTED | LV_STATE_CHECKED);
    }
}

// Saves the orientation toggled in the switch (applied on next boot)
static void orient_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    settings_save_orientation(lv_obj_has_state(sw, LV_STATE_CHECKED) ? 1 : 0);
}

// Adds the orientation switch (on = portrait) to a settings row
static void add_orientation_switch(lv_obj_t *row)
{
    lv_obj_t *sw = lv_switch_create(row);
    if (settings_load_orientation(0)) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, orient_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x444444), 0);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0xFF0000), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, lv_color_white(), LV_PART_KNOB);
}

// Builds the settings screen, returns the screen object
lv_obj_t *settings_screen_create()
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0F0F0F), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(scr, 12, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(scr, 8, 0);

    lv_obj_t *header = lv_obj_create(scr);
    lv_obj_remove_style_all(header);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(header, 8, 0);

    lv_obj_t *back = lv_btn_create(header);
    lv_obj_remove_style_all(back);
    lv_obj_set_size(back, 38, 38);
    lv_obj_set_style_radius(back, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x333333), LV_STATE_PRESSED);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *bicon = lv_label_create(back);
    lv_label_set_text(bicon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(bicon, lv_color_white(), 0);
    lv_obj_center(bicon);

    lv_obj_t *head = lv_label_create(header);
    lv_obj_set_style_text_font(head, &opensans_16, 0);
    lv_obj_set_style_text_color(head, lv_color_white(), 0);
    lv_label_set_text(head, L->settings_title);

    lv_obj_t *list = lv_obj_create(scr);
    lv_obj_remove_style_all(list);
    lv_obj_set_width(list, lv_pct(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, 8, 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLL_ELASTIC);

    lv_obj_t *lang_row = settings_row(list, L->language_label);
    add_language_dropdown(lang_row);

    lv_obj_t *orient_row = settings_row(list, L->orientation_label);
    add_orientation_switch(orient_row);

    return scr;
}
