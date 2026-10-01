#ifndef HALO_VITA_BUFFER_H
#define HALO_VITA_BUFFER_H
#include <psp2/types.h>
#include <stddef.h>
#include <stdint.h>
/* Initialize with {.uid=-1}; single owner. Registry refs guard destruction.
 * CPU writers must separately avoid ranges still consumed by the GPU. */
struct halo_vita_buffer { SceUID uid; void *base; size_t bytes; uint32_t allocation; int mapped; };
int halo_vita_buffer_create(struct halo_vita_buffer *buffer,size_t bytes);
int halo_vita_buffer_destroy(struct halo_vita_buffer *buffer);
int halo_vita_buffer_create_format(struct halo_vita_buffer *buffer,size_t bytes,uint32_t format);
#endif
