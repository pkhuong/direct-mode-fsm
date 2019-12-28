#pragma once

/*
 * Full slow path for slab allocation.
 */
struct imsm_entry *imsm_get_slow(struct imsm *);

/*
 * Reloads the imsm slab's allocation cache.
 */
void imsm_get_cache_reload(struct imsm *);

inline struct imsm_entry *
imsm_get(struct imsm *imsm)
{
        struct imsm_slab *slab = &imsm->slab;
        struct imsm_entry *ret;
        size_t alloc_index;

        /* If we have no allocation cache at all, enter the slow path. */
        if (__builtin_expect(slab->current_allocating == NULL, 0))
                return imsm_get_slow(imsm);

        alloc_index = --slab->current_alloc_index;
        ret = slab->current_allocating[alloc_index];
        if (__builtin_expect(alloc_index == 0, 0))
                imsm_get_cache_reload(imsm);

        return ret;
}

/*
 * Reloads the imsm slab's deallocation cache.
 */
void imsm_put_cache_reload(struct imsm *imsm);

inline void
imsm_put(struct imsm *imsm, struct imsm_entry *freed)
{
        struct imsm_slab *slab = &imsm->slab;
        size_t free_index;

        if (freed == NULL) {
                return;
        }

        free_index = --slab->current_free_index;
        slab->current_freeing[free_index] = freed;
        if (__builtin_expect(free_index == 0, 0))
                imsm_put_cache_reload(imsm);
        return;
}
