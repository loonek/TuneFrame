#include <Arduino.h>
#include <lvgl.h>

#include "ui_home.h"
#include "ui_nav.h"
#include "link.h"

LV_FONT_DECLARE(opensans_16);
LV_FONT_DECLARE(icons);

#define FEED_MAX_ITEMS 40

static lv_obj_t *mini_title;    // Label for the title of the currently playing song
static lv_obj_t *mini_artist;   // Label for the artist
static lv_obj_t *mini_pp;       // Label for the play/pause button

static lv_obj_t *home_feed;     // Container for the feed items
static lv_obj_t *feed_row;      // Horizontal card row of the current section

static char feed_ids[FEED_MAX_ITEMS][24];  // Array to store feed item IDs
static char feed_kinds[FEED_MAX_ITEMS][2];
static int  feed_count = 0;
static bool feed_ready = false;

// Sends search command to the host when the search button is clicked
static void search_cb(lv_event_t *e)
{
    link_send_cmd("search");
}

// Opens the "Now Playing" screen when the mini player is clicked
static void open_np_cb(lv_event_t *e)
{
    lv_scr_load(g_scr_np);
}

// Sends a command to the host when mini-player buttons are clicked
static void mini_cmd_cb(lv_event_t *e)
{
    link_send_cmd((const char *)lv_event_get_user_data(e));
}

// Sends a command to the host to play a specific item from the feed
static void card_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < feed_count) link_send_play(feed_ids[idx], feed_kinds[idx]);
}

// Creates a mini button with a symbol and associates it with an action
static lv_obj_t *make_mini_btn(lv_obj_t *parent, const char *symbol, const char *action)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, 38, 38);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x333333), LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, mini_cmd_cb, LV_EVENT_CLICKED, (void *)action);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, symbol);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);
    return lbl;
}

// Creates the search button in the top-right corner of the home screen
static void make_search(lv_obj_t *scr)
{
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_remove_style_all(btn);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(btn, 40, 40);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x272727), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x383838), LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, search_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *icon = lv_label_create(btn);
    lv_obj_set_style_text_font(icon, &icons, 0);
    lv_label_set_text(icon, "\xEF\x80\x82");
    lv_obj_set_style_text_color(icon, lv_color_hex(0xCCCCCC), 0);
    lv_obj_center(icon);
}

// Creates the mini player at the bottom of the home screen
static lv_obj_t *make_mini(lv_obj_t *scr)
{
    lv_obj_t *mini = lv_obj_create(scr);
    lv_obj_remove_style_all(mini);
    lv_obj_set_width(mini, lv_pct(100));
    lv_obj_set_height(mini, 54);
    lv_obj_clear_flag(mini, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(mini, 8, 0);
    lv_obj_set_style_bg_opa(mini, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(mini, lv_color_hex(0x1E1E1E), 0);
    lv_obj_set_flex_flow(mini, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mini, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(mini, 12, 0);
    lv_obj_set_style_pad_right(mini, 6, 0);

    lv_obj_t *info = lv_obj_create(mini);
    lv_obj_remove_style_all(info);
    lv_obj_set_height(info, lv_pct(100));
    lv_obj_set_flex_grow(info, 1);
    lv_obj_clear_flag(info, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(info, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_event_cb(info, open_np_cb, LV_EVENT_CLICKED, NULL);

    mini_title = lv_label_create(info);
    lv_obj_set_style_text_font(mini_title, &opensans_16, 0);
    lv_obj_set_style_text_color(mini_title, lv_color_white(), 0);
    lv_label_set_long_mode(mini_title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(mini_title, lv_pct(100));
    lv_label_set_text(mini_title, "- - -");

    mini_artist = lv_label_create(info);
    lv_obj_set_style_text_font(mini_artist, &opensans_16, 0);
    lv_obj_set_style_text_color(mini_artist, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(mini_artist, "-");

    make_mini_btn(mini, LV_SYMBOL_PREV, "prev");
    mini_pp = make_mini_btn(mini, LV_SYMBOL_PLAY, "playpause");
    make_mini_btn(mini, LV_SYMBOL_NEXT, "next");
    return mini;
}

// Creates the home screen with the mini player, search button, and feed container
lv_obj_t *home_screen_create()
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0F0F0F), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(scr, 12, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(scr, 10, 0);

    home_feed = lv_obj_create(scr);
    lv_obj_remove_style_all(home_feed);
    lv_obj_set_width(home_feed, lv_pct(100));
    lv_obj_set_flex_grow(home_feed, 1);
    lv_obj_set_flex_flow(home_feed, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(home_feed, 8, 0);
    lv_obj_set_scroll_dir(home_feed, LV_DIR_VER);

    make_mini(scr);
    make_search(scr);
    return scr;
}

// Updates the mini player with the current song's data
void home_set_np(const char *title, const char *artist, bool playing)
{
    if (mini_title) lv_label_set_text(mini_title, title);
    if (mini_artist) lv_label_set_text(mini_artist, artist);
    if (mini_pp) lv_label_set_text(mini_pp, playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
}

// Checks if the feed is ready to be displayed
bool home_feed_ready()
{
    return feed_ready;
}

// Clears the feed and prepares for new feed items
void feed_begin()
{
    if (!home_feed) return;
    lv_obj_clean(home_feed);
    feed_row = nullptr;
    feed_count = 0;
    feed_ready = true;
}

// Adds a new section to the feed with the given title
void feed_section(const char *title)
{
    if (!home_feed) return;

    lv_obj_t *head = lv_label_create(home_feed);
    lv_obj_set_style_text_font(head, &opensans_16, 0);
    lv_obj_set_style_text_color(head, lv_color_white(), 0);
    lv_label_set_text(head, title);

    feed_row = lv_obj_create(home_feed);
    lv_obj_remove_style_all(feed_row);
    lv_obj_set_width(feed_row, lv_pct(100));
    lv_obj_set_height(feed_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(feed_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(feed_row, 10, 0);
    lv_obj_set_scroll_dir(feed_row, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(feed_row, LV_SCROLL_SNAP_START);
}

// Adds a new item to the feed
void feed_item(const char *title, const char *sub, const char *id, const char *kind)
{
    if (!feed_row || feed_count >= FEED_MAX_ITEMS) return;

    int idx = feed_count;
    strncpy(feed_ids[idx], id, sizeof(feed_ids[idx]));
    feed_ids[idx][sizeof(feed_ids[idx]) - 1] = '\0';
    feed_kinds[idx][0] = kind[0];
    feed_kinds[idx][1] = '\0';
    feed_count++;

    lv_obj_t *card = lv_obj_create(feed_row);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 96, LV_SIZE_CONTENT);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 4, 0);
    lv_obj_add_event_cb(card, card_cb, LV_EVENT_CLICKED, (void *)(intptr_t)idx);

    lv_obj_t *thumb = lv_obj_create(card);
    lv_obj_remove_style_all(thumb);
    lv_obj_set_size(thumb, 96, 96);
    lv_obj_set_style_radius(thumb, 8, 0);
    lv_obj_set_style_bg_opa(thumb, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(thumb, lv_color_hex(0x282828), 0);

    lv_obj_t *t = lv_label_create(card);
    lv_obj_set_style_text_font(t, &opensans_16, 0);
    lv_obj_set_style_text_color(t, lv_color_white(), 0);
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
    lv_obj_set_width(t, 96);
    lv_label_set_text(t, title);

    lv_obj_t *s = lv_label_create(card);
    lv_obj_set_style_text_font(s, &opensans_16, 0);
    lv_obj_set_style_text_color(s, lv_color_hex(0x999999), 0);
    lv_label_set_long_mode(s, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s, 96);
    lv_label_set_text(s, sub);
}

// Empty for now, but could be used to finalize feed processing if needed
void feed_end()
{
}
