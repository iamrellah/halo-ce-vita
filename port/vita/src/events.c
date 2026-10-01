#include "halo_vita_events.h"
#include "halo_vita_wait.h"
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

struct vita_event {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    int manual, signaled;
};
static void unlock_event(void *data)
{ if (pthread_mutex_unlock(&((struct vita_event *)data)->lock)) abort(); }
static void destroy_event(void *data)
{
    struct vita_event *event = data;
    if (pthread_cond_destroy(&event->changed)) abort();
    if (pthread_mutex_destroy(&event->lock)) abort();
    free(event);
}

HANDLE WINAPI CreateEventA(void *security, BOOL manual, BOOL initial, const char *name)
{
    struct vita_event *event;
    pthread_condattr_t attr;
    HANDLE token;
    int error;
    /* Named objects require a shared namespace, not a fresh anonymous event. */
    if (security || name) { SetLastError(120L); return NULL; }
    event = calloc(1, sizeof(*event));
    if (!event) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return NULL; }
    error = pthread_mutex_init(&event->lock, NULL);
    if (error) { free(event); SetLastError(ERROR_NO_SYSTEM_RESOURCES); return NULL; }
    error = pthread_condattr_init(&attr);
    if (!error) {
        error = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
        if (!error) error = pthread_cond_init(&event->changed, &attr);
        if (pthread_condattr_destroy(&attr)) abort();
    }
    if (error) {
        pthread_mutex_destroy(&event->lock); free(event);
        SetLastError(ERROR_NO_SYSTEM_RESOURCES); return NULL;
    }
    event->manual = !!manual; event->signaled = !!initial;
    token = halo_vita_handle_create(HALO_VITA_HANDLE_EVENT, event, destroy_event);
    if (token == INVALID_HANDLE_VALUE) {
        DWORD saved = GetLastError();
        destroy_event(event); SetLastError(saved); return NULL;
    }
    SetLastError(ERROR_SUCCESS);
    return token;
}

void halo_vita_event_set(struct halo_vita_handle *reference)
{
    struct vita_event *event = halo_vita_handle_data(reference);
    if (pthread_mutex_lock(&event->lock)) abort();
    event->signaled = 1;
    if (event->manual) { if (pthread_cond_broadcast(&event->changed)) abort(); }
    else { if (pthread_cond_signal(&event->changed)) abort(); }
    unlock_event(event);
    halo_vita_wait_notify();
}
void halo_vita_event_reset(struct halo_vita_handle *reference)
{
    struct vita_event *event = halo_vita_handle_data(reference);
    if (pthread_mutex_lock(&event->lock)) abort();
    event->signaled = 0;
    unlock_event(event);
}
BOOL WINAPI SetEvent(HANDLE token)
{
    struct halo_vita_handle *reference = halo_vita_handle_acquire(token, HALO_VITA_HANDLE_EVENT);
    if (!reference) return FALSE;
    halo_vita_event_set(reference); halo_vita_handle_release(reference); return TRUE;
}
BOOL WINAPI ResetEvent(HANDLE token)
{
    struct halo_vita_handle *reference = halo_vita_handle_acquire(token, HALO_VITA_HANDLE_EVENT);
    if (!reference) return FALSE;
    halo_vita_event_reset(reference); halo_vita_handle_release(reference); return TRUE;
}

DWORD halo_vita_event_wait(struct halo_vita_handle *reference, DWORD milliseconds)
{
    struct vita_event *event = halo_vita_handle_data(reference);
    struct timespec deadline;
    DWORD result = WAIT_OBJECT_0;
    int error = 0;
    if (milliseconds && milliseconds != INFINITE) {
        if (clock_gettime(CLOCK_MONOTONIC, &deadline)) {
            halo_vita_set_last_error_from_errno(errno); return WAIT_FAILED;
        }
        deadline.tv_sec += milliseconds / 1000;
        deadline.tv_nsec += (milliseconds % 1000) * 1000000L;
        if (deadline.tv_nsec >= 1000000000L) { deadline.tv_sec++; deadline.tv_nsec -= 1000000000L; }
    }
    if (pthread_mutex_lock(&event->lock)) abort();
    pthread_cleanup_push(unlock_event, event);
    while (!event->signaled) {
        if (!milliseconds) { result = WAIT_TIMEOUT; break; }
        error = milliseconds == INFINITE ? pthread_cond_wait(&event->changed, &event->lock) :
            pthread_cond_timedwait(&event->changed, &event->lock, &deadline);
        if (error == ETIMEDOUT) {
            if (!event->signaled) result = WAIT_TIMEOUT;
            break;
        }
        if (error) { SetLastError(ERROR_GEN_FAILURE); result = WAIT_FAILED; break; }
    }
    if (result == WAIT_OBJECT_0 && !event->manual) event->signaled = 0;
    pthread_cleanup_pop(1);
    return result;
}
