#include <Arduino.h>
#include <ArduinoJson.h>

#include "link.h"

static char         line_buf[512];              // Buffer to store incoming line from Serial
static size_t       line_len = 0;               // Length of the current line in the buffer
static np_handler_t np_cb = nullptr;            // Callback for "now playing" events
static art_begin_t  art_begin_cb = nullptr;     // Callback for artwork begin
static art_end_t    art_end_cb = nullptr;       // Callback for artwork end
static feed_begin_t feed_begin_cb = nullptr;    // Callback for feed begin
static feed_sec_t   feed_sec_cb = nullptr;      // Callback for feed section
static feed_item_t  feed_item_cb = nullptr;     // Callback for feed item
static feed_end_t   feed_end_cb = nullptr;      // Callback for feed end
static feed_thumb_begin_t feed_thumb_begin_cb = nullptr; // Callback for feed thumbnail begin
static feed_thumb_end_t   feed_thumb_end_cb = nullptr;   // Callback for feed thumbnail end

static const size_t ART_CHUNK = 4096;           // Size of chunks to read artwork data in

// Opens serial comms.
void link_begin(unsigned long baud)
{
    Serial.setRxBufferSize(8192);
    Serial.begin(baud);
    Serial.setTimeout(1000);
}

// Sets the callback for "now playing" events
void link_on_now_playing(np_handler_t cb)
{
    np_cb = cb;
}

// Sets the callbacks for artwork events
void link_on_art(art_begin_t begin, art_end_t end)
{
    art_begin_cb = begin;
    art_end_cb = end;
}

// Sets the callbacks for feed events
void link_on_feed(feed_begin_t begin, feed_sec_t sec, feed_item_t item, feed_end_t end)
{
    feed_begin_cb = begin;
    feed_sec_cb = sec;
    feed_item_cb = item;
    feed_end_cb = end;
}

// Sets the callbacks for feed thumbnail events
void link_on_feed_thumb(feed_thumb_begin_t begin, feed_thumb_end_t end)
{
    feed_thumb_begin_cb = begin;
    feed_thumb_end_cb = end;
}

// Receives binary data into buf in chunks, acking each; returns true if complete
static bool receive_binary(uint8_t *buf, size_t total)
{
    size_t got = 0;
    while (got < total)
    {
        size_t want = total - got;
        if (want > ART_CHUNK) want = ART_CHUNK;
        size_t r = Serial.readBytes(buf + got, want);
        got += r;
        Serial.println("{\"t\":\"ok\"}");
        Serial.flush();
        if (r < want) return false;
    }
    return true;
}

// Processes incoming serial data, looking for complete JSON messages and dispatching them to the appropriate callbacks
void link_task()
{
    while (Serial.available())
    {
        char c = Serial.read();
        if (c == '\n')
        {
            line_buf[line_len] = '\0';
            JsonDocument doc;
            if (deserializeJson(doc, line_buf) == DeserializationError::Ok)
            {
                const char *t = doc["t"] | "";
                if (strcmp(t, "np") == 0 && np_cb)
                {
                    np_cb(doc);
                }
                else if (strcmp(t, "art") == 0 && art_begin_cb)
                {
                    int w = doc["w"] | 0;
                    int h = doc["h"] | 0;
                    uint8_t *buf = art_begin_cb(w, h);
                    if (buf && w > 0 && h > 0)
                    {
                        if (receive_binary(buf, (size_t)w * h * 2) && art_end_cb) art_end_cb();
                    }
                }
                else if (strcmp(t, "ft") == 0 && feed_thumb_begin_cb)
                {
                    int i = doc["i"] | -1;
                    int w = doc["w"] | 0;
                    int h = doc["h"] | 0;
                    uint8_t *buf = feed_thumb_begin_cb(i, w, h);
                    if (buf && w > 0 && h > 0)
                    {
                        if (receive_binary(buf, (size_t)w * h * 2) && feed_thumb_end_cb) feed_thumb_end_cb(i);
                    }
                }
                else if (strcmp(t, "fb") == 0 && feed_begin_cb)
                {
                    feed_begin_cb();
                }
                else if (strcmp(t, "fs") == 0 && feed_sec_cb)
                {
                    feed_sec_cb(doc["title"] | "");
                }
                else if (strcmp(t, "fi") == 0 && feed_item_cb)
                {
                    feed_item_cb(doc["title"] | "", doc["sub"] | "", doc["id"] | "", doc["k"] | "");
                }
                else if (strcmp(t, "fe") == 0 && feed_end_cb)
                {
                    feed_end_cb();
                }
            }
            line_len = 0;
        }
        else if (line_len < sizeof(line_buf) - 1)
        {
            line_buf[line_len++] = c;
        }
    }
}

// Sends a command to the host
void link_send_cmd(const char *action)
{
    Serial.print("{\"t\":\"cmd\",\"a\":\"");
    Serial.print(action);
    Serial.println("\"}");
}

// Sends a command to the host to play a specific item
void link_send_play(const char *id, const char *kind)
{
    Serial.print("{\"t\":\"cmd\",\"a\":\"play\",\"id\":\"");
    Serial.print(id);
    Serial.print("\",\"k\":\"");
    Serial.print(kind);
    Serial.println("\"}");
}
