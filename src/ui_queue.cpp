#include <Arduino.h>
#include <lvgl.h>

#include "ui_queue.h"
#include "ui_nav.h"
#include "link.h"
#include "strings.h"

LV_FONT_DECLARE(opensans_16);

#define QUEUE_MAX_ITEMS 50

static lv_obj_t *queue_list;        // Scrollable column
static lv_obj_t *queue_spinner;     // Loading indicator
static int       queue_count = 0;

// Shows or hides queue loading indicator
static void set_queue_loading(bool on)
{
    if (!queue_spinner) return;
    if (on) lv_obj_clear_flag(queue_spinner, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(queue_spinner, LV_OBJ_FLAG_HIDDEN);
}

// Jumps to the tapped queue item, then shows the now playing screen
static void queue_row_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    link_send_qjump(idx);
    lv_scr_load(g_scr_np);      // Maybe setting?
}

// Returns to the now playing screen
static void back_cb(lv_event_t *e)
{
    lv_scr_load(g_scr_np);
}

// Builds the queue screen, returns the screen object
lv_obj_t *queue_screen_create()
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
    lv_label_set_text(head, L->queue_title);

    queue_list = lv_obj_create(scr);
    lv_obj_remove_style_all(queue_list);
    lv_obj_set_width(queue_list, lv_pct(100));
    lv_obj_set_flex_grow(queue_list, 1);
    lv_obj_set_flex_flow(queue_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(queue_list, 6, 0);
    lv_obj_set_scroll_dir(queue_list, LV_DIR_VER);
    lv_obj_clear_flag(queue_list, LV_OBJ_FLAG_SCROLL_ELASTIC);

    queue_spinner = lv_spinner_create(scr, 1000, 60);
    lv_obj_add_flag(queue_spinner, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(queue_spinner, 44, 44);
    lv_obj_align(queue_spinner, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_arc_color(queue_spinner, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_color(queue_spinner, lv_color_hex(0xFF0000), LV_PART_INDICATOR);
    return scr;
}

// Clears the queue list and shows the spinner
void queue_begin()
{
    if (!queue_list) return;
    lv_obj_clean(queue_list);
    queue_count = 0;
    set_queue_loading(true);
}

// Adds one row to the queue list, highlights the currently playing one
void queue_item(const char *title, const char *sub, bool cur)
{
    if (!queue_list || queue_count >= QUEUE_MAX_ITEMS) return;
    int idx = queue_count++;

    lv_obj_t *row = lv_obj_create(queue_list);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(row, 6, 0);
    lv_obj_set_style_pad_row(row, 2, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x282828), LV_STATE_PRESSED);
    lv_obj_add_event_cb(row, queue_row_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);

    lv_obj_t *t = lv_label_create(row);
    lv_obj_set_style_text_font(t, &opensans_16, 0);
    lv_obj_set_style_text_color(t, cur ? lv_color_hex(0xFF0000) : lv_color_white(), 0);
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
    lv_obj_set_width(t, lv_pct(100));
    lv_label_set_text(t, title);

    lv_obj_t *s = lv_label_create(row);
    lv_obj_set_style_text_font(s, &opensans_16, 0);
    lv_obj_set_style_text_color(s, lv_color_hex(0x999999), 0);
    lv_label_set_long_mode(s, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s, lv_pct(100));
    lv_label_set_text(s, sub);
}

// Hides the spinner once queue is loaded
void queue_end()
{
    set_queue_loading(false);
}