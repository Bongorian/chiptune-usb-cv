#include "oled.h"
#include <string.h>

void oled_init(Oled *o, const uint8_t *frame, bool (*write)(void *, const uint8_t *, size_t),
               void (*render)(void *, uint32_t), void *ctx) {
    memset(o, 0, sizeof(*o));
    o->frame = frame;
    o->write = write;
    o->render = render;
    o->ctx = ctx;
}
static bool send(Oled *o, const uint8_t *bytes, size_t length, uint32_t now) {
    if (o->write(o->ctx, bytes, length))
        return true;
    o->ready = false;
    o->next = now + 1000;
    return false;
}
void oled_suspend(Oled *o) {
    const uint8_t off[] = {0, 0xae, 0x8d, 0x10};
    (void)o->write(o->ctx, off, sizeof(off));
    o->ready = o->display_on = o->pump_started = false;
    o->scheduled = false;
}
void oled_task(Oled *o, uint32_t now, bool power, uint8_t brightness) {
    if (o->scheduled && (int32_t)(now - o->next) < 0)
        return;
    o->scheduled = true;
    if (!o->ready) {
        const uint8_t init[] = {0,    0xae, 0xd5, 0x80, 0xa8, 0x3f, 0xd3, 0,    0x40,
                                0x8d, 0x10, 0x20, 2,    0xa1, 0xc8, 0xda, 0x12, 0x81,
                                0x50, 0xd9, 0xf1, 0xdb, 0x40, 0xa4, 0xa6};
        if (!send(o, init, sizeof(init), now))
            return;
        o->ready = true;
        o->next = now + 1;
        o->display_on = o->pump_started = false;
        o->contrast = 0; /* Reapply brightness after every controller reset/retry. */
        o->chunk = 0;
        o->render_next = now;
        return;
    }
    if (power && !o->pump_started) {
        const uint8_t pump[] = {0, 0x8d, 0x14};
        if (!send(o, pump, sizeof(pump), now))
            return;
        o->pump_started = true;
        o->next = now + 100;
        return;
    }
    if (!power && (o->display_on || o->pump_started)) {
        const uint8_t off[] = {0, 0xae, 0x8d, 0x10};
        if (!send(o, off, sizeof(off), now))
            return;
        o->display_on = o->pump_started = false;
    }
    if (!power) {
        o->next = now + 20;
        return;
    }
    brightness = brightness < 10 ? 10 : brightness > 100 ? 100 : brightness;
    if (o->contrast != brightness) {
        const uint8_t command[] = {0, 0x81, (uint8_t)(brightness * 255u / 100u)};
        if (!send(o, command, sizeof(command), now))
            return;
        o->contrast = brightness;
    }
    if (!o->chunk && (int32_t)(now - o->render_next) >= 0) {
        o->render(o->ctx, now);
        o->render_next = now + 100;
    }
    /* Freeze the rendered frame through all 64 bounded I2C transactions. */
    unsigned page = o->chunk / 8, col = (o->chunk % 8) * 16;
    const uint8_t cmd[] = {0, (uint8_t)(0xb0 | page), (uint8_t)(col & 15),
                           (uint8_t)(0x10 | (col >> 4))};
    uint8_t data[17] = {0x40};
    memcpy(data + 1, o->frame + page * 128 + col, 16);
    if (!send(o, cmd, sizeof(cmd), now) || !send(o, data, sizeof(data), now))
        return;
    /* Present actual frame data before enabling the panel. */
    if (!o->display_on) {
        const uint8_t on[] = {0, 0xaf};
        if (!send(o, on, sizeof(on), now))
            return;
        o->display_on = true;
    }
    o->chunk = (o->chunk + 1) % 64;
    o->next = now + 2;
}
