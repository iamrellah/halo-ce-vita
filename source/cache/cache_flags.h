/* Cache I/O publication only. Does not synchronize GPU or audio DMA access.
 * Callers retain the flag and payload until the operation completes. */
#ifndef HALO_CACHE_FLAGS_H
#define HALO_CACHE_FLAGS_H
#ifdef HALO_VITA
#define CACHE_FLAG_LOAD(p) __atomic_load_n((p), __ATOMIC_ACQUIRE)
#define CACHE_FLAG_STORE(p,v) __atomic_store_n((p), (v), __ATOMIC_RELEASE)
#else
#define CACHE_FLAG_LOAD(p) (*(p))
#define CACHE_FLAG_STORE(p,v) (*(p)=(v))
#endif
#endif
