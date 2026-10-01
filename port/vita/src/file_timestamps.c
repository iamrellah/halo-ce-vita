#include "halo_vita_file_handle.h"
#include "halo_vita_native_stat.h"
#include <psp2/rtc.h>
#include <string.h>
#include <stdlib.h>
#ifndef ERROR_INVALID_DATA
#define ERROR_INVALID_DATA 13
#endif

static int encode(const SceDateTime *date,FILETIME *value)
{
    SceUInt64 ticks;
    if (sceRtcCheckValid(date)<0 || sceRtcGetWin32FileTime(date,&ticks)<0) return -1;
    value->dwLowDateTime=(DWORD)ticks;value->dwHighDateTime=(DWORD)(ticks>>32);
    return 0;
}
static int decode(const FILETIME *value,SceDateTime *date)
{
    SceUInt64 ticks=((SceUInt64)value->dwHighDateTime<<32)|value->dwLowDateTime;
    /* Timestamp-update suppression sentinel is not supported by this backend. */
    if (ticks==UINT64_MAX || sceRtcSetWin32FileTime(date,ticks)<0 || sceRtcCheckValid(date)<0) return -1;
    return 0;
}
BOOL WINAPI GetFileTime(HANDLE token,FILETIME *creation,FILETIME *access,FILETIME *write)
{
    struct halo_vita_handle *ref=halo_vita_handle_acquire(token,HALO_VITA_HANDLE_FILE);
    struct halo_vita_file *file;
    SceIoStat stat;
    FILETIME c,a,w;
    int result;
    if (!ref) return FALSE;
    file=halo_vita_handle_data(ref);
    memset(&stat,0,sizeof(stat));
    if(pthread_mutex_lock(&file->io_lock))abort();
    result=halo_vita_native_stat(file->fd,&stat,0);
    if(pthread_mutex_unlock(&file->io_lock))abort();
    halo_vita_handle_release(ref);
    if(result<0) { SetLastError(ERROR_GEN_FAILURE);return FALSE; }
    if ((creation && encode(&stat.st_ctime,&c)<0) ||
        (access && encode(&stat.st_atime,&a)<0) || (write && encode(&stat.st_mtime,&w)<0)) {
        SetLastError(ERROR_INVALID_DATA);return FALSE;
    }
    if(creation)*creation=c;
    if(access)*access=a;
    if(write)*write=w;
    return TRUE;
}
BOOL WINAPI SetFileTime(HANDLE token,const FILETIME *creation,const FILETIME *access,const FILETIME *write)
{
    struct halo_vita_handle *ref=halo_vita_handle_acquire(token,HALO_VITA_HANDLE_FILE);
    struct halo_vita_file *file;
    SceIoStat stat;
    unsigned bits=0;
    int result=0;
    if(!ref)return FALSE;
    file=halo_vita_handle_data(ref);
    if(!(file->access&GENERIC_WRITE)) {
        halo_vita_handle_release(ref);SetLastError(ERROR_ACCESS_DENIED);return FALSE;
    }
    memset(&stat,0,sizeof(stat));
    if((creation && decode(creation,&stat.st_ctime)<0) ||
       (access && decode(access,&stat.st_atime)<0) || (write && decode(write,&stat.st_mtime)<0)) {
        halo_vita_handle_release(ref);SetLastError(ERROR_INVALID_PARAMETER);return FALSE;
    }
    if(creation)bits|=SCE_CST_CT;
    if(access)bits|=SCE_CST_AT;
    if(write)bits|=SCE_CST_MT;
    if(bits) {
        if(pthread_mutex_lock(&file->io_lock))abort();
        result=halo_vita_native_stat(file->fd,&stat,bits);
        if(pthread_mutex_unlock(&file->io_lock))abort();
    }
    halo_vita_handle_release(ref);
    if(result<0) { SetLastError(ERROR_GEN_FAILURE);return FALSE; }
    return TRUE;
}
