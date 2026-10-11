#include "midi.h"
#include <string.h>
static void emit(MidiParser *p, const uint8_t *d, uint8_t len, uint8_t cin) {
    if (p->callback)
        p->callback(p->ctx, d, len, cin);
}
void midi_init(MidiParser *p, MidiCallback cb, void *ctx) {
    memset(p, 0, sizeof(*p));
    p->callback = cb;
    p->ctx = ctx;
}
uint8_t midi_packet_length(uint8_t cin) {
    static const uint8_t length[16] = {0, 0, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1};
    return length[cin & 15];
}
void midi_byte(MidiParser *p, uint8_t b, uint32_t now) {
    p->last_byte = now;
    if (b >= 0xf8) {
        if (b == 0xfe)
            p->sensing = true;
        if (b != 0xf9 && b != 0xfd)
            emit(p, &b, 1, 15);
        return;
    }
    if (p->sysex) {
        if (b == 0xf7 || b < 0x80) {
            p->sysex_data[p->sysex_count++] = b;
            if (b == 0xf7) {
                emit(p, p->sysex_data, p->sysex_count, (uint8_t)(4 + p->sysex_count));
                p->sysex = false;
                p->sysex_count = 0;
            } else if (p->sysex_count == 3) {
                emit(p, p->sysex_data, 3, 4);
                p->sysex_count = 0;
            }
            return;
        }
        p->sysex = false;
        p->sysex_count = 0;
        p->errors++;
    }
    if (b & 0x80) {
        if (p->count)
            p->errors++;
        p->count = 0;
        p->status = b;
        if (b < 0xf0) {
            p->running = b;
            p->needed = (b >> 4 == 0xc || b >> 4 == 0xd) ? 1 : 2;
        } else {
            p->running = 0;
            if (b == 0xf0) {
                p->sysex = true;
                p->sysex_count = 1;
                p->sysex_data[0] = b;
                return;
            }
            p->needed = b == 0xf1 || b == 0xf3 ? 1 : b == 0xf2 ? 2 : 0;
            if (!p->needed) {
                if (b == 0xf6 || b == 0xf7)
                    emit(p, &b, 1, 5);
                p->status = 0;
            }
        }
        return;
    }
    if (!p->status) {
        if (!p->running) {
            p->errors++;
            return;
        }
        p->status = p->running;
    }
    p->data[0] = p->status;
    p->data[++p->count] = b;
    if (p->count == p->needed) {
        uint8_t cin = p->status < 0xf0 ? p->status >> 4 : p->needed == 1 ? 2 : 3;
        emit(p, p->data, (uint8_t)(p->needed + 1), cin);
        p->count = 0;
        p->status = p->running;
    }
}
bool midi_sensing_expired(MidiParser *p, uint32_t now) {
    if (p->sensing && now - p->last_byte > 300) {
        p->sensing = false;
        p->running = p->status = p->count = 0;
        p->sysex = false;
        p->sysex_count = 0;
        return true;
    }
    return false;
}
