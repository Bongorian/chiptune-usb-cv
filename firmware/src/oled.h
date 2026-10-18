#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool (*write)(void *ctx, const uint8_t *bytes, size_t length);
    void (*render)(void *ctx, uint32_t now);
    void *ctx;
    const uint8_t *frame;
    uint32_t next, render_next;
    unsigned chunk;
    bool ready, display_on, pump_started, scheduled;
    uint8_t contrast;
} Oled;

void oled_init(Oled *o, const uint8_t *frame, bool (*write)(void *, const uint8_t *, size_t),
               void (*render)(void *, uint32_t), void *ctx);
void oled_task(Oled *o, uint32_t now, bool power, uint8_t brightness);
void oled_suspend(Oled *o);
