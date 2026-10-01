#include "xdk_xbox.h"
#include "halo_vita_device.h"
#include "halo_vita_streams.h"
#include "halo_vita_vertex_buffer.h"
#include "halo_vita_d3d_render.h"
#include <psp2/kernel/threadmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Xbox wrappers treat these interfaces as opaque singleton identities. They
 * are never dereferenced as desktop COM vtables. */
static unsigned direct3d_identity,device_identity;
static int created;
static void device_error(const char *operation,int code)
{
    fprintf(stderr,"native Vita: %s failed (%08x)\n",operation,(unsigned)code);
    abort(); /* Void Xbox entry points cannot return a failed HRESULT. */
}
Direct3D *WINAPI Direct3DCreate8(UINT version)
{
    if(version!=0)return NULL; /* January engine D3D_SDK_VERSION. */
    return (Direct3D *)&direct3d_identity;
}
HRESULT WINAPI Direct3D_CreateDevice(UINT adapter,D3DDEVTYPE type,void *window,
    DWORD flags,D3DPRESENT_PARAMETERS *parameters,D3DDevice **output)
{
    int result;
    if(!output)return E_INVALIDARG;
    *output=NULL;
    if(created || halo_vita_device_is_ready())return E_FAIL;
    if(!parameters || adapter!=0 || type!=D3DDEVTYPE_HAL || window || flags!=0x40 ||
       parameters->BackBufferWidth!=640 || parameters->BackBufferHeight!=480 ||
       parameters->BackBufferFormat!=D3DFMT_A8R8G8B8 ||
       (parameters->BackBufferCount!=0 && parameters->BackBufferCount!=2) ||
       (parameters->MultiSampleType!=0 && parameters->MultiSampleType!=0x11) ||
       parameters->SwapEffect!=D3DSWAPEFFECT_DISCARD || parameters->hDeviceWindow ||
       parameters->Windowed || !parameters->EnableAutoDepthStencil ||
       parameters->AutoDepthStencilFormat!=D3DFMT_D24S8 ||
       parameters->Flags!=1 || parameters->FullScreen_RefreshRateInHz!=0 ||
       parameters->FullScreen_PresentationInterval>1)return E_INVALIDARG;
    /* Default/ONE only. TWO and IMMEDIATE need explicit presentation policy;
     * accepting them while always waiting one vblank would misreport support. */
    result=halo_vita_device_create();
    if(result<0){fprintf(stderr,"native Vita: device creation failed (%08x)\n",(unsigned)result);return E_FAIL;}
    halo_vita_d3d_render_reset();
    created=1;*output=(D3DDevice *)&device_identity;return S_OK;
}
void WINAPI D3DDevice_GetDeviceCaps(D3DCAPS8 *caps)
{
    if(!created || !caps)device_error("GetDeviceCaps",-1);
    /* No full Xbox shader/volume/light capability claim. Current engine only
     * stores this descriptor; capabilities grow with implemented entry points. */
    memset(caps,0,sizeof(*caps));caps->DeviceType=D3DDEVTYPE_HAL;
}
void WINAPI D3DDevice_Present(const RECT *source,const RECT *destination,
    void *window,void *dirty)
{
    unsigned attempt;
    int result;
    if(!created || source || destination || window || dirty)device_error("Present parameters",-1);
    for(attempt=0;attempt<10000;attempt++) {
        result=halo_vita_device_present_logical();
        if(result==0) {
            if(halo_vita_vertex_buffers_collect()<0)device_error("Collect vertex buffers",-1);
            return;
        }
        if(result<0)device_error("Present",result);
        /* Target-specific pending completion/slot availability, never Finish. */
        sceKernelDelayThread(1000);
    }
    device_error("Present stalled",-1);
}
ULONG WINAPI D3DDevice_Release(void)
{
    int result;
    if(!created)device_error("Release without device",-1);
    if(halo_vita_streams_reset()<0)device_error("Release streams",-1);
    if(halo_vita_vertex_buffers_collect()<0)device_error("Collect vertex buffers",-1);
    result=halo_vita_device_destroy();
    if(result<0)device_error("Release with outstanding ownership",result);
    created=0;return 0;
}
