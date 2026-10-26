#pragma once
#include "model.h"
#include <stddef.h>
#define STORE_BANK_BYTES 32768u
#define STORE_PHYSICAL_BYTES (2u * 1024u * 1024u)
#define STORE_FLASH_OFFSET (STORE_PHYSICAL_BYTES - 2u * STORE_BANK_BYTES)
#define STORE_COMMIT 0x43545031u
typedef struct {
    uint32_t commit, version, sequence, length, crc, reserved;
    Settings settings;
} StoreRecord;
uint32_t store_crc(const void *data, size_t length);
void store_record(StoreRecord *record, const Settings *s, uint32_t sequence);
bool store_valid(const StoreRecord *r);
bool store_load(const void *raw, Settings *s, uint32_t *sequence);
int store_select(const StoreRecord *a, const StoreRecord *b);
int store_restore(const uint8_t *area, bool defaults, Settings *s, uint32_t *sequence);
