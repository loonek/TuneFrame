#pragma once

#include <ArduinoJson.h>

typedef void (*np_handler_t)(JsonDocument &doc);
typedef uint8_t *(*art_begin_t)(int w, int h);
typedef void (*art_end_t)();

void link_begin(unsigned long baud = 115200);
void link_on_now_playing(np_handler_t cb);
void link_on_art(art_begin_t begin, art_end_t end);
void link_task();
void link_send_cmd(const char *action);
