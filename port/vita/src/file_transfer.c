#include "halo_vita_file_handle.h"
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

/* The strict C89 engine configuration hides these POSIX declarations. */
extern ssize_t pread(int, void *, size_t, off_t);
extern ssize_t pwrite(int, const void *, size_t, off_t);

DWORD halo_vita_file_transfer(struct halo_vita_handle *reference, void *buffer,
    DWORD count, DWORD *transferred, BOOL writing, BOOL positioned, uint64_t offset)
{
    struct halo_vita_file *file = halo_vita_handle_data(reference);
    DWORD total = 0, error = ERROR_SUCCESS;
    uint64_t maximum_offset = sizeof(off_t) == 8 ? UINT64_C(0x7fffffffffffffff) : UINT64_C(0x7fffffff);
    *transferred = 0;
    if (!buffer && count) return ERROR_INVALID_PARAMETER;
    if (!(file->access & (writing ? GENERIC_WRITE : GENERIC_READ))) return ERROR_ACCESS_DENIED;
    /* Never silently truncate an Xbox 64-bit OVERLAPPED offset to off_t. */
    if (positioned && (offset > maximum_offset || count > maximum_offset - offset))
        return ERROR_INVALID_PARAMETER;
    if (pthread_mutex_lock(&file->io_lock)) abort();
    while (total < count) {
        size_t amount = count - total;
        ssize_t result;
        if (amount > 65536) amount = 65536;
        if (writing)
            result = positioned ? pwrite(file->fd, (char *)buffer + total, amount, (off_t)(offset + total)) :
                write(file->fd, (char *)buffer + total, amount);
        else
            result = positioned ? pread(file->fd, (char *)buffer + total, amount, (off_t)(offset + total)) :
                read(file->fd, (char *)buffer + total, amount);
        if (result < 0) {
            if (errno == EINTR) continue;
            error = halo_vita_set_last_error_from_errno(errno);
            break;
        }
        if (!result) {
            /* EOF is a successful short read, never an infinite retry. */
            if (writing) error = ERROR_DISK_FULL;
            break;
        }
        total += (DWORD)result;
    }
    if (pthread_mutex_unlock(&file->io_lock)) abort();
    *transferred = total;
    return error;
}
