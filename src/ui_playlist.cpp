#include <Arduino.h>
#include <lvgl.h>

#include "ui_playlist.h"
#include "ui_nav.h"
#include "link.h"

LV_FONT_DECLARE(opensans_16);

#define PLAYLIST_MAX_ITEMS 100

static lv_obj_t *pl_list;
static lv_obj_t *pl_spinner;
static lv_obj_t *pl_title;
static char      pl_id[64];
static int       pl_count = 0;

// Shows or hides the playlist loading indicator
static void set_pl_loading(bool on)
{
    if (!pl_spinner) return;
    if (on) lv_obj_clear_flag(pl_spinner, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(pl_spinner, LV_OBJ_FLAG_HIDDEN);
}

// Returns to the home screen
static void pl_back_cb(lv_event_t *e)
{
    lv_scr_load(g_scr_home);
}

// Plays the open playlist shuffled, then shows now playing
static void pl_shuffle_cb(lv_event_t *e)
{
    link_send_play(pl_id, "s");
    lv_scr_load(g_scr_np);
}

// Plays the open playlist in order, then shows now playing
static void pl_play_cb(lv_event_t *e)
{
    link_send_play(pl_id, "p");
    lv_scr_load(g_scr_np);
}

// Creates a small round header button with a symbol
static void pl_header_btn(lv_obj_t *parent, const char *symbol, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, 38, 38);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x272727), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x383838), LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, symbol);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);
}

// Builds the playlist screen, returns the screen object
lv_obj_t *playlist_screen_create()
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

    pl_header_btn(header, LV_SYMBOL_LEFT, pl_back_cb);

    pl_title = lv_label_create(header);
    lv_obj_set_style_text_font(pl_title, &opensans_16, 0);
    lv_obj_set_style_text_color(pl_title, lv_color_white(), 0);
    lv_label_set_long_mode(pl_title, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(pl_title, 1);
    lv_label_set_text(pl_title, "");

    pl_header_btn(header, LV_SYMBOL_SHUFFLE, pl_shuffle_cb);
    pl_header_btn(header, LV_SYMBOL_PLAY, pl_play_cb);

    pl_list = lv_obj_create(scr);
    lv_obj_remove_style_all(pl_list);
    lv_obj_set_width(pl_list, lv_pct(100));
    lv_obj_set_flex_grow(pl_list, 1);
    lv_obj_set_flex_flow(pl_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(pl_list, 6, 0);
    lv_obj_set_scroll_dir(pl_list, LV_DIR_VER);
    lv_obj_clear_flag(pl_list, LV_OBJ_FLAG_SCROLL_ELASTIC);

    pl_spinner = lv_spinner_create(scr, 1000, 60);
    lv_obj_add_flag(pl_spinner, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(pl_spinner, 44, 44);
    lv_obj_align(pl_spinner, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_arc_color(pl_spinner, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_color(pl_spinner, lv_color_hex(0xFF0000), LV_PART_INDICATOR);
    return scr;
}

// Stores the playlist id and shows the loading state before the request
void playlist_open(const char *id)
{
    strncpy(pl_id, id, sizeof(pl_id));
    pl_id[sizeof(pl_id) - 1] = '\0';
    if (pl_list) lv_obj_clean(pl_list);
    pl_count = 0;
    set_pl_loading(true);
}

// Sets the playlist title and clears the list
void playlist_begin(const char *title)
{
    if (pl_title) lv_label_set_text(pl_title, title);
    if (pl_list) lv_obj_clean(pl_list);
    pl_count = 0;
    set_pl_loading(true);
}

// Adds one track row to the playlist list
void playlist_item(const char *title, const char *sub)
{
    if (!pl_list || pl_count >= PLAYLIST_MAX_ITEMS) return;
    pl_count++;

    lv_obj_t *row = lv_obj_create(pl_list);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(row, 6, 0);
    lv_obj_set_style_pad_row(row, 2, 0);

    lv_obj_t *t = lv_label_create(row);
    lv_obj_set_style_text_font(t, &opensans_16, 0);
    lv_obj_set_style_text_color(t, lv_color_white(), 0);
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

// Hides the spinner once the whole playlist has arrived
void playlist_end()
{
    set_pl_loading(false);
}