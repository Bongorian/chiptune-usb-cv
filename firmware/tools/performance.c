#define _POSIX_C_SOURCE 200809L
#include "engine.h"
#include "persistence.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static Settings settings;
static Engine engine;
static Project project;
static double ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + t.tv_nsec;
}
static void scenario(unsigned count) {
    settings_default(&settings);
    project_default(&project);
    if (count == 14)
        for (unsigned t = 0; t < 14; t++) {
            Track *a = &project.track[t];
            a->enabled = 1;
            patch_default(&a->patch, t < 6 ? CHIP_SAA : CHIP_SN);
            if (t >= 6) {
                a->patch.output = t < 9 || t == 12 ? OUTPUT_LEFT : OUTPUT_RIGHT;
                if (t >= 12)
                    a->patch.source = SOURCE_NOISE;
            }
            for (unsigned s = 0; s < SOURCES; s++)
                for (unsigned c = 0; c < 16; c++)
                    project.route[s][c] &= (uint16_t)~(1u << t);
            project.route[0][t] = (uint16_t)(1u << t);
        }
    for (unsigned t = 0; t < count; t++) {
        Track *a = &project.track[t];
        a->patch.attack = 8000;
        a->patch.decay = a->patch.release = 8000;
        a->patch.sustain = 100;
        a->patch.vibrato = 100;
        a->patch.tremolo = 100;
        a->patch.glide = 8000;
        a->patch.pitch_amount = 24;
        a->patch.pitch_decay = 8000;
        settings.patches[t] = a->patch;
        settings.patches[t].level = 25;
        a->morph_target = (uint8_t)(t + 1);
        a->morph = 50;
    }
    for (unsigned i = 0; i < MOD_SLOTS; i++)
        project.mod[i] = (ModRoute){.source = (uint8_t)(3 + i % 2),
                                    .track = (uint8_t)(i % count),
                                    .parameter = MOD_PITCH,
                                    .amount = 35};
    settings.project[0] = project;
    assert(settings_valid(&settings));
    engine_init(&engine, &project, (SoundSink){0});
    engine.library = &settings;
    for (unsigned t = 0; t < count; t++) {
        uint8_t data[] = {(uint8_t)(0x90 | t), (uint8_t)(48 + t), 127};
        engine_message(&engine, 0, data, 3);
    }
    assert(engine_active(&engine) == count);
}
int main(int argc, char **argv) {
    printf("{\"firmware\":\"v0.11.0-alpha\",\"host_only\":true,\"rp2040_wcet_certified\":false,"
           "\"scenarios\":[");
    for (unsigned run = 0; run < 2; run++) {
        unsigned count = run ? 14 : 10;
        scenario(count);
        if (argc > 1 && run) {
            FILE *f = fopen(argv[1], "wb");
            assert(f);
            assert(fwrite(&project, 1, sizeof(project), f) == sizeof(project));
            fclose(f);
        }
        if (argc > 2 && run) {
            FILE *f = fopen(argv[2], "wb");
            assert(f);
            assert(fwrite(&settings, 1, sizeof(settings), f) == sizeof(settings));
            fclose(f);
        }
        double max = 0, sum = 0;
        for (unsigned i = 0; i < 10000; i++) {
            double start = ns();
            engine_tick(&engine, 1);
            double elapsed = ns() - start;
            sum += elapsed;
            if (elapsed > max)
                max = elapsed;
        }
        unsigned coalesced = engine.bus.coalesced;
        printf(
            "%s{\"logical_voices\":%u,\"frames\":10000,\"mean_us\":%.3f,\"max_observed_us\":%."
            "3f,\"coalesced_without_bus_service\":%u}",
            run ? "," : "", count, sum / 10000 / 1000, max / 1000, coalesced);
    }
    printf(
        "],\"sizes\":{\"patch\":%zu,\"track\":%zu,\"project\":%zu,\"settings\":%zu,\"engine\":%"
        "zu},\"limitations\":\"Host scheduling and compiler differ from RP2040; device "
        "tick_max_us and overruns must be measured on hardware. Bus wait is asynchronous.\"}\n",
        sizeof(Patch), sizeof(Track), sizeof(Project), sizeof(Settings), sizeof(Engine));
}
