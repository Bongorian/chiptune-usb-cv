#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    void (*saa)(void *, uint8_t, uint8_t);
    void (*sn_begin)(void *, uint8_t, uint8_t);
    void (*sn_end)(void *);
    void *ctx;
} SoundSink;
typedef struct {
    SoundSink sink;
    uint8_t saa[32], saa_sent[32], volume[2][4], volume_sent[2][4], noise[2], noise_sent[2];
    uint16_t tone[2][3], tone_sent[2][3];
    uint32_t env_reset[2], env_reset_sent[2], reset[2], reset_sent[2], started, pending_wait,
        deadline, writes, coalesced, max_pending;
    uint16_t snapshot;
    uint8_t cursor, pending_chip, pending_kind, pending_channel, pending_value, stage;
    uint32_t pending_reset;
    bool busy, force_mute;
} RegisterBus;
void bus_init(RegisterBus *b, SoundSink sink);
void bus_saa(RegisterBus *b, unsigned reg, unsigned value);
void bus_tone(RegisterBus *b, unsigned chip, unsigned channel, unsigned divider);
void bus_volume(RegisterBus *b, unsigned chip, unsigned channel, unsigned attenuation);
void bus_noise(RegisterBus *b, unsigned chip, unsigned value, bool retrigger);
void bus_mute(RegisterBus *b);
/* At most one hardware transaction per call; WR-low time runs asynchronously. */
void bus_service(RegisterBus *b, uint32_t now_us, unsigned sn_wait_us);
unsigned bus_pending(const RegisterBus *b);
void bus_env_retrigger(RegisterBus *b, unsigned group);
