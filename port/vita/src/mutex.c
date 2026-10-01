#include "halo_vita_mutex.h"
#include "halo_vita_wait.h"
#include <pthread.h>
#include <stdlib.h>
struct vita_mutex { pthread_t owner; unsigned depth; int abandoned; };
struct owned_mutex { struct owned_mutex *next; struct halo_vita_handle *reference; };
static pthread_mutex_t ownership_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_key_t owned_key;
static pthread_once_t once = PTHREAD_ONCE_INIT;
static void lock_ownership(void) { if (pthread_mutex_lock(&ownership_lock)) abort(); }
static void unlock_ownership(void) { if (pthread_mutex_unlock(&ownership_lock)) abort(); }
static void abandon(void *data)
{
    struct owned_mutex *node = data;
    while (node) {
        struct owned_mutex *next = node->next;
        struct vita_mutex *mutex = halo_vita_handle_data(node->reference);
        lock_ownership();
        if (!mutex->depth || !pthread_equal(mutex->owner, pthread_self())) abort();
        mutex->depth = 0; mutex->abandoned = 1;
        unlock_ownership();
        halo_vita_handle_release(node->reference);
        free(node); node = next;
    }
    halo_vita_wait_notify();
}
static void initialize(void) { if (pthread_key_create(&owned_key, abandon)) abort(); }
void halo_vita_mutex_thread_exit(void)
{
    void *owned;
    if (pthread_once(&once, initialize)) abort();
    owned = pthread_getspecific(owned_key);
    if (!owned) return;
    if (pthread_setspecific(owned_key, NULL)) abort();
    abandon(owned);
}
static void destroy(void *data) { free(data); }
DWORD halo_vita_mutex_try_acquire(struct halo_vita_handle *reference)
{
    struct vita_mutex *mutex = halo_vita_handle_data(reference);
    struct owned_mutex *node;
    DWORD result;
    if (pthread_once(&once, initialize)) abort();
    lock_ownership();
    if (mutex->depth) {
        if (!pthread_equal(mutex->owner, pthread_self())) { unlock_ownership(); return WAIT_TIMEOUT; }
        if (mutex->depth == ~0U) { unlock_ownership(); SetLastError(ERROR_NO_SYSTEM_RESOURCES); return WAIT_FAILED; }
        mutex->depth++; unlock_ownership(); return WAIT_OBJECT_0;
    }
    node = malloc(sizeof(*node));
    if (!node) { unlock_ownership(); SetLastError(ERROR_NOT_ENOUGH_MEMORY); return WAIT_FAILED; }
    node->next = pthread_getspecific(owned_key); node->reference = reference;
    if (pthread_setspecific(owned_key, node)) {
        free(node); unlock_ownership(); SetLastError(ERROR_NO_SYSTEM_RESOURCES); return WAIT_FAILED;
    }
    /* Ownership keeps the object alive even if its public handle is closed. */
    halo_vita_handle_retain(reference);
    mutex->owner = pthread_self(); mutex->depth = 1;
    result = mutex->abandoned ? WAIT_ABANDONED : WAIT_OBJECT_0;
    mutex->abandoned = 0;
    unlock_ownership(); return result;
}
HANDLE WINAPI CreateMutexA(void *security, BOOL initial, const char *name)
{
    struct vita_mutex *mutex;
    struct halo_vita_handle *reference;
    HANDLE token;
    if (security || name) { SetLastError(120L); return NULL; }
    mutex = calloc(1, sizeof(*mutex));
    if (!mutex) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return NULL; }
    token = halo_vita_handle_create(HALO_VITA_HANDLE_MUTEX, mutex, destroy);
    if (token == INVALID_HANDLE_VALUE) { free(mutex); return NULL; }
    if (initial) {
        DWORD result;
        reference = halo_vita_handle_acquire(token, HALO_VITA_HANDLE_MUTEX);
        if (!reference) abort();
        result = halo_vita_mutex_try_acquire(reference);
        halo_vita_handle_release(reference);
        if (result != WAIT_OBJECT_0) {
            DWORD error = GetLastError(); CloseHandle(token); SetLastError(error); return NULL;
        }
    }
    SetLastError(ERROR_SUCCESS); return token;
}
BOOL WINAPI ReleaseMutex(HANDLE token)
{
    struct halo_vita_handle *reference = halo_vita_handle_acquire(token, HALO_VITA_HANDLE_MUTEX);
    struct vita_mutex *mutex;
    struct owned_mutex *head, **link, *removed = NULL;
    if (!reference) return FALSE;
    mutex = halo_vita_handle_data(reference);
    lock_ownership();
    if (!mutex->depth || !pthread_equal(mutex->owner, pthread_self())) {
        unlock_ownership(); halo_vita_handle_release(reference); SetLastError(ERROR_NOT_OWNER); return FALSE;
    }
    if (--mutex->depth == 0) {
        head = pthread_getspecific(owned_key);
        for (link = &head; *link && (*link)->reference != reference; link = &(*link)->next) {}
        if (!*link) abort();
        removed = *link; *link = removed->next;
        if (pthread_setspecific(owned_key, head)) abort();
    }
    unlock_ownership();
    if (removed) { halo_vita_handle_release(removed->reference); free(removed); halo_vita_wait_notify(); }
    halo_vita_handle_release(reference); return TRUE;
}
