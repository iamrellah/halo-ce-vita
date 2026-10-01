#ifndef HALO_VITA_NATIVE_STAT_H
#define HALO_VITA_NATIVE_STAT_H
#include <psp2/io/stat.h>
/* Retains Newlib descriptor during operation; never reopens a filename.
 * Caller retains Win32 handle and holds its I/O lock. bits=0 reads stat. */
int halo_vita_native_stat(int fd,SceIoStat *stat,unsigned bits);
#endif
