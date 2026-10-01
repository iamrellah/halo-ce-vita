#include "halo_vita_events.h"
#include "halo_vita_mutex.h"
#include "halo_vita_thread.h"
#include "halo_vita_completion.h"
#include "halo_vita_wait.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed;
static pthread_once_t once = PTHREAD_ONCE_INIT;
static uint64_t generation;
static void initialize(void)
{
    pthread_condattr_t attr;
    if (pthread_condattr_init(&attr)) abort();
    if (pthread_condattr_setclock(&attr, CLOCK_MONOTONIC)) abort();
    if (pthread_cond_init(&changed, &attr)) abort();
    if (pthread_condattr_destroy(&attr)) abort();
}
static void unlock(void *unused) { (void)unused; if (pthread_mutex_unlock(&lock)) abort(); }
static void release(void *reference) { halo_vita_handle_release(reference); }
void halo_vita_wait_notify(void)
{
    if (pthread_once(&once, initialize)) abort();
    if (pthread_mutex_lock(&lock)) abort();
    generation++;
    if (pthread_cond_broadcast(&changed)) abort();
    unlock(NULL);
}
DWORD WINAPI WaitForSingleObjectEx(HANDLE token, DWORD milliseconds, BOOL alertable)
{
    struct halo_vita_handle *reference;
    struct timespec deadline;
    DWORD result = WAIT_FAILED;
    int expired = 0;
    reference = halo_vita_handle_acquire(token, HALO_VITA_HANDLE_ANY);
    if (!reference) return WAIT_FAILED;
    if (halo_vita_handle_type(reference) != HALO_VITA_HANDLE_EVENT &&
        halo_vita_handle_type(reference) != HALO_VITA_HANDLE_MUTEX &&
        halo_vita_handle_type(reference) != HALO_VITA_HANDLE_THREAD) {
        halo_vita_handle_release(reference); SetLastError(ERROR_INVALID_HANDLE); return WAIT_FAILED;
    }
    if (pthread_once(&once, initialize)) abort();
    if (milliseconds && milliseconds != INFINITE) {
        if (clock_gettime(CLOCK_MONOTONIC, &deadline)) {
            int saved = errno;
            halo_vita_handle_release(reference);
            halo_vita_set_last_error_from_errno(saved); return WAIT_FAILED;
        }
        deadline.tv_sec += milliseconds / 1000;
        deadline.tv_nsec += (milliseconds % 1000) * 1000000L;
        if (deadline.tv_nsec >= 1000000000L) { deadline.tv_sec++; deadline.tv_nsec -= 1000000000L; }
    }
    pthread_cleanup_push(release, reference);
    for (;;) {
        uint64_t observed;
        int error = 0;
        if (pthread_mutex_lock(&lock)) abort();
        observed = generation;
        unlock(NULL);
        /* No notification lock held across object checks or callbacks. */
        if (alertable && halo_vita_completion_dispatch()) { result = WAIT_IO_COMPLETION; break; }
        result = halo_vita_handle_type(reference) == HALO_VITA_HANDLE_EVENT ?
            halo_vita_event_wait(reference, 0) :
            halo_vita_handle_type(reference) == HALO_VITA_HANDLE_MUTEX ?
                halo_vita_mutex_try_acquire(reference) : halo_vita_thread_poll(reference);
        if (result != WAIT_TIMEOUT || !milliseconds || expired) break;
        if (milliseconds != INFINITE) {
            struct timespec now;
            if (clock_gettime(CLOCK_MONOTONIC, &now)) {
                halo_vita_set_last_error_from_errno(errno); result = WAIT_FAILED; break;
            }
            if (now.tv_sec > deadline.tv_sec ||
                (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec)) break;
        }
        if (pthread_mutex_lock(&lock)) abort();
        pthread_cleanup_push(unlock, NULL);
        if (generation == observed) {
            error = milliseconds == INFINITE ? pthread_cond_wait(&changed, &lock) :
                pthread_cond_timedwait(&changed, &lock, &deadline);
        }
        pthread_cleanup_pop(1);
        if (error == ETIMEDOUT) expired = 1;
        else if (error) { SetLastError(ERROR_GEN_FAILURE); result = WAIT_FAILED; break; }
        /* Recheck predicates on wake/timeout; generation prevents missed wakes. */
    }
    pthread_cleanup_pop(1);
    return result;
}
DWORD WINAPI WaitForSingleObject(HANDLE token, DWORD milliseconds)
{ return WaitForSingleObjectEx(token, milliseconds, FALSE); }
