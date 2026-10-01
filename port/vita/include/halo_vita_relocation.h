#ifndef HALO_VITA_RELOCATION_H
#define HALO_VITA_RELOCATION_H
#include <stdint.h>
#include <stddef.h>
enum halo_vita_relocation_kind { HALO_VITA_REBASE_TAG = 1, HALO_VITA_CLEAR_POINTER = 2 };
struct halo_vita_relocation {
    uint32_t field_offset, expected, target_offset, span_bytes, kind;
};
/* Exclusive caller ownership required. Sorted, nonoverlapping patch fields;
 * plan storage must not overlap data. Checks all entries before any writes.
 * Does not establish that a schema correctly classified a field. */
int halo_vita_relocate_tags(void *data, size_t size,
    const struct halo_vita_relocation *plan, size_t count);
#endif
