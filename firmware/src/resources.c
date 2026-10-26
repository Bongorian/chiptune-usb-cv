#include "resources.h"
#include <stdio.h>
#include <string.h>
static unsigned bits(uint16_t n) {
    unsigned c = 0;
    while (n) {
        n &= (uint16_t)(n - 1);
        c++;
    }
    return c;
}
uint16_t resource_track_mask(const ResourcePlan *p, unsigned t, bool audible) {
    uint16_t m = 0;
    if (t < TRACKS)
        for (unsigned i = 0; i < p->count[t]; i++)
            m |= audible ? p->outputs[t][i] : p->resources[t][i];
    return m;
}
static bool noise(const Patch *p) {
    return p->source != SOURCE_TONE ||
           (p->chip == CHIP_SAA && p->layers > 1 && p->layer_source != SOURCE_TONE);
}
static unsigned count(const Track *t) {
    return !t->enabled ? 0 : t->mode == PLAY_POLY ? t->voices : 1;
}
static void error_set(ResourceConflict *e, unsigned t, unsigned reason, uint16_t used) {
    if (!e)
        return;
    memset(e, 0, sizeof(*e));
    e->track = (uint8_t)t;
    e->other = 255;
    e->reason = (uint8_t)reason;
    e->available = (uint8_t)(14 - bits(used));
    const char *why = reason == RESOURCE_INVALID        ? "Invalid track or patch"
                      : reason == RESOURCE_NOISE_SHARED ? "SAA noise rate shared"
                      : reason == RESOURCE_CLOCK_BUSY   ? "Clock circuit busy"
                                                        : "Not enough physical circuits";
    snprintf(e->message, sizeof(e->message), "%s", why);
    snprintf(e->suggestion, sizeof(e->suggestion), "%s",
             reason == RESOURCE_NOISE_SHARED ? "Match noise rate or free other SAA group"
                                             : "Fewer voices, DIRECT or disable a track");
}
typedef struct {
    uint8_t track, slot, priority;
} Task;
static bool search(const Project *p, const Task task[14], unsigned total, unsigned at,
                   ResourcePlan *plan, int colour[2], unsigned *failed, unsigned *reason) {
    if (at == total)
        return true;
    unsigned t = task[at].track, slot = task[at].slot;
    const Patch *s = &p->track[t].patch;
    for (unsigned choice = 0; choice < (s->chip == CHIP_SAA ? 6u : 7u); choice++) {
        uint16_t out = 0, need = 0;
        int next[2] = {colour[0], colour[1]};
        if (s->chip == CHIP_SAA) {
            unsigned v = choice;
            /* A tracked mixed voice needs a separate tone for the noise clock;
               otherwise editing noise pitch would retune its audible carrier. */
            if (s->source == SOURCE_MIX && s->noise_rate == 3 && v % 3 == 0)
                continue;
            out = (uint16_t)(1u << v);
            need = out;
            if (s->env) {
                if (v != 2 && v != 5)
                    continue;
                need |= (uint16_t)(1u << (v - 1));
            }
            if (s->layers > 1) {
                need |= (uint16_t)(1u << (v - 2));
                out |= (uint16_t)(1u << (v - 2));
            }
            if (noise(s)) {
                unsigned group = v / 3;
                if (next[group] >= 0 && next[group] != s->noise_rate) {
                    *reason = RESOURCE_NOISE_SHARED;
                    continue;
                }
                next[group] = s->noise_rate;
                if (s->noise_rate == 3)
                    need |= (uint16_t)(1u << (group * 3));
            }
        } else if (s->source == SOURCE_TONE) {
            unsigned mask = choice + 1;
            if (bits((uint16_t)mask) != s->layers)
                continue;
            for (unsigned n = 0; n < 3; n++)
                if (mask & (1u << n)) {
                    if (s->output != OUTPUT_RIGHT)
                        out |= (uint16_t)(1u << (6 + n));
                    if (s->output != OUTPUT_LEFT)
                        out |= (uint16_t)(1u << (9 + n));
                }
            need = out;
        } else {
            if (choice)
                break;
            if (s->output != OUTPUT_RIGHT)
                out |= 1u << 12;
            if (s->output != OUTPUT_LEFT)
                out |= 1u << 13;
            need = out;
            if (s->noise_rate == 3 || s->source == SOURCE_MIX) {
                if (out & (1u << 12))
                    need |= 1u << 8;
                if (out & (1u << 13))
                    need |= 1u << 11;
                if (s->source == SOURCE_MIX)
                    out = need;
            }
        }
        if (plan->used & need)
            continue;
        uint16_t before = plan->used;
        plan->used |= need;
        plan->outputs[t][slot] = out;
        plan->resources[t][slot] = need;
        if (search(p, task, total, at + 1, plan, next, failed, reason))
            return true;
        plan->used = before;
        plan->outputs[t][slot] = plan->resources[t][slot] = 0;
    }
    *failed = t;
    if (s->env || (noise(s) && s->noise_rate == 3))
        *reason = RESOURCE_CLOCK_BUSY;
    return false;
}
bool resource_plan(const Project *p, const Project *previous, const ResourcePlan *old,
                   ResourcePlan *plan, ResourceConflict *error) {
    memset(plan, 0, sizeof(*plan));
    if (error)
        memset(error, 0, sizeof(*error));
    if (!project_valid(p)) {
        error_set(error, 0, RESOURCE_INVALID, 0);
        return false;
    }
    Task task[14];
    unsigned total = 0;
    int colour[2] = {-1, -1};
    for (unsigned t = 0; t < TRACKS; t++) {
        unsigned n = count(&p->track[t]);
        plan->count[t] = (uint8_t)n;
        bool fixed = previous && old && count(&previous->track[t]) == n &&
                     patch_same_structure(&previous->track[t].patch, &p->track[t].patch);
        if (fixed) {
            memcpy(plan->outputs[t], old->outputs[t], sizeof(plan->outputs[t]));
            memcpy(plan->resources[t], old->resources[t], sizeof(plan->resources[t]));
            uint16_t mask = resource_track_mask(plan, t, false);
            if (plan->used & mask) {
                error_set(error, t, RESOURCE_CAPACITY, plan->used);
                return false;
            }
            plan->used |= mask;
            const Patch *s = &p->track[t].patch;
            if (n && s->chip == CHIP_SAA && noise(s))
                for (unsigned g = 0; g < 2; g++)
                    if (mask & (7u << (g * 3))) {
                        if (colour[g] >= 0 && colour[g] != s->noise_rate) {
                            error_set(error, t, RESOURCE_NOISE_SHARED, plan->used);
                            return false;
                        }
                        colour[g] = s->noise_rate;
                    }
        } else
            for (unsigned i = 0; i < n; i++) {
                if (total >= 14) {
                    error_set(error, t, RESOURCE_CAPACITY, plan->used);
                    return false;
                }
                const Patch *s = &p->track[t].patch;
                unsigned priority = s->chip == CHIP_SAA ? (s->env     ? 0
                                                           : noise(s) ? 1
                                                                      : 2)
                                                        : (s->source != SOURCE_TONE     ? 0
                                                           : s->output == OUTPUT_STEREO ? 1
                                                                                        : 2);
                task[total++] = (Task){(uint8_t)t, (uint8_t)i, (uint8_t)priority};
            }
    }
    for (unsigned i = 1; i < total; i++)
        for (unsigned j = i; j && task[j].priority < task[j - 1].priority; j--) {
            Task a = task[j];
            task[j] = task[j - 1];
            task[j - 1] = a;
        }
    unsigned failed = 0, reason = RESOURCE_CAPACITY;
    if (!search(p, task, total, 0, plan, colour, &failed, &reason)) {
        error_set(error, failed, reason, plan->used);
        return false;
    }
    return true;
}
