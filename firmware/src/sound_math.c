#include "sound_math.h"
#include <math.h>
#include <string.h>
#include "math_lut.inc"
float fast_ratio(float semitones) {
    if (!isfinite(semitones))
        return 0;
    semitones = fmaxf(-192, fminf(192, semitones));
    float scaled = semitones * 256;
    int q = (int)(scaled >= 0 ? scaled + .5f : scaled - .5f), octave = q / 3072, index = q % 3072;
    if (index < 0) {
        index += 3072;
        octave--;
    }
    union {
        float f;
        uint32_t u;
    } value = {.f = pitch_ratio[index]};
    value.u = (uint32_t)((int32_t)value.u + octave * 8388608);
    return value.f;
}
float midi_note_hz(float note) {
    return isfinite(note) && note >= -128 && note <= 255 ? 440 * fast_ratio(note - 69) : NAN;
}
float fast_sine(uint32_t phase) {
    return (float)sine_lut[phase >> 24] * (1.0f / 32767.0f);
}
float fast_decay(float x) {
    if (x <= 0)
        return 1;
    if (x >= 16)
        return 0;
    float n = x * 16;
    unsigned i = (unsigned)n;
    return decay_lut[i] + (decay_lut[i + 1] - decay_lut[i]) * (n - (float)i);
}
float fast_sqrt(float x) {
    if (x <= 0)
        return 0;
    float y = x > 1 ? x : 1;
    for (unsigned i = 0; i < 5; i++)
        y = .5f * (y + x / y);
    return y;
}
/* Nominal circuit estimate; see LEVELS_AND_PITCH.md for the assumptions and
   measurement procedure. Trims are linear amplitude, before chip quantization. */
#ifndef CHIPTUNE_SN1_TRIM
#define CHIPTUNE_SN1_TRIM 0.51627034f
#endif
#ifndef CHIPTUNE_SN2_TRIM
#define CHIPTUNE_SN2_TRIM 0.51627034f
#endif
float sound_output_gain(unsigned chip) {
    return chip == 0 ? 1.0f : chip == 1 ? CHIPTUNE_SN1_TRIM : CHIPTUNE_SN2_TRIM;
}
uint8_t saa_amplitude(float gain) {
    if (!isfinite(gain) || gain <= 0)
        return 0;
    return (uint8_t)fminf(15, roundf(15 * gain));
}
