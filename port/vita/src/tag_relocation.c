#include "halo_vita_relocation.h"
#include <string.h>
#include <limits.h>

int halo_vita_relocate_tags(void *data, size_t size,
    const struct halo_vita_relocation *plan, size_t count)
{
    uintptr_t base = (uintptr_t)data, source = (uintptr_t)plan;
    size_t i, plan_bytes;
    uint32_t previous = 0;
    if (!data || !size || size > UINT32_MAX || base > UINT32_MAX - (size - 1))
        return 0;
    if (!count) return 1;
    if (!plan || (source & 3) || count > SIZE_MAX / sizeof(*plan)) return 0;
    plan_bytes = count * sizeof(*plan);
    if (source > UINTPTR_MAX - (plan_bytes - 1)) return 0;
    /* Inclusive ranges avoid representing an end address past UINTPTR_MAX. */
    if (source <= base + size - 1 && base <= source + plan_bytes - 1) return 0;
    for (i = 0; i < count; i++) {
        const struct halo_vita_relocation *entry = &plan[i];
        uint32_t original;
        if (size < 4 || entry->field_offset > size - 4 ||
            (entry->field_offset & 3) || (i && entry->field_offset < previous + 4))
            return 0;
        previous = entry->field_offset;
        memcpy(&original, (unsigned char *)data + entry->field_offset, 4);
        if (original != entry->expected) return 0;
        if (entry->kind == HALO_VITA_REBASE_TAG) {
            if (!entry->span_bytes || entry->target_offset >= size ||
                entry->span_bytes > size - entry->target_offset) return 0;
        } else if (entry->kind == HALO_VITA_CLEAR_POINTER) {
            if (entry->target_offset || entry->span_bytes) return 0;
        } else return 0;
    }
    for (i = 0; i < count; i++) {
        uint32_t value = plan[i].kind == HALO_VITA_REBASE_TAG ?
            (uint32_t)(base + plan[i].target_offset) : 0;
        memcpy((unsigned char *)data + plan[i].field_offset, &value, 4);
    }
    return 1;
}
