#include "halo_vita_native_stat.h"
#include "../vendor/vitadescriptor.h"
#include <stddef.h>
/* Matched to installed Newlib _fstat_r disassembly; build fingerprints checked. */
typedef char short_enum_required[sizeof(DescriptorTypes)==1?1:-1];
typedef char uid_offset_required[offsetof(DescriptorTranslation,sce_uid)==0?1:-1];
typedef char type_offset_required[offsetof(DescriptorTranslation,type)==4?1:-1];
int halo_vita_native_stat(int fd,SceIoStat *stat,unsigned bits)
{
    DescriptorTranslation *entry=__vita_fd_grab(fd);
    int result;
    if (!entry) return -1;
    if (entry->type!=VITA_DESCRIPTOR_FILE) { __vita_fd_drop(entry);return -1; }
    result=bits ? sceIoChstatByFd(entry->sce_uid,stat,bits) : sceIoGetstatByFd(entry->sce_uid,stat);
    __vita_fd_drop(entry);
    return result;
}
