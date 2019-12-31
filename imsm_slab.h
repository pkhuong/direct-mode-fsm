#pragma once

#include <stddef.h>
#include <stdint.h>

struct imsm_slab_magazine;

struct imsm_slab {
        void *arena;
        size_t arena_size;
        size_t element_size;
        size_t element_count;
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

