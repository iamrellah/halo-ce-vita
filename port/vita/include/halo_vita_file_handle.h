#ifndef HALO_VITA_FILE_HANDLE_H
#define HALO_VITA_FILE_HANDLE_H
#include "halo_vita_handles.h"
#include <pthread.h>
#include <stdint.h>
struct halo_vita_file {
    int fd;
    DWORD access, sharing, flags;
    pthread_mutex_t io_lock;
    char path[1024];
    struct halo_vita_file *next;
};
/* Caller retains a FILE-typed reference for the entire transfer. Returns
 * a Win32 error code and the actual byte count; no callbacks run here. */
DWORD halo_vita_file_transfer(struct halo_vita_handle *reference, void *buffer,
    DWORD count, DWORD *transferred, BOOL writing, BOOL positioned, uint64_t offset);
/* Shutdown owner only; drain transfers before engine buffers are freed. */
int halo_vita_file_async_shutdown(void);
BOOL halo_vita_file_submit_event(HANDLE file, void *buffer, DWORD count,
    LPOVERLAPPED request, BOOL writing);
#endif
