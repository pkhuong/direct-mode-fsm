#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct imsm_list_cache;

void imsm_list_cache_deinit(struct imsm_list_cache *);

void imsm_list_cache_recycle(struct imsm_list_cache *);

inline size_t imsm_list_size(void **);

inline bool imsm_list_set_size(void **, size_t);

inline size_t imsm_list_capacity(void **);

inline uint64_t* imsm_list_aux(void **);

inline bool imsm_list_push(void **, void *ptr, uint64_t aux);

inline void **imsm_list_get(struct imsm_list_cache *, size_t capacity);

inline void imsm_list_put(struct imsm_list_cache *, void **list);

#define imsm_list_foreach(VAR, BUF)                                      \
        for (__typeof__(**(BUF))                                         \
             *const *VAR##_list_ = (BUF),                                \
             *const *VAR##_sentinel_ = VAR##_list_ + imsm_list_size(VAR##_list_), \
             *VAR;                                                       \
             VAR##_list_ < VAR##_sentinel_ && ((VAR = *VAR##_list_), 1); \
             VAR##_list_++)

/*
 * We represent state lists stretchy buffers.  This type-erased
 * implementation only offers lists of `void *`.
 */

struct imsm_list_header;

struct imsm_list_cache_head {
        /* This must match `TAILQ_HEAD` in `sys/queue.h`. */
        struct imsm_list_header *tqh_first;
        struct imsm_list_header **tqh_last;
};

/*
 * We have roughly power-of-two-sized allocations, and always keep
 * track of both active and free allocations.
 *
 * Very large allocations are never cached: it's not worth the
 * fragmentation.
 */
struct imsm_list_cache {
        struct {
                struct imsm_list_cache_head free;
                struct imsm_list_cache_head active;
        } per_size[32];

        struct imsm_list_cache_head uncached_active;
};

#define imsm_list_size(BUF)                                    \
  ({                                                           \
                __typeof__(**(BUF)) *const *imsm_buf_ = (BUF); \
                (imsm_list_size)((void **)imsm_buf_);          \
        })

#define imsm_list_set_size(BUF, SIZE)                             \
        ({                                                        \
                __typeof__(**(BUF)) **imsm_buf_ = (BUF);          \
                (imsm_list_set_size)((void **)imsm_buf_, (SIZE)); \
        })

#define imsm_list_capacity(BUF)                                 \
        ({                                                      \
                __typeof__(**(BUF)) *const *imsm_buf_ = (BUF);  \
                (imsm_list_capacity)((void **)imsm_buf_);       \
        })

#define imsm_list_aux(BUF)                                      \
        ({                                                      \
                __typeof__(**(BUF)) *const *imsm_buf_ = (BUF);  \
                (imsm_list_aux)((void **)imsm_buf_);            \
        })

#define imsm_list_push(BUF, PTR, AUX)                                   \
        ({                                                              \
                __typeof__(**(BUF)) **imsm_buf_ = (BUF),                \
                        *ptr_value_ = (PTR);                            \
                (imsm_list_push)((void **)imsm_buf_, ptr_value_, (AUX)); \
        })

#include "imsm_list.inl"
