#include "register_bus.h"
#include "sn_clock.h"
#include <string.h>
static bool saa_register(unsigned r) {
    return r < 6 || (r >= 8 && r <= 13) || (r >= 0x10 && r <= 0x16) || r == 0x18 || r == 0x19 ||
           r == 0x1c;
}
void bus_init(RegisterBus *b, SoundSink sink) {
    memset(b, 0, sizeof(*b));
    b->sink = sink;
    for (unsigned r = 0; r < 32; r++)
        b->saa_sent[r] = saa_register(r) ? 255 : 0;
    memset(b->tone_sent, 255, sizeof(b->tone_sent));
    memset(b->volume, 15, sizeof(b->volume));
    memset(b->volume_sent, 255, sizeof(b->volume_sent));
    memset(b->noise_sent, 255, 2);
    b->saa[0x1c] = 1;
    b->force_mute = true;
}
void bus_saa(RegisterBus *b, unsigned r, unsigned value) {
    if (saa_register(r))
        b->saa[r] = (uint8_t)value;
}
void bus_tone(RegisterBus *b, unsigned c, unsigned n, unsigned value) {
    if (c < 2 && n < 3) {
        if (b->tone[c][n] != b->tone_sent[c][n] && b->tone[c][n] != value)
            b->coalesced++;
        b->tone[c][n] = (uint16_t)value;
    }
}
void bus_volume(RegisterBus *b, unsigned c, unsigned n, unsigned value) {
    if (c < 2 && n < 4)
        b->volume[c][n] = (uint8_t)value;
}
void bus_noise(RegisterBus *b, unsigned c, unsigned value, bool retrigger) {
    if (c < 2) {
        b->noise[c] = (uint8_t)value;
        if (retrigger)
            b->reset[c]++;
    }
}
void bus_mute(RegisterBus *b) {
    memset(b->volume, 15, sizeof(b->volume));
    for (unsigned i = 0; i < 6; i++)
        b->saa[i] = 0;
    b->saa[0x14] = b->saa[0x15] = b->saa[0x18] = b->saa[0x19] = 0;
    b->force_mute = true;
}
unsigned bus_pending(const RegisterBus *b) {
    unsigned n = b->busy;
    for (unsigned g = 0; g < 2; g++)
        n += b->env_reset[g] != b->env_reset_sent[g];
    for (unsigned r = 0; r < 32; r++)
        n += b->saa[r] != b->saa_sent[r];
    for (unsigned c = 0; c < 2; c++) {
        for (unsigned v = 0; v < 3; v++)
            n += b->tone[c][v] != b->tone_sent[c][v];
        for (unsigned v = 0; v < 4; v++)
            n += b->volume[c][v] != b->volume_sent[c][v];
        n += b->noise[c] != b->noise_sent[c] || b->reset[c] != b->reset_sent[c];
    }
    return n;
}
static void start(RegisterBus *b, unsigned c, unsigned value, uint64_t now, uint64_t cycles) {
    if (b->sink.sn_begin)
        b->sink.sn_begin(b->sink.ctx, (uint8_t)c, (uint8_t)value);
    b->busy = true;
    b->started = now;
    b->started_cycles = b->sink.sn_cycles ? b->sink.sn_cycles(b->sink.ctx) : cycles;
    b->writes++;
}
void bus_service(RegisterBus *b, uint64_t now, uint64_t cycles, bool running) {
    b->clock_cycles = cycles;
    b->wait_age_us = b->busy ? now - b->started : 0;
    unsigned pending = bus_pending(b);
    if (pending > b->max_pending)
        b->max_pending = pending;
    if (b->busy) {
        if (cycles - b->started_cycles < SN_CLOCK_TRANSFER_CYCLES)
            return;
        b->last_wait_us = now - b->started;
        if (b->last_wait_us > b->longest_wait_us)
            b->longest_wait_us = b->last_wait_us;
        if (b->sink.sn_end)
            b->sink.sn_end(b->sink.ctx);
        b->busy = false;
        b->wait_age_us = 0;
        unsigned c = b->pending_chip, n = b->pending_channel;
        if (b->pending_kind == 0 && b->stage == 0) {
            b->stage = 1;
            start(b, c, (b->snapshot >> 4) & 63u, now, cycles);
            return;
        }
        if (b->pending_kind == 0)
            b->tone_sent[c][n] = b->snapshot;
        else if (b->pending_kind == 1)
            b->volume_sent[c][n] = b->pending_value;
        else {
            b->noise_sent[c] = b->pending_value;
            b->reset_sent[c] = b->pending_reset;
        }
    }
    /* Silence is urgent; stale pitch updates can never outrank a panic. */
    for (unsigned c = 0; c < 2; c++)
        for (unsigned n = 0; n < 4; n++)
            if (running && b->volume[c][n] == 15 && b->volume_sent[c][n] != 15) {
                b->pending_chip = (uint8_t)c;
                b->pending_channel = (uint8_t)n;
                b->pending_kind = 1;
                b->pending_value = 15;
                start(b, c, 0x9fu | (n << 5), now, cycles);
                return;
            }
    if (b->force_mute) {
        for (unsigned r = 0; r < 6; r++)
            if (b->saa[r] == 0 && b->saa_sent[r] != 0) {
                if (b->sink.saa)
                    b->sink.saa(b->sink.ctx, (uint8_t)r, 0);
                b->saa_sent[r] = 0;
                b->writes++;
                return;
            }
        b->force_mute = false;
    }
    for (unsigned g = 0; g < 2; g++)
        if (b->env_reset[g] != b->env_reset_sent[g]) {
            unsigned reg = 0x18 + g;
            if (b->sink.saa)
                b->sink.saa(b->sink.ctx, (uint8_t)reg, 0);
            b->saa_sent[reg] = 0;
            b->env_reset_sent[g] = b->env_reset[g];
            b->writes++;
            return;
        }
    /* Round robin prevents continuously changing pitches from starving envelopes. */
    for (unsigned tries = 0; tries < 48; tries++) {
        unsigned id = b->cursor;
        b->cursor = (uint8_t)((b->cursor + 1) % 48);
        if (id < 32) {
            if (b->saa[id] == b->saa_sent[id])
                continue;
            if (b->sink.saa)
                b->sink.saa(b->sink.ctx, (uint8_t)id, b->saa[id]);
            b->saa_sent[id] = b->saa[id];
            b->writes++;
            return;
        }
        if (!running)
            continue;
        unsigned c = (id - 32) / 8, n = (id - 32) % 8;
        b->pending_chip = (uint8_t)c;
        b->pending_channel = (uint8_t)n;
        b->stage = 0;
        if (n < 3) {
            if (b->tone[c][n] == b->tone_sent[c][n])
                continue;
            b->pending_kind = 0;
            b->snapshot = b->tone[c][n];
            start(b, c, 0x80u | (n << 5) | (b->snapshot & 15u), now, cycles);
            return;
        }
        if (n < 7) {
            n -= 3;
            if (b->volume[c][n] == b->volume_sent[c][n])
                continue;
            b->pending_kind = 1;
            b->pending_channel = (uint8_t)n;
            b->pending_value = b->volume[c][n];
            start(b, c, 0x90u | (n << 5) | b->pending_value, now, cycles);
            return;
        }
        if (b->noise[c] == b->noise_sent[c] && b->reset[c] == b->reset_sent[c])
            continue;
        b->pending_kind = 2;
        b->pending_value = b->noise[c];
        b->pending_reset = b->reset[c];
        start(b, c, 0xe0u | b->pending_value, now, cycles);
        return;
    }
}

void bus_env_retrigger(RegisterBus *b, unsigned g) {
    if (g < 2)
        b->env_reset[g]++;
}
