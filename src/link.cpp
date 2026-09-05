#include <Arduino.h>
#include <ArduinoJson.h>

#include "link.h"

static char         line_buf[512];
static size_t       line_len = 0;
static np_handler_t np_cb = nullptr;
static art_begin_t  art_begin_cb = nullptr;
static art_end_t    art_end_cb = nullptr;

static const size_t ART_CHUNK = 4096;

void link_begin(unsigned long baud)
{
    Serial.setRxBufferSize(8192);
    Serial.begin(baud);
    Serial.setTimeout(1000);
}

void link_on_now_playing(np_handler_t cb)
{
    np_cb = cb;
}

void link_on_art(art_begin_t begin, art_end_t end)
{
    art_begin_cb = begin;
    art_end_cb = end;
}

static void receive_art(uint8_t *buf, size_t total)
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
        if (r < want) return;
    }
    if (got == total && art_end_cb) art_end_cb();
}

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
                        receive_art(buf, (size_t)w * h * 2);
                    }
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

void link_send_cmd(const char *action)
{
    Serial.print("{\"t\":\"cmd\",\"a\":\"");
    Serial.print(action);
    Serial.println("\"}");
}
