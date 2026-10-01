#include "halo_vita_kernel.h"
#include "halo_vita_completion.h"
#include <time.h>
#include <sched.h>
#include <errno.h>
extern int nanosleep(const struct timespec *, struct timespec *);

void WINAPI Sleep(DWORD milliseconds)
{
    struct timespec requested, remaining;
    if (!milliseconds) { sched_yield(); return; }
    requested.tv_sec = milliseconds == 0xffffffffUL ? 86400 : milliseconds / 1000;
    requested.tv_nsec = milliseconds == 0xffffffffUL ? 0 : (milliseconds % 1000) * 1000000L;
    for (;;) {
        if (!nanosleep(&requested, &remaining)) {
            if (milliseconds == 0xffffffffUL) continue;
            return;
        }
        if (errno != EINTR) { halo_vita_set_last_error_from_errno(errno); return; }
        requested = remaining;
    }
}

DWORD WINAPI SleepEx(DWORD milliseconds, BOOL alertable)
{
    int ready;
    if (!alertable) { Sleep(milliseconds); return 0; }
    ready = halo_vita_completion_wait(milliseconds);
    if (ready < 0) { SetLastError(ERROR_GEN_FAILURE); return 0xffffffffUL; }
    if (ready && halo_vita_completion_dispatch()) return 0xc0UL; /* WAIT_IO_COMPLETION */
    if (!milliseconds) sched_yield();
    return 0;
}
