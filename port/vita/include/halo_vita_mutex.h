#ifndef HALO_VITA_MUTEX_H
#define HALO_VITA_MUTEX_H
#include "halo_vita_handles.h"
DWORD halo_vita_mutex_try_acquire(struct halo_vita_handle *reference);
void halo_vita_mutex_thread_exit(void);
#endif
