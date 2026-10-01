#include "halo_vita_vertex_program.h"
#include "halo_vita_graphics.h"
#include "halo_vita_logical_targets.h"
#include "xdk_xbox.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* One render owner. Catalog programs stay resident until device shutdown;
 * deleting/reusing a public handle never frees GPU-visible shader storage. */
static struct halo_vita_vertex_program programs[67];
static struct { DWORD handle; unsigned index; } handles[128];
static uint32_t next_handle=0x10000;
static unsigned slots[136],declaration,selected_address;
static int lookup(DWORD handle)
{
    unsigned i;
    for(i=0;i<128;i++)if(handle && handles[i].handle==handle)return (int)i;
    return -1;
}
static void invalid(const char *operation)
{
    fprintf(stderr,"native Vita: invalid vertex shader %s\n",operation);abort();
}
HRESULT WINAPI D3DDevice_CreateVertexShader(const DWORD *decl,const DWORD *code,DWORD *out,DWORD usage)
{
    unsigned i,n,index,instructions;int result;
    if(!decl || !code || !out || usage)return (HRESULT)0x80070057;
    instructions=code[0]>>16;
    if(!instructions || instructions>136)return (HRESULT)0x80070057;
    /* Native Halo declarations contain stream/data tokens only. Reject other
     * token classes instead of misreading constant payload as a terminator.
     * As with the original API, input pointers must refer to readable data. */
    for(n=0;n<256;n++) {
        DWORD token=decl[n];
        if(token==0xffffffffUL)break;
        if((token>>29)!=1 && (token>>29)!=2)return (HRESULT)0x80070057;
    }
    if(n==256 || halo_vita_vertex_catalog_find(code,4+instructions*16,decl,(n+1)*4,&index)<0)
        return (HRESULT)0x80070057;
    for(i=0;i<128 && handles[i].handle;i++);
    if(i==128 || next_handle>0xfffffffcU)return (HRESULT)0x8007000e;
    if(!programs[index].program) {
        result=halo_vita_vertex_catalog_load(index,&programs[index]);
        if(result<0)return result;
    }
    handles[i].index=index;handles[i].handle=next_handle;next_handle+=2;
    *out=handles[i].handle;return 0;
}
void WINAPI D3DDevice_DeleteVertexShader(DWORD handle)
{
    int i=lookup(handle);if(i<0)invalid("delete");
    handles[i].handle=0;
}
void WINAPI D3DDevice_GetVertexShaderSize(DWORD handle,UINT *size)
{
    int i=lookup(handle);if(i<0 || !size)invalid("size");
    *size=(programs[handles[i].index].desc->func_size-4)/16;
}
void WINAPI D3DDevice_LoadVertexShader(DWORD handle,DWORD address)
{
    int i=lookup(handle);unsigned size,j,start,end;
    if(i<0 || address>=136)invalid("load");
    size=(programs[handles[i].index].desc->func_size-4)/16;
    if(size>136-address)invalid("load extent");
    /* Replacing instructions invalidates every overlapping resident entry. */
    for(j=0;j<136;j++)if(slots[j]) {
        start=j;end=j+(programs[slots[j]-1].desc->func_size-4)/16;
        if(start<address+size && address<end)slots[j]=0;
    }
    slots[address]=handles[i].index+1;
}
void WINAPI D3DDevice_SetVertexShader(DWORD handle)
{
    int i=lookup(handle);if(i<0)invalid("set");
    D3DDevice_LoadVertexShader(handle,0);
    declaration=handles[i].index+1;selected_address=0;
}
void WINAPI D3DDevice_SelectVertexShader(DWORD handle,DWORD address)
{
    int i=handle?lookup(handle):-1;
    if(address>=136 || !slots[address] || (handle && i<0))invalid("select");
    if(handle)declaration=handles[i].index+1;
    if(!declaration)invalid("select declaration");
    selected_address=address;
}
int halo_vita_vertex_shader_current(const struct halo_vita_vertex_program **out)
{
    unsigned index;int result;
    if(!out || !declaration || !slots[selected_address])return -1;
    result=halo_vita_vertex_catalog_pair(slots[selected_address]-1,declaration-1,&index);
    if(result<0)return result;
    if(!programs[index].program) {
        result=halo_vita_vertex_catalog_load(index,&programs[index]);if(result<0)return result;
    }
    *out=&programs[index];return 0;
}
int halo_vita_vertex_shaders_shutdown(void)
{
    unsigned i;int result;
    if(halo_vita_graphics_scene_active() || halo_vita_logical_poll(0)!=0 ||
       halo_vita_logical_poll(1)!=0)return -1;
    for(i=0;i<67;i++) {
        result=halo_vita_vertex_program_destroy(&programs[i]);if(result<0)return result;
    }
    memset(handles,0,sizeof(handles));memset(slots,0,sizeof(slots));
    declaration=selected_address=0;return 0;
}
