#include "halo_vita_file_handle.h"
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

DWORD WINAPI SetFilePointer(HANDLE token, LONG low, PLONG high, DWORD method)
{
    struct halo_vita_handle *reference;
    struct halo_vita_file *file;
    int whence, error;
    int64_t distance = high ? (int64_t)*high * INT64_C(4294967296) + (DWORD)low : (int64_t)low;
    off_t position;
    switch (method) {
    case FILE_BEGIN: whence = SEEK_SET; break;
    case FILE_CURRENT: whence = SEEK_CUR; break;
    case FILE_END: whence = SEEK_END; break;
    default: SetLastError(ERROR_INVALID_PARAMETER); return INVALID_SET_FILE_POINTER;
    }
    /* Avoid truncating a caller's 64-bit displacement on a 32-bit off_t SDK. */
    if (sizeof(off_t) < 8 && (distance > INT64_C(2147483647) || distance < -INT64_C(2147483648))) {
        SetLastError(ERROR_INVALID_PARAMETER); return INVALID_SET_FILE_POINTER;
    }
    reference = halo_vita_handle_acquire(token, HALO_VITA_HANDLE_FILE);
    if (!reference) return INVALID_SET_FILE_POINTER;
    file = halo_vita_handle_data(reference);
    if (pthread_mutex_lock(&file->io_lock)) abort();
    do { position = lseek(file->fd, (off_t)distance, whence); } while (position < 0 && errno == EINTR);
    error = errno;
    if (pthread_mutex_unlock(&file->io_lock)) abort();
    halo_vita_handle_release(reference);
    if (position < 0) {
        halo_vita_set_last_error_from_errno(error); return INVALID_SET_FILE_POINTER;
    }
    if (high) *high = (LONG)((uint64_t)position >> 32);
    SetLastError(ERROR_SUCCESS);
    return (DWORD)position;
}

/* Strict engine C89 hides the POSIX declaration in the SDK headers. */
extern int ftruncate(int, off_t);

BOOL WINAPI SetEndOfFile(HANDLE token)
{
    struct halo_vita_handle *reference;
    struct halo_vita_file *file;
    off_t position;
    int result = -1, error;
    reference = halo_vita_handle_acquire(token, HALO_VITA_HANDLE_FILE);
    if (!reference) return FALSE;
    file = halo_vita_handle_data(reference);
    if (!(file->access & GENERIC_WRITE)) {
        halo_vita_handle_release(reference);
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    /* Seek position and resize are one operation relative to synchronous and
     * positioned transfers on this handle. Caller drains any queued async I/O
     * first when ordering against those operations matters. */
    if (pthread_mutex_lock(&file->io_lock)) abort();
    do { position = lseek(file->fd, 0, SEEK_CUR); }
    while (position < 0 && errno == EINTR);
    if (position >= 0) {
        do { result = ftruncate(file->fd, position); }
        while (result < 0 && errno == EINTR);
    }
    error = errno;
    if (pthread_mutex_unlock(&file->io_lock)) abort();
    halo_vita_handle_release(reference);
    if (result < 0) {
        halo_vita_set_last_error_from_errno(error);
        return FALSE;
    }
    return TRUE;
}
