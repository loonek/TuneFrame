#pragma once

#include <lvgl.h>
#include <ArduinoJson.h>

lv_obj_t *np_screen_create();
void np_apply(JsonDocument &doc);
void np_tick();

uint8_t *np_art_begin(int w, int h);
void     np_art_end();
