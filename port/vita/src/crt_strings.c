/* MSVC narrow comparisons. Keep unsigned-byte promotion before ctype calls;
 * signed char inputs otherwise invoke undefined behavior for bytes >= 0x80.
 * Uses the active C locale, as the upstream native-port implementation does. */
#include <ctype.h>
#include <stddef.h>

int _stricmp(const char *left, const char *right)
{
    for (;;) {
        int a=tolower((unsigned char)*left++);
        int b=tolower((unsigned char)*right++);
        if (a!=b || !a) return a-b;
    }
}

int _strnicmp(const char *left, const char *right, size_t count)
{
    while (count--) {
        int a=tolower((unsigned char)*left++);
        int b=tolower((unsigned char)*right++);
        if (a!=b || !a) return a-b;
    }
    return 0;
}
