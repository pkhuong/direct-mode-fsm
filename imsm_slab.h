#pragma once

#include <stddef.h>
#include <stdint.h>

struct imsm;
struct imsm_ctx;

struct imsm_slab_magazine;

struct imsm_slab {
        /* Allocation goes down to 0. */
        uint32_t current_alloc_index;
        /* Deallocation goes up to 0. */
        int32_t current_free_index;
        /*
         * These two arrays are caches populated or consumed down from
         * current_*_index to 0.  Once the item at zero is populated
         * or consumed, the cache must be recycled.
         *
         * `current_allocating` may be NULL if we're out of slab items;
         * `current_freeing` must always be a valid non-full magazine.
         */
        struct imsm_entry **current_allocating;
        struct imsm_entry **current_freeing;

        void (*deinit_fn)(void *);

        /*
         * The other two are intrusive linked stacks of full magazines
         * (freelist) or of empty ones (empty).
         */
        struct imsm_slab_magazine *freelist;
        struct imsm_slab_magazine *empty;

        void *arena;
        size_t arena_size;
        size_t element_size;
        size_t element_count;

        void (*init_fn)(void *);
};

/*
 * Initializes a slab with a pre-allocated backing storage at `arena`,
 * of `arena_size` char, for elements of size `elsize` char.  The
 * arena must be zero-initialized and suitably aligned for the
 * contents, and the element type is assumed to contain a `struct
 * imsm_entry` header.
 *
 * If non-NULL, `init_fn` will be called before returning every slab
 * for the first time out of `imsm_get`. If non-NULL, `deinit_fn` will
 * be called on every slab element `put` back on the slab.
 */
void imsm_slab_init(struct imsm_slab *slab, void *arena, size_t arena_size,
    size_t elsize, void (*init_fn)(void *), void (*deinit_fn)(void *));

/*
 * Allocates one object from the `imsm`'s slab, or NULL if the slab
 * has no free object.  The second argument is redundant, but serves
 * as a witness that the context is for the correct imsm.  This
 * function will abort on mismatches.
 *
 * See IMSM_GET for a type-safe version.
 */
inline struct imsm_entry *imsm_get(struct imsm_ctx *, struct imsm *);

/*
 * Deallocates one object back to the `imsm`'s slab.  Safe to call on
 * NULL pointers, but will abort on any other pointer not allocated
 * by the `imsm`.
 *
 * See IMSM_PUT for a type-safe version.
 */
inline void imsm_put(struct imsm_ctx *, struct imsm *, struct imsm_entry *);

/*
 * Accepts an interior pointer to an element of the `imsm_ctx`'s slab,
 * and returns a pointer to the entry header, or NULL if there is no
 * such element in the slab.
 */
inline struct imsm_entry *imsm_entry_of(struct imsm_ctx *, void *);

/*
 * Returns a pointer to the `i`th element of `imsm_ctx`'s slab, or
 * NULL if there is no such element, or the element is inactive.
 */
inline struct imsm_entry *imsm_traverse(struct imsm_ctx *, size_t i);
