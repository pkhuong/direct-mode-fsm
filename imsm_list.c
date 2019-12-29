#include "imsm_list.h"

#include <assert.h>
#include <stdlib.h>
#include <sys/queue.h>

extern size_t (imsm_list_size)(void **);
extern size_t (imsm_list_size)(void **);
extern bool (imsm_list_set_size)(void **, size_t);
extern size_t (imsm_list_capacity)(void **);
extern uint64_t *(imsm_list_aux)(void **);
extern bool (imsm_list_push)(void **, void *ptr, uint64_t aux);

static void
cache_head_deinit(struct imsm_list_cache_head *head)
{
        struct imsm_list_header *to_free;

        while ((to_free = TAILQ_FIRST(head)) != NULL) {
                TAILQ_REMOVE(head, to_free, linkage);
                free(to_free);
        }

        return;
}

void
imsm_list_cache_deinit(struct imsm_list_cache *cache)
{
        size_t nlists = sizeof(cache->per_size) / sizeof(cache->per_size[0]);

        for (size_t i = 0; i < nlists; i++) {
                cache_head_deinit(&cache->per_size[i].free);
                cache_head_deinit(&cache->per_size[i].active);
        }

        cache_head_deinit(&cache->uncached_active);
        return;
}

void
imsm_list_cache_recycle(struct imsm_list_cache *cache)
{
        size_t nlists = sizeof(cache->per_size) / sizeof(cache->per_size[0]);

        for (size_t i = 0; i < nlists; i++) {
                TAILQ_CONCAT(&cache->per_size[i].free,
                             &cache->per_size[i].active,
                             linkage);
        }

        cache_head_deinit(&cache->uncached_active);
        return;
}

void **
imsm_list_get_slow(struct imsm_list_cache *cache, size_t capacity_index)
{
        static const size_t per_size_limit =
            sizeof(cache->per_size) / sizeof(cache->per_size[0]);
        struct imsm_list_header *ret;
        struct imsm_list_cache_head *active;
        size_t rounded = (1ULL << capacity_index) - 2;

        ret = calloc(1,
            sizeof(*ret) + rounded * (sizeof(void *) + sizeof(uint64_t)));
        if (ret == NULL)
                return NULL;

        if (capacity_index < per_size_limit) {
                active = &cache->per_size[capacity_index].active;
                if (cache->per_size[capacity_index].free.tqh_last == NULL)
                        TAILQ_INIT(&cache->per_size[capacity_index].free);
        } else {
                active = &cache->uncached_active;
        }
        if (active->tqh_last == NULL)
                TAILQ_INIT(active);

        TAILQ_INSERT_HEAD(active, ret, linkage);
        return (void **)(ret + 1);
}

void
imsm_list_put_slow(struct imsm_list_cache *cache,
    struct imsm_list_header *header)
{
        static const size_t per_size_limit =
            sizeof(cache->per_size) / sizeof(cache->per_size[0]);

        assert(header->capacity_index >= per_size_limit);
        TAILQ_REMOVE(&cache->uncached_active, header, linkage);
        free(header);
        return;
}
