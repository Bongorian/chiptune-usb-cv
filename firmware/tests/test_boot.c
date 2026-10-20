#include "store.h"
#include "ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static union {
    uint32_t align;
    uint8_t bytes[2 * STORE_BANK_BYTES];
} flash, before;
static Settings settings, loaded;
static StoreRecord record;
static Engine engine;
static Ui ui;
static unsigned saves;
static bool save(void *ctx) {
    (void)ctx;
    saves++;
    return true;
}
static int boot(bool defaults, uint32_t *sequence) {
    before = flash;
    int bank = store_restore(flash.bytes, defaults, &loaded, sequence);
    assert(settings_valid(&loaded));
    engine_init(&engine, &loaded.preset[loaded.selected], (SoundSink){0});
    ui_init(&ui, &loaded, &engine, save, NULL, NULL);
    assert(settings_valid(&loaded));
    assert(saves == 0); /* Migration must never call an erase/program callback. */
    assert(!memcmp(&before, &flash, sizeof(flash)));
    return bank;
}
int main(void) {
    uint32_t sequence = 999;
    memset(&flash, 0xff, sizeof(flash));
    assert(boot(false, &sequence) == -1 && sequence == 0 && ui.layout_reset);
    const uint32_t false_header[] = {STORE_COMMIT, SETTINGS_VERSION, 5, sizeof(Settings), 0};
    memcpy(flash.bytes + sizeof(flash.bytes) - 4096, false_header, sizeof(false_header));
    assert(boot(false, &sequence) == -1 && sequence == 0); /* No read beyond the 4KiB slot. */
    memset(&flash, 0xff, sizeof(flash));
    settings_default(&settings);
    settings_reset_layout(&settings);
    settings.cv_zero[0] = 37;
    settings.brightness = 41;
    settings.sounds[0].level = 27;
    store_record(&record, &settings, 77);
    memcpy(flash.bytes, &record, sizeof(record));
    assert(boot(false, &sequence) == 0 && sequence == 77 && !ui.layout_reset);
    assert(loaded.cv_zero[0] == 37 && loaded.sounds[0].level == 27);
    settings.sounds[0].level = 29;
    store_record(&record, &settings, 78);
    memcpy(flash.bytes + STORE_BANK_BYTES, &record, sizeof(record));
    assert(boot(false, &sequence) == 1 && sequence == 78 && loaded.sounds[0].level == 29);
    assert(boot(true, &sequence) == -1 && sequence == 0 && ui.layout_reset);
    assert(loaded.brightness == 70); /* Recovery ignores both valid saved banks. */
    assert(boot(false, &sequence) == 1 && sequence == 78 && loaded.brightness == 41);
    flash.bytes[STORE_BANK_BYTES + 100] ^= 0x40;
    assert(boot(false, &sequence) == 0 && sequence == 77);
    flash.bytes[100] ^= 0x40;
    assert(boot(false, &sequence) == -1 && sequence == 0);
    /* An intact CRC with invalid settings must also fall back, never assert. */
    settings.selected = 255;
    store_record(&record, &settings, 79);
    memcpy(flash.bytes, &record, sizeof(record));
    assert(boot(false, &sequence) == -1 && sequence == 0);
    memset(&flash, 0xff, sizeof(flash));
    FILE *f = fopen(FIXTURE_DIR "/settings-v5.bin", "rb");
    assert(f && fread(flash.bytes, 1, 17328, f) == 17328);
    fclose(f);
    assert(boot(false, &sequence) == 0 && sequence == 93 && ui.layout_reset);
    assert(loaded.cv_zero[0] == 101 && loaded.cv_zero[1] == 202);
    assert(loaded.cv_codes_per_volt[0] == 2010 && loaded.brightness == 42);
    /* Without explicit save, another reboot redoes the RAM-only migration. */
    assert(boot(false, &sequence) == 0 && sequence == 93 && ui.layout_reset);
    const unsigned sizes[] = {24, 30, 102, 262};
    const unsigned lengths[] = {216, 264, 840, 2120};
    static uint8_t legacy[2140];
    for (unsigned version = 1; version <= 4; version++) {
        settings_default(&settings);
        settings.cv_zero[0] = 19;
        unsigned length = lengths[version - 1], size = sizes[version - 1];
        memset(legacy, 0, sizeof(legacy));
        uint32_t header[] = {STORE_COMMIT, version, 40 + version, length, 0};
        memcpy(legacy, header, sizeof(header));
        memcpy(legacy + 20, &version, 4);
        for (unsigned p = 0; p < PRESETS; p++)
            memcpy(legacy + 24 + p * size, &settings.preset[p], size);
        unsigned tail = 24 + PRESETS * size;
        memcpy(legacy + tail, settings.cv_zero, 8);
        memcpy(legacy + tail + 8, settings.cv_codes_per_volt, 8);
        uint32_t crc = store_crc(legacy + 4, length + 16);
        memcpy(legacy + 16, &crc, 4);
        for (unsigned slot = 0; slot < 2; slot++) {
            memset(&flash, 0xff, sizeof(flash));
            memcpy(flash.bytes + sizeof(flash.bytes) - 8192 + slot * 4096, legacy, length + 20);
            assert(boot(false, &sequence) == -1 && sequence == 40 + version && ui.layout_reset);
            assert(loaded.cv_zero[0] == 19);
        }
    }
    puts("PASS: erased/corrupt/invalid/valid banks, read-only v5 migration, recovery and reboot");
}
