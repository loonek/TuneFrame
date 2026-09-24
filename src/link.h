#pragma once

#include <ArduinoJson.h>

typedef void (*np_handler_t)(JsonDocument &doc);
typedef uint8_t *(*art_begin_t)(int w, int h);
typedef void (*art_end_t)();
typedef void (*feed_begin_t)();
typedef void (*feed_sec_t)(const char *title, const char *kind);
typedef void (*feed_item_t)(const char *title, const char *sub, const char *id, const char *kind);
typedef void (*feed_end_t)();
typedef uint8_t *(*feed_thumb_begin_t)(int idx, int w, int h);
typedef void (*feed_thumb_end_t)(int idx);
typedef void (*queue_begin_t)();
typedef void (*queue_item_t)(const char *title, const char *sub, bool cur);
typedef void (*queue_end_t)();
typedef void (*playlist_begin_t)(const char *title);
typedef void (*playlist_item_t)(const char *title, const char *sub);
typedef void (*playlist_end_t)();

void link_begin(unsigned long baud = 115200);
void link_on_now_playing(np_handler_t cb);
void link_on_art(art_begin_t begin, art_end_t end);
void link_on_feed(feed_begin_t begin, feed_sec_t sec, feed_item_t item, feed_end_t end);
void link_on_feed_thumb(feed_thumb_begin_t begin, feed_thumb_end_t end);
void link_task();
void link_send_cmd(const char *action);
void link_send_play(const char *id, const char *kind);
void link_on_queue(queue_begin_t begin, queue_item_t item, queue_end_t end);
void link_send_qjump(int index);
void link_on_playlist(playlist_begin_t begin, playlist_item_t item, playlist_end_t end);
void link_send_playlist(const char *id);