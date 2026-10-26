#pragma once
#include "engine.h"
#include "editor.h"
#include "persistence.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static Settings settings;
static Engine engine;
static inline void setup(void) {
    settings_default(&settings);
    assert(settings_valid(&settings));
    engine_init(&engine, &settings.project[0], (SoundSink){0});
    engine.library = &settings;
    engine.system = settings.system;
}
static inline void message(unsigned src, unsigned status, unsigned a, unsigned b) {
    uint8_t m[] = {(uint8_t)status, (uint8_t)a, (uint8_t)b};
    engine_message(&engine, (uint8_t)src, m, status >> 4 == 12 || status >> 4 == 13 ? 2 : 3);
    engine_tick(&engine, 1);
}
static inline void advance(unsigned count) {
    for (unsigned i = 0; i < count; i++)
        engine_tick(&engine, 1);
}
static inline void isolated(unsigned factory, unsigned mode, unsigned voices) {
    Project p = engine.project;
    for (unsigned t = 0; t < TRACKS; t++)
        p.track[t].enabled = t == 0;
    patch_factory(factory, &p.track[0].patch, p.track[0].name);
    p.track[0].mode = (uint8_t)mode;
    p.track[0].voices = (uint8_t)voices;
    p.track[0].patch.attack = p.track[0].patch.decay = p.track[0].patch.release = 0;
    p.track[0].patch.sustain = p.track[0].patch.level = 100;
    p.mix[0] = 100;
    assert(engine_load_project(&engine, &p));
}
