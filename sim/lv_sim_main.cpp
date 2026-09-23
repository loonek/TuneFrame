#include <SDL.h>
#include <lvgl.h>
#include <ArduinoJson.h>

#include <cstdint>
#include <cstring>

#include "board_pins.h"
#include "ui_home.h"
#include "ui_now_playing.h"
#include "ui_queue.h"
#include "ui_nav.h"
#include "link.h"

#define SCALE 2

// LVGL tick source (referenced through Arduino.h by lv_conf's LV_TICK_CUSTOM)
extern "C" uint32_t millis(void) { return SDL_GetTicks(); }

static SDL_Renderer *renderer;
static SDL_Texture  *texture;
static uint32_t      framebuf[LCD_WIDTH * LCD_HEIGHT];

static int  mouse_x = 0;
static int  mouse_y = 0;
static bool mouse_down = false;

// Converts LVGL's flushed area into the ARGB framebuffer
static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    for (int y = area->y1; y <= area->y2; y++)
    {
        for (int x = area->x1; x <= area->x2; x++)
        {
            lv_color32_t c;
            c.full = lv_color_to32(*color_p);
            framebuf[y * LCD_WIDTH + x] = (0xFFu << 24) | (c.ch.red << 16) | (c.ch.green << 8) | c.ch.blue;
            color_p++;
        }
    }
    lv_disp_flush_ready(drv);
}

// Feeds the current mouse state to LVGL as a touch pointer
static void mouse_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    data->point.x = mouse_x / SCALE;
    data->point.y = mouse_y / SCALE;
    data->state = mouse_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// Injects a sample play queue
static void sample_queue()
{
    const char *songs[8][2] = {
        {"Blinding Lights", "The Weeknd"}, {"Nawet jak rozbije bank", "Mata"},
        {"Cicho sza", "sanah"}, {"Supreme", "molodoikartel"},
        {"Funeral", "Ashe"}, {"W te noc", "Happysad"},
        {"HIGHJACK", "Alok, Laszewo & A$AP Rocky"}, {"car crash", "jigitz & Charlotte Plank"},
    };
    queue_begin();
    for (int i = 0; i < 8; i++) queue_item(songs[i][0], songs[i][1], i == 1);
    queue_end();
}

// Injects a sample now-playing message
static void sample_np()
{
    JsonDocument doc;
    doc["t"] = "np";
    doc["status"] = "playing";
    doc["title"] = "Blinding Lights";
    doc["artist"] = "The Weeknd";
    doc["pos"] = 83;
    doc["dur"] = 200;
    doc["vid"] = "demo";
    doc["vol"] = 60;
    np_apply(doc);
}

// Injects a sample feed with synthesized thumbnails
static void sample_feed()
{
    const char *songs[6][2] = {
        {"Blinding Lights", "The Weeknd"}, {"Nawet jak rozbije bank", "Mata"},
        {"Cicho sza", "sanah"}, {"Supreme", "molodoikartel"},
        {"Funeral", "Ashe"}, {"W te noc", "Happysad"},
    };

    feed_begin();
    feed_section("Quick picks", "");
    for (int i = 0; i < 6; i++) feed_item(songs[i][0], songs[i][1], "vid", "v");
    feed_section("Last played", "");
    for (int i = 0; i < 4; i++) feed_item(songs[i][0], songs[i][1], "vid", "v");
    feed_section("Your Playlists", "p");
    for (int i = 0; i < 4; i++) feed_item(songs[i][0], songs[i][1], "PLdemo", "s");
    feed_end();

    for (int idx = 0; idx < 14; idx++)
    {
        uint8_t *buf = feed_thumb_begin(idx, 96, 96);
        if (!buf) continue;
        for (int p = 0; p < 96 * 96; p++)
        {
            uint8_t r = (uint8_t)(idx * 26 + 40);
            uint8_t g = (uint8_t)((p / 96) * 2);
            uint8_t b = 140;
            uint16_t v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
            buf[2 * p] = v & 0xFF;
            buf[2 * p + 1] = (v >> 8) & 0xFF;
        }
        feed_thumb_end(idx);
    }
}

int main(int argc, char **argv)
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *win = SDL_CreateWindow("JC4827W543 sim",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        LCD_WIDTH * SCALE, LCD_HEIGHT * SCALE, 0);
    renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, LCD_WIDTH, LCD_HEIGHT);

    lv_init();

    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[LCD_WIDTH * 40];
    lv_disp_draw_buf_init(&draw_buf, buf1, nullptr, LCD_WIDTH * 40);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_WIDTH;
    disp_drv.ver_res = LCD_HEIGHT;
    disp_drv.flush_cb = flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = mouse_read_cb;
    lv_indev_drv_register(&indev_drv);

    g_scr_np = np_screen_create();
    g_scr_home = home_screen_create();
    g_scr_queue = queue_screen_create();
    lv_scr_load(g_scr_home);

    bool live = (argc > 1 && strcmp(argv[1], "live") == 0);
    if (live)
    {
        link_begin(115200);
        link_on_now_playing(np_apply);
        link_on_art(np_art_begin, np_art_end);
        link_on_feed(feed_begin, feed_section, feed_item, feed_end);
        link_on_feed_thumb(feed_thumb_begin, feed_thumb_end);
        link_on_queue(queue_begin, queue_item, queue_end);
    }
    else
    {
        sample_np();
        sample_feed();
        sample_queue();
    }

    uint32_t last_feed_req = 0;
    bool running = true;
    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT) running = false;
            else if (e.type == SDL_MOUSEMOTION) { mouse_x = e.motion.x; mouse_y = e.motion.y; }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) { mouse_down = true; mouse_x = e.button.x; mouse_y = e.button.y; }
            else if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) mouse_down = false;
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_SPACE)
                lv_scr_load(lv_scr_act() == g_scr_home ? g_scr_np : g_scr_home);
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_q)
                lv_scr_load(g_scr_queue);
        }

        if (live)
        {
            link_task();
            if (!home_feed_ready() && millis() - last_feed_req > 4000)
            {
                link_send_cmd("feed");
                last_feed_req = millis();
            }
        }

        np_tick();
        lv_timer_handler();
        SDL_UpdateTexture(texture, nullptr, framebuf, LCD_WIDTH * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
        SDL_Delay(5);
    }

    SDL_Quit();
    return 0;
}
