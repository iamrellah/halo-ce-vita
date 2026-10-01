/* Called once by native startup before launching engine threads.
 * Return Vita API errors unchanged, or -1 with errno for validation failures. */
#include "halo_vita_files.h"
#include <psp2/io/stat.h>
#include <errno.h>

static int ensure_directory(const char *path)
{
    SceIoStat status;
    int result = sceIoGetstat(path, &status);
    if (result >= 0) {
        if (SCE_S_ISDIR(status.st_mode)) return 0;
        errno = ENOTDIR;
        return -1;
    }
    result = sceIoMkdir(path, 0777);
    if (result >= 0) return 0;
    /* Another initializer may have created it between stat and mkdir. */
    if (sceIoGetstat(path, &status) >= 0 && SCE_S_ISDIR(status.st_mode)) return 0;
    return result;
}

int halo_vita_files_initialize(void)
{
    static const char *const roots[] = {
        "ux0:data",
        "ux0:data/xita-native",
        "ux0:data/xita-native/2342",
        "ux0:data/xita-native/2342/data",
        "ux0:data/xita-native/2342/saves"
    };
    unsigned int i;
    int result;
    char drive;
    for (i = 0; i < sizeof(roots) / sizeof(roots[0]); i++) {
        result = ensure_directory(roots[i]);
        if (result < 0) return result;
    }
    /* Xbox drive roots exist independently of save-directory creation. */
    for (drive = 'a'; drive <= 'z'; drive++) {
        char guest_root[] = "a:\\";
        char path[128];
        if (drive == 'd') continue;
        guest_root[0] = drive;
        if (halo_vita_translate_path(guest_root, path, sizeof(path), 1))
            return -1;
        result = ensure_directory(path);
        if (result < 0) return result;
    }
    return 0;
}
