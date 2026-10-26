#pragma once
#include <stdint.h>
#define SAA_CLOCK_HZ 8000000u
float midi_note_hz(float note);
float sound_output_gain(unsigned chip);
uint8_t saa_amplitude(float gain);
float fast_ratio(float semitones);
float fast_sine(uint32_t phase);
float fast_decay(float x);
float fast_sqrt(float x);
