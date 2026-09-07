#include <Arduino.h>

#include "display.h"
#include "link.h"
#include "ui_now_playing.h"
#include "ui_home.h"
#include "ui_nav.h"

lv_obj_t *g_scr_home = nullptr;
lv_obj_t *g_scr_np = nullptr;

void setup()
{
    link_begin(115200);
    display_begin();
    g_scr_np = np_screen_create();
    g_scr_home = home_screen_create();
    lv_scr_load(g_scr_home);
    link_on_now_playing(np_apply);
    link_on_art(np_art_begin, np_art_end);
    link_on_feed(feed_begin, feed_section, feed_item, feed_end);
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
