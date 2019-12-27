#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

/*
 * A type-stable slab allocator with versioning.
 */

/*
 * The first element in each slab element must be a `slab_entry`.
 * This property is checked in SLAB_INIT.
 */
struct slab_entry {
        uint64_t version;
};

struct slab_reference {
        uint64_t bits;
};

struct slab_magazine;

struct slab_base {
        void *arena;
        size_t arena_size;
        size_t element_size;
        size_t recip_elsize;
        /*
         * The first two magazines are caches, one for allocations and
         * another for deallocations.
         *
         * The other two are intrusive linked stacks of full magazines
         * (freelist) or of empty ones (empty).
         */
        struct slab_magazine *current_allocating;
        struct slab_magazine *current_freeing;
        struct slab_magazine *freelist;
        struct slab_magazine *empty;
};

void slab_init(struct slab_base *, void *arena, size_t arena_size,
    size_t element_size);

void *slab_alloc(struct slab_base *);
void slab_release(struct slab_base *, void *);

void slab_notify(struct slab_base *, struct slab_reference);

/*
 * Given an interior pointer for a slab allocated object, returns the
 * parent object, or NULL if there is no such parent in the slab.
 */
void *slab_object(struct slab_base *, void *);

/*
 * Given a pointer `offset` inside a slab allocated object, returns
 * the parent object, or NULL if there is no such parent in the slab.
 */
void *slab_object_offset(struct slab_base *, void *, size_t offset);

#define SLAB_HEAD(NAME, TYPE)                                           \
        struct NAME {                                                   \
                union {                                                 \
                        struct slab_base base;                          \
                        struct {                                        \
                                TYPE *object_ptr;                       \
                        } metadata;                                     \
                };                                                      \
        }

#define SLAB_INIT(SLAB, HEADER, ARENA, ARENA_SIZE)                      \
        ({                                                              \
                __typeof__(SLAB) *slab_ = &(SLAB);                      \
                typedef __typeof__(*slab_->metadata.object_ptr) slab_element_t_; \
                                                                        \
                static_assert(                                          \
                        __builtin_offsetof(slab_element_t_, HEADER) == 0, \
                        "The slab_entry header must be the first member."); \
                static_assert(__builtin_types_compatible_p(             \
                                      __typeof__(((slab_element_t_*)NULL)->HEADER), \
                                      struct slab_entry),               \
                        "The header member must be a slab_entry");      \
                slab_init(&slab_->base, (ARENA), (ARENA_SIZE),          \
                          sizeof(slab_element_t_));                     \
        })

#define SLAB_ALLOC(SLAB)                                                \
        ({                                                              \
                __typeof__(SLAB) *slab_ = &(SLAB);                      \
                (__typeof__(slab_->metadata.object_ptr))slab_alloc(&slab_->base); \
        })

#define SLAB_RELEASE(SLAB, PTR)                                         \
        ({                                                              \
                __typeof__(SLAB) *slab_ = &(SLAB);                      \
                __typeof__(slab_->metadata.object_ptr) ptr_ = (PTR);    \
                slab_release(&slab_->base, ptr_);                       \
        })

#define SLAB_OBJECT(SLAB, PTR)                                          \
        ({                                                              \
                __typeof__(SLAB) *slab_ = &(SLAB);                      \
                (__typeof__(slab_->metadata.object_ptr))                \
                        slab_object(&slab_->base, (PTR));            \
        })

#define SLAB_OBJECT_OF(SLAB, PTR, FIELD)                                \
        ({                                                              \
                __typeof__(SLAB) *slab_ = &(SLAB);                      \
                typedef __typeof__(*slab_->metadata.object_ptr) slab_element_t_; \
                                                                        \
                (__typeof__(slab_->metadata.object_ptr))                \
                        slab_object_offset(&slab_->base, (PTR),         \
                        __builtin_offsetof(slab_element_t_, FIELD));    \
        })
