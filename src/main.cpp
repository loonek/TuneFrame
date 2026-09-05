#include <Arduino.h>

#include "display.h"
#include "link.h"
#include "ui_now_playing.h"

void setup()
{
    link_begin(115200);
    display_begin();
    np_screen_create();
    link_on_now_playing(np_apply);
    link_on_art(np_art_begin, np_art_end);
}

void loop()
{
    link_task();
    np_tick();
    display_tick();
    delay(5);
}
