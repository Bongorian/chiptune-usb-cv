#include "test_common.h"
static void direct(void) {
    setup();
    for (unsigned f = 0; f < FACTORY_PATCHES; f++) {
        Patch p;
        char name[9];
        patch_factory(f, &p, name);
        assert(patch_valid(&p));
    }
    for (unsigned s = 0; s < 2; s++) {
        engine_panic(&engine, -1);
        for (unsigned t = 0; t < 10; t++)
            message(s, 0x90 | t, 45, 127);
        assert(engine_active(&engine) == 10);
        for (unsigned t = 0; t < 10; t++)
            assert(engine_track_notes(&engine, t) == 1);
        assert(engine.bus.tone[0][0] == 568 && engine.bus.tone[1][0] == 568);
        assert(engine.bus.volume[0][0] == engine.bus.volume[1][0]);
        message(s, 0x96, 69, 127);
        advance(10);
        unsigned elapsed = 0;
        for (unsigned i = 0; i < 14; i++)
            if (engine.voice[i].active && engine.voice[i].track == 6)
                elapsed = engine.voice[i].elapsed;
        message(s, 0x86, 45, 0);
        for (unsigned i = 0; i < 14; i++)
            if (engine.voice[i].active && engine.voice[i].track == 6)
                assert(engine.voice[i].elapsed > elapsed);
        assert(engine_track_notes(&engine, 6) == 1);
        assert(engine.bus.tone[0][0] == 142 && engine.bus.tone[1][0] == 142);
        message(s, 0xb6, 10, 0);
        assert(engine.bus.volume[1][0] == 15 && engine.bus.volume[0][0] < 15);
        message(s, 0xb6, 10, 127);
        assert(engine.bus.volume[0][0] == 15 && engine.bus.volume[1][0] < 15);
        message(s, 0x80, 45, 0);
        advance(200);
        assert(engine_track_notes(&engine, 0) == 0 && engine_track_notes(&engine, 1) == 1);
    }
}
static void ownership(void) {
    setup();
    isolated(20, PLAY_POLY, 3);
    message(0, 0x90, 60, 100);
    message(1, 0x90, 60, 100);
    assert(engine_active(&engine) == 2);
    message(0, 0xb0, 64, 127);
    message(0, 0x80, 60, 0);
    assert(engine_active(&engine) == 2);
    message(1, 0x80, 60, 0);
    assert(engine_active(&engine) == 1);
    message(0, 0xb0, 64, 0);
    assert(!engine_active(&engine));
    message(0, 0x90, 60, 100);
    message(0, 0x90, 60, 100);
    message(0, 0x80, 60, 0);
    assert(engine_active(&engine) == 1);
    message(0, 0x80, 60, 0);
    assert(!engine_active(&engine));
    for (unsigned n = 60; n < 65; n++)
        message(0, 0x90, n, 100);
    assert(engine_active(&engine) == 3 && engine.steals >= 2);
    engine_panic(&engine, 1);
    assert(engine_active(&engine) == 3);
    engine_panic(&engine, 0);
    assert(!engine_active(&engine));
    isolated(20, PLAY_DIRECT, 1);
    message(0, 0x90, 48, 100);
    message(0, 0x90, 55, 100);
    message(0, 0x80, 55, 0);
    assert(engine_active(&engine) == 1);
    for (unsigned i = 0; i < 14; i++)
        if (engine.voice[i].active)
            assert(engine.voice[i].note == 48);
    message(0, 0xb0, 120, 0);
    assert(!engine_active(&engine));
    message(0, 0x80, 48, 0);
    assert(!engine_active(&engine));
}
static void mono_sustain_restore(void) {
    setup();
    isolated(20, PLAY_LEGATO, 1);
    message(0, 0x90, 48, 100);
    advance(10);
    message(0, 0x90, 55, 100);
    message(0, 0xb0, 64, 127);
    message(0, 0x80, 55, 0);
    advance(10);
    message(0, 0xb0, 64, 0);
    assert(engine_active(&engine) == 1);
    for (unsigned v = 0; v < 14; v++)
        if (engine.voice[v].active) {
            assert(engine.voice[v].note == 48);
            assert(engine.voice[v].elapsed > 10); /* Legato return keeps envelope. */
        }
    message(0, 0x80, 48, 0);
    assert(!engine_active(&engine));
}
static void constraints(void) {
    setup();
    message(0, 0x96, 60, 100);
    assert(engine_poly_focus(&engine, 0));
    assert(engine.plan.count[0] == 6 && engine.project.track[0].mode == PLAY_POLY);
    assert(engine_track_notes(&engine, 6) == 1);

    setup();
    message(0, 0x90, 60, 100);
    message(0, 0x96, 60, 100);
    Project before = engine.project;
    ResourcePlan plan = engine.plan;
    Patch p;
    char name[9];
    patch_factory(18, &p, name);
    assert(!engine_load_patch(&engine, 0, &p, name));
    assert(!memcmp(&before, &engine.project, sizeof(before)) &&
           !memcmp(&plan, &engine.plan, sizeof(plan)));
    assert(engine_active(&engine) == 2);
    assert(engine.conflict.reason && engine.conflict.suggestion[0]);
    Project next = engine.project;
    next.track[0].enabled = next.track[1].enabled = 0;
    assert(engine_apply(&engine, &next));
    assert(engine_load_patch(&engine, 2, &p, name));
    assert(engine.plan.resources[2][0] == 7 || engine.plan.resources[2][0] == 56);
    setup();
    isolated(20, PLAY_DIRECT, 1);
    next = engine.project;
    next.track[0].patch.source = SOURCE_MIX;
    next.track[0].patch.noise_rate = 3;
    next.track[0].patch.noise_pitch = 12;
    assert(engine_apply(&engine, &next));
    assert(!(engine.plan.outputs[0][0] & 9)); /* Carrier cannot be its noise clock. */
    assert(engine.plan.resources[0][0] != engine.plan.outputs[0][0]);
    setup();
    p = engine.project.track[9].patch;
    p.noise_rate = 3;
    assert(!engine_load_patch(&engine, 9, &p, "TRACKED "));
    next = engine.project;
    next.track[8].enabled = 0;
    assert(engine_apply(&engine, &next));
    assert(engine_load_patch(&engine, 9, &p, "TRACKED "));
}
static void sound_features(void) {
    setup();
    isolated(18, PLAY_DIRECT, 1);
    message(0, 0x90, 45, 127);
    assert(engine.bus.saa[0x18] & 0x80);
    assert(engine.bus.saa[0] && engine.bus.saa[2]);
    assert((engine.bus.saa[0x15] & 1) != 0);
    advance(500);
    assert(!engine.bus.saa[0] && engine.bus.saa[2]);
    setup();
    isolated(17, PLAY_DIRECT, 1);
    message(0, 0x90, 60, 127);
    assert(engine.bus.saa[0x14] & engine.bus.saa[0x15]);
    setup();
    isolated(19, PLAY_DIRECT, 1);
    message(0, 0x90, 60, 127);
    assert(engine.bus.tone[0][0] > engine.bus.tone[0][1]);
    assert(engine_active(&engine) == 1);
    setup();
    isolated(11, PLAY_DIRECT, 1);
    message(0, 0x90, 45, 127);
    assert(engine.bus.volume[0][2] < 15 && engine.bus.volume[0][3] < 15);
    Project p = engine.project;
    p.track[0].patch.noise_sweep = 24;
    p.track[0].patch.pitch_decay = 200;
    assert(engine_apply(&engine, &p));
    message(0, 0x90, 45, 127);
    unsigned n = engine.bus.tone[0][2];
    advance(100);
    assert(engine.bus.tone[0][2] != n);
    settings.patches[0] = engine.project.track[0].patch;
    settings.patches[0].level = 10;
    p = engine.project;
    p.track[0].morph_target = 1;
    p.track[0].morph = 100;
    assert(engine_apply(&engine, &p));
    engine_tick(&engine, 1);
    assert(engine.effective[0].level == 10);
    p.track[0].patch.chip = CHIP_SAA;
    assert(!engine_apply(&engine, &p));
}
static void pressure(void) {
    setup();
    isolated(20, PLAY_POLY, 2);
    engine.project.mod[0] =
        (ModRoute){.source = 4, .track = 0, .parameter = MOD_VIBRATO, .amount = 100};
    message(0, 0x90, 60, 100);
    assert(engine.effective[0].vibrato == 0);
    message(0, 0xa0, 60, 127);
    assert(engine.effective[0].vibrato == 100);
    message(0, 0x80, 60, 0);
    advance(1);
    assert(engine.effective[0].vibrato == 0);
    message(1, 0xd0, 127, 0);
    assert(engine.effective[0].vibrato == 100);
}
static void waveforms(void) {
    for (unsigned chip = 0; chip < 2; chip++)
        for (unsigned wave = 0; wave < WAVE_COUNT; wave++) {
            Patch p;
            patch_default(&p, chip);
            Patch before = p;
            if (!patch_wave_available(chip, wave)) {
                assert(!patch_set_wave(&p, wave));
                assert(!memcmp(&p, &before, sizeof(p)));
            } else {
                assert(patch_set_wave(&p, wave));
                assert(patch_valid(&p) && patch_wave(&p) == wave);
                assert(p.attack == before.attack && p.release == before.release);
            }
        }
    setup();
    isolated(22, PLAY_DIRECT, 1);
    assert(!(patch_features(&engine.project.track[0].patch) &
             (PATCH_F_PITCH | PATCH_F_DETUNE | PATCH_F_ENV)));
    Project p = engine.project;
    assert(patch_set_wave(&p.track[0].patch, WAVE_PULSE));
    assert(engine_apply(&engine, &p));
    assert(engine.plan.resources[0][0] == 0x3900 && engine.plan.outputs[0][0] == 0x3000);
    message(0, 0x90, 45, 127);
    for (unsigned side = 0; side < 2; side++) {
        assert(engine.bus.noise[side] == 3); /* Periodic + Tone3, not white. */
        assert(engine.bus.tone[side][2] == 38);
        assert(engine.bus.volume[side][2] == 15); /* Clock is inaudible. */
        assert(engine.bus.volume[side][3] < 15);
        float hz = 2000000.0f / (32 * engine.bus.tone[side][2] * 15);
        assert(fabsf(hz - 110) < .4f);
    }
    setup();
    message(0, 0x96, 45, 127);
    p = engine.project;
    assert(patch_set_wave(&p.track[6].patch, WAVE_PULSE));
    ResourcePlan plan = engine.plan;
    assert(!engine_apply(&engine, &p));
    assert(engine_track_notes(&engine, 6) == 1 && !memcmp(&plan, &engine.plan, sizeof(plan)));
    assert(patch_wave(&engine.project.track[6].patch) == WAVE_SQUARE);
    Patch env;
    patch_default(&env, CHIP_SAA);
    assert(patch_set_wave(&env, WAVE_AM));
    env.shape = 0;
    assert(!(patch_features(&env) & PATCH_F_ENV_RATIO));
}
static void pitch(void) {
    assert(fabsf(midi_note_hz(45) - 110) < .001f && fabsf(midi_note_hz(69) - 440) < .001f);
    for (unsigned n = 0; n < 128; n++) {
        float expected = 440 * exp2f(((float)n - 69) / 12);
        assert(fabsf(midi_note_hz((float)n) / expected - 1) < .000001f);
    }
    for (int q = 23 * 256; q < 119 * 256; q += 13) {
        float note = (float)q / 256;
        uint8_t o, r;
        bool clamp;
        saa_note_register(note, &o, &r, &clamp);
        if (!clamp) {
            double actual = 15625.0 * (1u << o) / (511 - r),
                   target = 440 * pow(2, (note - 69) / 12);
            assert(fabs(1200 * log2(actual / target)) < 4.1);
        }
    }
    assert(isnan(midi_note_hz(NAN)));
    assert(fabsf(fast_sine(0)) < .0001f);
}
int main(void) {
    direct();
    ownership();
    mono_sustain_restore();
    constraints();
    sound_features();
    pressure();
    waveforms();
    pitch();
    puts("PASS synth: DIRECT/POLY, ownership/sustain/steal, atomic reservations, "
         "ENV/AM/layer/metal/noise sweep/morph, pitch");
}
