#include "halo_vita_file_handle.h"

/* Ordinary sequential I/O for tag files, cache headers, and saves; explicit
 * OVERLAPPED requests use the retained asynchronous worker path. */
static BOOL transfer(HANDLE token, void *buffer, DWORD count,
    LPDWORD transferred, LPOVERLAPPED overlapped, BOOL writing)
{
    struct halo_vita_handle *reference;
    struct halo_vita_file *file;
    DWORD bytes = 0, error;
    if (transferred) *transferred = 0;
    if (overlapped) return halo_vita_file_submit_event(token, buffer, count, overlapped, writing);
    if (!transferred || (!buffer && count)) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    reference = halo_vita_handle_acquire(token, HALO_VITA_HANDLE_FILE);
    if (!reference) return FALSE;
    file = halo_vita_handle_data(reference);
    if (file->flags & FILE_FLAG_OVERLAPPED) {
        halo_vita_handle_release(reference);
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    error = halo_vita_file_transfer(reference, buffer, count, &bytes,
        writing, FALSE, 0);
    halo_vita_handle_release(reference);
    *transferred = bytes;
    /* Releasing the final reference can close the descriptor. Publish the
     * transfer's result after cleanup, including successful short/EOF reads. */
    SetLastError(error);
    return error == ERROR_SUCCESS;
}

BOOL WINAPI ReadFile(HANDLE file, void *buffer, DWORD count,
    LPDWORD transferred, LPOVERLAPPED overlapped)
{ return transfer(file, buffer, count, transferred, overlapped, FALSE); }

BOOL WINAPI WriteFile(HANDLE file, const void *buffer, DWORD count,
    LPDWORD transferred, LPOVERLAPPED overlapped)
{ return transfer(file, (void *)buffer, count, transferred, overlapped, TRUE); }
