#include "halo_vita_completion.h"
#include "halo_vita_wait.h"
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>

struct halo_vita_completion_queue {
    pthread_mutex_t lock;
    pthread_cond_t ready;
    unsigned int references;
    int accepting;
    struct halo_vita_completion *head, *tail;
};
static pthread_key_t queue_key;
static pthread_once_t queue_once = PTHREAD_ONCE_INIT;
static void lock_queue(struct halo_vita_completion_queue *q) { if (pthread_mutex_lock(&q->lock)) abort(); }
static void unlock_queue(struct halo_vita_completion_queue *q) { if (pthread_mutex_unlock(&q->lock)) abort(); }

void halo_vita_completion_release(struct halo_vita_completion_queue *q)
{
    int destroy;
    lock_queue(q);
    if (!q->references) abort();
    destroy = --q->references == 0;
    unlock_queue(q);
    if (destroy) { pthread_cond_destroy(&q->ready); pthread_mutex_destroy(&q->lock); free(q); }
}

static void thread_queue_destroy(void *data)
{
    struct halo_vita_completion_queue *q = data;
    struct halo_vita_completion *item;
    lock_queue(q);
    q->accepting = 0;
    item = q->head; q->head = q->tail = NULL;
    unlock_queue(q);
    halo_vita_file_cancel_and_drain_issuer(q);
    while (item) {
        struct halo_vita_completion *next = item->next;
        item->dispose(item->context);
        item = next;
    }
    halo_vita_completion_release(q);
}
static void create_key(void) { if (pthread_key_create(&queue_key, thread_queue_destroy)) abort(); }
void halo_vita_completion_thread_exit(void)
{
    struct halo_vita_completion_queue *q;
    if (pthread_once(&queue_once, create_key)) abort();
    q = pthread_getspecific(queue_key);
    if (!q) return;
    if (pthread_setspecific(queue_key, NULL)) abort();
    thread_queue_destroy(q);
}

struct halo_vita_completion_queue *halo_vita_completion_current(void)
{
    struct halo_vita_completion_queue *q;
    if (pthread_once(&queue_once, create_key)) abort();
    q = pthread_getspecific(queue_key);
    if (!q) {
        pthread_condattr_t attributes;
        int result;
        q = calloc(1, sizeof(*q));
        if (!q) return NULL;
        if (pthread_mutex_init(&q->lock, NULL)) { free(q); return NULL; }
        if (pthread_condattr_init(&attributes)) { pthread_mutex_destroy(&q->lock); free(q); return NULL; }
        result = pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
        if (!result) result = pthread_cond_init(&q->ready, &attributes);
        pthread_condattr_destroy(&attributes);
        if (result) { pthread_mutex_destroy(&q->lock); free(q); return NULL; }
        q->references = 1; q->accepting = 1;
        if (pthread_setspecific(queue_key, q)) {
            pthread_cond_destroy(&q->ready); pthread_mutex_destroy(&q->lock); free(q); return NULL;
        }
    }
    lock_queue(q);
    if (q->references == ~0U) abort();
    q->references++;
    unlock_queue(q);
    return q;
}

int halo_vita_completion_post(struct halo_vita_completion_queue *q, struct halo_vita_completion *item)
{
    if (!item || !item->run || !item->dispose) return 0;
    lock_queue(q);
    if (!q->accepting) { unlock_queue(q); return 0; }
    item->next = NULL;
    if (q->tail) q->tail->next = item; else q->head = item;
    q->tail = item;
    if (pthread_cond_signal(&q->ready)) abort();
    unlock_queue(q);
    halo_vita_wait_notify();
    return 1;
}

static void release_queue_cleanup(void *q) { halo_vita_completion_release(q); }
unsigned int halo_vita_completion_dispatch(void)
{
    struct halo_vita_completion_queue *q = halo_vita_completion_current();
    unsigned int count = 0;
    if (!q) return 0;
    pthread_cleanup_push(release_queue_cleanup, q);
    while (count < 64) {
        struct halo_vita_completion *item;
        lock_queue(q);
        item = q->head;
        if (item) { q->head = item->next; if (!q->head) q->tail = NULL; }
        unlock_queue(q);
        if (!item) break;
        /* Also dispose the current request if its callback exits the thread. */
        pthread_cleanup_push(item->dispose, item->context);
        item->run(item->context);
        pthread_cleanup_pop(1);
        count++;
    }
    pthread_cleanup_pop(1);
    return count;
}

static void unlock_queue_cleanup(void *q) { unlock_queue(q); }
/* Return readiness without dispatching under the lock. The predicate and
 * condition wait share a mutex, so a completion cannot be lost between them. */
int halo_vita_completion_wait(unsigned long milliseconds)
{
    struct halo_vita_completion_queue *q = halo_vita_completion_current();
    struct timespec deadline;
    int result = 0, ready = 0;
    if (!q) return -1;
    if (milliseconds != 0xffffffffUL) {
        if (clock_gettime(CLOCK_MONOTONIC, &deadline)) { halo_vita_completion_release(q); return -1; }
        deadline.tv_sec += milliseconds / 1000;
        deadline.tv_nsec += (milliseconds % 1000) * 1000000L;
        if (deadline.tv_nsec >= 1000000000L) { deadline.tv_sec++; deadline.tv_nsec -= 1000000000L; }
    }
    pthread_cleanup_push(release_queue_cleanup, q);
    lock_queue(q);
    pthread_cleanup_push(unlock_queue_cleanup, q);
    while (!q->head && q->accepting && milliseconds && !result) {
        result = milliseconds == 0xffffffffUL ? pthread_cond_wait(&q->ready, &q->lock) :
            pthread_cond_timedwait(&q->ready, &q->lock, &deadline);
    }
    ready = q->head != NULL;
    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    return result && result != ETIMEDOUT ? -1 : ready;
}
