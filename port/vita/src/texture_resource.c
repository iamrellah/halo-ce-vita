#include "halo_vita_texture_resource.h"
#include <string.h>

static int matches(const struct halo_vita_texture_resource *r,uint32_t token)
{
    return r && r->registered && token && r->generation==token;
}
int halo_vita_texture_resource_register(struct halo_vita_texture_resource *r,
    const struct halo_vita_bitmap_layout *layout,const void *source,size_t bytes,
    unsigned mip,unsigned face,const unsigned char *loaded,uint32_t *token)
{
    struct halo_vita_texture_view view;
    if (!r || !loaded || !token || r->registered || r->generation==UINT32_MAX ||
        r->upload.uid>=0 || r->upload.mapped || r->upload.valid || r->upload.base ||
        r->upload.references) return -1;
    if (halo_vita_texture_select(layout,source,bytes,mip,face,&view)<0) return -1;
    r->view=view;r->loaded=loaded;r->failed=0;r->registered=1;
    r->generation++;*token=r->generation;
    return 0;
}
int halo_vita_texture_resource_prepare(struct halo_vita_texture_resource *r,uint32_t token)
{
    int result;
    if (!matches(r,token) || r->failed) return -1;
    if (!__atomic_load_n(r->loaded,__ATOMIC_ACQUIRE)) return 1;
    if (r->upload.valid) return 0;
    if (!r->view.linear && r->view.bitmap_format>=14 && r->view.bitmap_format<=16 &&
        r->view.width>=4 && r->view.height>=4)
        result=halo_vita_texture_upload_create_bc(&r->upload,&r->view);
    else result=halo_vita_texture_upload_create(&r->upload,&r->view);
    if (result<0) r->failed=1; /* unregister owns any partial cleanup */
    return result;
}
int halo_vita_texture_resource_busy(const struct halo_vita_texture_resource *r,uint32_t token)
{
    if (!matches(r,token)) return -1;
    return !__atomic_load_n(r->loaded,__ATOMIC_ACQUIRE) || r->upload.references!=0;
}
int halo_vita_texture_resource_pin(struct halo_vita_texture_resource *r,uint32_t token)
{
    if (!matches(r,token) || r->failed) return -1;
    return halo_vita_texture_upload_pin(&r->upload);
}
int halo_vita_texture_resource_retire(struct halo_vita_texture_resource *r,uint32_t token)
{
    if (!matches(r,token)) return -1;
    return halo_vita_texture_upload_retire(&r->upload);
}
const SceGxmTexture *halo_vita_texture_resource_get(const struct halo_vita_texture_resource *r,uint32_t token)
{
    if (!matches(r,token) || r->failed || !r->upload.valid || !r->upload.references) return NULL;
    return &r->upload.texture;
}
int halo_vita_texture_resource_unregister(struct halo_vita_texture_resource *r,uint32_t token)
{
    int result;
    if (halo_vita_texture_resource_busy(r,token)!=0) return -1;
    result=halo_vita_texture_upload_destroy(&r->upload);
    if (result<0) return result;
    r->registered=0;r->failed=0;r->loaded=NULL;
    memset(&r->view,0,sizeof(r->view));
    return 0;
}
