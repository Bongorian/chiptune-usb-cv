#pragma once
#include "engine.h"
bool cv_calibration(System *system, unsigned input, const int16_t samples[64], bool one,
                    bool zero_set[2]);

float cv_volts(const System *system, unsigned input, int16_t raw);
