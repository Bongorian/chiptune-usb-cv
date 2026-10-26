#include "ui.h"
#include "studio.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static Settings settings;
static Engine e;
static Ui ui;
static void setup(void) {
    settings_default(&settings);
    engine_init(&e, &settings.preset[0], (SoundSink){0});
    ui_init(&ui, &settings, &e, NULL, NULL, NULL);
    assert(ui.active_sound == ST_SOUNDS + 20);
    ui_click(&ui, 2, false, e.now);
    assert(!ui.confirm); /* Unedited basic factory sound can be switched directly. */
    studio_profile(&e.preset.studio, 4);
    studio_reset(&e);
    e.preset.mix[0] = 100;
    for (unsigned p = 0; p < 10; p++) {
        StSound *s = &e.preset.studio.part[p].sound;
        s->attack = s->decay = s->release = 0;
        s->sustain = s->level = 100;
    }
}
static void send(unsigned src, unsigned kind, unsigned ch, unsigned a, unsigned b) {
    uint8_t msg[] = {(uint8_t)(kind | ch), (uint8_t)a, (uint8_t)b};
    engine_message(&e, (uint8_t)src, msg, 3);
    engine_tick(&e, 1);
}
static void test_channels(void) {
    const uint16_t masks[] = {1, 2, 4, 8, 16, 32, 0x240, 0x480, 0x900, 0x3000};
    for (unsigned src = 0; src < 2; src++) {
        setup();
        assert(studio_valid(&e.preset.studio));
        for (unsigned p = 0; p < 10; p++) {
            assert(e.studio.outputs[p] == masks[p] && e.studio.resources[p] == masks[p]);
            assert(e.preset.studio.part[p].poly == 1);
            assert(e.preset.studio.part[p].sound.mode == 1);
            assert(e.preset.studio.route[src][p] == (1u << p));
            send(src, 0x90, p, 45, 127);
            assert(studio_note_count(&e, p) == 1);
        }
        assert(engine_active_notes(&e) == 10 && engine_active(&e) == 14);
        for (unsigned v = 0; v < 3; v++) {
            assert(e.sn_div[0][v] == 568 && e.sn_div[1][v] == 568);
            assert(e.sn_atten[0][v] == e.sn_atten[1][v]);
        }
        send(src, 0x90, 6, 69, 127);
        send(src, 0x80, 6, 45, 0); /* Old Note Off cannot stop the replacement. */
        assert(e.voice[6].active && e.voice[9].active && e.voice[6].note == 69);
        assert(e.sn_div[0][0] == 142 && e.sn_div[1][0] == 142);
        send(src, 0x80, 0, 45, 0);
        assert(!e.voice[0].active);
        for (unsigned p = 1; p < 10; p++)
            assert(studio_note_count(&e, p) == 1);
        send(src, 0xb0, 6, 10, 0);
        assert(e.sn_atten[0][0] < 15 && e.sn_atten[1][0] == 15);
        send(src, 0xb0, 6, 10, 127);
        assert(e.sn_atten[0][0] == 15 && e.sn_atten[1][0] < 15);
        send(src, 0xb0, 6, 10, 64);
        assert(e.sn_atten[0][0] == e.sn_atten[1][0]);
        send(src, 0xb0, 1, 10, 0);
        assert((e.saa_cache[1] & 15) && !(e.saa_cache[1] >> 4));
        send(src, 0xb0, 1, 10, 127);
        assert(!(e.saa_cache[1] & 15) && (e.saa_cache[1] >> 4));
        unsigned before = e.refused;
        /* Key-tracked noise would take ch9's tone pair, so reject atomically. */
        StSound noise = e.preset.studio.part[9].sound;
        noise.noise_rate = 3;
        assert(!studio_sound_load(&e, 9, &noise, "TRACKED "));
        assert(e.refused == before && studio_note_count(&e, 8) == 1);
        engine_panic(&e, (int)src);
        assert(!engine_active(&e));
    }
}
static void test_pitch_and_level(void) {
    const unsigned notes[] = {45, 48, 60, 69, 81};
    const double expected[] = {110, 130.8127826503, 261.6255653006, 440, 880};
    for (unsigned i = 0; i < 5; i++)
        assert(fabs((double)midi_note_hz((float)notes[i]) - expected[i]) < .0001);
    for (unsigned note = 0; note < 128; note++) {
        float hz = midi_note_hz((float)note);
        assert(fabs((double)hz - 440 * pow(2, ((double)note - 69) / 12)) / hz < .000001);
        if (note >= 23 && note <= 118) {
            uint8_t o, f;
            bool clamp;
            saa_frequency(hz, &o, &f, &clamp);
            double actual = 15625.0 * pow(2, o) / (511 - f);
            assert(!clamp && fabs(1200 * log2(actual / hz)) < 4);
        }
    }
    assert(isnan(midi_note_hz(NAN)) && isnan(midi_note_hz(INFINITY)));
    assert(fabsf(midi_note_hz(69.5f) - 440 * exp2f(.5f / 12)) < .0001f);
    assert(saa_amplitude(0) == 0 && saa_amplitude(1) == 15 && saa_amplitude(2) == 15);
    assert(saa_amplitude(NAN) == 0);
    /* Equal per-output nominal square-wave amplitude, within SN's 2 dB steps. */
    for (unsigned side = 1; side < 3; side++) {
        unsigned att = sn_attenuation(sound_output_gain(side));
        double saa_vpp = .000250 * 1500 * 10000 / 23500;
        double sn_vpp = .000160 / 4 * 17000 * 10000 / 22000 * pow(10, -(double)att / 10);
        assert(fabs(20 * log10(sn_vpp / saa_vpp)) < 1.01);
    }
}
static void test_v08_library_preserved(void) {
    settings_default(&settings);
    settings.layout_revision = 2;
    settings.sounds[0].level = 27;
    memcpy(settings.sound_name[0], "MY BASS ", 9);
    settings.cv_zero[0] = 17;
    settings.brightness = 43;
    engine_init(&e, &settings.preset[0], (SoundSink){0});
    ui_init(&ui, &settings, &e, NULL, NULL, NULL);
    assert(ui.layout_reset && settings.layout_revision == 3);
    assert(settings.sounds[0].level == 27 && !memcmp(settings.sound_name[0], "MY BASS ", 9));
    assert(settings.cv_zero[0] == 17 && settings.brightness == 43);
    assert(studio_valid(&e.preset.studio) && e.preset.studio.route[0][9] == (1u << 9));
}
int main(void) {
    test_channels();
    test_pitch_and_level();
    test_v08_library_preserved();
    puts("PASS: ten monophonic MIDI parts, parallel SN pitch, PAN, independent note ownership, pitch anchors, nominal levels, v0.8 migration");
}
