#include "halo_vita_kernel.h"
#include "halo_vita_files.h"
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

BOOL WINAPI CreateDirectoryA(const char *guest, void *security)
{
    char path[1024];
    int result, error;
    if (security) { SetLastError(120 /* ERROR_CALL_NOT_IMPLEMENTED */); return FALSE; }
    if (halo_vita_translate_path(guest, path, sizeof(path), 1)) {
        halo_vita_set_last_error_from_errno(errno); return FALSE;
    }
    result = mkdir(path, 0777);
    error = errno;
    if (result < 0) {
        if (error == ENOENT) SetLastError(ERROR_PATH_NOT_FOUND);
        else halo_vita_set_last_error_from_errno(error);
        return FALSE;
    }
    return TRUE;
}

BOOL WINAPI RemoveDirectoryA(const char *guest)
{
    char path[1024];
    int result, error;
    if (halo_vita_translate_path(guest, path, sizeof(path), 1)) {
        halo_vita_set_last_error_from_errno(errno); return FALSE;
    }
    result = rmdir(path);
    error = errno;
    if (result < 0) {
        if (error == ENOENT) SetLastError(ERROR_PATH_NOT_FOUND);
        else halo_vita_set_last_error_from_errno(error);
        return FALSE;
    }
    return TRUE;
}
