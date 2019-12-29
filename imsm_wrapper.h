#pragma once

#include <assert.h>

#define IMSM_PPOINT(NAME) IMSM_PPOINT_((NAME), __COUNTER__)
#define IMSM_PPOINT_(NAME, UNIQUE) \
        ({                         \
                static const struct imsm_ppoint ppoint_##UNIQUE = {     \
                        .name = NAME,                                   \
                        .function = __PRETTY_FUNCTION__,                \
                        .file = __FILE__,                               \
                        .lineno = __LINE__,                             \
                        .unique = UNIQUE,                               \
                };                                                      \
                                                                        \
                &ppoint_##UNIQUE;                                       \
        })

#define IMSM_PPOINT_RECORD(NAME, ...) IMSM_PPOINT_RECORD_((NAME), ##__VA_ARGS__, 0)
#define IMSM_PPOINT_RECORD_(NAME, ITER, ...)    \
        ((struct imsm_ppoint_record) {          \
                .ppoint = IMSM_PPOINT(NAME),    \
                .iteration = (ITER),            \
         })

#define IMSM_INDEX(CTX, NAME, ...)                                      \
        (imsm_index((CTX), IMSM_PPOINT_RECORD((NAME), ##__VA_ARGS__)))

#define IMSM(TYNAME, ELTYPE)                                    \
        struct TYNAME {                                         \
                union {                                         \
                        struct { ELTYPE *eltype; } *meta;       \
                        struct imsm imsm;                       \
                };                                              \
        }

#define IMSM_INIT(IMSM, HEADER, ARENA, ARENA_SIZE, POLL_FN)             \
        ({                                                              \
                __typeof__(IMSM) imsm_ = (IMSM);                        \
                typedef __typeof__(*imsm_->meta->eltype) elt_t_;        \
                                                                        \
                static_assert(                                          \
                    __builtin_offsetof(elt_t_, HEADER) == 0,            \
                    "The slab_entry header must be the first member."); \
                static_assert(__builtin_types_compatible_p(             \
                    __typeof__(((elt_t_*)NULL)->HEADER), struct imsm_entry), \
                   "The header member must be a struct imsm_entry");    \
                imsm_init(&imsm_->imsm, (ARENA), (ARENA_SIZE),          \
                    sizeof(elt_t_), (POLL_FN));                         \
        })

#define IMSM_GET(IMSM)                                                  \
        ({                                                              \
                __typeof__(IMSM) imsm_ = (IMSM);                        \
                typedef __typeof__(*imsm_->meta->eltype) elt_t_;        \
                                                                        \
                (elt_t_ *)imsm_get(&imsm_->imsm);                       \
        })

#define IMSM_PUT(IMSM, OBJ)                                             \
        ({                                                              \
                __typeof__(IMSM) imsm_ = (IMSM);                        \
                typedef __typeof__(*imsm_->meta->eltype) elt_t_;        \
                elt_t_* ptr_ = (OBJ);                                   \
                                                                        \
                /* We know there is an imsm_entry at offset 0. */       \
                imsm_put(&imsm_->imsm, (struct imsm_entry *)ptr_);      \
        })

#define IMSM_REGION(CTX, NAME, ...)                             \
        IMSM_REGION_(__COUNTER__, (CTX), (NAME), ##__VA_ARGS__)

#define IMSM_REGION_(UNIQUE, CTX, NAME, ...)                           \
        const struct imsm_unwind_record imsm_unwind_##UNIQUE##_        \
        __attribute__((__cleanup__(imsm_region_pop)))                  \
                = imsm_region_push(CTX, IMSM_PPOINT_RECORD(NAME, ##__VA_ARGS__))

#define WITH_IMSM_REGION(CTX, NAME, ...)        \
        WITH_IMSM_REGION_(__COUNTER__, (CTX), (NAME), ##__VA_ARGS__)

#define WITH_IMSM_REGION_(UNIQUE, CTX, NAME, ...)              \
        for (struct imsm_unwind_record imsm_unwind_##UNIQUE##_ \
             __attribute__((__cleanup__(imsm_region_pop)))     \
                 = imsm_region_push(CTX,                       \
                 IMSM_PPOINT_RECORD(NAME, ##__VA_ARGS__));     \
             imsm_unwind_##UNIQUE##_.scratch == 0;             \
             imsm_unwind_##UNIQUE##_.scratch = 1)
