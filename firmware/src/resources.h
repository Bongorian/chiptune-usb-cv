#pragma once
#include "model.h"
typedef struct {
    uint16_t outputs[TRACKS][MAX_POLYPHONY], resources[TRACKS][MAX_POLYPHONY];
    uint8_t count[TRACKS];
    uint16_t used;
} ResourcePlan;
typedef enum {
    RESOURCE_OK,
    RESOURCE_INVALID,
    RESOURCE_CAPACITY,
    RESOURCE_NOISE_SHARED,
    RESOURCE_CLOCK_BUSY,
    RESOURCE_MORPH
} ResourceReason;
typedef struct {
    uint8_t track, other, reason, available;
    char message[40], suggestion[56];
} ResourceConflict;
bool resource_plan(const Project *next, const Project *previous, const ResourcePlan *old,
                   ResourcePlan *plan, ResourceConflict *error);
uint16_t resource_track_mask(const ResourcePlan *p, unsigned track, bool audible);
