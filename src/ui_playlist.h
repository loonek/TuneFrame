#pragma once

#include <lvgl.h>

lv_obj_t *playlist_screen_create();
void playlist_open(const char *id);
void playlist_begin(const char *title);
void playlist_item(const char *title, const char *sub);
void playlist_end();