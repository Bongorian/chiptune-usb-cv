#include "persistence.h"
#include <string.h>
bool settings_save(Settings *s, Engine *e, SaveKind kind, unsigned track, unsigned slot,
                   bool (*commit)(void *), void *ctx) {
    if (track >= TRACKS || kind > SAVE_SYSTEM || (kind == SAVE_PATCH && slot >= USER_PATCHES) ||
        (kind == SAVE_TRACK && slot >= USER_TRACKS) || (kind == SAVE_PROJECT && slot >= PROJECTS) ||
        !commit)
        return false;
    static Settings before;
    before = *s;
    if (kind == SAVE_PATCH) {
        s->patches[slot] = e->project.track[track].patch;
        memcpy(s->patch_name[slot], e->project.track[track].name, 9);
        s->patch_used |= (uint16_t)(1u << slot);
    }
    if (kind == SAVE_SYSTEM)
        s->system = e->system;
    if (kind == SAVE_TRACK) {
        s->tracks[slot] = e->project.track[track];
        s->track_used |= (uint16_t)(1u << slot);
    }
    if (kind == SAVE_PROJECT) {
        s->project[slot] = e->project;
        s->selected = (uint8_t)slot;
    }
    bool morph_ok = true;
    for (unsigned t = 0; t < TRACKS; t++) {
        const Track *a = &e->project.track[t];
        if (a->morph_target && !patch_same_structure(&a->patch, &s->patches[a->morph_target - 1]))
            morph_ok = false;
    }
    if (!morph_ok || !settings_valid(s) || !commit(ctx)) {
        *s = before;
        return false;
    }
    return true;
}
bool engine_load_project(Engine *e, const Project *p) {
    static ResourcePlan next;
    if (!resource_plan(p, NULL, NULL, &next, &e->conflict))
        return false;
    engine_panic(e, -1);
    e->project = *p;
    e->plan = next;
    e->clocks = e->internal_clocks = e->internal_phase = 0;
    e->clock_seen = false;
    e->clock_running = true;
    memset(e->motion, 0, sizeof(e->motion));
    for (unsigned t = 0; t < TRACKS; t++) {
        e->motion[t].pulse = UINT32_MAX;
        memcpy(e->macro[t], p->track[t].macro, 4);
    }
    return true;
}
