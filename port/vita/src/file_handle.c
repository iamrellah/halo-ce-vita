#include "halo_vita_file_handle.h"
#include "halo_vita_files.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define SHARE_READ 1U
#define SHARE_WRITE 2U
#define SHARE_DELETE 4U
#define SHARING_VIOLATION 32U
static pthread_mutex_t open_lock = PTHREAD_MUTEX_INITIALIZER;
static struct halo_vita_file *open_files;
static void lock_open(void) { if (pthread_mutex_lock(&open_lock)) abort(); }
static void unlock_open(void) { if (pthread_mutex_unlock(&open_lock)) abort(); }

static void close_file(void *data)
{
    struct halo_vita_file *file = data;
    struct halo_vita_file **link;
    lock_open();
    for (link = &open_files; *link && *link != file; link = &(*link)->next) {}
    if (*link) *link = file->next;
    close(file->fd);
    unlock_open();
    pthread_mutex_destroy(&file->io_lock);
    free(file);
}

static int permits(DWORD sharing, DWORD access)
{
    return (!(access & GENERIC_READ) || (sharing & SHARE_READ)) &&
           (!(access & GENERIC_WRITE) || (sharing & SHARE_WRITE));
}

HANDLE WINAPI CreateFileA(const char *name, DWORD access, DWORD sharing,
    void *security, DWORD disposition, DWORD attributes, HANDLE template_file)
{
    struct halo_vita_file *file, *other;
    struct stat st;
    HANDLE handle;
    int flags, writing, existed, error;
    DWORD allowed = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED |
        FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING;
    if (security || template_file || (access & ~(GENERIC_READ | GENERIC_WRITE)) ||
        (sharing & ~(SHARE_READ | SHARE_WRITE | SHARE_DELETE)) || (attributes & ~allowed)) {
        SetLastError(ERROR_INVALID_PARAMETER); return INVALID_HANDLE_VALUE;
    }
    flags = (access & GENERIC_WRITE) ? ((access & GENERIC_READ) ? O_RDWR : O_WRONLY) : O_RDONLY;
    switch (disposition) {
    case CREATE_NEW: flags |= O_CREAT | O_EXCL; break;
    case CREATE_ALWAYS: flags |= O_CREAT | O_TRUNC; break;
    case OPEN_EXISTING: break;
    case OPEN_ALWAYS: flags |= O_CREAT; break;
    case TRUNCATE_EXISTING: flags |= O_TRUNC; break;
    default: SetLastError(ERROR_INVALID_PARAMETER); return INVALID_HANDLE_VALUE;
    }
    if ((flags & O_TRUNC) && !(access & GENERIC_WRITE)) {
        SetLastError(ERROR_ACCESS_DENIED); return INVALID_HANDLE_VALUE;
    }
    writing = (access & GENERIC_WRITE) || (flags & (O_CREAT | O_TRUNC));
    file = calloc(1, sizeof(*file));
    if (!file) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return INVALID_HANDLE_VALUE; }
    if (halo_vita_translate_path(name, file->path, sizeof(file->path), writing)) {
        error = errno; free(file); halo_vita_set_last_error_from_errno(error); return INVALID_HANDLE_VALUE;
    }
    error = pthread_mutex_init(&file->io_lock, NULL);
    if (error) { free(file); halo_vita_set_last_error_from_errno(error); return INVALID_HANDLE_VALUE; }
    lock_open();
    for (other = open_files; other; other = other->next) {
        if (!strcasecmp(file->path, other->path) &&
            (!permits(other->sharing, access) || !permits(sharing, other->access))) {
            unlock_open(); pthread_mutex_destroy(&file->io_lock); free(file);
            SetLastError(SHARING_VIOLATION); return INVALID_HANDLE_VALUE;
        }
    }
    existed = stat(file->path, &st) == 0;
    if (existed && S_ISDIR(st.st_mode)) {
        unlock_open(); pthread_mutex_destroy(&file->io_lock); free(file);
        SetLastError(ERROR_ACCESS_DENIED); return INVALID_HANDLE_VALUE;
    }
    file->fd = open(file->path, flags, 0666);
    if (file->fd < 0) {
        error = errno; unlock_open(); pthread_mutex_destroy(&file->io_lock); free(file);
        halo_vita_set_last_error_from_errno(error); return INVALID_HANDLE_VALUE;
    }
    file->access = access; file->sharing = sharing; file->flags = attributes;
    file->next = open_files; open_files = file;
    /* Close callbacks cannot run until this private token is published. */
    handle = halo_vita_handle_create(HALO_VITA_HANDLE_FILE, file, close_file);
    if (handle == INVALID_HANDLE_VALUE) {
        DWORD saved_error = GetLastError();
        open_files = file->next; close(file->fd); unlock_open();
        pthread_mutex_destroy(&file->io_lock); free(file); SetLastError(saved_error);
        return handle;
    }
    unlock_open();
    SetLastError(existed && (disposition == OPEN_ALWAYS || disposition == CREATE_ALWAYS) ?
        ERROR_ALREADY_EXISTS : ERROR_SUCCESS);
    return handle;
}
