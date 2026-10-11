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
/* v0.1 records have 24-byte presets without the SN clock controls. */
typedef struct {
    uint8_t mix[4];
    Route route[SOURCES];
    CvConfig cv[2];
    uint8_t reserve_cv, arp_mode, arp_rate, clock_source;
} LegacyPreset;
typedef struct {
    uint32_t version;
    LegacyPreset preset[PRESETS];
    float cv_zero[2], cv_codes_per_volt[2];
    uint8_t selected, padding[3];
} LegacySettings;
typedef struct {
    uint32_t commit, version, sequence, length, crc;
    LegacySettings settings;
} LegacyRecord;
_Static_assert(sizeof(LegacySettings) == 216, "v1 settings layout must stay fixed");
_Static_assert(sizeof(LegacyRecord) == 236, "v1 record layout must stay fixed");
bool store_load(const void *raw, Settings *settings, uint32_t *sequence) {
    const StoreRecord *current = raw;
    if (store_valid(current)) {
        *settings = current->settings;
        *sequence = current->sequence;
        return true;
    }
    const LegacyRecord *old = raw;
    if (old->commit != STORE_COMMIT || old->version != 1 || old->length != sizeof(LegacySettings) ||
        old->settings.version != 1)
        return false;
    LegacyRecord copy = *old;
    copy.crc = 0;
    if (old->crc != store_crc(&copy.version, sizeof(copy) - offsetof(LegacyRecord, version)))
        return false;
    Settings migrated;
    memset(&migrated, 0, sizeof(migrated));
    migrated.version = SETTINGS_VERSION;
    migrated.selected = old->settings.selected;
    memcpy(migrated.cv_zero, old->settings.cv_zero, sizeof(migrated.cv_zero));
    memcpy(migrated.cv_codes_per_volt, old->settings.cv_codes_per_volt,
           sizeof(migrated.cv_codes_per_volt));
    for (unsigned i = 0; i < PRESETS; i++) {
        const LegacyPreset *p = &old->settings.preset[i];
        Preset *n = &migrated.preset[i];
        memcpy(n->mix, p->mix, sizeof(n->mix));
        memcpy(n->route, p->route, sizeof(n->route));
        memcpy(n->cv, p->cv, sizeof(n->cv));
        n->reserve_cv = p->reserve_cv;
        n->arp_mode = p->arp_mode;
        n->arp_rate = p->arp_rate;
        n->clock_source = p->clock_source;
        n->sn_clock = (SnClockConfig){SN_CLOCK_OFF, 0, 24, 4, 0, 1};
    }
    if (!settings_valid(&migrated))
        return false;
    *settings = migrated;
    *sequence = old->sequence;
    return true;
}
int store_select(const StoreRecord *a, const StoreRecord *b) {
    Settings settings;
    uint32_t as = 0, bs = 0;
    bool av = store_load(a, &settings, &as), bv = store_load(b, &settings, &bs);
    if (!av)
        return bv ? 1 : -1;
    if (!bv)
        return 0;
    return (int32_t)(bs - as) > 0 ? 1 : 0;
}
