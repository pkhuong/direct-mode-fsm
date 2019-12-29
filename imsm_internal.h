#pragma once

#include <stddef.h>
#include <stdint.h>

struct imsm_slab_magazine;

struct imsm_slab {
        void *arena;
        size_t arena_size;
        size_t element_size;
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

        /*
         * The other two are intrusive linked stacks of full magazines
         * (freelist) or of empty ones (empty).
         */
        struct imsm_slab_magazine *freelist;
        struct imsm_slab_magazine *empty;
};

/*
 * We use statically allocated imsm program points to map queue
 * operations to program states.
 *
 * Only their pointer identity matters; the fields only exist for
 * pretty printing and debugging.
 *
 * Obtain a reference to one with IMSM_PPOINT(NAME).
 */
struct imsm_ppoint {
        /* The `name` should be a domain-specific description. */
        const char *name;
        /* `function` should be populated with `__PRETTY_FUNCTION__`. */
        const char *function;
        /* `file` should be populated with `__FILE__`. */
        const char *file;
        /* `line` should be populated with `__LINE__`. */
        size_t lineno;
        /*
         * `unique` can be used to make program points on the same
         * line look different.
         */
        size_t unique;
};

/*
 * In addition to regular program points, we can push and pop context.
 * These values are encoded in the program point index.
 */
enum imsm_ppoint_action {
        IMSM_PPOINT_ACTION_PUSH = -1,
        IMSM_PPOINT_ACTION_POP = -2,
};

/*
 * We can update the state index visiting a new program point.
 *
 * Obtain one (by value) with IMSM_PPOINT_RECORD(NAME, ITERATION?).
 */
struct imsm_ppoint_record {
        __uint128_t iteration;
        const struct imsm_ppoint *ppoint;
        size_t index;
};

/*
 * We can also augment the program point data with additional "program
 * region" context information.  This struct encapsulates the
 * information needed to subtract region from the context.
 *
 * The useful side-effect of a region is that returning into the same
 * program point in a new region yields a distinct index.
 */
struct imsm_unwind_record {
        struct imsm_ppoint_record position;
        struct imsm_ctx *context;
        size_t scratch;
};
