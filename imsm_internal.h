#pragma once

#include <stddef.h>

struct imsm_slab_magazine;

struct imsm_slab {
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
        struct imsm_slab_magazine *current_allocating;
        struct imsm_slab_magazine *current_freeing;
        struct imsm_slab_magazine *freelist;
        struct imsm_slab_magazine *empty;
};
