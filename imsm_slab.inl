/* -*- mode: C -*- */

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

        /* Make sure this code matches imsm_put_n. */
        slab->deinit_fn(freed);
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
