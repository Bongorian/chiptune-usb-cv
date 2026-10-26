#include "editor.h"
#include "studio.h"
#include "store.h"
#include "ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static Settings settings, loaded;
static Engine engine;
static Editor editor;
static Ui ui;
static bool save_ok;
static bool save_stub(void *ctx) {
    (void)ctx;
    return save_ok;
}
static uint8_t saa[32], sn_latch[2], sn_volume[2][4];
static uint16_t sn_period[2][3];
static void ws(void *c, uint8_t r, uint8_t v) {
    (void)c;
    saa[r] = v;
}
static void wn(void *c, uint8_t chip, uint8_t data) {
    (void)c;
    if (data & 128)
        sn_latch[chip] = (data >> 4) & 7;
    unsigned r = sn_latch[chip], n = r / 2;
    if (r & 1)
        sn_volume[chip][n] = data & 15;
    else if (n < 3) {
        if (data & 128)
            sn_period[chip][n] = (uint16_t)((sn_period[chip][n] & 0x3f0) | (data & 15));
        else
            sn_period[chip][n] = (uint16_t)((sn_period[chip][n] & 15) | ((data & 63) << 4));
    }
}
static void setup(void) {
    save_ok = false;
    settings_default(&settings);
    settings_reset_layout(&settings);
    engine_init(&engine, &settings.preset[0], (SoundSink){ws, wn, NULL});
    ui_init(&ui, &settings, &engine, NULL, NULL, NULL);
    editor_init(&editor);
}
static unsigned request(unsigned cmd, const uint8_t *d, unsigned n, bool blocked) {
    uint8_t frame[192] = {0xf0, 0x7d, 0x43, 0x54, 1, 17, (uint8_t)cmd};
    unsigned sum = 0;
    if (n)
        memcpy(frame + 7, d, n);
    for (unsigned i = 1; i < 7 + n; i++)
        sum += frame[i];
    frame[7 + n] = (uint8_t)((128 - (sum & 127)) & 127);
    frame[8 + n] = 0xf7;
    for (unsigned i = 0; i < 9 + n; i++) {
        editor_byte(&editor, frame[i], i);
        editor_byte(&editor, 0xf8, i);
    }
    editor_task(&editor, &engine, &settings, blocked, save_stub, NULL, 100);
    assert(editor.length >= 10);
    return editor.tx[7];
}
static void drain(void) {
    uint8_t packet[4];
    unsigned n = 0;
    while (editor_packet(&editor, packet)) {
        unsigned count = packet[0] == 4 ? 3 : packet[0] - 4;
        assert(count <= 3 && count > 0);
        n += count;
    }
    assert(n);
}
static void note(unsigned status, unsigned n) {
    uint8_t d[] = {(uint8_t)status, (uint8_t)n, 100};
    engine_message(&engine, 0, d, 3);
    engine_tick(&engine, 1);
}
static void only(unsigned model, unsigned poly) {
    StScene s = engine.preset.studio;
    for (unsigned p = 0; p < ST_PARTS; p++)
        s.part[p].enabled = p == 0;
    s.part[0].role = ST_ROLE_ANY;
    s.part[0].poly = (uint8_t)poly;
    studio_sound_default(&s.part[0].sound, model);
    s.part[0].sound.attack = 0;
    s.part[0].sound.sustain = 100;
    memset(s.mod, 0, sizeof(s.mod));
    engine_panic(&engine, -1);
    assert(studio_apply(&engine, &s));
}
static void test_structures(void) {
    setup();
    for (unsigned m = 0; m < ST_MODELS; m++) {
        StSound s;
        studio_sound_default(&s, m);
        assert(studio_sound_valid(&s));
    }
    for (unsigned p = 0; p < 5; p++) {
        StScene s;
        studio_profile(&s, p);
        assert(studio_valid(&s));
    }
    only(ST_SAA_ENV_NOISE, 2);
    note(0x90, 48);
    note(0x90, 52);
    assert((saa[0x15] & 0x24) == 0x24 && !saa[0x14]);
    assert(saa[0x18] & 128);
    assert(saa[0x19] & 128);
    only(ST_SAA_ENV_MIX, 2);
    note(0x90, 48);
    assert((saa[0x14] & saa[0x15]) != 0);
    only(ST_SAA_LAYER, 2);
    StSound *s = &engine.preset.studio.part[0].sound;
    s->level = 100;
    s->layer_level = 100;
    s->layer_decay = 30;
    s->layer_source = 1;
    note(0x90, 48);
    note(0x90, 52);
    assert(engine.studio.resources[0] == 0x3f && engine.studio.outputs[0] == 0x24);
    assert((saa[0x15] & 9) == 9 && saa[0] && saa[3] && saa[2] && saa[5]);
    for (unsigned i = 0; i < 300; i++)
        engine_tick(&engine, 1);
    assert(!saa[0] && !saa[3] && saa[2] && saa[5]);
    s->layer_decay = 0;
    s->macro[0] = -100;
    engine_tick(&engine, 1);
    assert(!saa[0] && !saa[3] && saa[2] && saa[5]);
    s->macro[0] = 0;
    engine_tick(&engine, 1);
    assert(saa[0] && saa[3]);
    only(ST_SN_STACK, 1);
    s = &engine.preset.studio.part[0].sound;
    s->layer_interval = 12;
    note(0x90, 60);
    assert(studio_note_count(&engine, 0) == 1 && engine_active(&engine) == 4);
    assert(sn_period[0][0] > sn_period[0][1] && sn_volume[0][0] < 15 && sn_volume[1][1] < 15);
    note(0x90, 64);
    assert(engine_active_notes(&engine) == 1);
    note(0x80, 60);
    assert(studio_note_count(&engine, 0) == 1);
    note(0x80, 64);
    for (unsigned i = 0; i < 1000; i++)
        engine_tick(&engine, 1);
    assert(!engine_active(&engine));
    StScene next = engine.preset.studio;
    next.part[0].sound.stack_size = 3;
    next.part[3].enabled = 1;
    studio_sound_default(&next.part[3].sound, ST_SN_NOISE);
    next.part[3].role = ST_ROLE_SN_NOISE;
    next.part[3].sound.noise_rate = 3;
    assert(!studio_valid(&next));
}
static void test_protocol(void) {
    setup();
    assert(!request(0, NULL, 0, false));
    assert(editor.tx[9] == 9);
    drain();
    uint8_t p = 0;
    assert(!request(1, &p, 1, false));
    uint8_t data[160];
    unsigned n = editor.length - 10 - 4; /* omit appended two resource masks */
    memcpy(data, editor.tx + 8, n);
    drain();
    assert(n == 14 + 2 * editor_field_count());
    data[5] = 'T';
    data[6] = 'E';
    data[7] = 'S';
    data[8] = 'T';
    assert(!request(2, data, n, false));
    assert(engine.preset.studio.part[0].name[0] == 'T');
    drain();
    StScene before = engine.preset.studio;
    data[13] = 0;
    assert(request(2, data, n, false) == 2);
    drain();
    assert(!memcmp(&before, &engine.preset.studio, sizeof(before)));
    assert(request(2, data, n, true) == 3);
    drain();
    assert(request(3, (uint8_t[]){0, 0}, 2, false) == 4);
    drain();
    assert(!memcmp(settings.preset[0].studio.part[0].name, "SAA TONE", 8));
    save_ok = true;
    assert(!request(3, (uint8_t[]){0, 0}, 2, false));
    drain();
    assert(settings.preset[0].studio.part[0].name[0] == 'T' && settings.sound_name[0][0] == 'T');
    save_ok = false;
    /* Recognized but corrupt requests get a negative acknowledgement without mutation. */
    editor.rx[5] ^= 1;
    editor.ready = true;
    editor_task(&editor, &engine, &settings, false, save_stub, NULL, 100);
    assert(editor.tx[7] == 1);
    drain();
    assert(!request(8, &p, 1, false));
    n = editor.length - 10;
    memcpy(data, editor.tx + 8, n);
    drain();
    assert(n == 88);
    /* Negative and positive modulation endpoints must survive 7-bit SysEx packing. */
    data[56] = 28;
    data[57] = 63;
    data[58] = 100;
    data[59] = 64;
    assert(!request(9, data, n, false));
    drain();
    assert(engine.preset.studio.part[0].motion[0] == -100 &&
           engine.preset.studio.part[0].motion[1] == 100);
    assert(!request(10, &p, 1, false));
    n = editor.length - 10;
    memcpy(data, editor.tx + 8, n);
    drain();
    assert(n == 14);
    data[12] = 28;
    data[13] = 63;
    assert(!request(11, data, n, false));
    drain();
    assert(engine.preset.studio.cv[0].amount == -100);
    uint8_t both[28];
    assert(!request(10, &p, 1, false));
    memcpy(both, editor.tx + 8, 14);
    drain();
    assert(!request(10, (uint8_t[]){1}, 1, false));
    memcpy(both + 14, editor.tx + 8, 14);
    drain();
    both[1] = both[15] = 1;
    both[2] = 0;
    both[16] = 1;
    assert(!request(11, both, 28, false));
    drain();
    both[2] = 1;
    both[16] = 0;
    assert(request(11, both, 14, false) == 2);
    drain(); /* Cannot swap half a CV route. */
    assert(!request(11, both, 28, false));
    drain();
    assert(engine.preset.studio.cv[0].part == 1 && engine.preset.studio.cv[1].part == 0);
    for (unsigned f = 0; f < ST_FACTORIES; f++) {
        uint8_t index = (uint8_t)f;
        assert(!request(12, &index, 1, false));
        drain();
    }
    /* Overflow, checksum corruption, interrupted and timed-out frames never mutate a sound. */
    before = engine.preset.studio;
    editor_byte(&editor, 0xf0, 0);
    for (unsigned i = 0; i < 300; i++)
        editor_byte(&editor, 1, i);
    editor_byte(&editor, 0xf7, 301);
    editor_task(&editor, &engine, &settings, false, NULL, NULL, 301);
    assert(!editor.length);
    editor_byte(&editor, 0xf0, 0);
    editor_byte(&editor, 0x7d, 3001);
    editor_byte(&editor, 0xf7, 3002);
    editor_task(&editor, &engine, &settings, false, NULL, NULL, 3002);
    assert(!editor.length);
    assert(!memcmp(&before, &engine.preset.studio, sizeof(before)));
}
static void test_legacy5(void) {
    static uint8_t raw[17328];
    FILE *f = fopen(FIXTURE_DIR "/settings-v5.bin", "rb");
    assert(f);
    assert(fread(raw, 1, sizeof(raw), f) == sizeof(raw));
    fclose(f);
    uint32_t seq = 0;
    assert(store_load(raw, &loaded, &seq) && seq == 93);
    assert(loaded.cv_zero[0] == 101 && loaded.cv_zero[1] == 202 && loaded.brightness == 42);
    engine_init(&engine, &loaded.preset[0], (SoundSink){0});
    ui_init(&ui, &loaded, &engine, NULL, NULL, NULL);
    assert(ui.layout_reset && loaded.layout_revision == LAYOUT_REVISION &&
           loaded.cv_codes_per_volt[1] == 2020);
    raw[100] ^= 1;
    assert(!store_load(raw, &loaded, &seq));
}
int main(void) {
    test_structures();
    test_protocol();
    test_legacy5();
    puts("PASS: structures, reservation, transients, stack, SysEx roundtrip, signed endpoints, "
         "rejection, v5 migration");
}
