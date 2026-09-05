#include <Arduino.h>
#include <lvgl.h>
#include <ArduinoJson.h>

#include "ui_now_playing.h"
#include "link.h"

LV_FONT_DECLARE(opensans_16);

#define ART_MAX 220

static lv_obj_t *ui_cover;
static lv_obj_t *ui_title;
static lv_obj_t *ui_artist;
static lv_obj_t *ui_bar;
static lv_obj_t *ui_time;
static lv_obj_t *ui_pp_icon;
static lv_obj_t *ui_vol;

static uint8_t     *art_buf = nullptr;
static lv_img_dsc_t art_dsc;

static int      np_pos = 0;
static int      np_dur = 0;
static bool     np_playing = false;
static uint32_t np_sync_ms = 0;
static int      np_last_reported = -1;
static char     np_prev_title[128] = "- - -";

static void fmt_time(char *out, int total)
{
    sprintf(out, "%d:%02d", total / 60, total % 60);
}

static void btn_cmd_cb(lv_event_t *e)
{
    link_send_cmd((const char *)lv_event_get_user_data(e));
}

static lv_obj_t *make_ctrl_btn(lv_obj_t *parent, const char *symbol, const char *action, lv_coord_t size)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, size, size);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x272727), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x404040), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(btn, lv_color_white(), 0);
    lv_obj_add_event_cb(btn, btn_cmd_cb, LV_EVENT_CLICKED, (void *)action);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, symbol);
    lv_obj_center(lbl);
    return lbl;
}

void np_screen_create()
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0F0F0F), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(scr, 14, 0);

    lv_coord_t screen_h  = lv_disp_get_ver_res(NULL);
    lv_coord_t btn_sz    = screen_h / 6;
    lv_coord_t cover_sz  = screen_h - 72;

    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(scr, 16, 0);

    art_buf = (uint8_t *)ps_malloc((size_t)ART_MAX * ART_MAX * 2);
    art_dsc.header.always_zero = 0;
    art_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    art_dsc.header.w = 0;
    art_dsc.header.h = 0;
    art_dsc.data = art_buf;
    art_dsc.data_size = 0;

    ui_cover = lv_img_create(scr);
    lv_obj_set_size(ui_cover, cover_sz, cover_sz);
    lv_obj_set_style_bg_color(ui_cover, lv_color_hex(0x222222), 0);
    lv_obj_set_style_bg_opa(ui_cover, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ui_cover, 8, 0);
    lv_obj_set_style_clip_corner(ui_cover, true, 0);

    lv_obj_t *col = lv_obj_create(scr);
    lv_obj_remove_style_all(col);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(col, lv_pct(100));
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 8, 0);

    ui_title = lv_label_create(col);
    lv_obj_set_style_text_font(ui_title, &opensans_16, 0);
    lv_obj_set_style_text_color(ui_title, lv_color_white(), 0);
    lv_label_set_long_mode(ui_title, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(ui_title, lv_pct(100));
    lv_obj_set_style_text_align(ui_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(ui_title, "- - -");

    ui_artist = lv_label_create(col);
    lv_obj_set_style_text_font(ui_artist, &opensans_16, 0);
    lv_obj_set_style_text_color(ui_artist, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(ui_artist, "-");

    lv_obj_t *row = lv_obj_create(col);
    lv_obj_remove_style_all(row);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    make_ctrl_btn(row, LV_SYMBOL_PREV, "prev", btn_sz);
    ui_pp_icon = make_ctrl_btn(row, LV_SYMBOL_PLAY, "playpause", btn_sz);
    make_ctrl_btn(row, LV_SYMBOL_NEXT, "next", btn_sz);

    ui_bar = lv_bar_create(col);
    lv_obj_set_size(ui_bar, lv_pct(95), 6);
    lv_bar_set_range(ui_bar, 0, 100);
    lv_bar_set_value(ui_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui_bar, lv_color_hex(0x383838), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_bar, lv_color_hex(0xFF0000), LV_PART_INDICATOR);

    ui_time = lv_label_create(col);
    lv_obj_set_style_text_font(ui_time, &opensans_16, 0);
    lv_obj_set_style_text_color(ui_time, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(ui_time, "0:00 / 0:00");

    lv_obj_t *row2 = lv_obj_create(col);
    lv_obj_remove_style_all(row2);
    lv_obj_clear_flag(row2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(row2, lv_pct(100));
    lv_obj_set_height(row2, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    make_ctrl_btn(row2, LV_SYMBOL_VOLUME_MID, "vol_down", btn_sz);

    ui_vol = lv_label_create(row2);
    lv_obj_set_style_text_font(ui_vol, &opensans_16, 0);
    lv_obj_set_style_text_color(ui_vol, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(ui_vol, "50%");

    make_ctrl_btn(row2, LV_SYMBOL_VOLUME_MAX, "vol_up", btn_sz);
}

uint8_t *np_art_begin(int w, int h)
{
    if (w <= 0 || h <= 0 || w > ART_MAX || h > ART_MAX) return nullptr;
    art_dsc.header.w = w;
    art_dsc.header.h = h;
    art_dsc.data_size = (uint32_t)w * h * 2;
    return art_buf;
}

void np_art_end()
{
    lv_obj_set_size(ui_cover, art_dsc.header.w, art_dsc.header.h);
    lv_img_set_src(ui_cover, &art_dsc);
}

void np_apply(JsonDocument &doc)
{
    const char *status = doc["status"] | "none";
    if (strcmp(status, "none") == 0)
    {
        lv_label_set_text(ui_title, "Nothing playing");
        lv_label_set_text(ui_artist, "");
        lv_label_set_text(ui_pp_icon, LV_SYMBOL_PLAY);
        np_playing = false;
        return;
    }

    int reported = doc["pos"] | 0;
    const char *title = doc["title"] | "- - -";
    if (strcmp(title, np_prev_title) != 0)
    {
        strncpy(np_prev_title, title, sizeof(np_prev_title));
        np_prev_title[sizeof(np_prev_title) - 1] = '\0';
        lv_label_set_text(ui_title, title);
        np_pos = 0;
        np_sync_ms = millis();
        np_last_reported = reported;
    }
    lv_label_set_text(ui_artist, doc["artist"] | "-");
    np_dur     = doc["dur"] | 0;
    np_playing = (strcmp(status, "playing") == 0);
    lv_label_set_text(ui_pp_icon, np_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);

    if (reported != np_last_reported)
    {
        np_pos = reported;
        np_sync_ms = millis();
        np_last_reported = reported;
    }

    int vol = doc["vol"] | 50;
    if (vol >= 0)
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", vol);
        lv_label_set_text(ui_vol, buf);
    }
}

void np_tick()
{
    int pos = np_pos;
    if (np_playing) pos += (millis() - np_sync_ms) / 1000;
    if (pos > np_dur) pos = np_dur;

    if (np_dur > 0) lv_bar_set_value(ui_bar, (pos * 100) / np_dur, LV_ANIM_OFF);

    char a[8], b[8], out[20];
    fmt_time(a, pos);
    fmt_time(b, np_dur);
    sprintf(out, "%s / %s", a, b);
    lv_label_set_text(ui_time, out);
}
