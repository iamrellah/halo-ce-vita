#ifndef HALO_VITA_KERNEL_H
#define HALO_VITA_KERNEL_H
#include "xdk_win32.h"
DWORD halo_vita_set_last_error_from_errno(int value);
BOOL WINAPI CreateDirectoryA(const char *guest, void *security);
BOOL WINAPI RemoveDirectoryA(const char *guest);
BOOL WINAPI CancelIo(HANDLE token);
#endif
