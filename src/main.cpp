#include <Arduino.h>

#include "display.h"
#include "link.h"
#include "ui_now_playing.h"
#include "ui_home.h"
#include "ui_nav.h"
#include "ui_queue.h"
#include "ui_settings.h"
#include "i18n.h"
#include "settings_store.h"
#include "ui_playlist.h"

lv_obj_t *g_scr_home = nullptr;
lv_obj_t *g_scr_np = nullptr;
lv_obj_t *g_scr_queue = nullptr;
lv_obj_t *g_scr_settings = nullptr;
lv_obj_t *g_scr_playlist = nullptr;

// pc-helper\.venv\Scripts\python.exe pc-helper\bridge.py
// pc-helper\.venv\Scripts\python.exe pc-helper\bridge.py --tcp

void setup()
{
    link_begin(115200);
    display_begin();
    set_language(settings_load_language(0));   // apply saved language before building screens
    g_scr_np = np_screen_create();
    g_scr_home = home_screen_create();
    g_scr_queue = queue_screen_create();
    g_scr_settings = settings_screen_create();
    g_scr_playlist = playlist_screen_create();
    lv_scr_load(g_scr_home);
    link_on_now_playing(np_apply);
    link_on_art(np_art_begin, np_art_end);
    link_on_feed(feed_begin, feed_section, feed_item, feed_end);
    link_on_feed_thumb(feed_thumb_begin, feed_thumb_end);
    link_on_queue(queue_begin, queue_item, queue_end);
    link_on_playlist(playlist_begin, playlist_item, playlist_end);
}

void loop()
{
    static uint32_t last_feed_req = 0;

    link_task();
    np_tick();
    display_tick();

    if (!home_feed_ready() && millis() - last_feed_req > 4000)
    {
        link_send_cmd("feed");
        last_feed_req = millis();
    }

    delay(5);
}
