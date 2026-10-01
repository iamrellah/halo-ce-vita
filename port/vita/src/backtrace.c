#include "halo_vita_backtrace.h"
#include <unwind.h>
struct trace_state { uintptr_t *frames; size_t capacity,count; unsigned skip; };
static _Unwind_Reason_Code record_frame(struct _Unwind_Context *context,void *arg)
{
    struct trace_state *state=arg;
    uintptr_t pc=(uintptr_t)_Unwind_GetIP(context);
    if (!pc) return _URC_END_OF_STACK;
    if (state->skip) { state->skip--;return _URC_NO_REASON; }
    if (state->count==state->capacity) return _URC_END_OF_STACK;
    state->frames[state->count++]=pc;
    return _URC_NO_REASON;
}
size_t halo_vita_backtrace(uintptr_t *frames,size_t capacity,unsigned skip)
{
    struct trace_state state;
    if (!frames || !capacity || skip==~0u) return 0;
    state.frames=frames;state.capacity=capacity;state.count=0;state.skip=skip+1;
    _Unwind_Backtrace(record_frame,&state);
    return state.count;
}
