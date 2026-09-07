#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>
#include <lvgl.h>

#include "board_pins.h"
#include "display.h"

static Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);
static Arduino_GFX *gfx = new Arduino_NV3041A(
    bus, GFX_NOT_DEFINED, 2 /* rotation (0(port left) or 2(port right)) */, true /* IPS */);

static TAMC_GT911 touch = TAMC_GT911(
    TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, LCD_WIDTH, LCD_HEIGHT);

static lv_disp_draw_buf_t draw_buf; // Display buffer
static lv_color_t        *buf1;     // Pointer to the first buffer
static const uint32_t     BUF_LINES = 40; // Number of lines in the buffer

// Send a rectangle of pixels to the display
static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)color_p, w, h);
    lv_disp_flush_ready(drv);
}

// Touch read callback function to read touch input
static void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    touch.read();
    if (touch.isTouched)
    {
        data->state   = LV_INDEV_STATE_PRESSED;
        data->point.x = touch.points[0].x;
        data->point.y = touch.points[0].y;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// Initialize the display and touch input
void display_begin()
{
    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH);

    gfx->begin();
    gfx->invertDisplay(false);
    touch.begin();
    touch.setRotation(ROTATION_NORMAL); // NORMAL: port on left, INVERTED: port on right

    lv_init();

    buf1 = (lv_color_t *)ps_malloc(LCD_WIDTH * BUF_LINES * sizeof(lv_color_t));
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, LCD_WIDTH * BUF_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = LCD_WIDTH;
    disp_drv.ver_res  = LCD_HEIGHT;
    disp_drv.flush_cb = flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&indev_drv);
}

// Call this function periodically to handle LVGL tasks
void display_tick()
{
    lv_timer_handler();
}
