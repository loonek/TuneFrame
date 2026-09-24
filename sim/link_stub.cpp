#include <lvgl.h>

#include "ui_nav.h"

// Screen pointers the UI navigates between (defined by main on the board)
lv_obj_t *g_scr_home = nullptr;
lv_obj_t *g_scr_np = nullptr;
lv_obj_t *g_scr_queue = nullptr;
lv_obj_t *g_scr_settings = nullptr;
lv_obj_t *g_scr_playlist = nullptr;

// ./sim/build/sim.exe live