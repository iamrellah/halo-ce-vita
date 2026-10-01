/* Native-source prototype file namespace. Win32 handle APIs must use this
 * same mapper when implemented. No existing Xita asset/save path is reused. */
#include "halo_vita_files.h"
#include <errno.h>
#include <string.h>
#undef fopen
#undef freopen
#undef remove

int halo_vita_translate_path(const char *path, char *out, size_t capacity, int writing)
{
    const char *cursor, *root;
    char drive = 'd';
    size_t used, length;
    if (!path || !out || !capacity) { errno = EINVAL; return -1; }
    out[0] = 0;
    cursor = path;
    if (cursor[0] && cursor[1] == ':') {
        drive = cursor[0];
        if (drive >= 'A' && drive <= 'Z') drive += 'a' - 'A';
        if (drive < 'a' || drive > 'z') { errno = EINVAL; return -1; }
        cursor += 2;
    }
    if (drive == 'd' && writing) { errno = EROFS; return -1; }
    root = drive == 'd' ? "ux0:data/xita-native/2342/data" : "ux0:data/xita-native/2342/saves";
    used = strlen(root);
    if (used + (drive == 'd' ? 1 : 3) > capacity) { errno = ENAMETOOLONG; return -1; }
    memcpy(out, root, used);
    if (drive != 'd') { out[used++] = '/'; out[used++] = drive; }
    out[used] = 0;
    while (*cursor) {
        const char *start;
        while (*cursor == '/' || *cursor == '\\') cursor++;
        start = cursor;
        while (*cursor && *cursor != '/' && *cursor != '\\') {
            if (*cursor == ':') { errno = EINVAL; goto fail; }
            cursor++;
        }
        length = (size_t)(cursor - start);
        if (!length || (length == 1 && start[0] == '.')) continue;
        if (length == 2 && start[0] == '.' && start[1] == '.') { errno = EACCES; goto fail; }
        if (length >= capacity - used - 1) { errno = ENAMETOOLONG; goto fail; }
        out[used++] = '/';
        memcpy(out + used, start, length);
        used += length;
        out[used] = 0;
    }
    return 0;
fail:
    out[0] = 0;
    return -1;
}

static int write_mode(const char *mode)
{
    return mode[0] == 'w' || mode[0] == 'a' || strchr(mode, '+') != NULL;
}

FILE *halo_vita_fopen(const char *path, const char *mode)
{
    char translated[1024];
    if (!mode) { errno = EINVAL; return NULL; }
    if (halo_vita_translate_path(path, translated, sizeof(translated), write_mode(mode))) return NULL;
    return fopen(translated, mode);
}

FILE *halo_vita_freopen(const char *path, const char *mode, FILE *stream)
{
    char translated[1024];
    /* A null path changes stream mode without a path to check. Reject until
     * stream ownership tracking can enforce the read-only game-data rule. */
    if (!path || !mode || !stream) { errno = EINVAL; return NULL; }
    if (halo_vita_translate_path(path, translated, sizeof(translated), write_mode(mode))) return NULL;
    return freopen(translated, mode, stream);
}

int halo_vita_remove(const char *path)
{
    char translated[1024];
    if (halo_vita_translate_path(path, translated, sizeof(translated), 1)) return -1;
    return remove(translated);
}
