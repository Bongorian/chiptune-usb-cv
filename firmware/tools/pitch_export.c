#include "engine.h"
#include <math.h>
#include <stdio.h>
int main(void) {
    puts("MIDI_note,note_name,target_Hz,SAA_octave,SAA_register,SAA_actual_Hz,SAA_error_cents,SAA_"
         "clamped,SN_clock_Hz,SN_divider,SN_actual_Hz,SN_error_cents,SN_clamped");
    static const char *names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    for (unsigned note = 0; note < 128; note++) {
        float hz = midi_note_hz((float)note);
        uint8_t o, f;
        bool sc, nc;
        saa_note_register((float)note, &o, &f, &sc);
        unsigned n = sn_divider_clock(hz, 2000000, &nc);
        double saa = 15625.0 * (1u << o) / (511 - f), sn = 2000000.0 / (32 * n);
        printf("%u,%s%d,%.6f,%u,%u,%.6f,%.3f,%u,2000000,%u,%.6f,%.3f,%u\n", note, names[note % 12],
               (int)note / 12 - 1, (double)hz, o, f, saa, 1200 * log2(saa / hz), sc, n, sn,
               1200 * log2(sn / hz), nc);
    }
}
