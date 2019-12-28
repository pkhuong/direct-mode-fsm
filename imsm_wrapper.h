#pragma once

#include <assert.h>

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
