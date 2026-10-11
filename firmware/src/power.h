#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    bool advertised;
    uint32_t since;
} PowerPolicy;
bool power_allowed(PowerPolicy *p, bool mounted, bool suspended, float cc_volts, uint32_t now);
