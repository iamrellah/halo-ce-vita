#ifndef HALO_VITA_DEVICE_H
#define HALO_VITA_DEVICE_H
/* Native device lifecycle: two 640x480 logical targets, three 960x544 display slots.
 * Not a Direct3D8 compatibility object or a report of supported Xbox caps. */
int halo_vita_device_create(void);
int halo_vita_device_destroy(void);
/* One fixed-color bring-up frame. 1 = no retired slot yet, <0 = failure. */
int halo_vita_device_test_frame(void);
/* Present selected logical color buffer and advance only on success.
 * 1 = target write/display slot pending; caller may retry without advancing. */
int halo_vita_device_present_logical(void);
int halo_vita_device_is_ready(void);
#endif
