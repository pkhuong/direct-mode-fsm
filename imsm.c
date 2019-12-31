#include "imsm.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#include "imsm_internal.h"
#include "imsm_list.h"

#define IMSM_MAX_REGISTERED 1024

#define VERSION_NUMBER_BITS 12

#define IMSM_ENCODING_MULTIPLIER ((1ULL << 31) + 1)
#define IMSM_DECODING_MULTIPLIER 4611686016279904257ULL

static void
imsm_slab_init(struct imsm_slab *slab, void *arena, size_t arena_size, size_t elsize);

static_assert(
    (uint64_t)IMSM_ENCODING_MULTIPLIER * IMSM_DECODING_MULTIPLIER == 1,
    "The encoding / decoding multipliers must be inverses.");

union imsm_encoded_reference {
        uint64_t bits;
        struct {
                uint64_t global_index : 12;
                uint64_t object_index : 40;
                uint64_t version : VERSION_NUMBER_BITS;
        };
};

static struct {
        size_t registered;
        struct imsm **list;
} imsm_list;

static void
imsm_register(struct imsm *imsm)
{

        if (imsm_list.list == NULL) {
                imsm_list.list =
                    calloc(IMSM_MAX_REGISTERED, sizeof(*imsm_list.list));
                assert(imsm_list.list != NULL && "Static allocation failed.");
        }

        assert(imsm_list.registered + 1 < IMSM_MAX_REGISTERED &&
            "Too many static imsm registered");
        assert(imsm->global_index == 0 && "Double registration?!");
        imsm->global_index = ++imsm_list.registered;
        imsm_list.list[imsm_list.registered] = imsm;
        return;
}

void
imsm_init(struct imsm *imsm, void *arena, size_t arena_size, size_t elsize,
    void (*poll_fn)(struct imsm_ctx *))
{

        assert(imsm->poll_fn == NULL &&
            "imsm must be initialized exactly once");

        if (arena_size > (1UL << 36))
                arena_size = 1UL << 36;

        imsm_slab_init(&imsm->slab, arena, arena_size, elsize);
        imsm->poll_fn = poll_fn;
        imsm_register(imsm);
        return;
}

struct imsm_ref
imsm_refer(struct imsm_ctx *ctx, void *object)
{
        struct imsm *imsm = ctx->imsm;
        struct imsm_ref ret = { 0 };
        union imsm_encoded_reference encoded;
        struct imsm_entry *header;
        uintptr_t arena_base;
        size_t element_size;
        size_t object_index;

        if (imsm == NULL ||
            imsm->global_index >= IMSM_MAX_REGISTERED ||
            imsm_list.list[imsm->global_index] != imsm)
                return ret;

        element_size = imsm->slab.element_size;
        header = imsm_entry_of(ctx, object);
        if (header == NULL)
                return ret;

        if ((header->version & 1) == 0)
                return ret;

        arena_base = (uintptr_t)imsm->slab.arena;
        object_index = ((uintptr_t)object - arena_base) / element_size;
        assert(object_index < (1UL << 40));

        encoded.global_index = imsm->global_index;
        encoded.object_index = object_index;
        encoded.version = header->version >> 1;

        ret.bits = encoded.bits * IMSM_ENCODING_MULTIPLIER;
        return ret;
}

inline struct imsm *
imsm_deref_machine(struct imsm_ref ref)
{
        union imsm_encoded_reference encoded = {
                .bits = ref.bits * IMSM_DECODING_MULTIPLIER
        };

        if (ref.bits == 0 ||
            encoded.global_index >= IMSM_MAX_REGISTERED)
                return NULL;

        return imsm_list.list[encoded.global_index];
}

inline struct imsm_entry *
imsm_deref(struct imsm_ref ref)
{
        static const size_t version_mask = (1UL << VERSION_NUMBER_BITS) - 1;
        union imsm_encoded_reference encoded = {
                .bits = ref.bits * IMSM_DECODING_MULTIPLIER
        };
        struct imsm *imsm;
        struct imsm_entry *header;
        size_t element_size;
        size_t offset;

        imsm = imsm_deref_machine(ref);
        if (imsm == NULL)
                return NULL;

        element_size = imsm->slab.element_size;
        offset = encoded.object_index;
        if (offset >= imsm->slab.element_count)
                return NULL;

        header = (void *)((char *)imsm->slab.arena + offset * element_size);
        if ((header->version & 1) == 0 ||
            ((header->version >> 1) & version_mask) != encoded.version)
                return NULL;

        return header;
}

bool
imsm_notify(struct imsm_ref ref)
{
        struct imsm *machine;
        struct imsm_entry *header;

        if (ref.bits == 0)
                return true;

        machine = imsm_deref_machine(ref);
        if (machine == NULL)
                return false;

        header = imsm_deref(ref);
        if (header != NULL)
                header->wakeup_pending = 1;

        return true;
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
        const struct imsm_slab *slab = &ctx->imsm->slab;
        const uintptr_t arena_base = (uintptr_t)slab->arena;

        for (size_t i = 0, n = slab->element_count; i < n; i++) {
                struct imsm_entry *entry;

                /*
                 * XXX: would imsm_traverse compile down to this with
                 * aliasing annotations?
                 */
                entry = (void *)(arena_base + i * slab->element_size);
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

#include "imsm_slab.inc"
