#include "store.h"
#include <string.h>
_Static_assert(sizeof(StoreRecord) < STORE_BANK_BYTES, "Settings must fit a flash sector");
uint32_t store_crc(const void *data, size_t length) {
    const uint8_t *p = data;
    uint32_t crc = 0xffffffffu;
    while (length--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++)
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
    r->crc = store_crc(
        &r->version,
        sizeof(*r) - offsetof(StoreRecord, version)); /* CRC field is zero during calculation */
}
bool store_valid(const StoreRecord *r) {
    if (r->commit != STORE_COMMIT || r->version != SETTINGS_VERSION ||
        r->length != sizeof(Settings) || !settings_valid(&r->settings))
        return false;
    StoreRecord copy = *r;
    copy.crc = 0;
    return r->crc == store_crc(&copy.version, sizeof(copy) - offsetof(StoreRecord, version));
}
int store_select(const StoreRecord *a, const StoreRecord *b) {
    bool av = store_valid(a), bv = store_valid(b);
    if (!av)
        return bv ? 1 : -1;
    if (!bv)
        return 0;
    return (int32_t)(b->sequence - a->sequence) > 0 ? 1 : 0;
}
