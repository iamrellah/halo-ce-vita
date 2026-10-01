#include "halo_vita_file_handle.h"
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

DWORD WINAPI GetFileSize(HANDLE token, DWORD *high)
{
    struct halo_vita_handle *reference = halo_vita_handle_acquire(token, HALO_VITA_HANDLE_FILE);
    struct halo_vita_file *file;
    struct stat status;
    uint64_t size;
    int result, error;
    if (!reference) return INVALID_FILE_SIZE;
    file = halo_vita_handle_data(reference);
    if (pthread_mutex_lock(&file->io_lock)) abort();
    result = fstat(file->fd, &status);
    error = errno;
    if (pthread_mutex_unlock(&file->io_lock)) abort();
    halo_vita_handle_release(reference);
    if (result < 0) { halo_vita_set_last_error_from_errno(error); return INVALID_FILE_SIZE; }
    if (status.st_size < 0) { SetLastError(ERROR_GEN_FAILURE); return INVALID_FILE_SIZE; }
    size = (uint64_t)status.st_size;
    if (high) *high = (DWORD)(size >> 32);
    /* Disambiguates a valid low word of 0xffffffff from failure. */
    SetLastError(ERROR_SUCCESS);
    return (DWORD)size;
}

BOOL WINAPI FlushFileBuffers(HANDLE token)
{
    struct halo_vita_handle *reference = halo_vita_handle_acquire(token, HALO_VITA_HANDLE_FILE);
    struct halo_vita_file *file;
    int result, error;
    if (!reference) return FALSE;
    file = halo_vita_handle_data(reference);
    if (!(file->access & GENERIC_WRITE)) {
        halo_vita_handle_release(reference);
        SetLastError(ERROR_ACCESS_DENIED); return FALSE;
    }
    if (pthread_mutex_lock(&file->io_lock)) abort();
    do { result = fsync(file->fd); } while (result < 0 && errno == EINTR);
    error = errno;
    if (pthread_mutex_unlock(&file->io_lock)) abort();
    halo_vita_handle_release(reference);
    if (result < 0) { halo_vita_set_last_error_from_errno(error); return FALSE; }
    return TRUE;
}
