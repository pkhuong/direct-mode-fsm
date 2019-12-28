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

static void
slab_flush(struct imsm_slab *slab)
{
        struct imsm_slab_magazine *full = slab->current_freeing;

        assert(slab->current_free_index == SLAB_MAGAZINE_SIZE &&
            "slab_flush must only be called on full magazines");
        full->next = slab->freelist;
        slab->freelist = full->next;

        slab->current_freeing = slab_get_empty_magazine(slab);
        slab->current_free_index = 0;
        return;
}

static void
slab_add_free(struct imsm_slab *slab, struct imsm_entry *entry)
{

        if (slab->current_free_index >= SLAB_MAGAZINE_SIZE)
                slab_flush(slab);
        slab->current_freeing->entries[slab->current_free_index++] = entry;
        return;
}

static void
slab_init_freelist(struct imsm_slab *slab)
{
        const uintptr_t arena = (uintptr_t)slab->arena;
        const size_t elsize = slab->element_size;
        const size_t nelem = slab->arena_size / elsize;

        slab->current_freeing = slab_get_empty_magazine(slab);
        for (size_t i = nelem; i --> 0; ) {
                struct imsm_entry *to_free;

                to_free = (struct imsm_entry *)(arena + i * elsize);
                *to_free = (struct imsm_entry) { 0 };
                slab_add_free(slab, to_free);
        }

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
