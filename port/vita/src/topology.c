#include "halo_vita_topology.h"
#include <stdint.h>
int halo_vita_topology(D3DPRIMITIVETYPE type,const uint16_t *in,size_t n,
    uint16_t *out,size_t capacity,SceGxmPrimitiveType *primitive,size_t *written)
{
    size_t total=n,i,k=0;
    SceGxmPrimitiveType mode;
    if(!primitive || !written || (!in && n>65536))return -1;
    switch(type) {
    case D3DPT_POINTLIST:mode=SCE_GXM_PRIMITIVE_POINTS;break;
    case D3DPT_LINELIST:if(n%2)return -1;mode=SCE_GXM_PRIMITIVE_LINES;break;
    case D3DPT_LINESTRIP:
    case D3DPT_LINELOOP:
        if(n && n<2)return -1;
        if(n>SIZE_MAX/2)return -1;
        total=n?(type==D3DPT_LINELOOP?n*2:(n-1)*2):0;
        mode=SCE_GXM_PRIMITIVE_LINES;break;
    case D3DPT_TRIANGLELIST:if(n%3)return -1;mode=SCE_GXM_PRIMITIVE_TRIANGLES;break;
    case D3DPT_TRIANGLESTRIP:
        if(n && n<3)return -1;mode=SCE_GXM_PRIMITIVE_TRIANGLE_STRIP;break;
    case D3DPT_QUADSTRIP:
        if(n && (n<4 || n%2))return -1;mode=SCE_GXM_PRIMITIVE_TRIANGLE_STRIP;break;
    case D3DPT_TRIANGLEFAN:
    case D3DPT_POLYGON:
        if(n && n<3)return -1;mode=SCE_GXM_PRIMITIVE_TRIANGLE_FAN;break;
    case D3DPT_QUADLIST:
        if(n%4 || n/4>SIZE_MAX/6)return -1;
        total=n/4*6;mode=SCE_GXM_PRIMITIVE_TRIANGLES;break;
    default:return -1;
    }
    if(total>capacity || total>SIZE_MAX/sizeof(*out) || (total && !out))return -1;
    if(n>SIZE_MAX/sizeof(*in))return -1;
    if(total && ((uintptr_t)out>UINTPTR_MAX-total*sizeof(*out)))return -1;
    if(in && n) {
        uintptr_t a=(uintptr_t)in,b=(uintptr_t)out;
        if(a>UINTPTR_MAX-n*sizeof(*in))return -1;
        if(total && a<b+total*sizeof(*out) && b<a+n*sizeof(*in))return -1;
    }
#define V(i) (in?in[(i)]:(uint16_t)(i))
    if(type==D3DPT_QUADLIST) {
        for(i=0;i<n;i+=4) {
            out[k++]=V(i);out[k++]=V(i+1);out[k++]=V(i+2);
            out[k++]=V(i);out[k++]=V(i+2);out[k++]=V(i+3);
        }
    } else if(type==D3DPT_LINESTRIP || type==D3DPT_LINELOOP) {
        for(i=1;i<n;i++){out[k++]=V(i-1);out[k++]=V(i);}
        if(n && type==D3DPT_LINELOOP){out[k++]=V(n-1);out[k++]=V(0);}
    } else for(i=0;i<n;i++)out[k++]=V(i);
#undef V
    *primitive=mode;*written=total;return 0;
}
