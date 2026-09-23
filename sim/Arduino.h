#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Provided by the simulator main (SDL_GetTicks); also used by LVGL's tick source
uint32_t millis(void);

#ifdef __cplusplus
}
#endif

// PSRAM allocators on the board map to plain malloc/realloc on PC
static inline void *ps_malloc(size_t n) { return malloc(n); }
static inline void *ps_realloc(void *p, size_t n) { return realloc(p, n); }

#ifdef __cplusplus
// Minimal Arduino Serial replacement backed by a TCP socket (see serial_tcp.cpp),
// so the real link.cpp runs unchanged in the simulator
class SimSerial
{
public:
    void   setRxBufferSize(size_t) {}
    void   begin(unsigned long baud);
    void   setTimeout(unsigned long ms) { timeout_ = ms; }
    int    available();
    int    read();
    size_t readBytes(uint8_t *buf, size_t n);
    void   print(const char *s);
    void   println(const char *s);
    void   flush() {}
    unsigned long timeout_ = 1000;
};
extern SimSerial Serial;
#endif
