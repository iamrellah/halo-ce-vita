#include "halo_vita_file_handle.h"
#include "halo_vita_completion.h"
#include "halo_vita_events.h"
#include <stdlib.h>

#define MAX_REQUESTS 64
struct io_request {
    struct io_request *next, *active_next;
    int registered, cancelled, io_done;
    struct halo_vita_completion completion;
    struct halo_vita_completion_queue *issuer;
    struct halo_vita_handle *file, *event;
    void *buffer;
    DWORD count, transferred, error;
    uint64_t offset;
    BOOL writing;
    LPOVERLAPPED overlapped;
    LPOVERLAPPED_COMPLETION_ROUTINE callback;
};
static pthread_mutex_t work_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t work_ready = PTHREAD_COND_INITIALIZER;
static pthread_cond_t work_done = PTHREAD_COND_INITIALIZER;
static pthread_once_t worker_once = PTHREAD_ONCE_INIT;
static pthread_t worker;
static int worker_error, stopping;
static unsigned int outstanding;
static struct io_request *head, *tail, *active;
static void lock_work(void) { if (pthread_mutex_lock(&work_lock)) abort(); }
static void unlock_work(void) { if (pthread_mutex_unlock(&work_lock)) abort(); }

/* work_lock held. Completed-but-undispatched requests remain registered. */
static void unregister_request(struct io_request *r)
{
    struct io_request **link;
    if (!r->registered) return;
    for (link = &active; *link && *link != r; link = &(*link)->active_next) {}
    if (!*link) abort();
    *link = r->active_next;
    r->registered = 0;
}

static void request_dispose(void *data)
{
    struct io_request *r = data;
    halo_vita_handle_release(r->file);
    if (r->event) halo_vita_handle_release(r->event);
    lock_work(); unregister_request(r); outstanding--; unlock_work();
    free(r);
}
static void request_callback(void *data)
{
    struct io_request *r = data;
    /* The callback may legally reuse the same record for its next request. */
    lock_work(); unregister_request(r); unlock_work();
    r->callback(r->error, r->transferred, r->overlapped);
}
static void *worker_main(void *unused)
{
    (void)unused;
    for (;;) {
        struct io_request *r;
        int cancelled;
        struct halo_vita_completion_queue *issuer;
        lock_work();
        while (!head && !stopping) if (pthread_cond_wait(&work_ready, &work_lock)) abort();
        if (!head && stopping) { unlock_work(); return NULL; }
        r = head; head = r->next; if (!head) tail = NULL;
        cancelled = r->cancelled;
        unlock_work();
        r->error = cancelled ? ERROR_OPERATION_ABORTED :
            halo_vita_file_transfer(r->file, r->buffer, r->count,
                &r->transferred, r->writing, TRUE, r->offset);
        lock_work();
        if (r->cancelled) r->error = ERROR_OPERATION_ABORTED;
        if (!r->writing && !r->error && r->count && !r->transferred) r->error = ERROR_HANDLE_EOF;
        r->overlapped->InternalHigh = r->transferred;
        __atomic_store_n(&r->overlapped->Internal, r->error, __ATOMIC_RELEASE);
        /* Serialize reset/reuse with completion; never look up a potentially
         * closed public event token here. The request owns the reference. */
        if (!r->callback) {
            if (r->event) halo_vita_event_set(r->event);
            unregister_request(r);
        }
        r->io_done = 1;
        if (pthread_cond_broadcast(&work_done)) abort();
        unlock_work();
        issuer = r->issuer;
        /* Posting may let the issuer free r immediately. Do not touch r after it. */
        if (!r->callback || !halo_vita_completion_post(issuer, &r->completion)) request_dispose(r);
        halo_vita_completion_release(issuer);
    }
}
static void start_worker(void) { worker_error = pthread_create(&worker, NULL, worker_main, NULL); }

static BOOL submit(HANDLE token, void *buffer, DWORD count, LPOVERLAPPED overlapped,
    LPOVERLAPPED_COMPLETION_ROUTINE callback, BOOL writing, BOOL event_mode)
{
    struct halo_vita_handle *reference;
    struct halo_vita_file *file;
    struct halo_vita_completion_queue *issuer;
    struct io_request *r;
    if (!overlapped || (!event_mode && !callback) || (!buffer && count)) {
        SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
    }
    reference = halo_vita_handle_acquire(token, HALO_VITA_HANDLE_FILE);
    if (!reference) return FALSE;
    file = halo_vita_handle_data(reference);
    if (!(file->flags & FILE_FLAG_OVERLAPPED) || !(file->access & (writing ? GENERIC_WRITE : GENERIC_READ))) {
        halo_vita_handle_release(reference); SetLastError(ERROR_ACCESS_DENIED); return FALSE;
    }
    issuer = halo_vita_completion_current();
    r = calloc(1, sizeof(*r));
    if (!issuer || !r) {
        if (issuer) halo_vita_completion_release(issuer);
        free(r); halo_vita_handle_release(reference); SetLastError(ERROR_NOT_ENOUGH_MEMORY); return FALSE;
    }
    if (pthread_once(&worker_once, start_worker)) abort();
    lock_work();
    if (worker_error || stopping || outstanding == MAX_REQUESTS) {
        unlock_work(); halo_vita_completion_release(issuer); halo_vita_handle_release(reference); free(r);
        SetLastError(ERROR_NO_SYSTEM_RESOURCES); return FALSE;
    }
    {
        struct io_request *pending;
        for (pending = active; pending; pending = pending->active_next) {
            if (pending->overlapped == overlapped) {
                unlock_work(); halo_vita_completion_release(issuer);
                halo_vita_handle_release(reference); free(r);
                SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
            }
        }
    }
    r->file = reference; r->issuer = issuer; r->buffer = buffer; r->count = count;
    if (event_mode && overlapped->hEvent) {
        r->event = halo_vita_handle_acquire(overlapped->hEvent, HALO_VITA_HANDLE_EVENT);
        if (!r->event) {
            unlock_work(); halo_vita_completion_release(issuer);
            halo_vita_handle_release(reference); free(r);
            SetLastError(ERROR_INVALID_HANDLE); return FALSE;
        }
        halo_vita_event_reset(r->event);
    }
    r->offset = ((uint64_t)overlapped->OffsetHigh << 32) | overlapped->Offset;
    r->overlapped = overlapped; r->callback = callback; r->writing = writing;
    r->completion.run = request_callback; r->completion.dispose = request_dispose; r->completion.context = r;
    overlapped->InternalHigh = 0;
    __atomic_store_n(&overlapped->Internal, ERROR_IO_PENDING, __ATOMIC_RELEASE);
    r->registered = 1; r->active_next = active; active = r;
    outstanding++;
    if (tail) tail->next = r; else head = r;
    tail = r;
    if (pthread_cond_signal(&work_ready)) abort();
    unlock_work();
    SetLastError(ERROR_SUCCESS);
    return TRUE;
}
BOOL WINAPI ReadFileEx(HANDLE file, void *buffer, DWORD count, LPOVERLAPPED request,
    LPOVERLAPPED_COMPLETION_ROUTINE callback)
{ return submit(file, buffer, count, request, callback, FALSE, FALSE); }
BOOL WINAPI WriteFileEx(HANDLE file, const void *buffer, DWORD count, LPOVERLAPPED request,
    LPOVERLAPPED_COMPLETION_ROUTINE callback)
{ return submit(file, (void *)buffer, count, request, callback, TRUE, FALSE); }

BOOL halo_vita_file_submit_event(HANDLE file, void *buffer, DWORD count,
    LPOVERLAPPED request, BOOL writing)
{
    if (!submit(file, buffer, count, request, NULL, writing, TRUE)) return FALSE;
    /* Ordinary APIs report accepted asynchronous work as FALSE/IO_PENDING. */
    SetLastError(ERROR_IO_PENDING);
    return FALSE;
}

static void result_wait_cleanup(void *file)
{
    unlock_work();
    halo_vita_handle_release(file);
}

BOOL WINAPI GetOverlappedResult(HANDLE token, LPOVERLAPPED request,
    LPDWORD transferred, BOOL wait)
{
    struct halo_vita_handle *file;
    struct io_request *pending;
    DWORD error;
    if (!request || !transferred) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    *transferred = 0;
    file = halo_vita_handle_acquire(token, HALO_VITA_HANDLE_FILE);
    if (!file) return FALSE;
    lock_work();
    pthread_cleanup_push(result_wait_cleanup, file);
    for (;;) {
        for (pending = active; pending && pending->overlapped != request; pending = pending->active_next) {}
        if (pending && pending->file != file) { error = ERROR_INVALID_PARAMETER; break; }
        error = __atomic_load_n(&request->Internal, __ATOMIC_ACQUIRE);
        if (error != ERROR_IO_PENDING) { *transferred = request->InternalHigh; break; }
        if (!pending) { error = ERROR_INVALID_PARAMETER; break; }
        if (!wait) { error = ERROR_IO_INCOMPLETE; break; }
        if (pthread_cond_wait(&work_done, &work_lock)) abort();
    }
    pthread_cleanup_pop(1);
    SetLastError(error);
    return error == ERROR_SUCCESS;
}

/* Startup/shutdown owner only, called once: drain transfers before platform
 * teardown. Issuing threads must still pump and retire their callbacks. */
int halo_vita_file_async_shutdown(void)
{
    if (pthread_once(&worker_once, start_worker)) abort();
    if (worker_error) return worker_error;
    lock_work(); stopping = 1;
    if (pthread_cond_broadcast(&work_ready)) abort();
    unlock_work();
    return pthread_join(worker, NULL);
}

/* CancelIo affects only this issuer's unfinished operations on the file. */
BOOL WINAPI CancelIo(HANDLE token)
{
    struct halo_vita_handle *file = halo_vita_handle_acquire(token, HALO_VITA_HANDLE_FILE);
    struct halo_vita_completion_queue *issuer;
    struct io_request *r;
    if (!file) return FALSE;
    issuer = halo_vita_completion_current();
    if (!issuer) { halo_vita_handle_release(file); SetLastError(ERROR_NOT_ENOUGH_MEMORY); return FALSE; }
    lock_work();
    for (r = active; r; r = r->active_next)
        if (r->issuer == issuer && r->file == file && !r->io_done) r->cancelled = 1;
    unlock_work();
    halo_vita_completion_release(issuer);
    halo_vita_handle_release(file);
    return TRUE;
}

/* No queue lock held. Native thread teardown calls this before dropping its
 * queue; pending transfers cannot outlive the drain and touch its storage. */
void halo_vita_file_cancel_and_drain_issuer(struct halo_vita_completion_queue *issuer)
{
    int pending, previous_cancel_state;
    struct io_request *r;
    if (pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &previous_cancel_state)) abort();
    lock_work();
    do {
        pending = 0;
        for (r = active; r; r = r->active_next) {
            if (r->issuer == issuer && !r->io_done) { r->cancelled = 1; pending = 1; }
        }
        if (pending && pthread_cond_wait(&work_done, &work_lock)) abort();
    } while (pending);
    unlock_work();
    if (pthread_setcancelstate(previous_cancel_state, NULL)) abort();
}
