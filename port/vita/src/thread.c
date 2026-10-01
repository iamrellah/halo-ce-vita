#include "halo_vita_thread.h"
#include "halo_vita_thread_native.h"
#include "halo_vita_completion.h"
#include "halo_vita_mutex.h"
#include "halo_vita_wait.h"
#include <pthread.h>
#include <stdlib.h>
#include <float.h>
struct vita_thread {
    pthread_mutex_t lock;
    pthread_cond_t ready;
    pthread_t native;
    struct halo_vita_handle *reference;
    LPTHREAD_START_ROUTINE start;
    void *argument;
    DWORD code;
    int suspended, finished, started, native_id;
};
static void destroy(void *data)
{
    struct vita_thread *t = data;
    if (pthread_cond_destroy(&t->ready)) abort();
    if (pthread_mutex_destroy(&t->lock)) abort();
    free(t);
}
static void unlock(void *data) { if (pthread_mutex_unlock(&((struct vita_thread *)data)->lock)) abort(); }
static void finish(void *data)
{
    struct vita_thread *t = data;
    int ignored;
    if (pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &ignored)) abort();
    /* Publish completion only after I/O drain and mutex abandonment. Engine
     * callbacks must retain buffers through their own return/exit cleanup. */
    halo_vita_completion_thread_exit();
    halo_vita_mutex_thread_exit();
    if (pthread_mutex_lock(&t->lock)) abort();
    t->finished = 1;
    unlock(t);
    halo_vita_wait_notify();
    halo_vita_handle_release(t->reference);
}
static void *run(void *data)
{
    struct vita_thread *t = data;
    DWORD code;
    pthread_cleanup_push(finish, t);
    if (pthread_mutex_lock(&t->lock)) abort();
    pthread_cleanup_push(unlock, t);
    t->native_id = halo_vita_native_thread_id();
    if (t->native_id < 0) abort();
    t->started = 1;
    if (pthread_cond_broadcast(&t->ready)) abort();
    while (t->suspended) if (pthread_cond_wait(&t->ready, &t->lock)) abort();
    pthread_cleanup_pop(1);
    /* The Win32-style start routine gets an explicit supported FP environment,
     * independent of pthread inheritance or its creator's rounding mode. */
    _control87(CW_DEFAULT,0xfffff);
    _clearfp();
    code = t->start(t->argument);
    if (pthread_mutex_lock(&t->lock)) abort();
    t->code = code;
    unlock(t);
    pthread_cleanup_pop(1);
    return NULL;
}
HANDLE WINAPI CreateThread(void *security, DWORD stack_size, LPTHREAD_START_ROUTINE start,
    void *argument, DWORD flags, LPDWORD id)
{
    struct vita_thread *t;
    struct halo_vita_handle *reference;
    pthread_attr_t attr;
    HANDLE token;
    int error;
    if (id) *id = 0;
    if (security || !start || (flags & ~CREATE_SUSPENDED)) {
        SetLastError(ERROR_INVALID_PARAMETER); return NULL;
    }
    t = calloc(1, sizeof(*t));
    if (!t) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return NULL; }
    if (pthread_mutex_init(&t->lock, NULL)) { free(t); SetLastError(ERROR_NO_SYSTEM_RESOURCES); return NULL; }
    if (pthread_cond_init(&t->ready, NULL)) {
        pthread_mutex_destroy(&t->lock); free(t); SetLastError(ERROR_NO_SYSTEM_RESOURCES); return NULL;
    }
    t->start = start; t->argument = argument; t->suspended = !!(flags & CREATE_SUSPENDED);
    /* Non-returning pthread exits without an engine exit code are abnormal. */
    t->code = ERROR_OPERATION_ABORTED;
    token = halo_vita_handle_create(HALO_VITA_HANDLE_THREAD, t, destroy);
    if (token == INVALID_HANDLE_VALUE) { DWORD saved = GetLastError(); destroy(t); SetLastError(saved); return NULL; }
    reference = halo_vita_handle_acquire(token, HALO_VITA_HANDLE_THREAD);
    if (!reference) abort();
    t->reference = reference;
    error = pthread_attr_init(&attr);
    if (!error) {
        error = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if (!error) error = pthread_attr_setstacksize(&attr, stack_size < 65536 ? 65536 : stack_size);
        if (!error) error = pthread_create(&t->native, &attr, run, t);
        if (pthread_attr_destroy(&attr)) abort();
    }
    if (error) {
        halo_vita_handle_release(reference); CloseHandle(token);
        SetLastError(ERROR_NO_SYSTEM_RESOURCES); return NULL;
    }
    /* Publish a real kernel ID even for a suspended start routine. */
    if (pthread_mutex_lock(&t->lock)) abort();
    while (!t->started) if (pthread_cond_wait(&t->ready, &t->lock)) abort();
    if (id) *id = (DWORD)t->native_id;
    unlock(t);
    return token;
}
BOOL WINAPI SetThreadPriority(HANDLE token, int priority)
{
    struct halo_vita_handle *reference;
    struct vita_thread *t;
    int native_priority, result;
    /* Keep the ordinary hints near the SDK default (native 159), within
     * its user range 128..191; lower native numbers have higher priority. */
    if (priority >= -2 && priority <= 2) native_priority = 159 - priority * 8;
    else if (priority == -15) native_priority = 191;
    else if (priority == 15) native_priority = 128;
    else { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    if (token == (HANDLE)-2) result = halo_vita_native_thread_priority(halo_vita_native_thread_id(), native_priority);
    else {
        reference = halo_vita_handle_acquire(token, HALO_VITA_HANDLE_THREAD);
        if (!reference) return FALSE;
        t = halo_vita_handle_data(reference);
        if (pthread_mutex_lock(&t->lock)) abort();
        result = t->finished ? -1 : halo_vita_native_thread_priority(t->native_id, native_priority);
        unlock(t); halo_vita_handle_release(reference);
    }
    if (result < 0) { SetLastError(ERROR_GEN_FAILURE); return FALSE; }
    return TRUE;
}
DWORD WINAPI ResumeThread(HANDLE token)
{
    struct halo_vita_handle *reference = halo_vita_handle_acquire(token, HALO_VITA_HANDLE_THREAD);
    struct vita_thread *t;
    DWORD previous;
    if (!reference) return (DWORD)-1;
    t = halo_vita_handle_data(reference);
    if (pthread_mutex_lock(&t->lock)) abort();
    previous = t->suspended;
    t->suspended = 0;
    if (pthread_cond_broadcast(&t->ready)) abort();
    unlock(t); halo_vita_handle_release(reference); return previous;
}
BOOL WINAPI GetExitCodeThread(HANDLE token, LPDWORD code)
{
    struct halo_vita_handle *reference;
    struct vita_thread *t;
    if (!code) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    reference = halo_vita_handle_acquire(token, HALO_VITA_HANDLE_THREAD);
    if (!reference) return FALSE;
    t = halo_vita_handle_data(reference);
    if (pthread_mutex_lock(&t->lock)) abort();
    *code = t->finished ? t->code : STILL_ACTIVE;
    unlock(t); halo_vita_handle_release(reference); return TRUE;
}
DWORD halo_vita_thread_poll(struct halo_vita_handle *reference)
{
    struct vita_thread *t = halo_vita_handle_data(reference);
    DWORD result;
    if (pthread_mutex_lock(&t->lock)) abort();
    result = t->finished ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
    unlock(t); return result;
}
