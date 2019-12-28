#include "imsm.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "imsm_internal.h"

#include "imsm_slab.inc"

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
