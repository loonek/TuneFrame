#pragma once

#include <lvgl.h>

lv_obj_t *home_screen_create();
void home_set_np(const char *title, const char *artist, bool playing);

bool home_feed_ready();
void feed_begin();
void feed_section(const char *title);
void feed_item(const char *title, const char *sub, const char *id, const char *kind);
void feed_end();
