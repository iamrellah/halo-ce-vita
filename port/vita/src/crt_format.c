/* Narrow legacy MSVC formatting bridge. Wide conversions are deliberately
 * rejected: engine wchar_t is 16-bit, while Vita newlib expects 32-bit. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>

static int digit(char c) { return c >= '0' && c <= '9'; }
static int contains(const char *set, char c) { return c && strchr(set,c)!=NULL; }

/* Translation never grows the string: I64 -> ll, I32 -> empty.
 * Parse conversions, not literal text or escaped percent signs. */
static int translate(const char *src, char *dst)
{
    while (*src) {
        const char *length;
        size_t n;
        char conversion;
        *dst++=*src;
        if (*src++!='%') continue;
        if (*src=='%') { *dst++=*src++; continue; }
        while (contains("-+ #0",*src)) *dst++=*src++;
        if (*src=='*') *dst++=*src++;
        else while (digit(*src)) *dst++=*src++;
        if (*src=='.') {
            *dst++=*src++;
            if (*src=='*') *dst++=*src++;
            else while (digit(*src)) *dst++=*src++;
        }
        length=src;
        if (strncmp(src,"I64",3)==0 || strncmp(src,"I32",3)==0) src+=3;
        else if (contains("hl",*src)) { char c=*src++; if (*src==c) ++src; }
        else if (contains("LwIztj",*src)) ++src;
        n=(size_t)(src-length);
        conversion=*src;
        if (!contains("diouxXfFeEgGaAcspn",conversion)) return -1;
        if (contains("cs",conversion)) {
            if (n && !(n==1 && *length=='h')) return -1;
            /* %hs and %hc are narrow in MSVC. */
        } else if (contains("diouxXn",conversion)) {
            if (n==3) {
                if (length[1]=='6') { *dst++='l'; *dst++='l'; }
            } else {
                if (n && !contains("hl",*length)) return -1;
                while (length<src) *dst++=*length++;
            }
        } else {
            /* Pointer presentation and long-double ABI need their own audit. */
            if (conversion=='p' || n) return -1;
        }
        *dst++=*src++;
    }
    *dst=0;
    return 0;
}

int _vsnprintf(char *buffer, size_t count, const char *format, va_list args)
{
    char *translated, *scratch;
    size_t length, copied;
    int result;
    va_list copy;
    if (!format || (!buffer && count) || count>(size_t)INT_MAX) {
        errno=EINVAL; return -1;
    }
    length=strlen(format);
    if (length==SIZE_MAX) { errno=EOVERFLOW; return -1; }
    translated=malloc(length+1);
    if (!translated) { errno=ENOMEM; return -1; }
    if (translate(format,translated)<0) {
        free(translated); errno=EINVAL; return -1;
    }
    /* One extra byte lets newlib produce all count payload bytes. Call once,
     * including for %n, rather than evaluating the argument list twice. */
    scratch=malloc(count+1);
    if (!scratch) { free(translated); errno=ENOMEM; return -1; }
    __builtin_va_copy(copy,args);
    result=vsnprintf(scratch,count+1,translated,copy);
    va_end(copy);
    if (result>=0) {
        copied=(size_t)result<count ? (size_t)result+1 : count;
        if (copied) memcpy(buffer,scratch,copied);
        if ((size_t)result>count) result=-1;
    }
    free(scratch);
    free(translated);
    return result;
}

int _snprintf(char *buffer, size_t count, const char *format, ...)
{
    va_list args;
    int result;
    va_start(args,format);
    result=_vsnprintf(buffer,count,format,args);
    va_end(args);
    return result;
}
