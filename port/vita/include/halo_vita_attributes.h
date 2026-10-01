#ifndef HALO_VITA_ATTRIBUTES_H
#define HALO_VITA_ATTRIBUTES_H
#include "halo_vita_streams.h"
void halo_vita_attributes_reset(void);
int halo_vita_attribute_snapshots_create(void);
int halo_vita_attribute_snapshots_destroy(void);
/* Called by logical begin only after the slot's previous GPU work retires. */
int halo_vita_attribute_snapshots_reset(unsigned slot);
/* Returns a retained CPU view; caller releases it after binding. */
int halo_vita_attributes_snapshot(struct halo_vita_stream_view *out);
#endif
