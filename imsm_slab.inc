/* -*- mode: C -*- */

/*
 * Slab implementation for imsm.
 */

#define SLAB_MAGAZINE_SIZE 15

struct imsm_slab_magazine {
        struct imsm_slab_magazine *next;
        struct imsm_entry *entries[SLAB_MAGAZINE_SIZE];
};

/*
 * Returns a pointer to the allocation cache in a magazine.
 * The freed entries cache counts from the end.
 */
static struct imsm_entry **
free_cache_of_magazine(struct imsm_slab_magazine *magazine)
{

        assert(magazine != NULL);
        return &magazine->entries[SLAB_MAGAZINE_SIZE - 1];
}

/*
 * Returns a pointer to the allocation cache in a magazine.
 */
static struct imsm_entry **
alloc_cache_of_magazine(struct imsm_slab_magazine *magazine)
{

        assert(magazine != NULL);
        return &magazine->entries[0];
}

/*
 * Inverts `free_cache_of_magazine`.
 */
static struct imsm_slab_magazine *
magazine_of_free_cache(struct imsm_entry **entries)
{
        const size_t offset = __builtin_offsetof(struct imsm_slab_magazine, entries);
        char *base = (void *)(entries - (SLAB_MAGAZINE_SIZE - 1));

        assert(entries != NULL);
        return (void *)(base - offset);
}

static struct imsm_slab_magazine *
magazine_of_alloc_cache(struct imsm_entry **entries)
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
        slab->current_freeing = free_cache_of_magazine(empty);
        slab->current_free_index = -SLAB_MAGAZINE_SIZE;
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
        size_t num_freed = SLAB_MAGAZINE_SIZE + slab->current_free_index;

        assert(slab->current_freeing != NULL &&
            "current_freeing must always be valid and never full.");
        assert(slab->current_free_index >= -SLAB_MAGAZINE_SIZE);

        /* If the current freeing cache is empty, there's nothing to convert. */
        if (num_freed == 0) {
                slab->current_alloc_index = 0;
                slab->current_allocating = NULL;
                return;
        }

        current_cache = magazine_of_free_cache(slab->current_freeing);

        /* Steal the current free cache, replace it with a new empty one. */
        slab->current_freeing = NULL;
        slab_refresh_current_freeing(slab);

        slab->current_allocating = alloc_cache_of_magazine(current_cache);
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

        slab->current_allocating = alloc_cache_of_magazine(full);
        slab->current_alloc_index = SLAB_MAGAZINE_SIZE;
        return;
}

/*
 * Flushes the current full freeing magazine to the freelist, and
 * replaces it with a new empty magazine.
 */
static inline void
slab_flush(struct imsm_slab *slab)
{
        struct imsm_slab_magazine *full;

        assert(slab->current_free_index == 0 &&
            "slab_flush must only be called on full magazines");

        full = magazine_of_free_cache(slab->current_freeing);
        full->next = slab->freelist;
        slab->freelist = full;

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

        slab->current_freeing[++slab->current_free_index] = entry;
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

        slab->element_count = nelem;
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
        assert(slab->current_free_index < 0 &&
            slab->current_free_index >= -SLAB_MAGAZINE_SIZE);

        /* The slab's allocation cache is initially empty. */
        assert(slab->current_allocating == NULL);
        assert(slab->current_freeing != NULL);
        assert(slab->empty == NULL);
        return;
}

struct imsm_entry *
imsm_get_slow(struct imsm_ctx *ctx, struct imsm *imsm)
{
        struct imsm_slab *slab = &imsm->slab;

        assert(ctx->imsm == imsm &&
            "imsm context and allocating imsm must match.");

        if (slab->current_allocating == NULL)
                imsm_get_cache_reload(imsm);
        if (slab->current_allocating == NULL)
                return NULL;

        /* imsm_get only calls get_slow if current_allocating == NULL. */
        return imsm_get(ctx, imsm);
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

                empty = magazine_of_alloc_cache(slab->current_allocating);
                empty->next = slab->empty;
                slab->empty = empty;
                slab->current_allocating = NULL;
        }

        slab_refresh_current_allocating(slab);
}

extern struct imsm_entry *imsm_get(struct imsm_ctx *, struct imsm *imsm);

void
imsm_put_cache_reload(struct imsm_ctx *ctx, struct imsm *imsm)
{
        struct imsm_slab *slab = &imsm->slab;

        assert(ctx->imsm == imsm &&
            "imsm context and allocating imsm must match.");
        assert(slab->current_free_index == 0 &&
            "Only empty free caches may be reloaded");
        slab_flush(slab);
        return;
}

extern void imsm_put(struct imsm_ctx *, struct imsm *imsm, struct imsm_entry *freed);

extern struct imsm_entry *imsm_entry_of(struct imsm_ctx *, void *);

extern struct imsm_entry *imsm_traverse(struct imsm_ctx *, size_t i);
