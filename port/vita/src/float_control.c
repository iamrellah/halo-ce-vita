#include <float.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

static uint32_t read_fpscr(void)
{
    uint32_t value;
    __asm__ volatile("vmrs %0, fpscr" : "=r"(value));
    return value;
}
static void write_fpscr(uint32_t value)
{
    __asm__ volatile("vmsr fpscr, %0" :: "r"(value) : "memory");
}
static unsigned exceptions(uint32_t flags)
{
    return ((flags&1)?_EM_INVALID:0)|((flags&2)?_EM_ZERODIVIDE:0)|
        ((flags&4)?_EM_OVERFLOW:0)|((flags&8)?_EM_UNDERFLOW:0)|
        ((flags&16)?_EM_INEXACT:0)|((flags&128)?_EM_DENORMAL:0);
}
static unsigned control_word(uint32_t value)
{
    static const unsigned rounds[4]={_RC_NEAR,_RC_UP,_RC_DOWN,_RC_CHOP};
    /* PC_53 identifies the supported startup compatibility request. ARM still
     * evaluates float/double at their C types, NOT emulated x87 precision. */
    return _PC_53|rounds[(value>>22)&3]|exceptions(~(value>>8));
}
unsigned _control87(unsigned new_value,unsigned mask)
{
    uint32_t value=read_fpscr();
    unsigned current=control_word(value),requested;
    if (!mask) return current;
    mask&=_MCW_EM|_MCW_RC|_MCW_PC|_MCW_IC;
    requested=(current&~mask)|(new_value&mask);
    /* Only the engine's PC_53/masked-exception startup policy is supported.
     * Refuse unsupported precision/traps instead of claiming they took effect. */
    if ((requested&_MCW_PC)!=_PC_53 || (requested&_MCW_EM)!=_MCW_EM ||
        (requested&_MCW_IC)) abort();
    value&=~((3u<<22)|(0x9fu<<8));
    switch(requested&_MCW_RC) {
    case _RC_UP:value|=1u<<22;break;
    case _RC_DOWN:value|=2u<<22;break;
    case _RC_CHOP:value|=3u<<22;break;
    }
    /* Preserve sticky status, flush-to-zero/default-NaN and unrelated state. */
    write_fpscr(value);
    return control_word(read_fpscr());
}
unsigned _controlfp(unsigned value,unsigned mask) { return _control87(value,mask&~_EM_DENORMAL); }
unsigned _statusfp(void) { return exceptions(read_fpscr()); }
unsigned _clearfp(void)
{
    uint32_t value=read_fpscr();
    write_fpscr(value&~0x9fu);
    return exceptions(value);
}
long fast_ftol_C(float value)
{
    uint32_t bits;
    float converted;
    int32_t result;
    typedef char long_must_be_32[sizeof(long)==4?1:-1];
    memcpy(&bits,&value,4);
    /* x87 FISTP's integer-indefinite result, including NaN/infinity. VFP alone
     * returns different saturated values on invalid conversion. */
    if ((bits&0x7fffffffu)>=0x4f000000u && bits!=0xcf000000u) {
        write_fpscr(read_fpscr()|1u);
        return INT32_MIN;
    }
    __asm__ volatile("vcvtr.s32.f32 %0, %1" : "=t"(converted) : "t"(value));
    memcpy(&result,&converted,4);
    return result;
}
