#pragma once
#include "engine.h"
#include <stddef.h>
#define EDITOR_BYTES 192u
typedef struct {
    uint8_t rx[EDITOR_BYTES], tx[EDITOR_BYTES];
    unsigned count, length, sent;
    uint32_t last_byte;
    bool receiving, ready, changed, negotiated;
} Editor;
void editor_init(Editor *e);
void editor_byte(Editor *e, uint8_t byte, uint32_t now);
void editor_task(Editor *ed, Engine *e, Settings *s, bool blocked, bool (*save)(void *), void *ctx,
                 uint32_t now);
bool editor_packet(Editor *e, uint8_t packet[4]);
unsigned editor_field_count(void);
int editor_field_get(const Patch *p, unsigned field);
bool editor_field_set(Patch *p, unsigned field, int value);
