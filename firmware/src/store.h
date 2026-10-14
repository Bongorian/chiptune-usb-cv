#pragma once
#include "engine.h"
#include <stddef.h>
#define STORE_BANK_BYTES 4096
#define STORE_COMMIT 0x43545031u
#define STORE_FLASH_OFFSET (2u * 1024u * 1024u - 2u * STORE_BANK_BYTES)
typedef struct {
    uint32_t commit, version, sequence, length, crc;
    Settings settings;
} StoreRecord;
uint32_t store_crc(const void *data, size_t length);
void store_record(StoreRecord *r, const Settings *s, uint32_t sequence);
bool store_valid(const StoreRecord *r);
/* settings may be NULL when validating/selecting a bank. */
bool store_load(const void *record, Settings *settings, uint32_t *sequence);
int store_select(const StoreRecord *a, const StoreRecord *b);
