#pragma once

#include <lvgl.h>

lv_obj_t *queue_screen_create();
void queue_begin();
void queue_item(const char *title, const char *sub, bool cur);
void queue_end();