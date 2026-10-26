#include "test_common.h"
#include "store.h"
static union {
    uint32_t align;
    uint8_t bytes[STORE_BANK_BYTES * 2];
} flash, before;
static StoreRecord a, b;
static Settings loaded, snapshot;
static bool success;
static unsigned commits;
static bool commit(void *ctx) {
    (void)ctx;
    commits++;
    return success;
}
int main(void) {
    setup();
    settings.project[1].track[0].level = 23;
    settings.system.brightness = 44;
    engine.system.brightness = 91;
    engine.system.cv_zero[0] = 10;
    snapshot = settings;
    engine.project.track[0].patch.attack = 777;
    engine.project.track[0].level = 34;
    success = true;
    assert(settings_save(&settings, &engine, SAVE_PATCH, 0, 5, commit, NULL));
    assert(settings.patches[5].attack == 777);
    assert(!memcmp(settings.project, snapshot.project, sizeof(settings.project)) &&
           !memcmp(&settings.system, &snapshot.system, sizeof(System)));
    assert(settings_save(&settings, &engine, SAVE_TRACK, 0, 2, commit, NULL));
    assert(settings.tracks[2].level == 34);
    assert(!memcmp(settings.project, snapshot.project, sizeof(settings.project)));
    assert(settings_save(&settings, &engine, SAVE_PROJECT, 0, 3, commit, NULL));
    assert(settings.project[3].track[0].level == 34 && settings.selected == 3);
    assert(!memcmp(&settings.system, &snapshot.system, sizeof(System)));
    assert(settings_save(&settings, &engine, SAVE_SYSTEM, 0, 0, commit, NULL));
    assert(settings.system.brightness == 91 && settings.system.cv_zero[0] == 10);
    snapshot = settings;
    engine.system.brightness = 33;
    success = false;
    assert(!settings_save(&settings, &engine, SAVE_SYSTEM, 0, 0, commit, NULL));
    assert(settings.system.brightness == 91 && engine.system.brightness == 33);
    engine.project.track[0].patch.attack = 111;
    assert(!settings_save(&settings, &engine, SAVE_PATCH, 0, 5, commit, NULL));
    assert(!memcmp(&snapshot, &settings, sizeof(settings)));
    store_record(&a, &settings, UINT32_MAX);
    assert(store_valid(&a));
    store_record(&b, &settings, 0);
    assert(store_select(&a, &b) == 1);
    memset(&flash, 255, sizeof(flash));
    uint32_t sequence;
    before = flash;
    assert(store_restore(flash.bytes, false, &loaded, &sequence) == -1);
    assert(!memcmp(&before, &flash, sizeof(flash)) && sequence == 0);
    memcpy(flash.bytes, &a, sizeof(a));
    assert(store_restore(flash.bytes, false, &loaded, &sequence) == 0 && sequence == UINT32_MAX);
    /* Power interruption at every programmed page, before final commit marker. */
    for (unsigned bytes = 0; bytes < sizeof(b); bytes += 256) {
        memset(flash.bytes + STORE_BANK_BYTES, 255, STORE_BANK_BYTES);
        memcpy(flash.bytes + STORE_BANK_BYTES, &b, bytes);
        memset(flash.bytes + STORE_BANK_BYTES, 255, 4);
        assert(store_restore(flash.bytes, false, &loaded, &sequence) == 0);
    }
    memcpy(flash.bytes + STORE_BANK_BYTES, &b, sizeof(b));
    assert(store_restore(flash.bytes, false, &loaded, &sequence) == 1 && sequence == 0);
    assert(store_restore(flash.bytes, true, &loaded, &sequence) == 1 && sequence == 0);
    assert(loaded.project[3].track[0].level == 100); /* Defaults in RAM, bank retained. */
    flash.bytes[STORE_BANK_BYTES + 200] ^= 1;
    assert(store_restore(flash.bytes, false, &loaded, &sequence) == 0);
    ((StoreRecord *)flash.bytes)->version = 6;
    before = flash;
    assert(store_restore(flash.bytes, false, &loaded, &sequence) == -1 && sequence == 0);
    assert(!memcmp(&before, &flash, sizeof(flash)));
    assert(store_restore(flash.bytes, true, &loaded, &sequence) == -1 && settings_valid(&loaded));
    engine.system.brightness = 47;
    engine.system.cv_zero[0] = 17;
    message(0, 0x90, 60, 100);
    assert(engine_active(&engine));
    engine_reset_audio(&engine, (SoundSink){0});
    assert(!engine_active(&engine) && engine.library == &settings);
    assert(engine.system.brightness == 47 && engine.system.cv_zero[0] == 17);
    assert(engine.project.track[0].level == 34);
    assert(commits == 6);
    puts("PASS storage: independent saves, rollback, every torn page, CRC, sequence wrap, "
         "unknown-version read-only boot");
}
