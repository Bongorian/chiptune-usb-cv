#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef void (*MidiCallback)(void *, const uint8_t *, uint8_t, uint8_t);
typedef struct {
    uint8_t running, status, needed, count, data[3], sysex_count, sysex_data[3];
    bool sysex, sensing;
    uint32_t last_byte, errors;
    MidiCallback callback;
    void *ctx;
} MidiParser;
void midi_init(MidiParser *p, MidiCallback cb, void *ctx);
void midi_byte(MidiParser *p, uint8_t b, uint32_t now);
bool midi_sensing_expired(MidiParser *p, uint32_t now);
uint8_t midi_packet_length(uint8_t cin);
