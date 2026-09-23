#include <Arduino.h>
#include <lvgl.h>

#include "ui_home.h"
#include "ui_nav.h"
#include "link.h"
#include "strings.h"

LV_FONT_DECLARE(opensans_16);
LV_FONT_DECLARE(icons);

#define FEED_MAX_ITEMS 64

static lv_obj_t *mini_title;    // Label for the title of the currently playing song
static lv_obj_t *mini_artist;   // Label for the artist
static lv_obj_t *mini_pp;       // Label for the play/pause button

static lv_obj_t *feed_page;      // Feed tab content (Quick picks, recently played)
static lv_obj_t *playlists_page; // Playlists tab content (user library)
static lv_obj_t *feed_row;       // Horizontal card row of the current section
static lv_obj_t *feed_spinner;   // Loading indicator shown while the feed refreshes

static char feed_ids[FEED_MAX_ITEMS][64];  // Item IDs (videoId ~11, playlistId up to ~43)
static char feed_kinds[FEED_MAX_ITEMS][2];
static int  feed_count = 0;
static bool feed_ready = false;

static lv_obj_t   *feed_thumbs[FEED_MAX_ITEMS];  // Thumbnail image object of each card
static uint8_t    *thumb_bufs[FEED_MAX_ITEMS];   // RGB565 pixel buffer of each thumbnail
static lv_img_dsc_t thumb_dscs[FEED_MAX_ITEMS];  // Image descriptor of each thumbnail

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

// Shows or hides the feed loading indicator
static void set_feed_loading(bool on)
{
    if (!feed_spinner) return;
    if (on) lv_obj_clear_flag(feed_spinner, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(feed_spinner, LV_OBJ_FLAG_HIDDEN);
}

// Clears the feed and requests a fresh one from the host
static void refresh_cb(lv_event_t *e)
{
    set_feed_loading(true);
    feed_begin();
    link_send_cmd("feed");
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

// Creates a round search button that asks the host to open YT Music search
static lv_obj_t *make_search(lv_obj_t *parent)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, 40, 40);
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
    return btn;
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
    lv_obj_set_style_pad_left(mini, 8, 0);
    lv_obj_set_style_pad_right(mini, 6, 0);
    lv_obj_set_style_pad_column(mini, 6, 0);

    make_search(mini);

    lv_obj_t *refresh = lv_btn_create(mini);
    lv_obj_remove_style_all(refresh);
    lv_obj_set_size(refresh, 40, 40);
    lv_obj_set_style_radius(refresh, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(refresh, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(refresh, lv_color_hex(0x272727), 0);
    lv_obj_set_style_bg_color(refresh, lv_color_hex(0x383838), LV_STATE_PRESSED);
    lv_obj_add_event_cb(refresh, refresh_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ricon = lv_label_create(refresh);
    lv_label_set_text(ricon, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_color(ricon, lv_color_hex(0xCCCCCC), 0);
    lv_obj_center(ricon);

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

// Configures a tab page as a vertical-scrolling column of sections
static void setup_page(lv_obj_t *page)
{
    lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_set_style_pad_top(page, 6, 0);
    lv_obj_set_style_pad_row(page, 8, 0);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(page, LV_DIR_VER);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLL_ELASTIC);
}

// Creates the home screen: Feed | Playlisty tabs, mini player, search
lv_obj_t *home_screen_create()
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0F0F0F), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(scr, 12, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(scr, 8, 0);

    lv_obj_t *tv = lv_tabview_create(scr, LV_DIR_TOP, 30);
    lv_obj_set_width(tv, lv_pct(100));
    lv_obj_set_flex_grow(tv, 1);
    lv_obj_set_style_bg_opa(tv, LV_OPA_TRANSP, 0);

    lv_obj_t *bar = lv_tabview_get_tab_btns(tv);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_text_font(bar, &opensans_16, LV_PART_ITEMS);
    lv_obj_set_style_text_color(bar, lv_color_hex(0x888888), LV_PART_ITEMS);
    lv_obj_set_style_text_color(bar, lv_color_white(), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xFF0000), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_radius(bar, 14, LV_PART_ITEMS);
    lv_obj_set_style_border_width(bar, 0, LV_PART_ITEMS);
    lv_obj_set_style_border_width(bar, 0, LV_PART_ITEMS | LV_STATE_CHECKED);

    feed_page = lv_tabview_add_tab(tv, L->tab_feed);
    playlists_page = lv_tabview_add_tab(tv, L->tab_playlists);
    setup_page(feed_page);
    setup_page(playlists_page);

    make_mini(scr);

    feed_spinner = lv_spinner_create(scr, 1000, 60);
    lv_obj_add_flag(feed_spinner, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(feed_spinner, 44, 44);
    lv_obj_align(feed_spinner, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_arc_color(feed_spinner, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_arc_color(feed_spinner, lv_color_hex(0xFF0000), LV_PART_INDICATOR);
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
    if (!feed_page) return;
    for (int i = 0; i < FEED_MAX_ITEMS; i++)
    {
        if (thumb_bufs[i]) { free(thumb_bufs[i]); thumb_bufs[i] = nullptr; }
        feed_thumbs[i] = nullptr;
    }
    lv_obj_clean(feed_page);
    lv_obj_clean(playlists_page);
    feed_row = nullptr;
    feed_count = 0;
    feed_ready = true;
}

// Adds a new section, routing the playlists section (kind "p") to the playlists tab
void feed_section(const char *title, const char *kind)
{
    if (!feed_page) return;
    bool playlists = strcmp(kind, "p") == 0;
    lv_obj_t *page = playlists ? playlists_page : feed_page;

    lv_obj_t *head = lv_label_create(page);
    lv_obj_set_style_text_font(head, &opensans_16, 0);
    lv_obj_set_style_text_color(head, lv_color_white(), 0);
    lv_label_set_text(head, title);

    feed_row = lv_obj_create(page);
    lv_obj_remove_style_all(feed_row);
    lv_obj_set_width(feed_row, lv_pct(100));
    lv_obj_set_height(feed_row, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(feed_row, 10, 0);

    if (playlists)
    {
        lv_obj_set_flex_flow(feed_row, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_style_pad_row(feed_row, 10, 0);
        lv_obj_clear_flag(feed_row, LV_OBJ_FLAG_SCROLLABLE);
    }
    else
    {
        lv_obj_set_flex_flow(feed_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_scroll_dir(feed_row, LV_DIR_HOR);
        lv_obj_set_scroll_snap_x(feed_row, LV_SCROLL_SNAP_START);
        lv_obj_clear_flag(feed_row, LV_OBJ_FLAG_SCROLL_ELASTIC);
    }
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

    lv_obj_t *thumb = lv_img_create(card);
    lv_obj_set_size(thumb, 96, 96);
    lv_obj_set_style_radius(thumb, 8, 0);
    lv_obj_set_style_clip_corner(thumb, true, 0);
    lv_obj_set_style_bg_opa(thumb, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(thumb, lv_color_hex(0x282828), 0);
    feed_thumbs[idx] = thumb;

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

// Hides the loading indicator once the feed has fully arrived
void feed_end()
{
    set_feed_loading(false);
}

// Allocates the pixel buffer for a card's thumbnail and returns it
uint8_t *feed_thumb_begin(int idx, int w, int h)
{
    if (idx < 0 || idx >= FEED_MAX_ITEMS || !feed_thumbs[idx]) return nullptr;
    if (w <= 0 || h <= 0 || w > 128 || h > 128) return nullptr;
    if (thumb_bufs[idx]) free(thumb_bufs[idx]);
    thumb_bufs[idx] = (uint8_t *)ps_malloc((size_t)w * h * 2);
    if (!thumb_bufs[idx]) return nullptr;
    thumb_dscs[idx].header.always_zero = 0;
    thumb_dscs[idx].header.cf = LV_IMG_CF_TRUE_COLOR;
    thumb_dscs[idx].header.w = w;
    thumb_dscs[idx].header.h = h;
    thumb_dscs[idx].data = thumb_bufs[idx];
    thumb_dscs[idx].data_size = (uint32_t)w * h * 2;
    return thumb_bufs[idx];
}

// Renders the received thumbnail into its card
void feed_thumb_end(int idx)
{
    if (idx < 0 || idx >= FEED_MAX_ITEMS || !feed_thumbs[idx]) return;
    lv_img_set_src(feed_thumbs[idx], &thumb_dscs[idx]);
}
