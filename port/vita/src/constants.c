#include "halo_vita_constants.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static float constants[192][4];
static float viewport_constants[2][4];
static DWORD mode;
static int valid(int first,unsigned count)
{ return first>=-96 && first<=95 && count<=(unsigned)(96-first); }
static void reserved(void)
{
    if(!(mode&0x10))memcpy(constants[58],viewport_constants,sizeof(viewport_constants));
}
void halo_vita_constants_reset(void)
{
    memset(constants,0,sizeof(constants));memset(viewport_constants,0,sizeof(viewport_constants));mode=0;
}
void halo_vita_constants_viewport(const D3DVIEWPORT8 *v)
{
    /* Current native targets are D24S8. Same reserved register values as
     * upstream viewport_update_constants; translated shaders consume them. */
    viewport_constants[0][0]=v->Width*0.5f;viewport_constants[0][1]=-(v->Height*0.5f);
    viewport_constants[0][2]=16777215.0f*(v->MaxZ-v->MinZ);viewport_constants[0][3]=0;
    viewport_constants[1][0]=v->X+v->Width*0.5f;viewport_constants[1][1]=v->Y+v->Height*0.5f;
    viewport_constants[1][2]=16777215.0f*v->MinZ;viewport_constants[1][3]=0;
    reserved();
}
void WINAPI D3DDevice_SetShaderConstantMode(DWORD value)
{
    if(value&~0x11UL){fprintf(stderr,"native Vita: unsupported shader constant mode\n");abort();}
    mode=value;reserved();
}
void WINAPI D3DDevice_SetVertexShaderConstant(INT first,const void *data,DWORD count)
{
    /* Match upstream clipping, but avoid signed overflow in first+96/count. */
    if(first<-96 || first>95 || !count)return;
    if(count>(DWORD)(96-first))count=96-first;
    if(!data){fprintf(stderr,"native Vita: null vertex constants\n");abort();}
    memcpy(constants[first+96],data,count*16);
}
int halo_vita_constants_copy(int first,unsigned count,void *out)
{
    if(!valid(first,count) || (count && !out))return -1;
    if(count)memcpy(out,constants[first+96],count*16);
    return 0;
}
int halo_vita_constants_upload(void *uniforms,const SceGxmProgramParameter *parameter,int first,unsigned count)
{
    if(!valid(first,count) || !uniforms || !parameter)return -1;
    if(!count)return 0;
    return sceGxmSetUniformDataF(uniforms,parameter,0,count*4,constants[first+96]);
}
