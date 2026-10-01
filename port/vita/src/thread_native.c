#include <psp2/kernel/threadmgr.h>
#include "halo_vita_thread_native.h"
int halo_vita_native_thread_id(void) { return sceKernelGetThreadId(); }
int halo_vita_native_thread_priority(int id, int priority)
{ return sceKernelChangeThreadPriority(id, priority); }
