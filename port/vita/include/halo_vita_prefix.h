#ifndef HALO_VITA_PREFIX_H
#define HALO_VITA_PREFIX_H
#if !defined(__arm__) || __SIZEOF_POINTER__ != 4
#error Vita engine requires ARM32 and 32-bit pointers
#endif
#define HALO_VITA 1
#define _InterlockedIncrement halo_vita_InterlockedIncrement
#define _InterlockedDecrement halo_vita_InterlockedDecrement
#define _InterlockedExchange halo_vita_InterlockedExchange
#define _InterlockedExchangeAdd halo_vita_InterlockedExchangeAdd
#define _InterlockedCompareExchange halo_vita_InterlockedCompareExchange
struct scenario;
struct location;
struct object_placement_data;
struct scenario_object_datum;
struct model;
struct memory_status;
#define __STRICT_ANSI__ 1
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdarg.h>
#include "halo_vita_files.h"
/* ISO headers omit this extension used by engine units. */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
/* Engine enums reuse these names after system declarations. */
#undef LONG_MAX
#undef LONG_MIN
#undef CHAR_MAX
#undef CHAR_MIN
#define __forceinline static __inline__ __attribute__((always_inline))
#define _inline static __inline__
/* MSVC C inline definitions have pick-any linkage; an out-of-line build
 * must resolve these explicitly before engine linking. */
#define __inline static __inline__
#endif
