#include "imsm.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#include "imsm_internal.h"
#include "imsm_list.h"

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

static void
imsm_stage_in(struct imsm_ctx *ctx, size_t ppoint_index,
    void **list_in, uint64_t aux_match)
{

        for (size_t i = 0, n = imsm_list_size(list_in); i < n; i++) {
                struct imsm_entry *entry;
                size_t offset;

                if (list_in[i] == NULL ||
                    imsm_list_aux(list_in)[i] != aux_match)
                        continue;

                entry = imsm_entry_of(ctx, list_in[i]);
                assert(entry != NULL);

                offset = (char *)list_in[i] - (char *)entry;
                assert(offset <= UINT8_MAX);
                entry->queue_id = ppoint_index;
                entry->offset = offset;
                entry->wakeup_pending = 1;
        }

        return;
}

static void
imsm_stage_out(void **list_out, struct imsm_ctx *ctx, size_t ppoint_index)
{

        for (size_t i = 0, n = ctx->imsm->slab.element_count; i < n; i++) {
                struct imsm_entry *entry;

                entry = imsm_traverse(ctx, i);
                if (entry != NULL &&
                    entry->queue_id == ppoint_index &&
                    entry->wakeup_pending != 0) {
                        bool success;
                        void *member = (char *)entry + entry->offset;

                        entry->wakeup_pending = 0;
                        success = imsm_list_push(list_out, member, 0);
                        assert(success);
                }
        }

        return;
}

void **
imsm_stage_io(struct imsm_ctx *ctx, struct imsm_ppoint_record ppoint,
    void **list_in, uint64_t aux_match)
{
        size_t ppoint_index;
        void **ret;

        ppoint_index = imsm_index(ctx, ppoint);
        assert(ppoint_index < UINT16_MAX && "Queue id too high");

        /* Register new list entries in the queue. */
        imsm_stage_in(ctx, ppoint_index, list_in, aux_match);

        ret = imsm_list_get(&ctx->cache, ctx->imsm->slab.element_count);
        /* Populate `ret` with all active entries. */
        imsm_stage_out(ret, ctx, ppoint_index);
        return ret;
}

extern size_t imsm_index(struct imsm_ctx *ctx,
    struct imsm_ppoint_record record);

extern struct imsm_unwind_record imsm_region_push(struct imsm_ctx *,
    struct imsm_ppoint_record);

extern void imsm_region_pop(const struct imsm_unwind_record *);
