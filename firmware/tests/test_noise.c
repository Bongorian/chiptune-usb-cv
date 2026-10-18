#include "engine.h"
#include "studio.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static Settings settings;
static Engine engine;
static uint8_t saa[32], sn_latch[2], sn_volume[2][4], sn_control[2];
static uint16_t sn_period[2][3];
static unsigned sn_resets[2];
static void write_saa(void *ctx, uint8_t reg, uint8_t data) {
    (void)ctx;
    assert(reg < 32);
    saa[reg] = data;
}
static void write_sn(void *ctx, uint8_t chip, uint8_t data) {
    (void)ctx;
    assert(chip < 2);
    if (data & 0x80)
        sn_latch[chip] = (data >> 4) & 7;
    unsigned reg = sn_latch[chip], channel = reg / 2;
    if (reg & 1)
        sn_volume[chip][channel] = data & 15;
    else if (channel == 3) {
        sn_control[chip] = data & 7;
        sn_resets[chip]++;
    } else if (data & 0x80)
        sn_period[chip][channel] = (uint16_t)((sn_period[chip][channel] & 0x3f0) | (data & 15));
    else
        sn_period[chip][channel] = (uint16_t)((sn_period[chip][channel] & 15) | ((data & 63) << 4));
}
static void setup(unsigned model, unsigned rate, unsigned white) {
    memset(saa, 0, sizeof(saa));
    memset(sn_period, 0, sizeof(sn_period));
    memset(sn_control, 0, sizeof(sn_control));
    memset(sn_resets, 0, sizeof(sn_resets));
    settings_default(&settings);
    engine_init(&engine, &settings.preset[0], (SoundSink){write_saa, write_sn, NULL});
    studio_activate(&engine);
    StScene *s = &engine.preset.studio;
    for (unsigned p = 0; p < ST_PARTS; p++)
        s->part[p].enabled = p == 0;
    studio_sound_default(&s->part[0].sound, model);
    StSound *sound = &s->part[0].sound;
    sound->noise_rate = (uint8_t)rate;
    sound->shape = (uint8_t)white;
    sound->attack = sound->decay = sound->release = 0;
    sound->sustain = 100;
    sound->level = 100;
    s->part[0].poly = 1;
    for (unsigned src = 0; src < 2; src++)
        for (unsigned c = 0; c < 16; c++)
            s->route[src][c] = c == 0 ? 1 : 0;
    memset(s->mod, 0, sizeof(s->mod));
    studio_reset(&engine);
    assert(studio_valid(s));
}
static void note(bool on) {
    uint8_t data[] = {on ? 0x90 : 0x80, 48, 127};
    engine_message(&engine, 0, data, 3);
    engine_tick(&engine, 1);
}
static void test_sn_noise(void) {
    const unsigned models[] = {ST_SN_NOISE, ST_SN_NOISE_L, ST_SN_NOISE_R, ST_SN_METAL};
    for (unsigned m = 0; m < 4; m++)
        for (unsigned rate = 0; rate < 4; rate++)
            for (unsigned white = 0; white < 2; white++) {
                if (models[m] == ST_SN_METAL && rate != 3)
                    continue;
                setup(models[m], rate, white);
                note(true);
                for (unsigned chip = 0; chip < 2; chip++) {
                    bool selected = models[m] == ST_SN_NOISE || models[m] == ST_SN_METAL ||
                                    (models[m] == ST_SN_NOISE_L ? chip == 0 : chip == 1);
                    if (!selected) {
                        assert(sn_volume[chip][3] == 15 && !sn_resets[chip]);
                        continue;
                    }
                    assert(sn_resets[chip] == 1);
                    assert(sn_control[chip] == (white * 4 + rate));
                    assert(sn_volume[chip][3] < 15);
                    if (rate == 3) {
                        bool clamped;
                        float note_hz = 440.0f * exp2f((48.0f - 69.0f) / 12.0f);
                        uint16_t expected =
                            sn_divider_clock(note_hz * (white ? 32 : 15), 2000000, &clamped);
                        assert(!clamped && sn_period[chip][2] == expected);
                        if (white)
                            assert(2000000.0f / (32 * sn_period[chip][2]) > 4000);
                    }
                    for (unsigned ch = 0; ch < 3; ch++)
                        if (ch != 2 || models[m] != ST_SN_METAL)
                            assert(sn_volume[chip][ch] == 15);
                }
                unsigned resets[2] = {sn_resets[0], sn_resets[1]};
                for (unsigned i = 0; i < 100; i++)
                    engine_tick(&engine, 1);
                assert(sn_resets[0] == resets[0] && sn_resets[1] == resets[1]);
                note(false);
                assert(sn_volume[0][3] == 15 && sn_volume[1][3] == 15);
                note(true);
                for (unsigned chip = 0; chip < 2; chip++)
                    if (resets[chip])
                        assert(sn_resets[chip] == resets[chip] + 1);
            }
}
static void test_saa_noise(void) {
    for (unsigned rate = 0; rate < 4; rate++) {
        setup(ST_SAA_NOISE, rate, 0);
        note(true);
        assert(saa[0x15] && !saa[0x14]);
        unsigned output = 0;
        while (!(saa[0x15] & (1u << output)))
            output++;
        assert(output < 6 && saa[output]);
        assert(((saa[0x16] >> ((output / 3) * 4)) & 3) == rate);
        assert(!(saa[0x18 + output / 3] & 0x80));
        note(false);
        assert(!saa[0x15] && !saa[output]);
    }
}
static void test_default_drum_colour(void) {
    setup(ST_SN_NOISE, 3, 1);
    studio_default(&engine.preset.studio);
    engine.preset.studio.enabled = 1;
    studio_reset(&engine);
    const uint8_t note_on[] = {0x99, 38, 127};
    engine_message(&engine, 0, note_on, 3);
    engine_tick(&engine, 1);
    for (unsigned chip = 0; chip < 2; chip++) {
        assert(sn_control[chip] == 7 && sn_volume[chip][3] < 15);
        float clock = 2000000.0f / (32 * sn_period[chip][2]);
        assert(clock > 2200 && clock < 2500);
    }
}
int main(void) {
    test_sn_noise();
    test_saa_noise();
    test_default_drum_colour();
    puts("PASS: SN noise control/volume/tone3/retrigger and SAA noise enable/amplitude/rate");
}
