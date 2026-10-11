#include "power.h"
#include <math.h>
bool power_allowed(PowerPolicy *p, bool mounted, bool suspended, float cc_volts, uint32_t now) {
    /* CC is measured after the board's 1:2 divider and converted back to pin voltage.
       0.68V threshold lies between default-current and 1.5A ranges.
       Loss of advertisement cuts sound power immediately; increases settle for 250ms. */
    bool high = isfinite(cc_volts) && cc_volts >= 0.68f && cc_volts < 2.60f;
    if (!isfinite(cc_volts) || cc_volts < 0.68f || cc_volts >= 2.60f) {
        p->advertised = false;
        p->since = now;
    } else if (high && !p->advertised) {
        p->advertised = true;
        p->since = now;
    }
    if (suspended)
        return false;
    return mounted || (p->advertised && now - p->since >= 250);
}
