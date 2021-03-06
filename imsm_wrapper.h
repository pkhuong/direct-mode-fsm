#pragma once

#include <assert.h>

/*
 * A pointer to the current imsm context should be bound to
 * IMSM_CTX_PTR_VAR.  IMSM_CTX_PTR(context_address) will
 * setup such a binding.
 */
#define IMSM_CTX_PTR_VAR imsm_ctx_ptr_
#define IMSM_CTX_PTR(CTX)                                               \
        struct imsm_ctx *const __attribute__((__unused__))              \
                IMSM_CTX_PTR_VAR = (CTX)

#define IMSM(TYNAME, ELTYPE)                                    \
        struct TYNAME {                                         \
                union {                                         \
                        struct { ELTYPE *eltype; } *meta;       \
                        struct imsm imsm;                       \
                };                                              \
        }

#define IMSM_INIT(IMSM, HEADER, ARENA, ARENA_SIZE,                      \
                  INIT_FN, DEINIT_FN, POLL_FN)                          \
        ({                                                              \
                __typeof__(IMSM) imsm_ = (IMSM);                        \
                typedef __typeof__(*imsm_->meta->eltype) elt_t_;        \
                                                                        \
                static_assert(                                          \
                    __builtin_offsetof(elt_t_, HEADER) == 0,            \
                    "The imsm_entry header must be the first member."); \
                static_assert(__builtin_types_compatible_p(             \
                    __typeof__(((elt_t_*)NULL)->HEADER), struct imsm_entry), \
                   "The header member must be a struct imsm_entry");    \
                imsm_init(&imsm_->imsm, (ARENA), (ARENA_SIZE),          \
                    sizeof(elt_t_), (INIT_FN), (DEINIT_FN), (POLL_FN)); \
        })

#define IMSM_REFER(OBJECT) (imsm_refer((IMSM_CTX_PTR_VAR), (OBJECT)))

/*
 * LOC_INFO is either a NAME, or (NAME, ITERATION)
 */

#define IMSM_STAGE(LOC_INFO, LIST_IN, AUX_MATCH)                        \
        ({                                                              \
                __typeof__(**(LIST_IN)) **stage_list_in_ = (LIST_IN);   \
                struct imsm_ctx *ctx_ = (IMSM_CTX_PTR_VAR);             \
                                                                        \
                (__typeof__(list_in_))imsm_stage_io(                    \
                    ctx_, IMSM_PPOINT_RECORD(IMSM_UNPAREN(LOC_INFO)),   \
                    (void **)stage_list_in_, (AUX_MATCH));              \
        })

#define IMSM_GET(IMSM)                                                  \
        ({                                                              \
                __typeof__(IMSM) imsm_ = (IMSM);                        \
                struct imsm_ctx *ctx_ = (IMSM_CTX_PTR_VAR);             \
                typedef __typeof__(*imsm_->meta->eltype) elt_t_;        \
                                                                        \
                (elt_t_ *)imsm_get(ctx_, &imsm_->imsm);                 \
        })

#define IMSM_PUT(IMSM, OBJ)                                             \
        ({                                                              \
                __typeof__(IMSM) imsm_ = (IMSM);                        \
                struct imsm_ctx *ctx_ = (IMSM_CTX_PTR_VAR);             \
                typedef __typeof__(*imsm_->meta->eltype) elt_t_;        \
                elt_t_* ptr_ = (OBJ);                                   \
                                                                        \
                /* We know there is an imsm_entry at offset 0. */       \
                imsm_put(ctx_, &imsm_->imsm, (struct imsm_entry *)ptr_); \
        })

#define IMSM_PUT_N(IMSM, LIST, LIST_SIZE)                               \
        ({                                                              \
                __typeof__(IMSM) imsm_ = (IMSM);                        \
                struct imsm_ctx *ctx_ = (IMSM_CTX_PTR_VAR);             \
                typedef __typeof__(*imsm_->meta->eltype) elt_t_;        \
                elt_t_ **ptr_list_ = (LIST);                            \
                                                                        \
                /* We know there is an imsm_entry at offset 0. */       \
                imsm_put_n(ctx_, &imsm_->imsm,                          \
                    (struct imsm_entry **)ptr_list_, (LIST_SIZE));      \
        })

#define IMSM_LIST_GET(TYPE, CAPACITY)                                   \
        ((TYPE **)imsm_list_get(&(IMSM_CTX_PTR_VAR)->cache, (CAPACITY)))

#define IMSM_LIST_PUT(LIST)                                             \
        ({                                                              \
                __typeof__(**(LIST)) **list_ = (LIST);                  \
                imsm_list_put(&(IMSM_CTX_PTR_VAR)->cache, (void **)list_); \
        })

#define IMSM_REGION(NAME, ...)                                  \
        IMSM_REGION_(__COUNTER__, (NAME), ##__VA_ARGS__)

#define IMSM_REGION_(UNIQUE, NAME, ...)                                 \
        IMSM_REGION__(UNIQUE, (IMSM_CTX_PTR_VAR), NAME, ##__VA_ARGS__)

#define IMSM_REGION__(UNIQUE, CTX, NAME, ...)                           \
        const struct imsm_unwind_record imsm_unwind_##UNIQUE##_         \
        __attribute__((__cleanup__(imsm_region_pop), __unused__))       \
                = imsm_region_push(CTX, IMSM_PPOINT_RECORD(NAME, ##__VA_ARGS__))

#define WITH_IMSM_REGION(NAME, ...)                                     \
        WITH_IMSM_REGION_(__COUNTER__, (NAME), ##__VA_ARGS__)

#define WITH_IMSM_REGION_(UNIQUE, NAME, ...)                            \
        WITH_IMSM_REGION__(UNIQUE, (IMSM_CTX_PTR_VAR), NAME, ##__VA_ARGS__)

#define WITH_IMSM_REGION__(UNIQUE, CTX, NAME, ...)                      \
        for (struct imsm_unwind_record imsm_unwind_##UNIQUE##_          \
             __attribute__((__cleanup__(imsm_region_pop), __unused__))  \
                 = imsm_region_push(CTX,                                \
                 IMSM_PPOINT_RECORD(NAME, ##__VA_ARGS__));              \
             imsm_unwind_##UNIQUE##_.scratch == 0;                      \
             imsm_unwind_##UNIQUE##_.scratch = 1)

/*
 * Internals that shouldn't be used directly.
 */

/* https://www.mikeash.com/pyblog/friday-qa-2015-03-20-preprocessor-abuse-and-optional-parentheses.html */
#define IMSM_UNPAREN(X) IMSM_PASTE(IMSM_NOTHING_, IMSM_EXTRACT X)
#define IMSM_EXTRACT(...) IMSM_EXTRACT __VA_ARGS__
#define IMSM_PASTE(X, ...) IMSM_PASTE_(X, __VA_ARGS__)
#define IMSM_PASTE_(X, ...) X ## __VA_ARGS__
#define IMSM_NOTHING_IMSM_EXTRACT

#define IMSM_PPOINT(NAME) IMSM_PPOINT_((NAME), __COUNTER__)
#define IMSM_PPOINT_(NAME, UNIQUE) IMSM_PPOINT__(NAME, UNIQUE)
#define IMSM_PPOINT__(NAME, UNIQUE) \
        ({                         \
                static const struct imsm_ppoint ppoint_##UNIQUE##_ = {  \
                        .name = NAME,                                   \
                        .function = __PRETTY_FUNCTION__,                \
                        .file = __FILE__,                               \
                        .lineno = __LINE__,                             \
                        .unique = UNIQUE,                               \
                };                                                      \
                                                                        \
                &ppoint_##UNIQUE##_;                                    \
        })

#define IMSM_PPOINT_RECORD(NAME, ...) IMSM_PPOINT_RECORD_(NAME, ##__VA_ARGS__, 0)
#define IMSM_PPOINT_RECORD_(NAME, ITER, ...)    \
        ((struct imsm_ppoint_record) {          \
                .ppoint = IMSM_PPOINT(NAME),    \
                .iteration = (ITER),            \
         })

#define IMSM_INDEX(NAME, ...)                                      \
        (imsm_index((IMSM_CTX_PTR_VAR),                            \
             IMSM_PPOINT_RECORD((NAME), ##__VA_ARGS__)))
