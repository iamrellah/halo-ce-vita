/* Reuse upstream's 16-bit MSVC text ABI, never the native CRT wide ABI. */
#ifndef HALO_VITA_WCHAR_H
#define HALO_VITA_WCHAR_H
#ifndef HALO_VITA
#error Vita compatibility header used outside the Vita build
#endif
#include "../../linux/include/wchar.h"
#endif
