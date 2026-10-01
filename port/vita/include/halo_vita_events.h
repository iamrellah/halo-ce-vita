#ifndef HALO_VITA_EVENTS_H
#define HALO_VITA_EVENTS_H
#include "halo_vita_handles.h"
/* Retain an EVENT-typed reference throughout these calls. The wait caller
 * must release its reference on cancellation as well as normal return. */
void halo_vita_event_set(struct halo_vita_handle *event);
void halo_vita_event_reset(struct halo_vita_handle *event);
DWORD halo_vita_event_wait(struct halo_vita_handle *event, DWORD milliseconds);
#endif
