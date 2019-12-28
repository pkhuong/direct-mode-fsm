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

        /* If we have no allocation cache at all, enter the slow path. */
        if (__builtin_expect(slab->current_allocating == NULL, 0))
                return imsm_get_slow(imsm);

        ret = slab->current_allocating[--slab->current_alloc_index];
        if (slab->current_alloc_index == 0)
                imsm_get_cache_reload(imsm);

        return ret;
}
