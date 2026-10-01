/* Public handles are generation-tagged integers, never unchecked pointers.
 * Closing detaches the public token; acquired operations retain the resource. */
#include "halo_vita_handles.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

#define HANDLE_SLOTS 256
#define MAX_GENERATION 0x7fffffU
struct halo_vita_handle {
    enum halo_vita_handle_type type;
    unsigned int references;
    void *data;
    void (*destroy)(void *);
};
struct handle_slot {
    struct halo_vita_handle *object;
    unsigned int generation;
};
static struct handle_slot slots[HANDLE_SLOTS];
static pthread_mutex_t table_lock = PTHREAD_MUTEX_INITIALIZER;

static void lock_table(void) { if (pthread_mutex_lock(&table_lock)) abort(); }
static void unlock_table(void) { if (pthread_mutex_unlock(&table_lock)) abort(); }
static void destroy_object(struct halo_vita_handle *object)
{
    if (object->destroy) object->destroy(object->data);
    free(object);
}

HANDLE halo_vita_handle_create(enum halo_vita_handle_type type, void *data, void (*destroy)(void *))
{
    unsigned int i;
    struct halo_vita_handle *object;
    if (type < HALO_VITA_HANDLE_FILE || type > HALO_VITA_HANDLE_FIND) {
        SetLastError(ERROR_INVALID_PARAMETER); return INVALID_HANDLE_VALUE;
    }
    object = malloc(sizeof(*object));
    if (!object) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return INVALID_HANDLE_VALUE; }
    object->type = type; object->references = 1; object->data = data; object->destroy = destroy;
    lock_table();
    for (i = 0; i < HANDLE_SLOTS; i++) {
        if (!slots[i].object && slots[i].generation < MAX_GENERATION) {
            uintptr_t token;
            slots[i].generation++;
            slots[i].object = object;
            token = ((uintptr_t)slots[i].generation << 8) | i;
            unlock_table();
            return (HANDLE)token;
        }
    }
    unlock_table(); free(object);
    SetLastError(ERROR_TOO_MANY_OPEN_FILES);
    return INVALID_HANDLE_VALUE;
}

/* Must hold table_lock. */
static struct handle_slot *lookup(HANDLE token)
{
    uintptr_t value = (uintptr_t)token;
    unsigned int index = value & 255U;
    uintptr_t generation = value >> 8;
    if (!generation || generation > MAX_GENERATION ||
        slots[index].generation != generation || !slots[index].object) return NULL;
    return &slots[index];
}

struct halo_vita_handle *halo_vita_handle_acquire(HANDLE token, enum halo_vita_handle_type type)
{
    struct handle_slot *slot;
    struct halo_vita_handle *object = NULL;
    lock_table();
    slot = lookup(token);
    if (slot && (!type || slot->object->type == type) && slot->object->references != ~0U) {
        object = slot->object;
        object->references++;
    }
    unlock_table();
    if (!object) SetLastError(ERROR_INVALID_HANDLE);
    return object;
}

void *halo_vita_handle_data(struct halo_vita_handle *object) { return object->data; }
enum halo_vita_handle_type halo_vita_handle_type(struct halo_vita_handle *object) { return object->type; }
void halo_vita_handle_retain(struct halo_vita_handle *object)
{
    lock_table();
    if (!object->references || object->references == ~0U) abort();
    object->references++;
    unlock_table();
}

void halo_vita_handle_release(struct halo_vita_handle *object)
{
    int destroy;
    lock_table();
    if (!object->references) abort();
    destroy = --object->references == 0;
    unlock_table();
    if (destroy) destroy_object(object);
}

BOOL WINAPI CloseHandle(HANDLE token)
{
    struct handle_slot *slot;
    struct halo_vita_handle *object;
    int destroy;
    lock_table();
    slot = lookup(token);
    if (!slot) { unlock_table(); SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
    object = slot->object;
    slot->object = NULL;
    destroy = --object->references == 0;
    unlock_table();
    if (destroy) destroy_object(object);
    return TRUE;
}
