/* Native ARM32 implementations of the engine's interlocked operations.
 * Keep full ordering across cores, as callers expect from Xbox interlocked
 * operations. Increment/decrement return the new value; exchange/add and
 * compare-exchange return the original value. */
#include "xdk_win32.h"

typedef char vita_interlocked_word_is_32_bits[sizeof(LONG) == 4 ? 1 : -1];
typedef char vita_interlocked_word_is_lock_free[
    __atomic_always_lock_free(sizeof(LONG), 0) ? 1 : -1];

LONG WINAPI halo_vita_InterlockedIncrement(LPLONG value)
{
    return __atomic_add_fetch(value, 1, __ATOMIC_SEQ_CST);
}

LONG WINAPI halo_vita_InterlockedDecrement(LPLONG value)
{
    return __atomic_sub_fetch(value, 1, __ATOMIC_SEQ_CST);
}

LONG WINAPI halo_vita_InterlockedExchange(LPLONG value, LONG replacement)
{
    return __atomic_exchange_n(value, replacement, __ATOMIC_SEQ_CST);
}

LONG WINAPI halo_vita_InterlockedExchangeAdd(LPLONG value, LONG amount)
{
    return __atomic_fetch_add(value, amount, __ATOMIC_SEQ_CST);
}

LONG WINAPI halo_vita_InterlockedCompareExchange(
    LPLONG value, LONG replacement, LONG comparand)
{
    __atomic_compare_exchange_n(value, &comparand, replacement, 0,
        __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return comparand;
}
