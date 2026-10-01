#ifndef HALO_VITA_COMPLETION_H
#define HALO_VITA_COMPLETION_H
struct halo_vita_completion_queue;
/* Embed in the request before issuing I/O: posting needs no allocation.
 * run executes only on the issuing thread. dispose releases request storage. */
struct halo_vita_completion {
    struct halo_vita_completion *next;
    void (*run)(void *);
    void (*dispose)(void *);
    void *context;
};
struct halo_vita_completion_queue *halo_vita_completion_current(void);
void halo_vita_completion_release(struct halo_vita_completion_queue *queue);
/* Caller retains queue. On rejection caller still owns the completion. */
int halo_vita_completion_post(struct halo_vita_completion_queue *queue, struct halo_vita_completion *item);
/* Run up to 64 callbacks on this thread; never called from an I/O worker. */
unsigned int halo_vita_completion_dispatch(void);
/* 1 ready, 0 timed out, -1 failure; 0xffffffff means infinite. */
int halo_vita_completion_wait(unsigned long milliseconds);
/* File worker teardown hook; caller must not hold the queue mutex. */
void halo_vita_file_cancel_and_drain_issuer(struct halo_vita_completion_queue *queue);
void halo_vita_completion_thread_exit(void);
#endif
