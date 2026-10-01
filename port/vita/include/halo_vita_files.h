#ifndef HALO_VITA_FILES_H
#define HALO_VITA_FILES_H
#include <stdio.h>
#include <stddef.h>
/* Prototype-specific roots; never share the existing Xita save namespace. */
/* Startup only; returns zero, a negative Vita API error, or -1 with errno. */
int halo_vita_files_initialize(void);
int halo_vita_translate_path(const char *path, char *out, size_t capacity, int writing);
FILE *halo_vita_fopen(const char *path, const char *mode);
FILE *halo_vita_freopen(const char *path, const char *mode, FILE *stream);
int halo_vita_remove(const char *path);
#define fopen halo_vita_fopen
#define freopen halo_vita_freopen
#define remove halo_vita_remove
#endif
