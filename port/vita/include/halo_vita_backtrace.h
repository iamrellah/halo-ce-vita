#ifndef HALO_VITA_BACKTRACE_H
#define HALO_VITA_BACKTRACE_H
#include <stdint.h>
#include <stddef.h>
/* Current calling thread only, never an Xbox CONTEXT. Requires ARM unwind
 * tables; returns actual recorded PCs, possibly zero if unwinding unavailable. */
size_t halo_vita_backtrace(uintptr_t *frames,size_t capacity,unsigned skip);
#endif
