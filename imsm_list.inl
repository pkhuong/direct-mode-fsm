/* -*- mode: C -*- */

#pragma once

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <sys/queue.h>

struct imsm_list_header {
        TAILQ_ENTRY(imsm_list_header) linkage;
        size_t size;
        /* Actual capacity is (1 << capacity_index) - 2. */
        size_t capacity_index;
};

inline size_t
(imsm_list_size)(void **buf)
{
        struct imsm_list_header *header;

        if (buf == NULL)
                return 0;

        header = (void *)((char *)buf) - sizeof(struct imsm_list_header);
        return header->size;
}

inline bool
(imsm_list_set_size)(void **buf, size_t size)
{
        struct imsm_list_header *header =
            (void *)((char *)buf) - sizeof(struct imsm_list_header);

        if (size > imsm_list_capacity(buf))
                return false;
        header->size = size;
        return true;
}

inline size_t
(imsm_list_capacity)(void **buf)
{
        struct imsm_list_header *header;

        if (buf == NULL)
                return 0;

        header = (void *)((char *)buf) - sizeof(struct imsm_list_header);
        return (1ULL << header->capacity_index) - 2;
}

inline uint64_t *
(imsm_list_aux)(void **buf)
{

        if (buf == NULL)
                return NULL;

        return (uint64_t *)(buf + imsm_list_capacity(buf));
}

inline bool
(imsm_list_push)(void **buf, void *ptr, uint64_t aux)
{
        size_t current_size;

        current_size = imsm_list_size(buf);
        if (current_size >= imsm_list_capacity(buf))
                return false;

        imsm_list_set_size(buf, current_size + 1);
        buf[current_size] = ptr;
        imsm_list_aux(buf)[current_size] = aux;
        return true;
}

void **imsm_list_get_slow(struct imsm_list_cache *cache, size_t capacity_index);

inline void **
(imsm_list_get)(struct imsm_list_cache *cache, size_t capacity)
{
        static const size_t min_capacity = 6;
        static const size_t per_size_limit =
            sizeof(cache->per_size) / sizeof(cache->per_size[0]);
        static const size_t llbits = sizeof(long long) * CHAR_BIT;
        struct imsm_list_header *ret;
        size_t capacity_index;

        if (capacity == 0)
                return NULL;

        if (capacity < min_capacity)
                capacity = min_capacity;

        capacity_index = llbits - __builtin_clzll(capacity + 1);

        assert(capacity <= (1ULL << capacity_index) - 2);
        assert(capacity > (1ULL << (capacity_index - 1)) - 2);
        if (capacity_index >= per_size_limit ||
            (ret = TAILQ_FIRST(&cache->per_size[capacity_index].free)) == NULL)
                return imsm_list_get_slow(cache, capacity_index);

        TAILQ_REMOVE(&cache->per_size[capacity_index].free, ret, linkage);
        TAILQ_INSERT_HEAD(&cache->per_size[capacity_index].active, ret, linkage);

        ret->size = 0;
        return (void **)(ret + 1);
}

void imsm_list_put_slow(struct imsm_list_cache *cache,
    struct imsm_list_header *header);

inline void
(imsm_list_put)(struct imsm_list_cache *cache, void **buf)
{
        static const size_t per_size_limit =
            sizeof(cache->per_size) / sizeof(cache->per_size[0]);
        struct imsm_list_header *header;

        if (buf == NULL)
                return;

        header = (void *)((char *)buf) - sizeof(struct imsm_list_header);
        if (header->capacity_index >= per_size_limit)
                imsm_list_put_slow(cache, header);

        /* list_get initializes the TAILQ heads on demand. */
        TAILQ_REMOVE(&cache->per_size[header->capacity_index].active,
            header, linkage);
        TAILQ_INSERT_HEAD(&cache->per_size[header->capacity_index].free,
            header, linkage);
        return;
}
