#pragma once
#include <stdint.h>
#define SAA_CLOCK_HZ 8000000u
float midi_note_hz(float note);
float sound_output_gain(unsigned chip);
uint8_t saa_amplitude(float gain);
