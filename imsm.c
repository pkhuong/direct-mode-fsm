#include "imsm.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

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
static struct imsm_slab_magazine *
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
