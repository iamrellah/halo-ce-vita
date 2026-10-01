#include "halo_vita_kernel.h"
#include "halo_vita_clock.h"
#include <stdint.h>

BOOL WINAPI QueryPerformanceCounter(LARGE_INTEGER *count)
{
    if (!count) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    count->QuadPart = halo_vita_profile_timebase();
    return TRUE;
}

BOOL WINAPI QueryPerformanceFrequency(LARGE_INTEGER *frequency)
{
    if (!frequency) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    frequency->QuadPart = HALO_VITA_CLOCK_FREQUENCY;
    return TRUE;
}

DWORD WINAPI GetTickCount(void)
{
    /* Convert before truncating: a 32-bit microsecond truncation would wrap
     * every ~72 minutes rather than the required millisecond counter period. */
    return (DWORD)((uint64_t)halo_vita_profile_timebase() / 1000);
}
