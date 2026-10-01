/* One shared monotonic clock across application cores; no CPU-cycle estimate. */
#include <psp2/kernel/threadmgr.h>
#include "halo_vita_clock.h"

long long halo_vita_profile_timebase(void)
{
    return sceKernelGetSystemTimeWide();
}
