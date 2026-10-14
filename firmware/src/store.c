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
/* Validate the CRC without copying the enlarged record onto the MCU stack. */
static uint32_t record_crc(const StoreRecord *r) {
    const uint8_t *bytes = (const uint8_t *)r;
    uint32_t crc = 0xffffffffu;
    for (unsigned i = offsetof(StoreRecord, version); i < sizeof(*r); i++) {
        uint8_t value =
            i >= offsetof(StoreRecord, crc) && i < offsetof(StoreRecord, crc) + 4 ? 0 : bytes[i];
        crc ^= value;
        for (unsigned bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ ((0u - (crc & 1)) & 0xedb88320u);
    }
    return ~crc;
}
bool store_valid(const StoreRecord *r) {
    if (r->commit != STORE_COMMIT || r->version != SETTINGS_VERSION ||
        r->length != sizeof(Settings) || !settings_valid(&r->settings))
        return false;
    return r->crc == record_crc(r);
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
typedef struct {
    LegacyPreset old;
    SnClockConfig clock;
} LegacyPreset2;
typedef struct {
    uint32_t version;
    LegacyPreset2 preset[PRESETS];
    float cv_zero[2], cv_codes_per_volt[2];
    uint8_t selected, padding[3];
} LegacySettings2;
typedef struct {
    uint32_t commit, version, sequence, length, crc;
    LegacySettings2 settings;
} LegacyRecord2;
_Static_assert(sizeof(LegacySettings2) == 264, "v2 settings layout must stay fixed");
_Static_assert(sizeof(LegacyRecord2) == 284, "v2 record layout must stay fixed");
/* v0.3 has 102-byte presets. This is deliberately independent of today's Preset size. */
typedef struct {
    uint8_t bytes[102];
} LegacyPreset3;
typedef struct {
    uint32_t version;
    LegacyPreset3 preset[PRESETS];
    float cv_zero[2], cv_codes_per_volt[2];
    uint8_t selected, padding[3];
} LegacySettings3;
typedef struct {
    uint32_t commit, version, sequence, length, crc;
    LegacySettings3 settings;
} LegacyRecord3;
_Static_assert(sizeof(LegacySettings3) == 840, "v3 settings layout must stay fixed");
_Static_assert(sizeof(LegacyRecord3) == 860, "v3 record layout must stay fixed");
_Static_assert(offsetof(Preset, performance) == 102, "legacy prefix must remain intact");
bool store_load(const void *raw, Settings *settings, uint32_t *sequence) {
    const StoreRecord *current = raw;
    if (store_valid(current)) {
        if (settings)
            *settings = current->settings;
        *sequence = current->sequence;
        return true;
    }
    uint32_t header[5];
    memcpy(header, raw, sizeof(header));
    if (header[0] != STORE_COMMIT || (header[1] != 1 && header[1] != 2 && header[1] != 3))
        return false;
    unsigned version = header[1];
    unsigned length = version == 1   ? sizeof(LegacySettings)
                      : version == 2 ? sizeof(LegacySettings2)
                                     : sizeof(LegacySettings3);
    if (header[3] != length)
        return false;
    uint8_t copy[sizeof(LegacyRecord3)] = {0};
    memcpy(copy, raw, length + sizeof(header));
    memset(copy + 16, 0, 4);
    if (header[4] != store_crc(copy + 4, length + sizeof(header) - 4))
        return false;
    uint32_t stored_version;
    memcpy(&stored_version, copy + 20, 4);
    if (stored_version != version)
        return false;
    /* Called only from the single main thread; keep the expanded image off the 4KiB stack. */
    static Settings migrated;
    memset(&migrated, 0, sizeof(migrated));
    migrated.version = SETTINGS_VERSION;
    unsigned preset_size = version == 1   ? sizeof(LegacyPreset)
                           : version == 2 ? sizeof(LegacyPreset2)
                                          : sizeof(LegacyPreset3);
    for (unsigned i = 0; i < PRESETS; i++) {
        LegacyPreset old;
        memcpy(&old, copy + 24 + i * preset_size, sizeof(old));
        Preset *n = &migrated.preset[i];
        preset_performance_default(n);
        if (version == 3) {
            memcpy(n, copy + 24 + i * preset_size, sizeof(LegacyPreset3));
            continue;
        }
        memcpy(n->mix, old.mix, sizeof(n->mix));
        memcpy(n->route, old.route, sizeof(n->route));
        memcpy(n->cv, old.cv, sizeof(n->cv));
        n->reserve_cv = old.reserve_cv;
        n->arp_mode = old.arp_mode;
        n->arp_rate = old.arp_rate;
        n->clock_source = old.clock_source;
        n->sn_clock = (SnClockConfig){SN_CLOCK_OFF, 0, 24, 4, 0, 1};
        if (version == 2)
            memcpy(&n->sn_clock, copy + 24 + i * preset_size + sizeof(old), sizeof(n->sn_clock));
        /* Preserve the old 4MHz reference, mono CV assignments and whole-input routing. */
        for (unsigned s = 0; s < SOURCES; s++)
            for (unsigned c = 0; c < 16; c++)
                n->part[s][c] = (MidiPart){old.route[s].target, old.route[s].program};
    }
    unsigned tail = 24 + PRESETS * preset_size;
    memcpy(migrated.cv_zero, copy + tail, sizeof(migrated.cv_zero));
    memcpy(migrated.cv_codes_per_volt, copy + tail + 8, sizeof(migrated.cv_codes_per_volt));
    migrated.selected = copy[tail + 16];
    if (!settings_valid(&migrated))
        return false;
    if (settings)
        *settings = migrated;
    *sequence = header[2];
    return true;
}
int store_select(const StoreRecord *a, const StoreRecord *b) {
    uint32_t as = 0, bs = 0;
    bool av = store_load(a, NULL, &as), bv = store_load(b, NULL, &bs);
    if (!av)
        return bv ? 1 : -1;
    if (!bv)
        return 0;
    return (int32_t)(bs - as) > 0 ? 1 : 0;
}
