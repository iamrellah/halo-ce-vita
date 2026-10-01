#include "halo_vita_texture_state.h"
#include "halo_vita_fragment_uniforms.h"
#include "halo_vita_fragment_program.h"
#include "halo_vita_pixel_state.h"
#include "halo_vita_attributes.h"
/* Standalone physical-Vita test, not an engine entry point. */
#include "halo_vita_file_handle.h"
#include "halo_vita_allocation.h"
#include "halo_vita_d3d_view.h"
#include "halo_vita_d3d_surface.h"
#include "halo_vita_backbuffer.h"
#include "halo_vita_d3d_render.h"
#include "halo_vita_d3d_state.h"
#include "halo_vita_topology.h"
#include "halo_vita_streams.h"
#include "halo_vita_buffer.h"
#include "halo_vita_vertex_buffer.h"
#include "halo_vita_indices.h"
#include "halo_vita_constants.h"
#include "halo_vita_vertex_program.h"
#include "halo_vita_index_snapshot.h"
#include "halo_vita_backtrace.h"
#include "halo_vita_events.h"
#include "halo_vita_files.h"
#include "halo_vita_relocation.h"
#include "halo_vita_arenas.h"
#include "halo_vita_graphics.h"
#include "halo_vita_surfaces.h"
#include "halo_vita_logical_targets.h"
#include "halo_vita_present.h"
#include "halo_vita_device.h"
#include "halo_vita_clear_draw.h"
#include "halo_vita_texture_draw.h"
#include "halo_vita_shader_patcher.h"
#include "cache/physical_memory_map.h"
#include "halo_vita_texture.h"
#include "halo_vita_texture_upload.h"
#include "halo_vita_texture_resource.h"
#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <float.h>
#undef fopen
static FILE *report;
static unsigned checks;
#define CHECK(x) do { __atomic_add_fetch(&checks, 1, __ATOMIC_RELAXED); if (!(x)) { fprintf(report,"FAIL line %d: %s error=%lu\n",__LINE__,#x,GetLastError()); fflush(report); abort(); } } while (0)
static void test_d3d_views(void)
{
    unsigned char pixels[128];
    /* Xbox pixel-container prefix, 16x2 at 64-byte pitch, one mip. */
    uint32_t header[5]={0x00040001,0,0,0x00011229,0x0000100f};
    struct halo_vita_d3d_view view,sentinel;
    uint32_t id;
    header[1]=(uint32_t)(uintptr_t)pixels;
    memset(&view,0xa5,sizeof(view));sentinel=view;
    CHECK(halo_vita_allocation_register(pixels,128,HALO_VITA_ALLOCATION_ARGB_LINEAR,&id)==0);
    CHECK(halo_vita_d3d_view_acquire(header,19,&view)<0);
    CHECK(!memcmp(&view,&sentinel,sizeof(view)));
    CHECK(halo_vita_d3d_view_acquire(header,sizeof(header),&view)==0);
    CHECK(view.allocation==id && view.width==16 && view.height==2 && view.pitch==64);
    CHECK(view.pixels==pixels && view.format==HALO_VITA_ALLOCATION_ARGB_LINEAR);
    CHECK(halo_vita_allocation_unregister(id)<0);
    CHECK(halo_vita_allocation_release(view.allocation)==0);
    header[0]=0x00050001; /* Same prefix in a copied surface header. */
    CHECK(halo_vita_d3d_view_acquire(header,sizeof(header),&view)==0);
    CHECK(halo_vita_allocation_release(view.allocation)==0);
    header[3]=0x00013f29; /* ABGR cannot silently reinterpret ARGB storage. */
    CHECK(halo_vita_d3d_view_acquire(header,sizeof(header),&view)<0);
    header[3]=0x0001122d; /* Cube flag. */
    CHECK(halo_vita_d3d_view_acquire(header,sizeof(header),&view)<0);
    header[3]=0x00021229; /* Two mips. */
    CHECK(halo_vita_d3d_view_acquire(header,sizeof(header),&view)<0);
    header[3]=0x00011229;header[4]=0x00001010; /* Row exceeds pitch. */
    CHECK(halo_vita_d3d_view_acquire(header,sizeof(header),&view)<0);
    header[4]=0x0000200f; /* Span exceeds backing allocation. */
    CHECK(halo_vita_d3d_view_acquire(header,sizeof(header),&view)<0);
    CHECK(halo_vita_allocation_unregister(id)==0);
    fprintf(report,"PASS D3D pixel-container view ownership and format checks\n");fflush(report);
}
static void test_allocation_aliases(void)
{
    unsigned char storage[128];
    uint32_t id,alias=0,reused,unchanged=0xdeadbeef;
    CHECK(halo_vita_allocation_register(storage,sizeof(storage),7,&id)==0);
    CHECK(halo_vita_allocation_register(storage+64,64,7,&unchanged)<0);
    CHECK(unchanged==0xdeadbeef);
    CHECK(halo_vita_allocation_resolve(storage+16,32,7,&alias)==0 && alias==id);
    CHECK(halo_vita_allocation_resolve(storage+120,16,7,&unchanged)<0);
    CHECK(halo_vita_allocation_resolve(storage,128,8,&unchanged)<0);
    CHECK(unchanged==0xdeadbeef);
    CHECK(halo_vita_allocation_pin(id)<0);
    CHECK(halo_vita_allocation_retain(id)==0);
    CHECK(halo_vita_allocation_retain(alias)==0);
    CHECK(halo_vita_allocation_pin(alias)==0);
    CHECK(halo_vita_allocation_gpu_busy(id)==1);
    CHECK(halo_vita_allocation_unregister(id)<0);
    CHECK(halo_vita_allocation_release(id)==0);
    CHECK(halo_vita_allocation_unregister(id)<0);
    CHECK(halo_vita_allocation_release(alias)==0);
    CHECK(halo_vita_allocation_unregister(id)<0); /* GPU still owns it */
    CHECK(halo_vita_allocation_release(id)<0);
    CHECK(halo_vita_allocation_retire(id)==0);
    CHECK(halo_vita_allocation_gpu_busy(id)==0);
    CHECK(halo_vita_allocation_retire(id)<0);
    CHECK(halo_vita_allocation_unregister(id)==0);
    CHECK(halo_vita_allocation_register(storage,sizeof(storage),7,&reused)==0 && reused!=id);
    CHECK(halo_vita_allocation_retain(id)<0 && halo_vita_allocation_retire(id)<0);
    CHECK(halo_vita_allocation_unregister(reused)==0);
    CHECK(halo_vita_allocation_register(NULL,128,7,&unchanged)<0);
    CHECK(halo_vita_allocation_register(storage,0,7,&unchanged)<0);
    CHECK(halo_vita_allocation_register((void *)(UINTPTR_MAX-3),8,7,&unchanged)<0);
    CHECK(unchanged==0xdeadbeef);
    fprintf(report,"PASS allocation alias ownership and stale IDs\n");fflush(report);
}
extern int _stricmp(const char *, const char *);
extern int _strnicmp(const char *, const char *, size_t);
extern int _snprintf(char *, size_t, const char *, ...);
__attribute__((noinline)) static size_t trace_inner(uintptr_t *frames,size_t capacity)
{
    size_t count=halo_vita_backtrace(frames,capacity,0);
    __asm__ volatile("" ::: "memory");
    return count;
}
__attribute__((noinline)) static void test_backtrace(void)
{
    uintptr_t frames[9];
    size_t count,i;
    frames[8]=0xdeadbeef;
    count=trace_inner(frames,8);
    CHECK(count>=2 && count<=8 && frames[8]==0xdeadbeef);
    for(i=0;i<count;i++) CHECK(frames[i]!=0);
    CHECK(halo_vita_backtrace(NULL,8,0)==0);
    fprintf(report,"PASS ARM current-thread unwind: %u frames\n",(unsigned)count);fflush(report);
}
extern long fast_ftol_C(float);
static void test_float_control(void)
{
    unsigned saved=_control87(0,0);
    uint32_t nan_bits=0x7fc00000;
    float nan_value;
    CHECK((_control87(CW_DEFAULT,0xfffff)&(_MCW_EM|_MCW_RC|_MCW_PC))==CW_DEFAULT);
    CHECK(fast_ftol_C(2.5f)==2 && fast_ftol_C(3.5f)==4 && fast_ftol_C(-2.5f)==-2);
    _control87(_RC_DOWN,_MCW_RC);
    CHECK(fast_ftol_C(2.5f)==2 && fast_ftol_C(-2.5f)==-3);
    _control87(_RC_UP,_MCW_RC);
    CHECK(fast_ftol_C(2.5f)==3 && fast_ftol_C(-2.5f)==-2);
    _control87(_RC_CHOP,_MCW_RC);
    CHECK(fast_ftol_C(2.5f)==2 && fast_ftol_C(-2.5f)==-2);
    _clearfp();
    memcpy(&nan_value,&nan_bits,4);
    CHECK(fast_ftol_C(nan_value)==INT32_MIN && (_statusfp()&_EM_INVALID));
    CHECK((_clearfp()&_EM_INVALID) && !(_statusfp()&_EM_INVALID));
    CHECK(fast_ftol_C(2147483648.0f)==INT32_MIN);
    _clearfp();
    CHECK(fast_ftol_C(-2147483648.0f)==INT32_MIN && !(_statusfp()&_EM_INVALID));
    _control87(saved,_MCW_RC);
    fprintf(report,"PASS ARM rounding modes and x87 integer-indefinite conversion boundaries\n");fflush(report);
}
struct fp_thread_context { HANDLE ready,go; };
static DWORD WINAPI fp_thread_test(void *argument)
{
    struct fp_thread_context *ctx=argument;
    uint32_t bits=0x7fc00000;
    float value;
    CHECK((_control87(0,0)&_MCW_RC)==_RC_NEAR);
    CHECK(!(_statusfp()&_EM_INVALID));
    CHECK(fast_ftol_C(-2.5f)==-2);
    _control87(_RC_UP,_MCW_RC);
    memcpy(&value,&bits,4);
    CHECK(fast_ftol_C(value)==INT32_MIN);
    CHECK(SetEvent(ctx->ready));
    CHECK(WaitForSingleObject(ctx->go,5000)==WAIT_OBJECT_0);
    CHECK((_control87(0,0)&_MCW_RC)==_RC_UP);
    CHECK((_statusfp()&_EM_INVALID)!=0);
    CHECK(fast_ftol_C(2.5f)==3);
    return 0;
}
static void test_thread_float_isolation(void)
{
    struct fp_thread_context ctx;
    unsigned saved=_control87(0,0);
    DWORD code;
    HANDLE thread;
    ctx.ready=CreateEventA(NULL,FALSE,FALSE,NULL);
    ctx.go=CreateEventA(NULL,FALSE,FALSE,NULL);
    CHECK(ctx.ready && ctx.go);
    _control87(_RC_DOWN,_MCW_RC);_clearfp();
    thread=CreateThread(NULL,65536,fp_thread_test,&ctx,0,NULL);
    CHECK(thread!=NULL);
    CHECK(WaitForSingleObject(ctx.ready,5000)==WAIT_OBJECT_0);
    CHECK((_control87(0,0)&_MCW_RC)==_RC_DOWN);
    CHECK(!(_statusfp()&_EM_INVALID));
    CHECK(fast_ftol_C(-2.5f)==-3);
    CHECK(SetEvent(ctx.go));
    CHECK(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0);
    CHECK(GetExitCodeThread(thread,&code) && code==0);
    CHECK(CloseHandle(thread));CHECK(CloseHandle(ctx.ready));CHECK(CloseHandle(ctx.go));
    _control87(saved,_MCW_RC);_clearfp();
    fprintf(report,"PASS worker FP defaults and rounding/status isolation across waits\n");fflush(report);
}
static void test_file_time(void)
{
    SYSTEMTIME time={0};
    FILETIME value,low={0xffffffff,0},high={0,1};
    CHECK(CompareFileTime(&low,&high)<0 && CompareFileTime(&high,&low)>0);
    CHECK(CompareFileTime(&high,&high)==0);
    time.wYear=1970;time.wMonth=1;time.wDay=1;
    CHECK(SystemTimeToFileTime(&time,&value));
    CHECK((((uint64_t)value.dwHighDateTime<<32)|value.dwLowDateTime)==116444736000000000ULL);
    time.wMilliseconds=123;
    CHECK(SystemTimeToFileTime(&time,&value));
    CHECK((((uint64_t)value.dwHighDateTime<<32)|value.dwLowDateTime)==116444736001230000ULL);
    time.wYear=1900;time.wMonth=2;time.wDay=29;
    value=high;
    CHECK(!SystemTimeToFileTime(&time,&value) && value.dwHighDateTime==1 && value.dwLowDateTime==0);
    time.wYear=2000;
    CHECK(SystemTimeToFileTime(&time,&value));
    time.wMilliseconds=1000;
    CHECK(!SystemTimeToFileTime(&time,&value));
    GetSystemTime(&time);
    CHECK(SystemTimeToFileTime(&time,&value));
    CHECK(time.wDayOfWeek<=6);
    {
        FILETIME c,a,w,actual_c,actual_a,actual_w;
        HANDLE file=CreateFileA("t:\\timestamp-test.bin",GENERIC_READ|GENERIC_WRITE,
            0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
        CHECK(file!=INVALID_HANDLE_VALUE);
        memset(&time,0,sizeof(time));time.wMonth=1;time.wDay=1;
        time.wYear=2020;CHECK(SystemTimeToFileTime(&time,&c));
        time.wYear=2021;CHECK(SystemTimeToFileTime(&time,&a));
        time.wYear=2022;CHECK(SystemTimeToFileTime(&time,&w));
        CHECK(SetFileTime(file,&c,&a,&w));
        CHECK(GetFileTime(file,&actual_c,&actual_a,&actual_w));
        CHECK(CompareFileTime(&c,&actual_c)==0);
        CHECK(CompareFileTime(&a,&actual_a)==0);
        CHECK(CompareFileTime(&w,&actual_w)==0);
        time.wYear=2023;CHECK(SystemTimeToFileTime(&time,&c));
        CHECK(SetFileTime(file,&c,NULL,NULL));
        CHECK(GetFileTime(file,&actual_c,&actual_a,&actual_w));
        CHECK(CompareFileTime(&c,&actual_c)==0);
        CHECK(CompareFileTime(&a,&actual_a)==0 && CompareFileTime(&w,&actual_w)==0);
        CHECK(CloseHandle(file));
        CHECK(!GetFileTime(file,&actual_c,NULL,NULL));
        file=CreateFileA("t:\\timestamp-test.bin",GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
        CHECK(file!=INVALID_HANDLE_VALUE);
        CHECK(!SetFileTime(file,&c,NULL,NULL) && GetLastError()==ERROR_ACCESS_DENIED);
        CHECK(CloseHandle(file));
    }
    fprintf(report,"PASS UTC filetime conversion, separate creation/access/write times and handle checks\n");fflush(report);
}
static void test_crt_format(void)
{
    char b[96];
    int written=-1;
    memset(b,'!',sizeof(b));
    CHECK(_snprintf(b,4,"abc")==3 && memcmp(b,"abc\0!",5)==0);
    memset(b,'!',sizeof(b));
    CHECK(_snprintf(b,3,"abc")==3 && memcmp(b,"abc!",4)==0);
    memset(b,'!',sizeof(b));
    CHECK(_snprintf(b,3,"abcd")==-1 && memcmp(b,"abc!",4)==0);
    CHECK(_snprintf(b,0,"x")==-1 && b[0]=='a');
    CHECK(_snprintf(NULL,0,"")==0);
    CHECK(_snprintf(b,sizeof(b),"%I64d %I32d",(long long)4294967297LL,7)==12);
    CHECK(strcmp(b,"4294967297 7")==0);
    CHECK(_snprintf(b,sizeof(b),"I64 %%I64d")==9 && strcmp(b,"I64 %I64d")==0);
    CHECK(_snprintf(b,sizeof(b),"%*.*f%n",6,2,1.25,&written)==6);
    CHECK(strcmp(b,"  1.25")==0 && written==6);
    CHECK(_snprintf(b,sizeof(b),"%hs %hc","ab",'c')==4 && strcmp(b,"ab c")==0);
    memset(b,'!',sizeof(b));
    CHECK(_snprintf(b,sizeof(b),"%ls",L"wide")==-1 && b[0]=='!');
    CHECK(_snprintf(b,sizeof(b),"trailing%")==-1 && b[0]=='!');
    CHECK(_snprintf(b,sizeof(b),"%I64")==-1 && b[0]=='!');
    fprintf(report,"PASS legacy narrow formatting boundaries\n"); fflush(report);
}
static void test_file_resize(void)
{
    HANDLE file;
    char data[8];
    DWORD bytes, high=99;
    file=CreateFileA("t:\\resize-test.bin",GENERIC_READ|GENERIC_WRITE,0,NULL,
        CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    CHECK(file!=INVALID_HANDLE_VALUE);
    CHECK(WriteFile(file,"abcdef",6,&bytes,NULL) && bytes==6);
    CHECK(SetFilePointer(file,3,NULL,FILE_BEGIN)==3);
    CHECK(SetEndOfFile(file));
    CHECK(GetFileSize(file,&high)==3 && high==0);
    CHECK(SetFilePointer(file,0,NULL,FILE_CURRENT)==3);
    CHECK(ReadFile(file,data,sizeof(data),&bytes,NULL) && bytes==0);
    CHECK(SetFilePointer(file,8,NULL,FILE_BEGIN)==8);
    CHECK(SetEndOfFile(file) && GetFileSize(file,NULL)==8);
    CHECK(SetFilePointer(file,0,NULL,FILE_CURRENT)==8);
    CHECK(SetFilePointer(file,0,NULL,FILE_BEGIN)==0);
    CHECK(ReadFile(file,data,3,&bytes,NULL) && bytes==3 && !memcmp(data,"abc",3));
    /* Do not require a particular byte value in the extended region. */
    CHECK(SetFilePointer(file,0,NULL,FILE_BEGIN)==0);
    CHECK(SetEndOfFile(file) && GetFileSize(file,NULL)==0);
    CHECK(CloseHandle(file));
    CHECK(!SetEndOfFile(file) && GetLastError()==ERROR_INVALID_HANDLE);
    file=CreateFileA("t:\\resize-test.bin",GENERIC_READ,0,NULL,
        OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    CHECK(file!=INVALID_HANDLE_VALUE);
    CHECK(!SetEndOfFile(file) && GetLastError()==ERROR_ACCESS_DENIED);
    CHECK(GetFileSize(file,NULL)==0);
    CHECK(CloseHandle(file));
    fprintf(report,"PASS file truncation, extension, position and access checks\n"); fflush(report);
}
static void test_crt_strings(void)
{
    const char high1[]={ (char)0x80, 0 }, high2[]={ (char)0xff, 0 };
    CHECK(_stricmp("Levels/B30", "levels/b30")==0);
    CHECK(_stricmp("prefix", "prefix-long")<0);
    CHECK(_strnicmp("EqualA", "equalB", 5)==0);
    CHECK(_strnicmp("EqualA", "equalB", 6)<0);
    CHECK(_strnicmp(NULL, NULL, 0)==0);
    CHECK(_stricmp(high1,high2)<0);
    fprintf(report,"PASS MSVC narrow comparison boundaries\n"); fflush(report);
}
static void test_texture_layout(void)
{
    static unsigned char payload[16896];
    struct halo_vita_bitmap_layout layout={128,32,1,0,15,3,7};
    struct halo_vita_texture_view view;
    CHECK(halo_vita_texture_select(&layout,payload,5504,5,0,&view)==0);
    CHECK(view.source==payload+5472 && view.source_bytes==16);
    CHECK(view.width==4 && view.height==1);
    CHECK(halo_vita_texture_select(&layout,payload,5504,6,0,&view)<0);
    CHECK(view.source==payload+5472); /* failed selection leaves output intact */
    CHECK(halo_vita_texture_select(&layout,payload,5503,5,0,&view)<0);
    layout.width=64;layout.height=64;layout.type=2;layout.format=14;layout.mip_count=6;
    CHECK(halo_vita_texture_select(&layout,payload,sizeof(payload),0,1,&view)==0);
    CHECK(view.source==payload+5632 && view.source_bytes==2048);
    layout.width=10;layout.height=3;layout.type=0;layout.format=0;layout.flags=16;layout.mip_count=0;
    CHECK(halo_vita_texture_select(&layout,payload,256,0,0,&view)==0);
    CHECK(view.source_bytes==192 && view.linear_pitch==64);
    layout.type=1;layout.depth=2;
    CHECK(halo_vita_texture_select(&layout,payload,sizeof(payload),0,0,&view)<0);
    fprintf(report,"PASS Xbox mip/face selection and payload bounds\n"); fflush(report);
}
static void test_texture_resource(void)
{
    struct halo_vita_texture_resource resource=HALO_VITA_TEXTURE_RESOURCE_INITIALIZER;
    struct halo_vita_bitmap_layout layout={4,4,1,0,14,3,0};
    unsigned char payload[128]={0};
    unsigned char loaded=0;
    uint32_t token=0,old;
    void *base;
    CHECK(halo_vita_texture_resource_register(&resource,&layout,payload,sizeof(payload),0,0,&loaded,&token)==0);
    CHECK(token!=0 && resource.upload.uid<0);
    CHECK(halo_vita_texture_resource_prepare(&resource,token)==1);
    CHECK(halo_vita_texture_resource_busy(&resource,token)==1);
    CHECK(halo_vita_texture_resource_unregister(&resource,token)<0);
    CHECK(halo_vita_texture_resource_pin(&resource,token)<0);
    payload[0]=0;payload[1]=0xf8; /* data arrives AFTER registration */
    __atomic_store_n(&loaded,1,__ATOMIC_RELEASE);
    CHECK(halo_vita_texture_resource_prepare(&resource,token)==0);
    base=resource.upload.base;
    CHECK(sceGxmTextureGetFormat(&resource.upload.texture)==SCE_GXM_TEXTURE_FORMAT_UBC1_ABGR);
    CHECK(sceGxmTextureGetType(&resource.upload.texture)==SCE_GXM_TEXTURE_SWIZZLED);
    CHECK(halo_vita_texture_resource_prepare(&resource,token)==0 && resource.upload.base==base);
    CHECK(!halo_vita_texture_resource_get(&resource,token));
    CHECK(halo_vita_texture_resource_pin(&resource,token)==0);
    CHECK(halo_vita_texture_resource_get(&resource,token)!=NULL);
    CHECK(halo_vita_texture_resource_unregister(&resource,token)<0);
    /* No draw was submitted, so this test pin has no GPU references. */
    CHECK(halo_vita_texture_resource_retire(&resource,token)==0);
    CHECK(halo_vita_graphics_create(960,544)==0);
    CHECK(halo_vita_surfaces_create(960,544)==0);
    CHECK(halo_vita_present_initialize()==0);
    CHECK(halo_vita_shader_patcher_create()==0);
    CHECK(halo_vita_texture_draw_create_resource(&resource,token)==0);
    CHECK(resource.upload.base==base);
    CHECK(halo_vita_present_use_texture(&resource,token)<0); /* no active scene */
    CHECK(resource.upload.references==1);
    CHECK(halo_vita_texture_resource_unregister(&resource,token)<0);
    {
        unsigned frame,attempt,slot;
        int result;
        for (frame=0;frame<6;frame++) {
            for (attempt=0;attempt<2000;attempt++) {
                result=halo_vita_present_begin(&slot);
                if (result!=1) break;
                Sleep(1);
            }
            CHECK(result==0);
            {
                unsigned references=resource.upload.references;
                CHECK(halo_vita_present_use_texture(&resource,token)==0);
                CHECK(resource.upload.references==references+1);
                CHECK(halo_vita_present_use_texture(&resource,token)==0);
                CHECK(resource.upload.references==references+1);
            }
            /* Independent regression: a previous draw rejects every fragment.
         * The fullscreen helper must override it before the pixel readback. */
        sceGxmSetTwoSidedEnable(halo_vita_graphics_context(),SCE_GXM_TWO_SIDED_ENABLED);
        sceGxmSetFrontStencilFunc(halo_vita_graphics_context(),SCE_GXM_STENCIL_FUNC_NEVER,
            SCE_GXM_STENCIL_OP_KEEP,SCE_GXM_STENCIL_OP_KEEP,
            SCE_GXM_STENCIL_OP_KEEP,0xff,0xff);
        sceGxmSetBackStencilFunc(halo_vita_graphics_context(),SCE_GXM_STENCIL_FUNC_NEVER,
            SCE_GXM_STENCIL_OP_KEEP,SCE_GXM_STENCIL_OP_KEEP,
            SCE_GXM_STENCIL_OP_KEEP,0xff,0xff);
        CHECK(halo_vita_texture_draw_submit()==0);
            CHECK(resource.upload.references>=2 && resource.upload.references<=4);
            CHECK(halo_vita_present_end()==0);
        }
        CHECK(halo_vita_present_shutdown()==0);
        CHECK(resource.upload.references==1); /* only helper lifetime pin remains */
        for (frame=0;frame<HALO_VITA_SURFACE_SLOTS;frame++) {
            const struct halo_vita_surface_slot *surface=halo_vita_surface_get(frame);
            volatile uint32_t *pixels=surface->pixels;
            CHECK(pixels[272*surface->stride+480]==0xff0000ff);
        }
    }
    CHECK(halo_vita_texture_draw_destroy()==0);
    CHECK(halo_vita_logical_targets_create()==0);
    CHECK(halo_vita_logical_use_texture(&resource,token)<0);
    {
        unsigned attempt;int result=1;
        CHECK(halo_vita_logical_begin(0)==0);
        CHECK(halo_vita_logical_use_texture(&resource,token+1)<0);
        CHECK(halo_vita_logical_use_texture(&resource,token)==0);
        CHECK(resource.upload.references==1);
        CHECK(halo_vita_logical_use_texture(&resource,token)==0 && resource.upload.references==1);
        CHECK(halo_vita_texture_resource_unregister(&resource,token)<0);
        CHECK(halo_vita_logical_end()==0);
        /* Ownership remains until the explicit completion poll. */
        CHECK(resource.upload.references==1);
        for(attempt=0;attempt<2000;attempt++) {
            result=halo_vita_logical_poll(0);if(result!=1)break;Sleep(1);
        }
        CHECK(result==0 && resource.upload.references==0);
    }
    CHECK(halo_vita_logical_targets_destroy()==0);

    CHECK(halo_vita_texture_resource_busy(&resource,token)==0);
    CHECK(halo_vita_texture_resource_unregister(&resource,token)==0);
    CHECK(halo_vita_shader_patcher_destroy()==0);
    CHECK(halo_vita_surfaces_destroy()==0);
    CHECK(halo_vita_graphics_destroy()==0);
    old=token;
    CHECK(halo_vita_texture_resource_register(&resource,&layout,payload,sizeof(payload),0,0,&loaded,&token)==0);
    CHECK(token!=old);
    CHECK(halo_vita_texture_resource_prepare(&resource,old)<0);
    CHECK(halo_vita_texture_resource_pin(&resource,old)<0);
    CHECK(halo_vita_texture_resource_retire(&resource,old)<0);
    CHECK(halo_vita_texture_resource_unregister(&resource,old)<0);
    CHECK(halo_vita_texture_resource_unregister(&resource,token)==0);
    resource.generation=UINT32_MAX;
    CHECK(halo_vita_texture_resource_register(&resource,&layout,payload,sizeof(payload),0,0,&loaded,&token)<0);
    fprintf(report,"PASS deferred texture preparation, retained ownership and stale-generation rejection\n");fflush(report);
}
static void test_bc_chain_descriptor(void)
{
    struct halo_vita_texture_upload upload=HALO_VITA_TEXTURE_UPLOAD_INITIALIZER;
    struct halo_vita_bitmap_layout layout={8,8,1,0,14,3,1};
    unsigned char payload[128]={0};
    unsigned i;
    for(i=0;i<4;i++) { payload[i*8]=0;payload[i*8+1]=0xf8; }
    payload[32]=0xe0;payload[33]=0x07;
    CHECK(halo_vita_texture_upload_create_bc_chain(&upload,&layout,payload,127)<0);
    CHECK(upload.uid<0);
    CHECK(halo_vita_texture_upload_create_bc_chain(&upload,&layout,payload,sizeof(payload))==0);
    CHECK(memcmp(upload.base,payload,40)==0); /* identical red blocks; green mip 1 */
    CHECK(sceGxmTextureGetMipmapCount(&upload.texture)==2);
    CHECK(halo_vita_texture_upload_destroy(&upload)==0);
    layout.height=4;
    CHECK(halo_vita_texture_upload_create_bc_chain(&upload,&layout,payload,sizeof(payload))<0);
    fprintf(report,"PASS experimental square BC chain storage/descriptor only; no mip sampling proved\n");fflush(report);
}
static void test_bc_reorder(void)
{
    unsigned char input[64],output[65];
    const unsigned order[8]={0,4,1,5,2,6,3,7};
    struct halo_vita_texture_view view={input,sizeof(input),16,8,14,0,0,NULL,0};
    unsigned i,j;
    for (i=0;i<8;i++) memset(input+i*8,(int)i,8);
    memset(output,0xcd,sizeof(output));
    CHECK(halo_vita_reorder_bc(&view,output,64)==0);
    for (i=0;i<8;i++) for (j=0;j<8;j++) CHECK(output[i*8+j]==order[i]);
    CHECK(output[64]==0xcd);
    CHECK(halo_vita_reorder_bc(&view,input,64)<0);
    memset(output,0xcd,sizeof(output));
    view.source_bytes=63;
    CHECK(halo_vita_reorder_bc(&view,output,64)<0 && output[0]==0xcd);
    view.source_bytes=64;
    CHECK(halo_vita_reorder_bc(&view,output,63)<0 && output[0]==0xcd);
    view.width=12;
    CHECK(halo_vita_reorder_bc(&view,output,64)<0 && output[0]==0xcd);
    fprintf(report,"PASS rectangular BC block ordering and input/output bounds\n");fflush(report);
}
static void test_texture_adapter(void)
{
    unsigned char source[16] = { 0x00,0xf8,0,0,0,0,0,0 };
    uint32_t output[17], palette[256];
    struct halo_vita_texture_view view = {source,8,4,4,14,0,0,NULL,0};
    unsigned i;
    for (i=0;i<17;i++) output[i]=0xdeadbeef;
    CHECK(halo_vita_decode_texture(&view,output,64)==0);
    for (i=0;i<16;i++) CHECK(output[i]==0xff0000ff); /* DXT1 red */
    CHECK(output[16]==0xdeadbeef);
    memset(source,0,4); memset(source+4,0xff,4);
    CHECK(halo_vita_decode_texture(&view,output,64)==0);
    for (i=0;i<16;i++) CHECK(output[i]==0); /* DXT1 transparent index */
    output[0]=0xdeadbeef;
    view.source_bytes=7;
    CHECK(halo_vita_decode_texture(&view,output,64)<0 && output[0]==0xdeadbeef);
    view.source_bytes=8;
    CHECK(halo_vita_decode_texture(&view,output,63)<0 && output[0]==0xdeadbeef);
    view.width=1;view.height=1;view.bitmap_format=17;view.source_bytes=1;source[0]=7;
    CHECK(halo_vita_decode_texture(&view,output,4)<0 && output[0]==0xdeadbeef);
    memset(palette,0,sizeof(palette));palette[7]=0xff112233;
    view.palette=palette;view.palette_entries=256;
    CHECK(halo_vita_decode_texture(&view,output,4)==0 && output[0]==0xff332211);
    view.bitmap_format=11;view.linear=1;view.linear_pitch=4;view.source_bytes=4;
    source[0]=0x33;source[1]=0x22;source[2]=0x11;source[3]=0x44;
    CHECK(halo_vita_decode_texture(&view,output,4)==0 && output[0]==0x44332211);
    view.source=output;
    CHECK(halo_vita_decode_texture(&view,output,4)<0 && output[0]==0x44332211);
    fprintf(report,"PASS texture known pixels, transparency and buffer rejection\n"); fflush(report);
}
static void test_graphics_surfaces(void)
{
    const struct halo_vita_surface_slot *slot;
    unsigned i;
    CHECK(halo_vita_surfaces_create(961,544)<0);
    CHECK(halo_vita_surfaces_create(640,480)==0);
    CHECK(halo_vita_surfaces_create(640,480)<0);
    for (i=0;i<HALO_VITA_SURFACE_SLOTS;i++) {
        slot=halo_vita_surface_get(i);
        CHECK(slot && slot->pixels && slot->sync);
        CHECK(slot->width==640 && slot->height==480 && slot->stride==640);
        {
            D3DSurface *owned;
            D3DSurface copy;
            struct halo_vita_d3d_view alias;
            unsigned count;
            CHECK(halo_vita_d3d_surface_create(i,&owned)==0);
            copy=*owned;
            CHECK(halo_vita_d3d_view_acquire(&copy,sizeof(copy),&alias)==0);
            CHECK(halo_vita_d3d_resource_retain((D3DResource *)owned,&count)==0 && count==2);
            CHECK(D3DResource_Release((D3DResource *)owned)==1);
            CHECK(halo_vita_allocation_pin(alias.allocation)==0);
            CHECK(D3DResource_Release((D3DResource *)owned)==0);
            CHECK(halo_vita_surfaces_destroy()<0); /* Copied view and GPU pin. */
            CHECK(halo_vita_allocation_release(alias.allocation)==0);
            CHECK(halo_vita_surfaces_destroy()<0); /* GPU alone still protects storage. */
            CHECK(halo_vita_allocation_retire(alias.allocation)==0);
            count=0xdeadbeef;
            CHECK(halo_vita_d3d_resource_release((D3DResource *)&copy,&count)<0);
            CHECK(count==0xdeadbeef); /* Unmanaged copied header must not free anything. */
        }
        {
            D3DSurface surface;
            D3DSURFACE_DESC desc,sentinel;
            uint32_t reference;
            CHECK(halo_vita_d3d_surface_export(i,&surface,&reference)==0);
            CHECK(reference==slot->color_allocation && surface.Data==(uintptr_t)slot->pixels);
            CHECK(surface.Size==0x271df27f && surface.Parent==NULL);
            D3DSurface_GetDesc(&surface,&desc);
            CHECK(desc.Width==640 && desc.Height==480 && desc.Size==640*480*4);
            CHECK(desc.Format==D3DFMT_LIN_A8B8G8R8 && desc.MultiSampleType==0x0011);
            sentinel=desc;
            CHECK(halo_vita_d3d_surface_describe(&surface,sizeof(surface),1,&desc)<0);
            CHECK(!memcmp(&desc,&sentinel,sizeof(desc)));
            CHECK(halo_vita_surfaces_destroy()<0);
            CHECK(halo_vita_allocation_release(reference)==0);
        }
        {
            uint32_t header[5]={0x00050001,0,0,0x00013f29,0x271df27f};
            struct halo_vita_d3d_view view;
            SceGxmTexture texture;
            header[1]=(uint32_t)(uintptr_t)slot->pixels;
            CHECK(halo_vita_d3d_texture_acquire(header,sizeof(header),&view,&texture)==0);
            CHECK(view.allocation==slot->color_allocation);
            CHECK(sceGxmTextureGetData(&texture)==slot->pixels);
            CHECK(sceGxmTextureGetWidth(&texture)==640 && sceGxmTextureGetHeight(&texture)==480);
            CHECK(sceGxmTextureGetFormat(&texture)==SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR);
            CHECK(sceGxmTextureGetStride(&texture)==2560);
            CHECK(halo_vita_surfaces_destroy()<0);
            CHECK(halo_vita_allocation_release(view.allocation)==0);
        }
        CHECK(halo_vita_surface_pin(i)==0);
    }
    CHECK(halo_vita_surface_get(3)==NULL);
    CHECK(halo_vita_surfaces_destroy()<0);
    for (i=0;i<HALO_VITA_SURFACE_SLOTS;i++) CHECK(halo_vita_surface_retire(i)==0);
    CHECK(halo_vita_surface_retire(0)<0);
    {
        uint32_t alias=0;
        slot=halo_vita_surface_get(0);
        CHECK(halo_vita_allocation_resolve(slot->pixels,64,
            HALO_VITA_ALLOCATION_ABGR_LINEAR,&alias)==0 && alias==slot->color_allocation);
        CHECK(halo_vita_allocation_resolve(slot->pixels,64,
            HALO_VITA_ALLOCATION_S8D24_TILED,&alias)<0);
        CHECK(halo_vita_allocation_retain(alias)==0);
        CHECK(halo_vita_surfaces_destroy()<0);
        CHECK(halo_vita_surface_get(0)==slot);
        CHECK(halo_vita_allocation_pin(alias)==0);
        CHECK(halo_vita_allocation_release(alias)==0);
        CHECK(halo_vita_surfaces_destroy()<0);
        CHECK(halo_vita_allocation_retire(alias)==0);
    }
    CHECK(halo_vita_surfaces_destroy()==0);
    CHECK(halo_vita_surface_get(0)==NULL);
    CHECK(halo_vita_surfaces_destroy()==0);
    CHECK(halo_vita_surfaces_create(960,544)==0);
    CHECK(halo_vita_surfaces_destroy()==0);
    fprintf(report,"PASS three color/depth/sync slots and reference ownership (no draws)\n"); fflush(report);
}
static void test_graphics_context(void)
{
    CHECK(halo_vita_graphics_create(0,480)<0);
    CHECK(!halo_vita_graphics_context() && !halo_vita_graphics_target());
    CHECK(halo_vita_graphics_create(640,480)==0);
    CHECK(halo_vita_graphics_context() && halo_vita_graphics_target());
    CHECK(halo_vita_graphics_create(640,480)<0);
    CHECK(halo_vita_graphics_destroy()==0);
    CHECK(!halo_vita_graphics_context() && !halo_vita_graphics_target());
    CHECK(halo_vita_graphics_destroy()==0);
    CHECK(halo_vita_graphics_create(960,544)==0);
    CHECK(halo_vita_graphics_destroy()==0);
    fprintf(report,"PASS GXM context/target create, destroy, recreate (no draws)\n"); fflush(report);
}
static void test_texture_upload(void)
{
    unsigned char source[8]={0,0xf8,0,0,0,0,0,0};
    struct halo_vita_texture_view view={source,8,4,4,14,0,0,NULL,0};
    struct halo_vita_texture_upload upload=HALO_VITA_TEXTURE_UPLOAD_INITIALIZER;
    CHECK(halo_vita_texture_upload_create(&upload,&view)==0);
    CHECK(sceGxmTextureGetWidth(&upload.texture)==4);
    CHECK(sceGxmTextureGetHeight(&upload.texture)==4);
    CHECK(sceGxmTextureGetData(&upload.texture)==upload.base);
    CHECK(((uint32_t *)upload.base)[0]==0xff0000ff);
    CHECK(halo_vita_texture_upload_pin(&upload)==0);
    CHECK(halo_vita_texture_upload_destroy(&upload)<0 && upload.valid);
    /* No draw submitted: release the simulated reference without a GPU wait. */
    CHECK(halo_vita_texture_upload_retire(&upload)==0);
    CHECK(halo_vita_texture_upload_retire(&upload)<0);
    CHECK(halo_vita_texture_upload_destroy(&upload)==0);
    CHECK(halo_vita_texture_upload_destroy(&upload)==0);
    fprintf(report,"PASS texture descriptor/upload ownership (no sampling)\n"); fflush(report);
}
static void test_shader_patcher(void)
{
    CHECK(halo_vita_shader_patcher_create()==0);
    CHECK(halo_vita_shader_patcher_get()!=NULL);
    CHECK(halo_vita_shader_patcher_create()<0);
    CHECK(halo_vita_shader_patcher_retain()==0);
    CHECK(halo_vita_shader_patcher_destroy()<0);
    CHECK(halo_vita_shader_patcher_get()!=NULL);
    CHECK(halo_vita_shader_patcher_release()==0);
    CHECK(halo_vita_shader_patcher_release()<0);
    CHECK(halo_vita_shader_patcher_destroy()==0);
    CHECK(halo_vita_shader_patcher_get()==NULL);
    CHECK(halo_vita_shader_patcher_destroy()==0);
    CHECK(halo_vita_shader_patcher_create()==0);
    CHECK(halo_vita_shader_patcher_destroy()==0);
    fprintf(report,"PASS shader patcher pool lifecycle (no programs registered)\n"); fflush(report);
}
static void test_xbox_device_lifecycle(void)
{
    D3DPRESENT_PARAMETERS params;
    D3DDevice *device,*duplicate;
    D3DCAPS8 caps;
    unsigned cycle;
    memset(&params,0,sizeof(params));
    params.BackBufferWidth=640;params.BackBufferHeight=480;
    params.BackBufferFormat=D3DFMT_A8R8G8B8;params.SwapEffect=D3DSWAPEFFECT_DISCARD;
    params.EnableAutoDepthStencil=1;params.AutoDepthStencilFormat=D3DFMT_D24S8;params.Flags=1;
    CHECK(Direct3DCreate8(0)!=NULL && Direct3DCreate8(1)==NULL);
    params.BackBufferWidth=960;device=(D3DDevice *)(uintptr_t)1;
    CHECK(Direct3D_CreateDevice(0,D3DDEVTYPE_HAL,NULL,0x40,&params,&device)<0 && device==NULL);
    CHECK(!halo_vita_device_is_ready());params.BackBufferWidth=640;
    params.FullScreen_PresentationInterval=2;
    CHECK(Direct3D_CreateDevice(0,D3DDEVTYPE_HAL,NULL,0x40,&params,&device)<0);
    params.FullScreen_PresentationInterval=0;
    for(cycle=0;cycle<2;cycle++) {
        CHECK(Direct3D_CreateDevice(0,D3DDEVTYPE_HAL,NULL,0x40,&params,&device)==0 && device);
        duplicate=(D3DDevice *)(uintptr_t)1;
        CHECK(Direct3D_CreateDevice(0,D3DDEVTYPE_HAL,NULL,0x40,&params,&duplicate)<0 && duplicate==NULL);
        D3DDevice_GetDeviceCaps(&caps);
        CHECK(caps.DeviceType==D3DDEVTYPE_HAL && caps.VertexShaderVersion==0);
        D3DDevice_Present(NULL,NULL,NULL,NULL);
        {
            D3DSurface *color,*depth;
            D3DVIEWPORT8 v={0,0,640,480,0,1};
            unsigned frame;
            D3DDevice_GetBackBuffer(0,0,&color);
            CHECK(D3DDevice_GetDepthStencilSurface(&depth)==0);
            CHECK(halo_vita_d3d_target_set(color,NULL)<0);
            v.Width=641;CHECK(halo_vita_d3d_viewport_set(&v)<0);v.Width=640;
            D3DDevice_SetViewport(&v);
            /* Reuse the same cached headers across both buffer selections. */
            for(frame=0;frame<4;frame++) {
                uint32_t transient_allocation;
                D3DDevice_SetRenderTarget(color,depth);
                CHECK(halo_vita_logical_active_index()==(int)((frame+1)&1));
                D3DDevice_Clear(0,NULL,0xf3,0xff204080,1.0f,0);
                D3DDevice_Clear(0,NULL,0x80,0x7f000000,1.0f,0); /* Preserve RGB. */
                D3D__RenderState[D3DRS_COLORWRITEENABLE]=0x00010101; /* RGB only. */
                D3D__RenderState[D3DRS_ALPHABLENDENABLE]=TRUE;
                D3D__RenderState[D3DRS_SRCBLEND]=D3DBLEND_ONE;
                D3D__RenderState[D3DRS_DESTBLEND]=D3DBLEND_ONE;
                D3D__RenderState[D3DRS_BLENDOP]=D3DBLENDOP_ADD;
                D3D__RenderState[D3DRS_ZENABLE]=FALSE;
                D3D__RenderState[D3DRS_CULLMODE]=D3DCULL_NONE;
                CHECK(halo_vita_color_quad(0x00010203,0)==0);
                /* A second additive draw must be rejected by saved depth state. */
                D3D__RenderState[D3DRS_ZENABLE]=TRUE;
                D3D__RenderState[D3DRS_ZFUNC]=D3DCMP_NEVER;
                CHECK(halo_vita_color_quad(0x00ffffff,0)==0);
                D3D__RenderState[D3DRS_ZENABLE]=FALSE;
                /* Exactly one of complementary cull modes must draw. This
                 * checks complementarity, not absolute winding orientation. */
                D3D__RenderState[D3DRS_CULLMODE]=D3DCULL_CW;
                CHECK(halo_vita_color_quad(0x00010101,0)==0);
                D3D__RenderState[D3DRS_CULLMODE]=D3DCULL_CCW;
                CHECK(halo_vita_color_quad(0x00010101,0)==0);
                {
                    struct vertex { float x,y,z; uint32_t color; };
                    const struct vertex verts[5]={{100,100,0,0x00ffffff},{-1,-1,0,0x00000100},{1,-1,0,0x00000100},
                        {1,1,0,0x00000100},{-1,1,0,0x00000100}};
                    D3DVertexBuffer *buffer;
                    BYTE *data;
                    CHECK(D3DDevice_CreateVertexBuffer(sizeof(verts),8,0,1,&buffer)==0);
                    D3DVertexBuffer_Lock(buffer,0,sizeof(verts),&data,0);
                    memcpy(data,verts,sizeof(verts));
                    CHECK(halo_vita_allocation_resolve(data,sizeof(verts),HALO_VITA_ALLOCATION_VERTEX_BYTES,&transient_allocation)==0);
                    D3DDevice_SetStreamSource(0,buffer,sizeof(struct vertex));
                    D3D__RenderState[D3DRS_CULLMODE]=D3DCULL_NONE;
                    CHECK(halo_vita_color_stream(0,1)==0);
                    CHECK(halo_vita_color_stream(0,2)<0); /* Overruns allocation. */
                    {
                        D3DIndexBuffer *index_buffer;
                        const uint16_t quad[4]={0,1,2,3};
                        CHECK(D3DDevice_CreateIndexBuffer(sizeof(quad),8,D3DFMT_INDEX16,1,&index_buffer)==0);
                        memcpy((void *)(uintptr_t)index_buffer->Data,quad,sizeof(quad));
                        D3DDevice_SetIndices(index_buffer,1); /* Skip dummy vertex. */
                        CHECK(halo_vita_color_indexed(0,D3DPT_QUADLIST,0,4)==0);
                        /* The GPU must consume the immutable copy, not this source. */
                        memset((void *)(uintptr_t)index_buffer->Data,0,sizeof(quad));
                        D3DDevice_SetIndices(NULL,0);
                        CHECK(D3DResource_Release((D3DResource *)index_buffer)==0);
                    }
                    CHECK(D3DResource_Release((D3DResource *)buffer)==0);
                    D3DDevice_SetStreamSource(0,NULL,0);
                    CHECK(halo_vita_vertex_buffers_collect()==0);
                    CHECK(halo_vita_allocation_gpu_busy(transient_allocation)==1);
                }
                {
                    D3DIndexBuffer *indices;
                    struct halo_vita_index_snapshot a,b;
                    const uint16_t input[4]={0,1,2,3},expected[6]={0,1,2,0,2,3};
                    CHECK(D3DDevice_CreateIndexBuffer(sizeof(input),8,D3DFMT_INDEX16,1,&indices)==0);
                    memcpy((void *)(uintptr_t)indices->Data,input,sizeof(input));
                    D3DDevice_SetIndices(indices,7);
                    CHECK(halo_vita_index_snapshot(D3DPT_QUADLIST,0,4,&a)==0);
                    CHECK(a.count==6 && a.base_vertex==7 && a.minimum==0 && a.maximum==3);
                    CHECK(!memcmp(a.data,expected,sizeof(expected)));
                    memset((void *)(uintptr_t)indices->Data,0,sizeof(input));
                    CHECK(halo_vita_index_snapshot(D3DPT_QUADLIST,0,4,&b)==0);
                    CHECK(b.data!=a.data && b.maximum==0 && !memcmp(a.data,expected,sizeof(expected)));
                    CHECK(halo_vita_index_snapshots_destroy()<0);
                    D3DDevice_SetIndices(NULL,0);
                    CHECK(D3DResource_Release((D3DResource *)indices)==0);
                }
                halo_vita_d3d_state_reset();
                {
                    unsigned attempt;
                    int result;
                    const struct halo_vita_surface_slot *target=halo_vita_logical_target_get((frame+1)&1);
                    CHECK(halo_vita_logical_end()==0);
                    for(attempt=0;attempt<2000;attempt++) {
                        result=halo_vita_logical_poll((frame+1)&1);
                        if(result!=1)break;
                        Sleep(1);
                    }
                    CHECK(result==0);
                    CHECK(((volatile uint32_t *)target->pixels)[240*640+320]==0x7f224584);
                    CHECK(halo_vita_vertex_buffers_collect()==0);
                    CHECK(halo_vita_allocation_busy(transient_allocation)<0);
                }
                D3DDevice_Present(NULL,NULL,NULL,NULL);
                CHECK(halo_vita_graphics_scene_active()==0);
            }
            CHECK(D3DResource_Release((D3DResource *)color)==1);
            CHECK(D3DResource_Release((D3DResource *)depth)==1);
        }
        CHECK(D3DDevice_Release()==0 && !halo_vita_device_is_ready());
    }
    fprintf(report,"PASS Xbox device probe/create/present/release/recreate\n");fflush(report);
}
static void test_device_lifecycle(void)
{
    unsigned cycle,attempt,frame;
    int result;
    CHECK(!halo_vita_device_is_ready());
    CHECK(halo_vita_device_test_frame()<0);
    for (cycle=0;cycle<2;cycle++) {
        CHECK(halo_vita_device_create()==0);
        CHECK(halo_vita_device_is_ready());
        CHECK(halo_vita_device_create()<0);
        for (attempt=0;attempt<2000;attempt++) {
            result=halo_vita_device_test_frame();
            if (result!=1) break;
            Sleep(1);
        }
        CHECK(result==0);
        CHECK(halo_vita_logical_target_get(0) && halo_vita_logical_target_get(1));
        for(frame=0;frame<6;frame++) {
            D3DSurface *current;
            D3DDevice_GetBackBuffer(0,0,&current);
            CHECK(current->Data==(uintptr_t)halo_vita_logical_target_get(frame&1)->pixels);
            CHECK(D3DResource_Release((D3DResource *)current)==1);
            for(attempt=0;attempt<2000;attempt++) {
                result=halo_vita_device_present_logical();
                if(result!=1)break;
                Sleep(1);
            }
            CHECK(result==0);
        }
        {
            D3DSurface *retained;
            D3DVertexBuffer *vertex_buffer;
            CHECK(D3DDevice_CreateVertexBuffer(64,8,0,1,&vertex_buffer)==0);
            D3DDevice_GetBackBuffer(0,0,&retained);
            CHECK(halo_vita_device_destroy()<0); /* Caller still owns header. */
            CHECK(D3DResource_Release((D3DResource *)retained)==1);
            CHECK(halo_vita_device_destroy()<0); /* Vertex mapping still owned. */
            CHECK(!halo_vita_vertex_buffers_empty());
            CHECK(D3DResource_Release((D3DResource *)vertex_buffer)==0);
            CHECK(halo_vita_vertex_buffers_empty());
        }
        CHECK(halo_vita_device_destroy()==0);
        CHECK(!halo_vita_device_is_ready());
        CHECK(halo_vita_device_destroy()==0);
    }
    fprintf(report,"PASS integrated device create/draw/present/destroy/recreate\n"); fflush(report);
}
static void test_logical_targets(void)
{
    const struct halo_vita_surface_slot *first,*second;
    uint32_t header[5]={0x00040001,0,0,0x00011229,0x271df27f};
    struct halo_vita_d3d_view view;
    SceGxmTexture texture;
    CHECK(halo_vita_surfaces_create(960,544)==0);
    CHECK(halo_vita_logical_targets_create()==0);
    CHECK(halo_vita_logical_targets_create()<0);
    {
        D3DSurface *a,*b,*again;
        D3DSURFACE_DESC desc;
        CHECK(halo_vita_backbuffers_open()==0);
        CHECK(halo_vita_backbuffers_open()<0);
        D3DDevice_GetBackBuffer(0,0,&a);
        D3DSurface_GetDesc(a,&desc);
        CHECK(desc.Format==D3DFMT_LIN_A8R8G8B8 && desc.Width==640 && desc.Height==480);
        CHECK(a->Data==(uintptr_t)halo_vita_logical_target_get(0)->pixels);
        {
            D3DSurface *depth;
            struct halo_vita_d3d_view view;
            SceGxmTexture texture;
            CHECK(D3DDevice_GetDepthStencilSurface(&depth)==0);
            CHECK(depth->Data==(uintptr_t)halo_vita_logical_target_get(0)->depth_pixels);
            D3DSurface_GetDesc(depth,&desc);
            CHECK(desc.Format==D3DFMT_LIN_D24S8 && desc.Width==640 && desc.Height==480);
            CHECK(halo_vita_d3d_depth_view_acquire(depth,sizeof(*depth),&view)==0);
            CHECK(view.allocation==halo_vita_logical_target_get(0)->depth_allocation);
            CHECK(halo_vita_allocation_release(view.allocation)==0);
            CHECK(halo_vita_d3d_texture_acquire(depth,sizeof(*depth),&view,&texture)<0);
            CHECK(D3DResource_Release((D3DResource *)depth)==1);
        }
        CHECK(halo_vita_backbuffers_close()<0);
        CHECK(halo_vita_backbuffers_select(1)==0);
        D3DDevice_GetBackBuffer(0,0,&b);
        CHECK(a!=b && a->Data!=b->Data);
        CHECK(halo_vita_backbuffers_select(0)==0);
        D3DDevice_GetBackBuffer(0,0,&again);
        CHECK(again==a);
        CHECK(D3DResource_Release((D3DResource *)again)==2);
        CHECK(D3DResource_Release((D3DResource *)a)==1);
        CHECK(D3DResource_Release((D3DResource *)b)==1);
        CHECK(halo_vita_logical_targets_destroy()<0); /* Device owns both headers. */
        CHECK(halo_vita_backbuffers_close()==0);
        CHECK(halo_vita_backbuffers_close()==0);
        CHECK(halo_vita_backbuffer_acquire(&again)<0);
    }
    first=halo_vita_logical_target_get(0);second=halo_vita_logical_target_get(1);
    CHECK(first && second && first->pixels!=second->pixels);
    CHECK(first->pixels!=halo_vita_surface_get(0)->pixels);
    CHECK(first->width==640 && first->height==480 && first->stride==640);
    CHECK(halo_vita_logical_render_target()!=NULL && halo_vita_logical_target_get(2)==NULL);
    header[1]=(uint32_t)(uintptr_t)first->pixels;
    CHECK(halo_vita_d3d_texture_acquire(header,sizeof(header),&view,&texture)==0);
    CHECK(view.allocation==first->color_allocation);
    CHECK(sceGxmTextureGetFormat(&texture)==SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB);
    CHECK(halo_vita_allocation_pin(view.allocation)==0);
    CHECK(halo_vita_allocation_release(view.allocation)==0);
    CHECK(halo_vita_logical_targets_destroy()<0);
    CHECK(halo_vita_logical_target_get(0)==first);
    CHECK(halo_vita_allocation_retire(view.allocation)==0);
    CHECK(halo_vita_logical_targets_destroy()==0);
    CHECK(halo_vita_logical_target_get(0)==NULL && halo_vita_logical_render_target()==NULL);
    CHECK(halo_vita_surfaces_destroy()==0);
    CHECK(halo_vita_logical_targets_create()==0);
    CHECK(halo_vita_logical_targets_destroy()==0);
    fprintf(report,"PASS stable logical ARGB/depth targets separate from display\n");fflush(report);
}
static void test_d3d_texture_draw(void)
{
    const struct halo_vita_surface_slot *logical;
    void *pixels=NULL;
    uint32_t id,header[5]={0x00040001,0,0,0x00011229,0x271df27f};
    unsigned frame,slot,attempt,i;
    int result;
    CHECK(halo_vita_graphics_create(960,544)==0);
    CHECK(halo_vita_surfaces_create(960,544)==0);
    CHECK(halo_vita_present_initialize()==0);
    CHECK(halo_vita_shader_patcher_create()==0);
    CHECK(halo_vita_logical_targets_create()==0);
    logical=halo_vita_logical_target_get(0);
    CHECK(logical!=NULL);
    pixels=logical->pixels;id=logical->color_allocation;
    /* Known CPU fixture in an actual 640x480 logical render allocation.
     * This tests GPU sampling/scaling, not an offscreen render transition. */
    for(i=0;i<640*480;i++) {
        const uint32_t argb[4]={0xff800000,0xff008000,0xff000080,0xff808080};
        unsigned quadrant=((i/640)>=240 ? 2:0)+((i%640)>=320 ? 1:0);
        ((uint32_t *)pixels)[i]=argb[quadrant];
    }
    __asm__ volatile("dsb sy" ::: "memory");
    header[1]=(uint32_t)(uintptr_t)pixels;
    CHECK(halo_vita_texture_draw_create_d3d(header,sizeof(header))==0);
    CHECK(halo_vita_present_use_allocation(id)<0); /* No open scene. */
    for(frame=0;frame<6;frame++) {
        for(attempt=0;attempt<2000;attempt++) {
            result=halo_vita_present_begin(&slot);
            if(result!=1)break;
            Sleep(1);
        }
        CHECK(result==0);
        CHECK(halo_vita_present_use_allocation(halo_vita_surface_get(slot)->color_allocation)<0);
        CHECK(halo_vita_present_use_allocation(halo_vita_surface_get(slot)->depth_allocation)<0);
        /* Independent regression: a previous draw rejects every fragment.
         * The fullscreen helper must override it before the pixel readback. */
        sceGxmSetTwoSidedEnable(halo_vita_graphics_context(),SCE_GXM_TWO_SIDED_ENABLED);
        sceGxmSetFrontStencilFunc(halo_vita_graphics_context(),SCE_GXM_STENCIL_FUNC_NEVER,
            SCE_GXM_STENCIL_OP_KEEP,SCE_GXM_STENCIL_OP_KEEP,
            SCE_GXM_STENCIL_OP_KEEP,0xff,0xff);
        sceGxmSetBackStencilFunc(halo_vita_graphics_context(),SCE_GXM_STENCIL_FUNC_NEVER,
            SCE_GXM_STENCIL_OP_KEEP,SCE_GXM_STENCIL_OP_KEEP,
            SCE_GXM_STENCIL_OP_KEEP,0xff,0xff);
        CHECK(halo_vita_texture_draw_submit()==0);
        CHECK(halo_vita_present_use_allocation(id)==0); /* Must deduplicate. */
        CHECK(halo_vita_allocation_gpu_busy(id)==1);
        CHECK(halo_vita_present_end()==0);
    }
    CHECK(halo_vita_present_shutdown()==0);
    CHECK(halo_vita_allocation_gpu_busy(id)==0);
    CHECK(halo_vita_logical_targets_destroy()<0); /* Draw helper still owns view. */
    for(slot=0;slot<3;slot++) {
        const struct halo_vita_surface_slot *s=halo_vita_surface_get(slot);
        const volatile uint32_t *out=(const volatile uint32_t *)s->pixels;
        CHECK(out[16*s->stride+16]==0xff000080); /* ARGB red -> ABGR red */
        CHECK(out[16*s->stride+943]==0xff008000);
        CHECK(out[527*s->stride+16]==0xff800000);
        CHECK(out[527*s->stride+943]==0xff808080);
    }
    CHECK(halo_vita_texture_draw_destroy()==0);
    CHECK(halo_vita_logical_targets_destroy()==0);
    CHECK(halo_vita_shader_patcher_destroy()==0);
    CHECK(halo_vita_surfaces_destroy()==0);
    CHECK(halo_vita_graphics_destroy()==0);
    fprintf(report,"PASS logical 640x480 ARGB to 960x544 ABGR scaling, scene pins and readback\n");fflush(report);
}
static void test_vertex_catalog(void)
{
    unsigned i,j,found,linked=0;
    for(i=0;i<halo_vita_fragment_count();i++) {
        const struct halo_vita_fragment_entry *entry=halo_vita_fragment_entry(i);
        CHECK(halo_vita_fragment_find(entry->vs_hash,entry->key,entry->c2d_mask,&found)==0 && found==i);
    }
    found=123456;
    CHECK(halo_vita_fragment_find(0,0,16,&found)<0 && found==123456);
    CHECK(halo_vita_fragment_entry(halo_vita_fragment_count())==NULL);
    CHECK(halo_vita_shader_patcher_create()==0);
    CHECK(halo_vita_graphics_create(960,544)==0);
    CHECK(halo_vita_logical_targets_create()==0);
    for(i=0;i<67;i++) {
        struct halo_vita_vertex_program v={0};
        CHECK(halo_vita_vertex_catalog_load(i,&v)==0);
        CHECK(v.program && v.desc && v.attributes);
        for(j=0;j<halo_vita_fragment_count();j++) {
            const struct halo_vita_fragment_entry *entry=halo_vita_fragment_entry(j);
            if(entry->vs_hash==v.desc->func_hash && !entry->c2d_mask) {
                struct halo_vita_fragment_program fragment={0};
                CHECK(halo_vita_fragment_load(j,&v,NULL,&fragment)==0);
                CHECK(fragment.program && fragment.entry==entry);
                {
                    struct halo_vita_fragment_uniforms uniforms={0};
                    struct halo_vita_texture_resource texture_resource=HALO_VITA_TEXTURE_RESOURCE_INITIALIZER;
                    struct halo_vita_bitmap_layout layout={4,4,1,0,14,3,0};
                    unsigned char payload[128]={0},loaded=0;
                    uint32_t token;unsigned stage;
                    unsigned attempt;int result=1;
                    CHECK(halo_vita_texture_resource_register(&texture_resource,&layout,payload,sizeof(payload),0,0,&loaded,&token)==0);
                    halo_vita_texture_state_reset();
                    uniforms.key=entry->key;uniforms.has_texscale=uniforms.has_border=1;
                    CHECK(halo_vita_fragment_bind(&fragment,&uniforms)<0); /* No scene. */
                    CHECK(halo_vita_logical_begin(0)==0);
                    sceGxmSetVertexProgram(halo_vita_graphics_context(),v.program);
                    uniforms.key^=1;
                    CHECK(halo_vita_fragment_bind(&fragment,&uniforms)<0);
                    uniforms.key^=1;
                    CHECK(halo_vita_fragment_bind(&fragment,&uniforms)==0);
                    for(stage=0;stage<4;stage++)if(fragment.textures[stage]>=0 && !(entry->cube_mask&(1u<<stage))) {
                        SceGxmTexture snapshot;
                        if(!loaded) {
                            CHECK(halo_vita_fragment_texture_bind(&fragment,stage,&texture_resource,token,D3D__TextureState[stage],&snapshot)==1);
                            __atomic_store_n(&loaded,1,__ATOMIC_RELEASE);
                        }
                        CHECK(halo_vita_fragment_texture_bind(&fragment,stage,&texture_resource,token,D3D__TextureState[stage],&snapshot)==0);
                        CHECK(sceGxmTextureGetWidth(&snapshot)==4 && texture_resource.upload.references==1);
                        CHECK(halo_vita_texture_resource_unregister(&texture_resource,token)<0);
                    }
                    CHECK(halo_vita_logical_end()==0);
                    for(attempt=0;attempt<2000;attempt++) {
                        result=halo_vita_logical_poll(0);if(result!=1)break;Sleep(1);
                    }
                    CHECK(result==0 && texture_resource.upload.references==0);
                    __atomic_store_n(&loaded,1,__ATOMIC_RELEASE);
                    CHECK(halo_vita_texture_resource_unregister(&texture_resource,token)==0);
                }

                CHECK(halo_vita_shader_patcher_destroy()<0);
                CHECK(halo_vita_fragment_destroy(&fragment)==0);
                linked++;break;
            }
        }
        CHECK(halo_vita_shader_patcher_destroy()<0);
        CHECK(halo_vita_vertex_program_destroy(&v)==0);
    }
    CHECK(halo_vita_shader_patcher_destroy()==0);
    CHECK(halo_vita_logical_targets_destroy()==0);
    CHECK(halo_vita_graphics_destroy()==0);
    CHECK(linked>0);
    fprintf(report,"PASS 67 vertex programs and %u representative fragment links; no draws\n",linked);fflush(report);
}
static void test_vertex_shader_handles(void)
{
    static const DWORD code[]={
#include "../../source/rasterizer/xbox/rasterizer_xbox_vertex_shaders_data.inc"
    };
    static const DWORD decl[]={0x20000000,0x40320000,0x40400009,0xffffffff};
    const struct halo_vita_vertex_program *v=NULL,*saved;
    DWORD a=0,b=0,c=0,bad=0xdeadbeef;UINT size=0;
    unsigned index=999;
    CHECK(halo_vita_shader_patcher_create()==0);
    CHECK(halo_vita_vertex_catalog_find(code,0x74,decl,sizeof(decl),&index)==0 && index==0);
    index=999;
    CHECK(halo_vita_vertex_catalog_find(code,0x70,decl,sizeof(decl),&index)<0 && index==999);
    CHECK(D3DDevice_CreateVertexShader(decl,code,&bad,1)<0 && bad==0xdeadbeef);
    CHECK(D3DDevice_CreateVertexShader(decl,code,&a,0)==0);
    CHECK(D3DDevice_CreateVertexShader(decl,code,&b,0)==0 && a!=b && !(a&1));
    D3DDevice_GetVertexShaderSize(a,&size);CHECK(size==7);
    D3DDevice_SetVertexShader(a);
    CHECK(halo_vita_vertex_shader_current(&v)==0);saved=v;
    {
        D3DVertexBuffer *buffer=NULL;unsigned attempt;int result=1;
        CHECK(halo_vita_graphics_create(960,544)==0);
        CHECK(halo_vita_logical_targets_create()==0);
        CHECK(D3DDevice_CreateVertexBuffer(64,8,0,1,&buffer)==0);
        CHECK(halo_vita_stream_set(0,buffer,16)==0);
        CHECK(halo_vita_vertex_shader_bind(0,4,NULL)<0); /* No scene. */
        CHECK(halo_vita_logical_begin(0)==0);
        CHECK(halo_vita_vertex_shader_bind(2,4,NULL)<0); /* Beyond allocation. */
        CHECK(halo_vita_vertex_shader_bind(0,4,NULL)==0);
        {
            struct halo_vita_stream_view first,again,changed;
            static const DWORD decl1[]={0x20000000,0x40320000,0x40210004,0xffffffff};
            DWORD persistent_shader=0;
            const float *values;
            CHECK(halo_vita_attribute_snapshots_create()==0);
            D3DDevice_SetVertexData4f(9,1,2,3,4);
            CHECK(halo_vita_attributes_snapshot(&first)==0);
            CHECK(halo_vita_attributes_snapshot(&again)==0 && first.data==again.data);
            D3DDevice_SetVertexDataColor(9,0xff00ff00);
            CHECK(halo_vita_attributes_snapshot(&changed)==0 && changed.data!=first.data);
            values=(const float *)first.data;
            CHECK(values[36]==1 && values[37]==2 && values[38]==3 && values[39]==4);
            values=(const float *)changed.data;
            CHECK(values[36]==0 && values[37]==1 && values[38]==0 && values[39]==1);
            CHECK(halo_vita_attribute_snapshots_reset(0)<0);
            CHECK(halo_vita_allocation_release(first.allocation)==0);
            CHECK(halo_vita_allocation_release(again.allocation)==0);
            CHECK(halo_vita_allocation_release(changed.allocation)==0);
            CHECK(D3DDevice_CreateVertexShader(decl1,code+0x78/4,&persistent_shader,0)==0);
            D3DDevice_SetVertexShader(persistent_shader);
            CHECK(halo_vita_vertex_shader_bind(0,4,NULL)==0);
            CHECK(halo_vita_attribute_snapshots_destroy()<0);
            D3DDevice_DeleteVertexShader(persistent_shader);
            D3DDevice_SetVertexShader(a);
        }
        CHECK(halo_vita_vertex_shaders_shutdown()<0);
        CHECK(D3DResource_Release((D3DResource *)buffer)==0);
        CHECK(halo_vita_stream_set(0,NULL,0)==0);
        CHECK(halo_vita_vertex_buffers_collect()==0 && !halo_vita_vertex_buffers_empty());
        CHECK(halo_vita_logical_end()==0);
        for(attempt=0;attempt<2000;attempt++) {
            result=halo_vita_logical_poll(0);if(result!=1)break;Sleep(1);
        }
        CHECK(result==0);
        CHECK(halo_vita_attribute_snapshots_destroy()==0);
        CHECK(halo_vita_vertex_buffers_collect()==0 && halo_vita_vertex_buffers_empty());
        CHECK(halo_vita_logical_targets_destroy()==0);
        CHECK(halo_vita_graphics_destroy()==0);
    }
    D3DDevice_LoadVertexShader(b,20);
    D3DDevice_SelectVertexShader(0,20);
    CHECK(halo_vita_vertex_shader_current(&v)==0 && v==saved);
    D3DDevice_DeleteVertexShader(a);D3DDevice_DeleteVertexShader(b);
    CHECK(halo_vita_vertex_shader_current(&v)==0 && v==saved);
    CHECK(D3DDevice_CreateVertexShader(decl,code,&c,0)==0 && c!=a && c!=b);
    /* Loading [22,29) invalidates the old [20,27) instruction slot. */
    D3DDevice_LoadVertexShader(c,22);
    CHECK(halo_vita_vertex_shader_current(&v)<0);
    D3DDevice_SelectVertexShader(c,22);
    CHECK(halo_vita_vertex_shader_current(&v)==0);
    D3DDevice_DeleteVertexShader(c);
    CHECK(halo_vita_shader_patcher_destroy()<0);
    CHECK(halo_vita_vertex_shaders_shutdown()==0);
    CHECK(halo_vita_vertex_shader_current(&v)<0);
    CHECK(halo_vita_shader_patcher_destroy()==0);
    fprintf(report,"PASS native vertex shader handles, packed slots and deferred lifetime; no draws\n");fflush(report);
}
static void test_texture_stage_state(void)
{
    struct halo_vita_buffer buffer={.uid=-1};
    SceGxmTexture texture,saved;
    halo_vita_texture_state_reset();
    CHECK(halo_vita_buffer_create(&buffer,1024)==0);
    CHECK(sceGxmTextureInitLinearStrided(&texture,buffer.base,SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR,16,16,64)==0);
    D3DDevice_SetTextureState_Deferred(0,D3DTSS_MINFILTER,D3DTEXF_LINEAR);
    D3D__TextureState[0][D3DTSS_ADDRESSU]=D3DTADDRESS_CLAMPTOEDGE;
    D3D__TextureState[0][D3DTSS_ADDRESSV]=D3DTADDRESS_MIRROR;
    CHECK(halo_vita_sampler_apply(D3D__TextureState[0],&texture)==0);
    CHECK(sceGxmTextureGetMinFilter(&texture)==SCE_GXM_TEXTURE_FILTER_LINEAR);
    CHECK(sceGxmTextureGetMagFilter(&texture)==SCE_GXM_TEXTURE_FILTER_POINT);
    CHECK(sceGxmTextureGetUAddrMode(&texture)==SCE_GXM_TEXTURE_ADDR_CLAMP);
    CHECK(sceGxmTextureGetVAddrMode(&texture)==SCE_GXM_TEXTURE_ADDR_MIRROR);
    D3D__TextureState[0][D3DTSS_ADDRESSU]=D3DTADDRESS_BORDER;
    D3DDevice_SetTextureState_BorderColor(0,0xffff0000);
    saved=texture;
    CHECK(halo_vita_sampler_apply(D3D__TextureState[0],&texture)<0 && !memcmp(&texture,&saved,sizeof(texture)));
    D3DDevice_SetTextureState_BorderColor(0,0);
    CHECK(halo_vita_sampler_apply(D3D__TextureState[0],&texture)==0);
    CHECK(sceGxmTextureGetUAddrMode(&texture)==SCE_GXM_TEXTURE_ADDR_CLAMP_FULL_BORDER);
    D3D__TextureState[0][D3DTSS_MINFILTER]=D3DTEXF_ANISOTROPIC;saved=texture;
    CHECK(halo_vita_sampler_apply(D3D__TextureState[0],&texture)<0 && !memcmp(&texture,&saved,sizeof(texture)));
    CHECK(halo_vita_buffer_destroy(&buffer)==0);
    halo_vita_texture_state_reset();
    CHECK(D3D__TextureState[0][D3DTSS_MINFILTER]==D3DTEXF_POINT);
    fprintf(report,"PASS texture stage capture and bounded single-mip sampler mapping; no draws\n");fflush(report);
}
static void test_pixel_state(void)
{
    D3DPIXELSHADERDEF definition={0};
    struct halo_vita_pixel_state first,next,unchanged;
    halo_vita_d3d_state_reset();
    definition.PSCombinerCount=1;definition.PSAlphaInputs[0]=0x12345678;
    definition.PSTextureModes=0x21;
    definition.PSConstant0[0]=0xff00ff00;definition.PSC0Mapping=0x12345678;
    D3DDevice_SetPixelShaderProgram(&definition);
    memset(&definition,0,sizeof(definition)); /* Caller storage no longer owned. */
    CHECK(halo_vita_pixel_state_capture(&first)==0);
    CHECK(first.definition.PSAlphaInputs[0]==0x12345678 && first.definition.PSC0Mapping==0x12345678);
    CHECK(first.definition.PSTextureModes==0x21 && D3D__RenderState[D3DRS_PSTEXTUREMODES]==0x21);
    CHECK(first.constants[0][0]==0 && first.constants[0][1]==1 && first.constants[0][2]==0 && first.constants[0][3]==1);
    {
        struct halo_vita_fragment_uniforms u,saved;
        float scales[16]={1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0};
        float border[8]={0,1,0,1,-64,-32,1,0};unsigned function;
        D3D__RenderState[D3DRS_FOGCOLOR]=0xff0000ff;
        D3D__RenderState[D3DRS_ALPHATESTENABLE]=1;
        D3D__RenderState[D3DRS_ALPHAREF]=255;
        for(function=D3DCMP_NEVER;function<=D3DCMP_ALWAYS;function++) {
            D3D__RenderState[D3DRS_ALPHAFUNC]=function;
            CHECK(halo_vita_fragment_uniforms_capture(&first,D3D__RenderState,scales,border,&u)==0);
            CHECK(u.atest[0]==1 && u.atest[1]==function-D3DCMP_NEVER && u.atest[2]==1);
        }
        CHECK(u.key==first.key && u.fog[0]==0 && u.fog[2]==1 && u.fog[3]==1);
        CHECK(u.has_texscale && u.has_border && !memcmp(u.border,border,sizeof(border)));
        CHECK(!memcmp(u.psc,first.constants,sizeof(u.psc)));
        saved=u;D3D__RenderState[D3DRS_ALPHAFUNC]=0xffffffff;
        CHECK(halo_vita_fragment_uniforms_capture(&first,D3D__RenderState,NULL,NULL,&u)<0 && !memcmp(&u,&saved,sizeof(u)));
        D3D__RenderState[D3DRS_ALPHATESTENABLE]=0;
        CHECK(halo_vita_fragment_uniforms_capture(&first,D3D__RenderState,NULL,NULL,&u)==0);
        CHECK(u.atest[2]==0 && !u.has_texscale && !u.has_border);
    }

    D3D__RenderState[D3DRS_PSCONSTANT0_0]=0xffff0000;
    CHECK(halo_vita_pixel_state_capture(&next)==0 && next.key==first.key && next.hash==first.hash);
    CHECK(next.constants[0][0]==1 && next.constants[0][1]==0);
    D3D__RenderState[D3DRS_PSALPHAINPUTS7]=0x87654321; /* Inactive stage excluded from key. */
    CHECK(halo_vita_pixel_state_capture(&next)==0 && next.key==first.key);
    D3DDevice_SetRenderStateNotInline(D3DRS_PSALPHAINPUTS0,0x87654321);
    CHECK(halo_vita_pixel_state_capture(&next)==0 && next.key!=first.key);
    D3DDevice_SetRenderState_PSTextureModes(0x42);
    CHECK(halo_vita_pixel_state_capture(&next)==0 && next.definition.PSTextureModes==0x42);
    unchanged=next;D3D__RenderState[D3DRS_PSCOMBINERCOUNT]=9;
    CHECK(halo_vita_pixel_state_capture(&next)<0 && !memcmp(&next,&unchanged,sizeof(next)));
    halo_vita_d3d_state_reset();
    CHECK(halo_vita_pixel_state_capture(&next)==0 && next.definition.PSC0Mapping==0);
    CHECK(next.constants[0][0]==0 && next.constants[0][1]==0);
    fprintf(report,"PASS mutable pixel state, direct updates, canonical identity and colors\n");fflush(report);
}
static void test_vertex_constants(void)
{
    float input[8]={1,2,3,4,5,6,7,8},out[8];
    D3DVIEWPORT8 viewport={0,0,640,480,0,1};
    halo_vita_constants_reset();
    D3DDevice_SetVertexShaderConstant(-96,input,2);
    CHECK(halo_vita_constants_copy(-96,2,out)==0 && !memcmp(input,out,sizeof(input)));
    D3DDevice_SetVertexShaderConstant(95,input,2); /* Only one register remains. */
    CHECK(halo_vita_constants_copy(95,1,out)==0 && !memcmp(input,out,16));
    CHECK(halo_vita_constants_copy(95,2,out)<0);
    D3DDevice_SetVertexShaderConstant(INT_MAX,input,1);
    halo_vita_constants_viewport(&viewport);
    CHECK(halo_vita_constants_copy(-38,2,out)==0);
    CHECK(out[0]==320 && out[1]==-240 && out[2]==16777215.0f && out[4]==320 && out[5]==240);
    D3DDevice_SetShaderConstantMode(0x11);
    D3DDevice_SetVertexShaderConstant(-38,input,2);
    viewport.Width=320;halo_vita_constants_viewport(&viewport);
    CHECK(halo_vita_constants_copy(-38,2,out)==0 && !memcmp(input,out,sizeof(input)));
    D3DDevice_SetShaderConstantMode(1);
    CHECK(halo_vita_constants_copy(-38,2,out)==0 && out[0]==160 && out[4]==160);
    halo_vita_constants_reset();
    fprintf(report,"PASS vertex constant window, clipping and reserved viewport registers\n");fflush(report);
}
static void test_index_resource(void)
{
    D3DIndexBuffer *buffer;
    struct halo_vita_index_view view;
    uint32_t id;
    CHECK(D3DDevice_CreateIndexBuffer(7,8,D3DFMT_INDEX16,1,&buffer)<0 && !buffer);
    CHECK(D3DDevice_CreateIndexBuffer(8,8,D3DFMT_INDEX16,1,&buffer)==0);
    ((uint16_t *)(uintptr_t)buffer->Data)[0]=3;
    ((uint16_t *)(uintptr_t)buffer->Data)[1]=1;
    CHECK(halo_vita_stream_set(0,(D3DVertexBuffer *)buffer,2)<0);
    D3DDevice_SetIndices(buffer,17);
    CHECK(D3D__IndexData==(WORD *)(uintptr_t)buffer->Data);
    CHECK(halo_vita_indices_acquire(0,2,&view)==0);
    CHECK(view.data[0]==3 && view.data[1]==1 && view.base_vertex==17);
    id=view.allocation;
    CHECK(halo_vita_indices_acquire(3,2,&view)<0);
    CHECK(D3DResource_Release((D3DResource *)buffer)==0);
    CHECK(halo_vita_indices_set(NULL,0)==0 && !D3D__IndexData);
    CHECK(halo_vita_vertex_buffers_collect()==0 && halo_vita_allocation_busy(id)==1);
    CHECK(halo_vita_allocation_release(id)==0);
    CHECK(halo_vita_vertex_buffers_collect()==0 && halo_vita_allocation_busy(id)<0);
    fprintf(report,"PASS index buffer format, base vertex, bounds and binding lifetime\n");fflush(report);
}
static void test_vertex_resource(void)
{
    D3DVertexBuffer *buffer=(D3DVertexBuffer *)(uintptr_t)1;
    struct halo_vita_stream_view view;
    uint32_t id;
    CHECK(D3DDevice_CreateVertexBuffer(0,8,0,1,&buffer)<0 && !buffer);
    CHECK(D3DDevice_CreateVertexBuffer(64,8,0,1,&buffer)==0 && buffer);
    CHECK(halo_vita_vertex_buffer_owned((D3DResource *)buffer));
    {
        BYTE *data=NULL,*unchanged;
        CHECK(halo_vita_vertex_buffer_lock(buffer,16,0,0,&data)==0);
        CHECK(data==(BYTE *)(uintptr_t)buffer->Data+16);
        data[0]=0x35;data[47]=0x79;
        unchanged=data;
        CHECK(halo_vita_vertex_buffer_lock(buffer,16,49,0,&data)<0 && data==unchanged);
        CHECK(halo_vita_vertex_buffer_lock(buffer,64,0,0,&data)<0 && data==unchanged);
        CHECK(halo_vita_vertex_buffer_lock(buffer,0,1,0x40,&data)<0 && data==unchanged);
        CHECK(halo_vita_allocation_resolve((void *)(uintptr_t)buffer->Data,64,HALO_VITA_ALLOCATION_VERTEX_BYTES,&id)==0);
        CHECK(halo_vita_allocation_retain(id)==0 && halo_vita_allocation_pin(id)==0);
        CHECK(halo_vita_vertex_buffer_lock(buffer,0,16,0x80,&data)==1 && data==unchanged);
        CHECK(halo_vita_vertex_buffer_lock(buffer,0,16,0x20,&data)==1 && data==unchanged);
        CHECK(halo_vita_allocation_retire(id)==0 && halo_vita_allocation_release(id)==0);
        D3DVertexBuffer_Lock(buffer,16,48,&data,0);
        CHECK(data[0]==0x35 && data[47]==0x79);
    }
    CHECK(halo_vita_stream_set(0,buffer,16)==0);
    CHECK(halo_vita_stream_acquire(0,0,4,16,&view)==0);
    id=view.allocation;
    CHECK(D3DResource_Release((D3DResource *)buffer)==0);
    CHECK(halo_vita_allocation_busy(id)==1);
    CHECK(halo_vita_streams_reset()==0);
    CHECK(halo_vita_vertex_buffers_collect()==0 && halo_vita_allocation_busy(id)==1);
    CHECK(halo_vita_allocation_release(id)==0);
    CHECK(halo_vita_vertex_buffers_collect()==0 && halo_vita_allocation_busy(id)<0);
    fprintf(report,"PASS vertex resource release deferred through bound/draw views\n");fflush(report);
}
static void test_owned_buffer(void)
{
    struct halo_vita_buffer b={.uid=-1};
    uint32_t id;
    CHECK(halo_vita_buffer_create(&b,65)==0);
    CHECK(halo_vita_allocation_resolve(b.base,65,HALO_VITA_ALLOCATION_VERTEX_BYTES,&id)==0);
    CHECK(halo_vita_allocation_resolve(b.base,66,HALO_VITA_ALLOCATION_VERTEX_BYTES,&id)<0);
    CHECK(halo_vita_allocation_retain(b.allocation)==0);
    CHECK(halo_vita_buffer_destroy(&b)<0 && b.base && b.mapped);
    CHECK(halo_vita_allocation_release(b.allocation)==0);
    CHECK(halo_vita_buffer_destroy(&b)==0 && b.uid<0 && !b.base);
    CHECK(halo_vita_buffer_destroy(&b)==0);
    fprintf(report,"PASS GPU buffer allocation, logical extent and busy destruction\n");fflush(report);
}
static void test_stream_bounds(void)
{
    unsigned char storage[64];
    uint32_t id;
    D3DVertexBuffer h={1,(DWORD)(uintptr_t)storage,0};
    struct halo_vita_stream_view view,before;
    CHECK(halo_vita_allocation_register(storage,sizeof(storage),HALO_VITA_ALLOCATION_VERTEX_BYTES,&id)==0);
    CHECK(halo_vita_stream_set(0,&h,16)==0);
    CHECK(halo_vita_allocation_unregister(id)<0);
    CHECK(halo_vita_stream_acquire(0,1,3,12,&view)==0);
    CHECK(view.data==storage+16 && view.bytes==44 && view.allocation==id);
    before=view;
    CHECK(halo_vita_stream_acquire(0,1,4,12,&view)<0);
    CHECK(!memcmp(&before,&view,sizeof(view)));
    CHECK(halo_vita_stream_acquire(0,SIZE_MAX,2,12,&view)<0);
    h.Common=0x40001;
    CHECK(halo_vita_stream_set(0,&h,16)<0);
    CHECK(halo_vita_streams_reset()==0);
    CHECK(halo_vita_allocation_unregister(id)<0); /* Acquired draw view remains. */
    CHECK(halo_vita_allocation_release(view.allocation)==0);
    CHECK(halo_vita_allocation_unregister(id)==0);
    CHECK(halo_vita_stream_acquire(0,0,1,12,&view)<0);
    fprintf(report,"PASS stream spans, overflow, failed bind and retained lifetime\n");fflush(report);
}
static void test_topology(void)
{
    const uint16_t in[4]={7,2,9,4},expected[6]={7,2,9,7,9,4};
    const uint16_t loop[8]={7,2,2,9,9,4,4,7};
    uint16_t out[12],saved[12];
    SceGxmPrimitiveType mode=SCE_GXM_PRIMITIVE_POINTS;
    size_t count=999;
    memset(out,0xa5,sizeof(out));memcpy(saved,out,sizeof(out));
    CHECK(halo_vita_topology(D3DPT_QUADLIST,in,4,out,5,&mode,&count)<0);
    CHECK(!memcmp(out,saved,sizeof(out)) && count==999 && mode==SCE_GXM_PRIMITIVE_POINTS);
    CHECK(halo_vita_topology(D3DPT_QUADLIST,in,4,out,12,&mode,&count)==0);
    CHECK(count==6 && mode==SCE_GXM_PRIMITIVE_TRIANGLES && !memcmp(out,expected,sizeof(expected)));
    CHECK(halo_vita_topology(D3DPT_LINELOOP,in,4,out,12,&mode,&count)==0);
    CHECK(count==8 && !memcmp(out,loop,sizeof(loop)));
    CHECK(halo_vita_topology(D3DPT_TRIANGLELIST,NULL,6,out,12,&mode,&count)==0);
    CHECK(count==6 && out[0]==0 && out[5]==5);
    CHECK(halo_vita_topology(D3DPT_TRIANGLELIST,out,6,out,12,&mode,&count)<0);
    CHECK(halo_vita_topology(D3DPT_POINTLIST,NULL,65537,out,12,&mode,&count)<0);
    CHECK(halo_vita_topology(D3DPT_QUADLIST,NULL,0,NULL,0,&mode,&count)==0 && count==0);
    CHECK(halo_vita_topology(D3DPT_QUADLIST,in,3,out,12,&mode,&count)<0);
    fprintf(report,"PASS topology order, bounded capacity and alias rejection\n");fflush(report);
}
static void test_render_state_capture(void)
{
    halo_vita_d3d_state_reset();
    CHECK(D3D__RenderState[D3DRS_ZFUNC]==D3DCMP_LESSEQUAL);
    CHECK(D3D__RenderState[D3DRS_ZENABLE]==TRUE);
    D3DDevice_SetRenderState_Simple(0x40354,D3DCMP_EQUAL);
    CHECK(D3D__RenderState[D3DRS_ZFUNC]==D3DCMP_EQUAL);
    D3DDevice_SetRenderState_ZEnable(FALSE);
    CHECK(D3D__RenderState[D3DRS_ZENABLE]==FALSE);
    D3DDevice_SetRenderState_ZBias(8);
    CHECK(D3D__RenderState[D3DRS_POLYGONOFFSETZSLOPESCALE]==0xc0000000u);
    CHECK(D3D__RenderState[D3DRS_POLYGONOFFSETZOFFSET]==0xc1000000u);
    CHECK(D3D__RenderState[D3DRS_SOLIDOFFSETENABLE]==1);
    D3DDevice_SetRenderStateNotInline(D3DRS_ZBIAS,0);
    CHECK(D3D__RenderState[D3DRS_SOLIDOFFSETENABLE]==0);
    D3DDevice_SetRenderState_Deferred(D3DRS_MAX,123);
    halo_vita_d3d_state_reset();
    CHECK(D3D__RenderState[D3DRS_ZENABLE]==TRUE);
    CHECK(D3D__RenderState[D3DRS_ZFUNC]==D3DCMP_LESSEQUAL);
    {
        struct halo_vita_depth_stencil decoded,before;
        CHECK(halo_vita_depth_stencil_decode(D3D__RenderState,&decoded)==0);
        CHECK(decoded.depth_func==SCE_GXM_DEPTH_FUNC_LESS_EQUAL);
        CHECK(decoded.depth_write==SCE_GXM_DEPTH_WRITE_ENABLED && decoded.write_mask==0);
        D3D__RenderState[D3DRS_STENCILENABLE]=1;
        D3D__RenderState[D3DRS_STENCILPASS]=D3DSTENCILOP_INCRSAT;
        D3D__RenderState[D3DRS_STENCILZFAIL]=D3DSTENCILOP_INCR;
        D3D__RenderState[D3DRS_STENCILREF]=0x123;
        CHECK(halo_vita_depth_stencil_decode(D3D__RenderState,&decoded)==0);
        CHECK(decoded.pass==SCE_GXM_STENCIL_OP_INCR && decoded.zfail==SCE_GXM_STENCIL_OP_INCR_WRAP);
        CHECK(decoded.ref==0x23 && decoded.write_mask==255);
        before=decoded;
        D3D__RenderState[D3DRS_STENCILPASS]=0xdead;
        CHECK(halo_vita_depth_stencil_decode(D3D__RenderState,&decoded)<0);
        CHECK(memcmp(&before,&decoded,sizeof(before))==0);
        D3D__RenderState[D3DRS_STENCILENABLE]=0;
        D3D__RenderState[D3DRS_ZENABLE]=0;
        D3D__RenderState[D3DRS_ZFUNC]=0xdead;
        CHECK(halo_vita_depth_stencil_decode(D3D__RenderState,&decoded)==0);
        CHECK(decoded.depth_func==SCE_GXM_DEPTH_FUNC_ALWAYS && decoded.depth_write==SCE_GXM_DEPTH_WRITE_DISABLED);
        D3D__RenderState[D3DRS_ZENABLE]=D3DZB_USEW;
        CHECK(halo_vita_depth_stencil_decode(D3D__RenderState,&decoded)<0);
        halo_vita_d3d_state_reset();
    }
    {
        SceGxmBlendInfo b,before;
        CHECK(halo_vita_blend_decode(D3D__RenderState,&b)==0);
        CHECK(b.colorMask==15 && b.colorFunc==SCE_GXM_BLEND_FUNC_NONE);
        D3D__RenderState[D3DRS_COLORWRITEENABLE]=0x01000000;
        D3D__RenderState[D3DRS_ALPHABLENDENABLE]=1;
        D3D__RenderState[D3DRS_SRCBLEND]=D3DBLEND_SRCALPHA;
        D3D__RenderState[D3DRS_DESTBLEND]=D3DBLEND_INVSRCALPHA;
        CHECK(halo_vita_blend_decode(D3D__RenderState,&b)==0);
        CHECK(b.colorMask==SCE_GXM_COLOR_MASK_A && b.colorFunc==SCE_GXM_BLEND_FUNC_ADD);
        CHECK(b.colorSrc==SCE_GXM_BLEND_FACTOR_SRC_ALPHA && b.colorDst==SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
        D3D__RenderState[D3DRS_SRCBLEND]=D3DBLEND_SRCALPHASAT;
        CHECK(halo_vita_blend_decode(D3D__RenderState,&b)==0);
        CHECK(b.colorSrc==SCE_GXM_BLEND_FACTOR_SRC_ALPHA_SATURATE && b.alphaSrc==SCE_GXM_BLEND_FACTOR_ONE);
        before=b;
        D3D__RenderState[D3DRS_SRCBLEND]=D3DBLEND_CONSTANTCOLOR;
        CHECK(halo_vita_blend_decode(D3D__RenderState,&b)<0);
        CHECK(memcmp(&b,&before,sizeof(b))==0);
        D3D__RenderState[D3DRS_BLENDOP]=D3DBLENDOP_MIN;
        CHECK(halo_vita_blend_decode(D3D__RenderState,&b)==0);
        CHECK(b.colorFunc==SCE_GXM_BLEND_FUNC_MIN && b.colorSrc==SCE_GXM_BLEND_FACTOR_ONE);
        D3D__RenderState[D3DRS_BLENDOP]=D3DBLENDOP_ADDSIGNED;
        CHECK(halo_vita_blend_decode(D3D__RenderState,&b)<0);
        D3D__RenderState[D3DRS_ALPHABLENDENABLE]=0;
        CHECK(halo_vita_blend_decode(D3D__RenderState,&b)==0);
        halo_vita_d3d_state_reset();
    }
    {
        SceGxmCullMode c=SCE_GXM_CULL_NONE;
        CHECK(halo_vita_cull_decode(D3DCULL_CW,&c)==0 && c==SCE_GXM_CULL_CW);
        CHECK(halo_vita_cull_decode(D3DCULL_CCW,&c)==0 && c==SCE_GXM_CULL_CCW);
        CHECK(halo_vita_cull_decode(0xdead,&c)<0 && c==SCE_GXM_CULL_CCW);
    }
    fprintf(report,"PASS render state capture, SDK method decoding and decal bias\n");fflush(report);
}
static void test_logical_render_completion(void)
{
    unsigned i,attempt,slot;
    int result;
    CHECK(halo_vita_graphics_create(960,544)==0);
    CHECK(halo_vita_surfaces_create(960,544)==0);
    CHECK(halo_vita_present_initialize()==0);
    CHECK(halo_vita_logical_targets_create()==0);
    CHECK(halo_vita_shader_patcher_create()==0);
    CHECK(halo_vita_clear_create()==0);
    for(i=0;i<2;i++) {
        const struct halo_vita_surface_slot *s=halo_vita_logical_target_get(i);
        CHECK(halo_vita_logical_begin(i)==0);
        CHECK(halo_vita_present_begin(&slot)<0); /* Prevent nested context use. */
        CHECK(halo_vita_logical_poll(i)==1);
        CHECK(halo_vita_present_shutdown()<0);
        CHECK(halo_vita_logical_targets_destroy()<0);
        CHECK(halo_vita_clear_draw_size(640,480)==0);
        {
            /* Registry lifetime test only: GPU does not fetch this stack data. */
            unsigned char bytes[32];
            uint32_t allocation;
            CHECK(halo_vita_allocation_register(bytes,sizeof(bytes),HALO_VITA_ALLOCATION_VERTEX_BYTES,&allocation)==0);
            CHECK(halo_vita_allocation_retain(allocation)==0);
            CHECK(halo_vita_logical_use_allocation(s->color_allocation)<0);
            CHECK(halo_vita_logical_use_allocation(allocation)==0);
            CHECK(halo_vita_logical_use_allocation(allocation)==0); /* Deduplicated. */
            CHECK(halo_vita_allocation_release(allocation)==0);
            CHECK(halo_vita_allocation_unregister(allocation)<0);
            CHECK(halo_vita_logical_end()==0);
            CHECK(halo_vita_logical_use_allocation(allocation)<0);
            for(attempt=0;attempt<2000;attempt++) {
                result=halo_vita_logical_poll(i);
                if(result!=1)break;
                Sleep(1);
            }
            CHECK(result==0 && halo_vita_allocation_gpu_busy(allocation)==0);
            CHECK(halo_vita_allocation_unregister(allocation)==0);
        }
        for(attempt=0;attempt<2000;attempt++) {
            result=halo_vita_logical_poll(i);
            if(result!=1)break;
            Sleep(1);
        }
        CHECK(result==0);
        CHECK(((volatile uint32_t *)s->pixels)[240*640+320]==0xff204080); /* ARGB */
        CHECK(halo_vita_allocation_gpu_busy(s->color_allocation)==0);
    }
    {
        uint32_t header[5]={0x00040001,0,0,0x00011229,0x271df27f};
        header[1]=(uint32_t)(uintptr_t)halo_vita_logical_target_get(0)->pixels;
        CHECK(halo_vita_texture_draw_create_d3d(header,sizeof(header))==0);
        for(i=0;i<12;i++) {
            unsigned logical_index=i&1;
            /* Reuse a logical target before beginning the next display scene:
             * this must reap the older display's sampling references itself. */
            for(attempt=0;attempt<2000;attempt++) {
                result=halo_vita_logical_begin(logical_index);
                if(result!=1)break;
                Sleep(1);
            }
            CHECK(result==0);
            CHECK(halo_vita_clear_draw_size(640,480)==0);
            CHECK(halo_vita_logical_end()==0);
            for(attempt=0;attempt<2000;attempt++) {
                result=halo_vita_logical_poll(logical_index);
                if(result!=1)break;
                Sleep(1);
            }
            CHECK(result==0);
            header[1]=(uint32_t)(uintptr_t)halo_vita_logical_target_get(logical_index)->pixels;
            CHECK(halo_vita_texture_draw_bind_d3d(header,sizeof(header))==0);
            for(attempt=0;attempt<2000;attempt++) {
                result=halo_vita_present_begin(&slot);
                if(result!=1)break;
                Sleep(1);
            }
            CHECK(result==0);
            CHECK(halo_vita_logical_begin(1)<0); /* Display currently owns context. */
            CHECK(halo_vita_texture_draw_bind_d3d(header,sizeof(header))<0);
            /* Independent regression: a previous draw rejects every fragment.
         * The fullscreen helper must override it before the pixel readback. */
        sceGxmSetTwoSidedEnable(halo_vita_graphics_context(),SCE_GXM_TWO_SIDED_ENABLED);
        sceGxmSetFrontStencilFunc(halo_vita_graphics_context(),SCE_GXM_STENCIL_FUNC_NEVER,
            SCE_GXM_STENCIL_OP_KEEP,SCE_GXM_STENCIL_OP_KEEP,
            SCE_GXM_STENCIL_OP_KEEP,0xff,0xff);
        sceGxmSetBackStencilFunc(halo_vita_graphics_context(),SCE_GXM_STENCIL_FUNC_NEVER,
            SCE_GXM_STENCIL_OP_KEEP,SCE_GXM_STENCIL_OP_KEEP,
            SCE_GXM_STENCIL_OP_KEEP,0xff,0xff);
        CHECK(halo_vita_texture_draw_submit()==0);
            CHECK(halo_vita_present_end()==0);
        }
    }
    CHECK(halo_vita_present_shutdown()==0);
    for(i=0;i<3;i++) {
        const struct halo_vita_surface_slot *s=halo_vita_surface_get(i);
        CHECK(((volatile uint32_t *)s->pixels)[272*s->stride+480]==0xff804020);
    }
    CHECK(halo_vita_texture_draw_destroy()==0);
    CHECK(halo_vita_clear_destroy()==0);
    CHECK(halo_vita_shader_patcher_destroy()==0);
    CHECK(halo_vita_logical_targets_destroy()==0);
    CHECK(halo_vita_surfaces_destroy()==0);
    CHECK(halo_vita_graphics_destroy()==0);
    fprintf(report,"PASS repeated logical render/sample reuse, notifications and ownership\n");fflush(report);
}
static void test_presentation(void)
{
    unsigned frame,slot,attempt;
    int result;
    CHECK(halo_vita_graphics_create(960,544)==0);
    CHECK(halo_vita_surfaces_create(960,544)==0);
    CHECK(halo_vita_present_initialize()==0);
    CHECK(halo_vita_present_initialize()<0);
    CHECK(halo_vita_shader_patcher_create()==0);
    CHECK(halo_vita_clear_create()==0);
    for (frame=0;frame<6;frame++) {
        for (attempt=0;attempt<2000;attempt++) {
            result=halo_vita_present_begin(&slot);
            if (result!=1) break;
            Sleep(1);
        }
        CHECK(result==0 && slot<HALO_VITA_SURFACE_SLOTS);
        CHECK(halo_vita_present_begin(&slot)<0);
        CHECK(halo_vita_surfaces_destroy()<0);
        /* Independent regression: a previous draw rejects every fragment.
         * The fullscreen helper must override it before the pixel readback. */
        sceGxmSetTwoSidedEnable(halo_vita_graphics_context(),SCE_GXM_TWO_SIDED_ENABLED);
        sceGxmSetFrontStencilFunc(halo_vita_graphics_context(),SCE_GXM_STENCIL_FUNC_NEVER,
            SCE_GXM_STENCIL_OP_KEEP,SCE_GXM_STENCIL_OP_KEEP,
            SCE_GXM_STENCIL_OP_KEEP,0xff,0xff);
        sceGxmSetBackStencilFunc(halo_vita_graphics_context(),SCE_GXM_STENCIL_FUNC_NEVER,
            SCE_GXM_STENCIL_OP_KEEP,SCE_GXM_STENCIL_OP_KEEP,
            SCE_GXM_STENCIL_OP_KEEP,0xff,0xff);
        CHECK(halo_vita_clear_draw()==0);
        CHECK(halo_vita_present_end()==0);
    }
    CHECK(halo_vita_present_shutdown()==0);
    CHECK(halo_vita_present_shutdown()==0);
    /* GPU finished and scanout detached; buffers remain allocated for readback. */
    for (frame=0;frame<HALO_VITA_SURFACE_SLOTS;frame++) {
        const struct halo_vita_surface_slot *surface=halo_vita_surface_get(frame);
        volatile uint32_t *pixels=(volatile uint32_t *)surface->pixels;
        uint32_t center=pixels[272*surface->stride+480];
        fprintf(report,"slot %u center ABGR %08lx\n",frame,(unsigned long)center);fflush(report);
        CHECK(center==0xff804020);
        CHECK(pixels[16*surface->stride+16]==0xff804020);
        CHECK(pixels[527*surface->stride+943]==0xff804020);
    }
    CHECK(halo_vita_clear_destroy()==0);
    CHECK(halo_vita_shader_patcher_destroy()==0);
    CHECK(halo_vita_surfaces_destroy()==0);
    CHECK(halo_vita_graphics_destroy()==0);
    fprintf(report,"PASS six shader-backed color draws, slot reuse and scanout detach\n"); fflush(report);
}
#include "../../build/vita/texture_fixture.h"
static void texture_presentation_check(const struct halo_vita_texture_view *view,
    const uint32_t expected[4])
{
    unsigned frame,slot,attempt;
    int result;
    CHECK(halo_vita_graphics_create(960,544)==0);
    CHECK(halo_vita_surfaces_create(960,544)==0);
    CHECK(halo_vita_present_initialize()==0);
    CHECK(halo_vita_present_initialize()<0);
    CHECK(halo_vita_shader_patcher_create()==0);
    CHECK(halo_vita_texture_draw_create(view)==0);
    for (frame=0;frame<6;frame++) {
        for (attempt=0;attempt<2000;attempt++) {
            result=halo_vita_present_begin(&slot);
            if (result!=1) break;
            Sleep(1);
        }
        CHECK(result==0 && slot<HALO_VITA_SURFACE_SLOTS);
        CHECK(halo_vita_present_begin(&slot)<0);
        CHECK(halo_vita_surfaces_destroy()<0);
        /* Independent regression: a previous draw rejects every fragment.
         * The fullscreen helper must override it before the pixel readback. */
        sceGxmSetTwoSidedEnable(halo_vita_graphics_context(),SCE_GXM_TWO_SIDED_ENABLED);
        sceGxmSetFrontStencilFunc(halo_vita_graphics_context(),SCE_GXM_STENCIL_FUNC_NEVER,
            SCE_GXM_STENCIL_OP_KEEP,SCE_GXM_STENCIL_OP_KEEP,
            SCE_GXM_STENCIL_OP_KEEP,0xff,0xff);
        sceGxmSetBackStencilFunc(halo_vita_graphics_context(),SCE_GXM_STENCIL_FUNC_NEVER,
            SCE_GXM_STENCIL_OP_KEEP,SCE_GXM_STENCIL_OP_KEEP,
            SCE_GXM_STENCIL_OP_KEEP,0xff,0xff);
        CHECK(halo_vita_texture_draw_submit()==0);
        CHECK(halo_vita_present_end()==0);
    }
    CHECK(halo_vita_present_shutdown()==0);
    CHECK(halo_vita_present_shutdown()==0);
    /* GPU finished and scanout detached; buffers remain allocated for readback. */
    for (frame=0;frame<HALO_VITA_SURFACE_SLOTS;frame++) {
        const struct halo_vita_surface_slot *surface=halo_vita_surface_get(frame);
        volatile uint32_t *pixels=(volatile uint32_t *)surface->pixels;
        CHECK(pixels[136*surface->stride+240]==expected[0]); /* red */
        CHECK(pixels[136*surface->stride+720]==expected[1]); /* green */
        CHECK(pixels[408*surface->stride+240]==expected[2]); /* blue */
        CHECK(pixels[408*surface->stride+720]==expected[3]); /* white */
    }
    CHECK(halo_vita_texture_draw_destroy()==0);
    CHECK(halo_vita_shader_patcher_destroy()==0);
    CHECK(halo_vita_surfaces_destroy()==0);
    CHECK(halo_vita_graphics_destroy()==0);
    fprintf(report,"PASS texture decode/upload/sample and four-quadrant readback\n"); fflush(report);
}
static void test_texture_presentation(void)
{
    unsigned x,y;
    uint32_t pixels_argb[16];
    const uint32_t expected[4]={0xff0000ff,0xff00ff00,0xffff0000,0xffffffff};
    struct halo_vita_texture_view view={pixels_argb,sizeof(pixels_argb),4,4,11,16,1,NULL,0};
    for (y=0;y<4;y++) for (x=0;x<4;x++)
        pixels_argb[y*4+x]=y<2 ? (x<2 ? 0xffff0000 : 0xff00ff00) :
                               (x<2 ? 0xff0000ff : 0xffffffff);
    texture_presentation_check(&view,expected);
#if HALO_PRIVATE_TEXTURE_FIXTURE
    {
        uint32_t *decoded;
        uint32_t real_expected[4];
        unsigned i;
        const unsigned screen_x[4]={240,720,240,720};
        const unsigned screen_y[4]={136,136,408,408};
        CHECK(halo_vita_texture_select(&private_layout,private_pixels,
            sizeof(private_pixels),0,0,&view)==0);
        decoded=malloc(view.width*view.height*sizeof(*decoded));
        CHECK(decoded!=NULL);
        CHECK(halo_vita_decode_texture(&view,decoded,view.width*view.height*sizeof(*decoded))==0);
        for (i=0;i<4;i++) {
            unsigned tx=((2*screen_x[i]+1)*view.width)/(2*960);
            unsigned ty=((2*screen_y[i]+1)*view.height)/(2*544);
            real_expected[i]=decoded[ty*view.width+tx];
        }
        texture_presentation_check(&view,real_expected);
        free(decoded);
        fprintf(report,"PASS private retail DXT mip selection/upload/GPU samples match CPU decoder; not independent decoder validation\n");
        fflush(report);
    }
#endif
}
static void test_engine_arenas(void)
{
    unsigned i;
    void *bases[4];
    CHECK(!physical_memory_get_game_state_base_address());
    physical_memory_allocate();
    physical_memory_verify();
    bases[0]=physical_memory_get_game_state_base_address();
    bases[1]=physical_memory_get_tag_cache_base_address();
    bases[2]=physical_memory_get_texture_cache_base_address();
    bases[3]=physical_memory_get_sound_cache_base_address();
    for (i=0;i<4;i++) {
        unsigned char *bytes=bases[i];
        size_t size=halo_vita_arena_size((enum halo_vita_arena)i);
        CHECK(bases[i] && bases[i]==halo_vita_arena_base((enum halo_vita_arena)i));
        bytes[0]=0x35; bytes[size-1]=0x79;
        CHECK(bytes[0]==0x35 && bytes[size-1]==0x79);
    }
    physical_memory_free();
    CHECK(!physical_memory_get_game_state_base_address());
    CHECK(!physical_memory_get_tag_cache_base_address());
    CHECK(!physical_memory_get_texture_cache_base_address());
    CHECK(!physical_memory_get_sound_cache_base_address());
    physical_memory_free();
    physical_memory_allocate();
    physical_memory_verify();
    physical_memory_free();
    fprintf(report,"PASS engine memory arena allocation, cleanup and reinitialization\n"); fflush(report);
}
static void test_arenas(void)
{
    unsigned i;
    SceGxmInitializeParams parameters;
    CHECK(!halo_vita_arena_base(HALO_VITA_GAME_STATE));
    CHECK(!halo_vita_arenas_initialize());
    CHECK(halo_vita_arenas_initialize() < 0);
    for (i = 0; i < HALO_VITA_ARENA_COUNT; i++) {
        volatile unsigned char *base = halo_vita_arena_base(i);
        size_t size = halo_vita_arena_size(i);
        CHECK(base && size && !((uintptr_t)base & 4095));
        base[0] = (unsigned char)(i+1);
        base[size-1] = (unsigned char)(i+17);
        CHECK(base[0] == i+1 && base[size-1] == i+17);
    }
    memset(&parameters, 0, sizeof(parameters));
    halo_vita_present_configure(&parameters);
    parameters.parameterBufferSize = SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE;
    CHECK(sceGxmInitialize(&parameters) == 0);
    CHECK(halo_vita_arena_map_gpu(HALO_VITA_SOUND_CACHE) < 0);
    for (i = 0; i < HALO_VITA_SOUND_CACHE; i++) {
        CHECK(halo_vita_arena_map_gpu(i) == 0);
        CHECK(halo_vita_arena_map_gpu(i) < 0);
    }
    CHECK(halo_vita_arenas_dispose() < 0);
    CHECK(halo_vita_arena_base(HALO_VITA_GAME_STATE) != NULL);
    /* No GPU work was submitted, so no resource references need retirement. */
    for (i = 0; i < HALO_VITA_SOUND_CACHE; i++) {
        CHECK(halo_vita_arena_unmap_gpu(i) == 0);
        CHECK(halo_vita_arena_unmap_gpu(i) < 0);
    }
    test_graphics_context();
    test_graphics_surfaces();
    test_texture_upload();
    test_bc_chain_descriptor();
    test_texture_resource();
    test_shader_patcher();
    test_logical_targets();
    test_d3d_texture_draw();
    test_vertex_catalog();
    test_vertex_shader_handles();
    test_vertex_constants();
    test_pixel_state();
    test_texture_stage_state();
    test_index_resource();
    test_vertex_resource();
    test_owned_buffer();
    test_stream_bounds();
    test_topology();
    test_render_state_capture();
    test_logical_render_completion();
    test_presentation();
    test_texture_presentation();
    CHECK(sceGxmTerminate() == 0);
    CHECK(!halo_vita_arenas_dispose());
    CHECK(!halo_vita_arena_base(HALO_VITA_GAME_STATE));
    CHECK(!halo_vita_arenas_dispose());
    fprintf(report,"PASS native CPU arenas and GXM mapping lifecycle (no draws)\n"); fflush(report);
}
static void test_directories(void)
{
    const char *parent = "t:\\platform-directory-test";
    const char *child = "t:\\platform-directory-test\\child";
    /* Previous interrupted test directories may exist; remove empty children. */
    RemoveDirectoryA(child);
    RemoveDirectoryA(parent);
    CHECK(!CreateDirectoryA("d:\\platform-test", NULL));
    CHECK(GetLastError() == ERROR_ACCESS_DENIED);
    CHECK(CreateDirectoryA(parent, NULL));
    CHECK(!CreateDirectoryA(parent, NULL));
    CHECK(GetLastError() == ERROR_ALREADY_EXISTS);
    CHECK(CreateDirectoryA(child, NULL));
    CHECK(!RemoveDirectoryA(parent));
    CHECK(GetLastError() == ERROR_DIR_NOT_EMPTY);
    CHECK(RemoveDirectoryA(child));
    CHECK(RemoveDirectoryA(parent));
    CHECK(!CreateDirectoryA(child, NULL));
    CHECK(GetLastError() == ERROR_PATH_NOT_FOUND);
    fprintf(report,"PASS isolated directory operations\n"); fflush(report);
}
static void test_relocation(void)
{
    uint32_t data[8] = { 0x803a6010, 0x2270040, 0, 0, 77, 0, 0, 0 };
    uint32_t before[8];
    struct halo_vita_relocation plan[2] = {
        { 0, 0x803a6010, 16, 4, HALO_VITA_REBASE_TAG },
        { 4, 0x2270040, 0, 0, HALO_VITA_CLEAR_POINTER }
    };
    memcpy(before, data, sizeof(data));
    plan[1].expected = 1;
    CHECK(!halo_vita_relocate_tags(data, sizeof(data), plan, 2));
    CHECK(!memcmp(data, before, sizeof(data)));
    plan[1].expected = 0x2270040;
    plan[0].span_bytes = 20;
    CHECK(!halo_vita_relocate_tags(data, sizeof(data), plan, 2));
    CHECK(!memcmp(data, before, sizeof(data)));
    plan[0].span_bytes = 4;
    plan[1].field_offset = 0;
    CHECK(!halo_vita_relocate_tags(data, sizeof(data), plan, 2));
    plan[1].field_offset = 4;
    CHECK(!halo_vita_relocate_tags(data, sizeof(data), (const void *)data, 1));
    CHECK(!halo_vita_relocate_tags(data, sizeof(data),
        (const void *)((unsigned char *)plan + 1), 1));
    CHECK(halo_vita_relocate_tags(data, sizeof(data), plan, 2));
    CHECK(data[0] == (uint32_t)(uintptr_t)&data[4] && data[1] == 0 && data[4] == 77);
    CHECK(!halo_vita_relocate_tags(data, sizeof(data), plan, 2));
    fprintf(report,"PASS transactional tag relocation\n"); fflush(report);
}
struct handshake { HANDLE start, done; unsigned payload; };
static void *handshake_worker(void *data)
{
    struct handshake *h = data;
    unsigned i;
    SetLastError(0x1234);
    for (i = 1; i <= 1000; i++) {
        CHECK(WaitForSingleObject(h->start, 5000) == WAIT_OBJECT_0);
        CHECK(h->payload == i);
        CHECK(GetLastError() == 0x1234);
        h->payload = i + 1000;
        CHECK(SetEvent(h->done));
    }
    return NULL;
}
static void test_handshake(void)
{
    struct handshake h;
    pthread_t thread;
    unsigned i;
    h.start = CreateEventA(NULL, FALSE, FALSE, NULL);
    h.done = CreateEventA(NULL, FALSE, FALSE, NULL);
    CHECK(h.start && h.done);
    h.payload = 0;
    SetLastError(0x5678);
    CHECK(!pthread_create(&thread, NULL, handshake_worker, &h));
    for (i = 1; i <= 1000; i++) {
        h.payload = i;
        CHECK(SetEvent(h.start));
        CHECK(WaitForSingleObject(h.done, 5000) == WAIT_OBJECT_0);
        CHECK(h.payload == i + 1000);
        CHECK(GetLastError() == 0x5678);
    }
    CHECK(!pthread_join(thread, NULL));
    CHECK(CloseHandle(h.start)); CHECK(CloseHandle(h.done));
    fprintf(report,"PASS 1000 two-thread handshakes and TLS isolation\n"); fflush(report);
}
static void *mutex_contender(void *token)
{
    CHECK(WaitForSingleObject(token, 0) == WAIT_TIMEOUT);
    CHECK(!ReleaseMutex(token) && GetLastError() == ERROR_NOT_OWNER);
    return NULL;
}
static void *mutex_abandoner(void *token)
{
    CHECK(WaitForSingleObject(token, 5000) == WAIT_OBJECT_0);
    return NULL; /* TLS cleanup must abandon the owned mutex. */
}
static void test_mutex(void)
{
    HANDLE token = CreateMutexA(NULL, TRUE, NULL);
    pthread_t thread;
    CHECK(token != NULL);
    CHECK(WaitForSingleObject(token, 0) == WAIT_OBJECT_0);
    CHECK(!pthread_create(&thread, NULL, mutex_contender, token));
    CHECK(!pthread_join(thread, NULL));
    CHECK(ReleaseMutex(token));
    CHECK(!pthread_create(&thread, NULL, mutex_contender, token));
    CHECK(!pthread_join(thread, NULL));
    CHECK(ReleaseMutex(token));
    CHECK(!pthread_create(&thread, NULL, mutex_abandoner, token));
    CHECK(!pthread_join(thread, NULL));
    CHECK(WaitForSingleObject(token, 5000) == WAIT_ABANDONED);
    CHECK(ReleaseMutex(token));
    CHECK(WaitForSingleObject(token, 0) == WAIT_OBJECT_0);
    CHECK(ReleaseMutex(token)); CHECK(CloseHandle(token));
    fprintf(report,"PASS recursive mutex ownership and abandonment\n"); fflush(report);
}
static DWORD WINAPI native_thread_test(void *data)
{
    unsigned *called = data;
    __atomic_add_fetch(called, 1, __ATOMIC_RELEASE);
    return 42;
}
static void test_thread(void)
{
    unsigned called = 0;
    DWORD id, code;
    HANDLE thread = CreateThread(NULL, 0x4000, native_thread_test, &called, CREATE_SUSPENDED, &id);
    CHECK(thread && id);
    CHECK(SetThreadPriority(thread, THREAD_PRIORITY_ABOVE_NORMAL));
    CHECK(!SetThreadPriority(thread, 3) && GetLastError() == ERROR_INVALID_PARAMETER);
    CHECK(SetThreadPriority(thread, THREAD_PRIORITY_NORMAL));
    CHECK(WaitForSingleObject(thread, 1) == WAIT_TIMEOUT);
    CHECK(!__atomic_load_n(&called, __ATOMIC_ACQUIRE));
    CHECK(GetExitCodeThread(thread, &code) && code == STILL_ACTIVE);
    CHECK(ResumeThread(thread) == 1);
    CHECK(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    CHECK(__atomic_load_n(&called, __ATOMIC_ACQUIRE) == 1);
    CHECK(GetExitCodeThread(thread, &code) && code == 42);
    CHECK(WaitForSingleObject(thread, 0) == WAIT_OBJECT_0);
    CHECK(CloseHandle(thread));
    fprintf(report,"PASS suspended startup and thread completion\n"); fflush(report);
}
static unsigned callbacks;
static pthread_t issuer;
static void WINAPI completed(DWORD error, DWORD bytes, LPOVERLAPPED request)
{
    CHECK(pthread_equal(issuer, pthread_self()));
    CHECK(error == ERROR_SUCCESS && bytes == 4 && request->InternalHigh == 4);
    callbacks++;
}
int main(void)
{
    HANDLE event, file;
    struct halo_vita_handle *retained;
    OVERLAPPED request;
    DWORD bytes;
    char data[8] = {0};
    unsigned i;
    LARGE_INTEGER frequency, before, after;
    DWORD tick_before, tick_after;
    if (halo_vita_files_initialize()) return 2;
    report = fopen("ux0:data/xita-native/2342/platform-test.txt", "w");
    if (!report) return 3;
    fprintf(report,"native platform test v66 START\n"); fflush(report);
    CHECK(QueryPerformanceFrequency(&frequency) && frequency.QuadPart == 1000000);
    CHECK(QueryPerformanceCounter(&before)); tick_before = GetTickCount();
    Sleep(20);
    tick_after = GetTickCount(); CHECK(QueryPerformanceCounter(&after));
    CHECK(after.QuadPart >= before.QuadPart + 20000);
    CHECK((DWORD)(tick_after - tick_before) >= 19);
    fprintf(report,"clock delay: %lld us, %lu ms\n", after.QuadPart - before.QuadPart,
        (DWORD)(tick_after - tick_before)); fflush(report);
    event = CreateEventA(NULL, FALSE, FALSE, NULL);
    CHECK(event != NULL);
    retained = halo_vita_handle_acquire(event, HALO_VITA_HANDLE_EVENT);
    CHECK(retained != NULL);
    CHECK(halo_vita_event_wait(retained, 0) == WAIT_TIMEOUT);
    CHECK(SetEvent(event));
    CHECK(halo_vita_event_wait(retained, 0) == WAIT_OBJECT_0);
    CHECK(halo_vita_event_wait(retained, 1) == WAIT_TIMEOUT);
    CHECK(CloseHandle(event));
    CHECK(!SetEvent(event));
    halo_vita_event_set(retained);
    CHECK(halo_vita_event_wait(retained, 0) == WAIT_OBJECT_0);
    halo_vita_handle_release(retained);
    test_file_resize();
    test_crt_strings();
    test_crt_format();
    test_file_time();
    test_float_control();
    test_thread_float_isolation();
    test_d3d_views();
    test_allocation_aliases();
    test_backtrace();
    test_texture_layout();
    test_texture_adapter();
    test_bc_reorder();
    test_arenas();
    test_device_lifecycle();
    test_xbox_device_lifecycle();
    test_engine_arenas();
    test_directories();
    test_relocation();
    test_handshake();
    test_mutex();
    test_thread();
    file = CreateFileA("t:\\platform-test.bin", GENERIC_READ | GENERIC_WRITE,
        0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(file != INVALID_HANDLE_VALUE);
    CHECK(WriteFile(file, "VITA", 4, &bytes, NULL) && bytes == 4);
    CHECK(SetFilePointer(file, 0, NULL, FILE_BEGIN) == 0);
    CHECK(ReadFile(file, data, sizeof(data), &bytes, NULL) && bytes == 4);
    CHECK(!memcmp(data, "VITA", 4));
    CHECK(ReadFile(file, data, sizeof(data), &bytes, NULL) && bytes == 0);
    CHECK(CloseHandle(file));
    file = CreateFileA("t:\\platform-test.bin", GENERIC_READ, 0, NULL,
        OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    CHECK(file != INVALID_HANDLE_VALUE);
    event = CreateEventA(NULL, TRUE, TRUE, NULL);
    CHECK(event != NULL);
    retained = halo_vita_handle_acquire(event, HALO_VITA_HANDLE_EVENT);
    CHECK(retained != NULL);
    memset(&request, 0, sizeof(request)); request.hEvent = event;
    CHECK(!ReadFile(file, data, 4, &bytes, &request) && GetLastError() == ERROR_IO_PENDING);
    CHECK(WaitForSingleObject(event, 5000) == WAIT_OBJECT_0);
    CHECK(GetOverlappedResult(file, &request, &bytes, FALSE) && bytes == 4);
    CHECK(!memcmp(data, "VITA", 4));
    CHECK(halo_vita_event_wait(retained, 0) == WAIT_OBJECT_0);
    CHECK(ResetEvent(event));
    CHECK(halo_vita_event_wait(retained, 0) == WAIT_TIMEOUT);
    memset(&request, 0, sizeof(request)); issuer = pthread_self();
    CHECK(ReadFileEx(file, data, 4, &request, completed));
    CHECK(!callbacks);
    for (i = 0; i < 50 && !callbacks; i++) {
        DWORD waited = WaitForSingleObjectEx(event, 100, TRUE);
        CHECK(waited == WAIT_TIMEOUT || waited == WAIT_IO_COMPLETION);
    }
    CHECK(callbacks == 1);
    CHECK(CloseHandle(file)); CHECK(CloseHandle(event));
    halo_vita_handle_release(retained);
    CHECK(!halo_vita_file_async_shutdown());
    fprintf(report,"PASS %u checks\n", checks); fclose(report);
    return 0;
}
