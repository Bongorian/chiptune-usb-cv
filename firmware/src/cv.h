#pragma once
#include "engine.h"
float cv_pitch(const Settings *settings, unsigned input, int16_t raw, uint8_t base_note);
bool cv_calibration(Settings *settings, unsigned input, const int16_t samples[64], bool one,
                    bool zero_set[2]);
