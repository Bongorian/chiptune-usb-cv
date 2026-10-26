#include "engine.h"
#include <math.h>
static float limit(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
uint16_t sn_divider_clock(float hz, float clock, bool *clamped) {
    if (!isfinite(clock) || clock <= 0 || !isfinite(hz) || hz <= 0) {
        *clamped = true;
        return 1;
    }
    float min = (float)clock / (32 * 1023), max = (float)clock / 32;
    *clamped = !isfinite(hz) || hz < min || hz > max;
    hz = isfinite(hz) ? limit(hz, min, max) : min;
    return (uint16_t)limit(roundf((float)clock / (32 * hz)), 1, 1023);
}
uint8_t sn_attenuation(float gain) {
    static const float threshold[15] = {0.891251f, 0.707946f, 0.562341f, 0.446684f, 0.354813f,
                                        0.281838f, 0.223872f, 0.177828f, 0.141254f, 0.112202f,
                                        0.089125f, 0.070795f, 0.056234f, 0.044668f, 0.035481f};
    if (!isfinite(gain))
        return 15;
    for (uint8_t attenuation = 0; attenuation < 15; attenuation++)
        if (gain >= threshold[attenuation])
            return attenuation;
    return 15;
}

#include "saa_pitch_lut.inc"
void saa_note_register(float note, uint8_t *oct, uint8_t *reg, bool *clamped) {
    if (!isfinite(note)) {
        *oct = *reg = 0;
        *clamped = true;
        return;
    }
    note = fmaxf(-128, fminf(255, note));
    float scaled = (note - 60) * 256;
    int q = (int)(scaled >= 0 ? scaled + .5f : scaled - .5f), shift = q / 3072,
        index = q % 3072;
    if (index < 0) {
        index += 3072;
        shift--;
    }
    unsigned packed = saa_pitch_lut[index];
    int o = (int)(packed >> 8) + shift;
    *clamped = o < 0 || o > 7;
    if (o < 0) {
        *oct = *reg = 0;
    } else if (o > 7) {
        *oct = 7;
        *reg = 255;
    } else {
        *oct = (uint8_t)o;
        *reg = (uint8_t)packed;
    }
}
