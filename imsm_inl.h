#pragma once

#include <stdint.h>

/*
 * Full slow path for slab allocation.
 */
struct imsm_entry *imsm_get_slow(struct imsm_ctx *, struct imsm *);

/*
 * Reloads the imsm slab's allocation cache.
 */
void imsm_get_cache_reload(struct imsm *);

inline struct imsm_entry *
imsm_get(struct imsm_ctx *ctx, struct imsm *imsm)
{
        struct imsm_slab *slab = &imsm->slab;
        struct imsm_entry *ret;
        size_t alloc_index;

        /* If we have no allocation cache at all, enter the slow path. */
        if (__builtin_expect(
            ctx->imsm != imsm || slab->current_allocating == NULL, 0))
                return imsm_get_slow(ctx, imsm);

        alloc_index = slab->current_alloc_index - 1;
        slab->current_alloc_index = alloc_index;
        ret = slab->current_allocating[alloc_index];
        ret->version++;
        if (__builtin_expect(alloc_index == 0, 0))
                imsm_get_cache_reload(imsm);

        return ret;
}

/*
 * Reloads the imsm slab's deallocation cache.
 */
void imsm_put_cache_reload(struct imsm_ctx *, struct imsm *imsm);

inline void
imsm_put(struct imsm_ctx *ctx, struct imsm *imsm, struct imsm_entry *freed)
{
        struct imsm_slab *slab = &imsm->slab;
        long free_index;

        if (__builtin_expect(freed == NULL, 0)) {
                return;
        }

        freed->version = (freed->version + 1) & ~1;
        freed->queue_id = -1;
        free_index = slab->current_free_index + 1;
        slab->current_free_index = free_index;
        slab->current_freeing[free_index] = freed;
        if (__builtin_expect(free_index == 0 || ctx->imsm != imsm, 0))
                imsm_put_cache_reload(ctx, imsm);
        return;
}

inline struct imsm_entry *
imsm_entry_of(struct imsm_ctx *ctx, void *ptr)
{
        struct imsm_slab *slab = &ctx->imsm->slab;
        uintptr_t arena_base = (uintptr_t)slab->arena;
        uintptr_t address = (uintptr_t)ptr;

        if ((address - arena_base) >= slab->arena_size)
                return NULL;

        return imsm_traverse(ctx, (address - arena_base) / slab->element_size);
}

inline struct imsm_entry *
imsm_traverse(struct imsm_ctx *ctx, size_t i)
{
        struct imsm_slab *slab = &ctx->imsm->slab;
        uintptr_t arena_base = (uintptr_t)slab->arena;
        struct imsm_entry *ret;

        if (__builtin_expect(i >= slab->element_count, 0))
                return NULL;

        ret = (void *)(arena_base + i * slab->element_size);
        return ((ret->version & 1) != 0) ? ret : NULL;
}

inline size_t
imsm_index(struct imsm_ctx *ctx, struct imsm_ppoint_record record)
{
        uintptr_t ppoint_diff
            = (uintptr_t)ctx->position.ppoint ^ (uintptr_t)record.ppoint;
        uint64_t iteration_diff_high
            = (uint64_t)(ctx->position.iteration >> 64)
            ^ (uint64_t)(record.iteration >> 64);
        uint64_t iteration_diff_lo =
            (uint64_t)(ctx->position.iteration) ^ (uint64_t)(record.iteration);
        /* Only increment if the record differs in ppoint or iteration. */
        size_t increment = !!(ppoint_diff | iteration_diff_high | iteration_diff_lo);

        ctx->position.iteration = record.iteration;
        ctx->position.ppoint = record.ppoint;
        ctx->position.index += increment;
        /*
         * If we just incremented, undo that to simulate
         * post-increment; if we did not increment, subtract 1 to
         * override the post-increment.
         */
        return ctx->position.index - 1;
}

inline struct imsm_unwind_record
imsm_region_push(struct imsm_ctx *ctx, struct imsm_ppoint_record record)
{

        /*
         * Clear out the current position to make sure the next
         * program point gets a fresh index.
         */
        ctx->position.ppoint = NULL;
        record.index = (size_t)IMSM_PPOINT_ACTION_POP;
        return (struct imsm_unwind_record) {
                .position = record,
                .context = ctx,
        };
}

inline void
imsm_region_pop(const struct imsm_unwind_record *unwind)
{

        /*
         * We only need to clear the current position.
         */
        unwind->context->position.ppoint = NULL;
        return;
}
