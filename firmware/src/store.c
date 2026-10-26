#include "store.h"
#include <string.h>
_Static_assert(sizeof(StoreRecord) < STORE_BANK_BYTES, "Snapshot must fit either settings bank");
uint32_t store_crc(const void *data, size_t length) {
    const uint8_t *p = data;
    uint32_t crc = 0xffffffffu;
    while (length--) {
        crc ^= *p++;
        for (unsigned bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ ((0u - (crc & 1)) & 0xedb88320u);
    }
    return ~crc;
}
static uint32_t record_crc(const StoreRecord *r) {
    const uint8_t *p = (const uint8_t *)r;
    uint32_t crc = 0xffffffffu;
    for (size_t i = 4; i < sizeof(*r); i++) {
        crc ^= i >= offsetof(StoreRecord, crc) && i < offsetof(StoreRecord, crc) + 4 ? 0 : p[i];
        for (unsigned bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ ((0u - (crc & 1)) & 0xedb88320u);
    }
    return ~crc;
}
void store_record(StoreRecord *r, const Settings *s, uint32_t sequence) {
    memset(r, 0, sizeof(*r));
    r->commit = STORE_COMMIT;
    r->version = SETTINGS_VERSION;
    r->sequence = sequence;
    r->length = sizeof(Settings);
    r->settings = *s;
    r->crc = record_crc(r);
}
bool store_valid(const StoreRecord *r) {
    return r->commit == STORE_COMMIT && r->version == SETTINGS_VERSION &&
           r->length == sizeof(Settings) && r->crc == record_crc(r) && settings_valid(&r->settings);
}
bool store_load(const void *raw, Settings *s, uint32_t *sequence) {
    const StoreRecord *r = raw;
    if (!store_valid(r))
        return false;
    if (s)
        *s = r->settings;
    if (sequence)
        *sequence = r->sequence;
    return true;
}
int store_select(const StoreRecord *a, const StoreRecord *b) {
    bool av = store_valid(a), bv = store_valid(b);
    return !av ? (bv ? 1 : -1) : !bv ? 0 : (int32_t)(b->sequence - a->sequence) > 0 ? 1 : 0;
}
int store_restore(const uint8_t *area, bool defaults, Settings *s, uint32_t *sequence) {
    *sequence = 0;
    const StoreRecord *a = (const StoreRecord *)area,
                      *b = (const StoreRecord *)(area + STORE_BANK_BYTES);
    int bank = store_select(a, b);
    if (bank >= 0) {
        const StoreRecord *record = bank ? b : a;
        *sequence = record->sequence;
        if (defaults)
            settings_default(s);
        else
            *s = record->settings;
        /* Even a defaults-only boot remembers the last committed bank so an
           explicit save cannot erase the only recoverable snapshot first. */
        return bank;
    }
    settings_default(s);
    return -1; /* Old versions are ignored without erase or migration. */
}
