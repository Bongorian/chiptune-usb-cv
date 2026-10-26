#include "sound_math.h"
#include <math.h>
/* Equal temperament, MIDI 0 = C-1, MIDI 69 = A4 = 440 Hz. Keep note naming
   independent of DAWs that label middle C as C3 instead of C4. */
float midi_note_hz(float note) {
    static const float base[12] = {8.175798916f, 8.661957218f, 9.177023997f, 9.722718241f,
        10.30086115f, 10.91338223f, 11.56232571f, 12.24985737f,
        12.97827180f, 13.75f, 14.56761755f, 15.43385316f};
    if (!isfinite(note) || note < -128 || note > 255)
        return NAN;
    float whole = floorf(note);
    int octave = (int)floorf(whole / 12);
    unsigned key = (unsigned)((int)whole - octave * 12);
    return ldexpf(base[key], octave) * exp2f((note - whole) / 12);
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
