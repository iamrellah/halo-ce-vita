#ifndef HALO_VITA_HANDLES_H
#define HALO_VITA_HANDLES_H
#include "halo_vita_kernel.h"
enum halo_vita_handle_type {
    HALO_VITA_HANDLE_ANY = 0, /* acquisition only; inspect before use */
    HALO_VITA_HANDLE_FILE = 1,
    HALO_VITA_HANDLE_EVENT,
    HALO_VITA_HANDLE_MUTEX,
    HALO_VITA_HANDLE_THREAD,
    HALO_VITA_HANDLE_FIND
};
struct halo_vita_handle;
/* On failure the caller retains ownership of data. */
HANDLE halo_vita_handle_create(enum halo_vita_handle_type type, void *data, void (*destroy)(void *));
/* Every successful acquisition must be released, including pending I/O. */
struct halo_vita_handle *halo_vita_handle_acquire(HANDLE token, enum halo_vita_handle_type type);
void *halo_vita_handle_data(struct halo_vita_handle *handle);
enum halo_vita_handle_type halo_vita_handle_type(struct halo_vita_handle *handle);
void halo_vita_handle_retain(struct halo_vita_handle *handle);
void halo_vita_handle_release(struct halo_vita_handle *handle);
#endif
