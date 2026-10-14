#include "cv.h"
#include <math.h>
float cv_volts(const Settings *s, unsigned input, int16_t raw) {
    if (input >= 2 || !isfinite(s->cv_codes_per_volt[input]) || s->cv_codes_per_volt[input] <= 0)
        return NAN;
    return (raw - s->cv_zero[input]) / s->cv_codes_per_volt[input];
}
float cv_pitch(const Settings *s, unsigned input, int16_t raw, uint8_t base_note) {
    return base_note + 12 * cv_volts(s, input, raw);
}
bool cv_calibration(Settings *s, unsigned input, const int16_t samples[64], bool one,
                    bool zero_set[2]) {
    if (input >= 2)
        return false;
    double sum = 0, square = 0;
    for (int i = 0; i < 64; i++) {
        double x = samples[i];
        sum += x;
        square += x * x;
    }
    float mean = (float)(sum / 64);
    double variance = square / 64 - (sum / 64) * (sum / 64);
    if (variance > 16)
        return false; /* standard deviation <=4 ADC codes */
    if (!one) {
        if (fabsf(mean) > 200)
            return false;
        s->cv_zero[input] = mean;
        zero_set[input] = true;
    } else {
        float gain = mean - s->cv_zero[input];
        if (!zero_set[input] || gain < 500 || gain > 3000)
            return false;
        s->cv_codes_per_volt[input] = gain;
        zero_set[input] = false;
    }
    return true;
}
