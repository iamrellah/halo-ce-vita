/* Win32 last-error state belongs to the calling thread. Use the Vita pthread
 * TLS API rather than assuming compiler ELF TLS is supported by the loader. */
#include "halo_vita_kernel.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

static pthread_once_t last_error_once = PTHREAD_ONCE_INIT;
static pthread_key_t last_error_key;

static void create_last_error_key(void)
{
    /* Continuing with a shared/fake error value would corrupt retry logic. */
    if (pthread_key_create(&last_error_key, NULL) != 0) abort();
}

DWORD WINAPI GetLastError(void)
{
    if (pthread_once(&last_error_once, create_last_error_key) != 0) abort();
    return (DWORD)(uintptr_t)pthread_getspecific(last_error_key);
}

void WINAPI SetLastError(DWORD value)
{
    if (pthread_once(&last_error_once, create_last_error_key) != 0) abort();
    if (pthread_setspecific(last_error_key, (void *)(uintptr_t)value) != 0) abort();
}

DWORD halo_vita_set_last_error_from_errno(int value)
{
    DWORD error;
    switch (value) {
    case 0: error = ERROR_SUCCESS; break;
    case ENOENT: error = ERROR_FILE_NOT_FOUND; break;
    case ENOTDIR: error = ERROR_PATH_NOT_FOUND; break;
    case EACCES: case EPERM: case EROFS: error = ERROR_ACCESS_DENIED; break;
    case EEXIST: error = ERROR_ALREADY_EXISTS; break;
    case ENOTEMPTY: error = ERROR_DIR_NOT_EMPTY; break;
    case ENOSPC: error = ERROR_DISK_FULL; break;
    case ENOMEM: error = ERROR_NOT_ENOUGH_MEMORY; break;
    case EBADF: error = ERROR_INVALID_HANDLE; break;
    case EINVAL: error = ERROR_INVALID_PARAMETER; break;
    case EMFILE: case ENFILE: error = ERROR_TOO_MANY_OPEN_FILES; break;
    case EBUSY: error = ERROR_BUSY; break;
    default: error = ERROR_GEN_FAILURE; break;
    }
    SetLastError(error);
    return error;
}
