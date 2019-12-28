#include "imsm.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "imsm_internal.h"

#define SLAB_MAGAZINE_SIZE 15

struct imsm_slab_magazine {
        struct imsm_slab_magazine *next;
        struct imsm_entry *entries[SLAB_MAGAZINE_SIZE];
};

/*
 * Returns a pointer to the allocation cache in a magazine.
 */
static struct imsm_entry **
cache_of_magazine(struct imsm_slab_magazine *magazine)
{

        assert(magazine != NULL);
        return &magazine->entries[0];
}

/*
 * Inverses `cache_of_magazine`.
 */
static struct imsm_slab_magazine *
magazine_of_cache(struct imsm_entry **entries)
{
        const size_t offset = __builtin_offsetof(struct imsm_slab_magazine, entries);
        char *base = (void *)entries;

        assert(entries != NULL);
        return (void *)(base - offset);
}

/*
 * Returns an empty magazine, either from the slab's list cache of empty magazines,
 * of via `calloc`.
 */
static inline struct imsm_slab_magazine *
slab_get_empty_magazine(struct imsm_slab *slab)
{
        struct imsm_slab_magazine *ret = slab->empty;

        if (ret != NULL) {
                slab->empty = ret->next;
                return ret;
        }

        /* XXX: allocation. */
        return calloc(1, sizeof(*ret));
}

/*
 * Returns a full magazine, or NULL if none is available.
 */
static inline struct imsm_slab_magazine *
slab_get_full_magazine(struct imsm_slab *slab)
{
        struct imsm_slab_magazine *ret = slab->freelist;

        if (ret != NULL) {
                slab->freelist = ret->next;
        }

        return ret;
}

/*
 * Replaces a slab's current freeing magazine.
 */
static inline void
slab_refresh_current_freeing(struct imsm_slab *slab)
{
        struct imsm_slab_magazine *empty;

        assert(slab->current_freeing == NULL);

        empty = slab_get_empty_magazine(slab);
        slab->current_freeing = cache_of_magazine(empty);
        slab->current_free_index = SLAB_MAGAZINE_SIZE;
        return;
}

/*
 * Steals the current freeing cache as the new allocation cache if
 * possible.
 *
 * We need this edge cache to guarantee a slab will allocate
 * successfully even if it has capacity for less than two magazines.
 */
static void
slab_convert_freeing_to_allocating(struct imsm_slab *slab)
{
        struct imsm_slab_magazine *current_cache;
        size_t num_freed = SLAB_MAGAZINE_SIZE - slab->current_free_index;

        assert(slab->current_freeing != NULL &&
            "current_freeing must always be valid and never full.");
        assert(slab->current_free_index <= SLAB_MAGAZINE_SIZE);

        /* If the current freeing cache is empty, there's nothing to convert. */
        if (num_freed == 0) {
                slab->current_alloc_index = 0;
                slab->current_allocating = NULL;
                return;
        }

        current_cache = magazine_of_cache(slab->current_freeing);

        /* Steal the current free cache, replace it with a new empty one. */
        slab->current_freeing = NULL;
        slab_refresh_current_freeing(slab);

        /*
         * We pop in the same order we push, so we have to slide the
         * freed entries to the front of the cache, and invert the
         * index to point to the populated entries in the cache.
         */
        memmove(&current_cache->entries[0],
            &current_cache->entries[SLAB_MAGAZINE_SIZE - num_freed],
            num_freed * sizeof(current_cache->entries[0]));

        slab->current_allocating = cache_of_magazine(current_cache);
        slab->current_alloc_index = num_freed;
        return;
}

/*
 * Replaces a slab's current allocating magazine.
 */
static inline void
slab_refresh_current_allocating(struct imsm_slab *slab)
{
        struct imsm_slab_magazine *full;

        assert(slab->current_allocating == NULL);

        full = slab_get_full_magazine(slab);
        if (full == NULL) {
                slab_convert_freeing_to_allocating(slab);
                return;
        }

        slab->current_allocating = cache_of_magazine(full);
        slab->current_alloc_index = SLAB_MAGAZINE_SIZE;
        return;
}

/*
 * Flushes the current full freeing magazine to the freelist, and
 * replaces it with a new empty magazine.
 */
static void
slab_flush(struct imsm_slab *slab)
{
        struct imsm_slab_magazine *full;

        assert(slab->current_free_index == 0 &&
            "slab_flush must only be called on full magazines");

        full = magazine_of_cache(slab->current_freeing);
        full->next = slab->freelist;
        slab->freelist = full->next;

        slab->current_freeing = NULL;
        slab_refresh_current_freeing(slab);
        return;
}

/*
 * Pushes `entry` to the freelist.
 */
static void
slab_add_free(struct imsm_slab *slab, struct imsm_entry *entry)
{

        slab->current_freeing[--slab->current_free_index] = entry;
        if (slab->current_free_index == 0)
                slab_flush(slab);
        return;
}

static void
slab_init_freelist(struct imsm_slab *slab)
{
        char *const arena = slab->arena;
        const size_t elsize = slab->element_size;
        const size_t nelem = slab->arena_size / elsize;

        slab_refresh_current_freeing(slab);
        for (size_t i = nelem; i --> 0; ) {
                struct imsm_entry *to_free;

                to_free = (struct imsm_entry *)(arena + i * elsize);
                *to_free = (struct imsm_entry) { 0 };
                slab_add_free(slab, to_free);
        }

        /*
         * Confirm that we setup a valid slab.
         */
        assert(slab->current_alloc_index == 0);
        assert(slab->current_free_index > 0 &&
            slab->current_free_index <= SLAB_MAGAZINE_SIZE);

        /* The slab's allocation cache is initially empty. */
        assert(slab->current_allocating == NULL);
        assert(slab->current_freeing != NULL);
        assert(slab->empty == NULL);
        return;
}

struct imsm_entry *
imsm_get_slow(struct imsm *imsm)
{
        struct imsm_slab *slab = &imsm->slab;

        if (slab->current_allocating == NULL)
                imsm_get_cache_reload(imsm);
        if (slab->current_allocating == NULL)
                return NULL;

        /* imsm_get only calls get_slow if current_allocating == NULL. */
        return imsm_get(imsm);
}

void
imsm_get_cache_reload(struct imsm *imsm)
{
        struct imsm_slab *slab = &imsm->slab;

        assert(slab->current_alloc_index == 0 &&
            "Only empty allocation caches may be reloaded");
        /*
         * If we have an empty allocation cache, push it to the list
         * of empty magazines.
         */
        if (slab->current_allocating != NULL) {
                struct imsm_slab_magazine *empty;

                empty = magazine_of_cache(slab->current_allocating);
                empty->next = slab->empty;
                slab->empty = empty;
                slab->current_allocating = NULL;
        }

        slab_refresh_current_allocating(slab);
}

extern struct imsm_entry *imsm_get(struct imsm *imsm);

void
imsm_init(struct imsm *imsm, void *arena, size_t arena_size, size_t elsize,
    void (*poll_fn)(struct imsm_ctx *))
{

        assert(imsm->poll_fn == NULL &&
            "imsm must be initialized exactly once");
        imsm->poll_fn = poll_fn;
        imsm->slab.arena = arena;
        imsm->slab.arena_size = arena_size;
        imsm->slab.element_size = elsize;

        slab_init_freelist(&imsm->slab);
        return;
}
