#include "imsm.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#include "imsm_internal.h"
#include "imsm_list.h"

#include "imsm_slab.inc"

#define IMSM_MAX_REGISTERED 1024

#define IMSM_ENCODING_MULTIPLIER ((1ULL << 31) + 1)
#define IMSM_DECODING_MULTIPLIER 4611686016279904257ULL

static_assert(
    (uint64_t)IMSM_ENCODING_MULTIPLIER * IMSM_DECODING_MULTIPLIER == 1,
    "The encoding / decoding multipliers must be inverses.");

union imsm_encoded_reference {
        uint64_t bits;
        struct {
                uint64_t global_index : 12;
                uint64_t arena_offset : 36;
                uint64_t version : 16;
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

        imsm->poll_fn = poll_fn;
        imsm->slab.arena = arena;
        imsm->slab.arena_size = arena_size;
        imsm->slab.element_size = elsize;

        slab_init_freelist(&imsm->slab);
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
        size_t arena_offset;

        if (imsm == NULL ||
            imsm->global_index >= IMSM_MAX_REGISTERED ||
            imsm_list.list[imsm->global_index] != imsm)
                return ret;

        header = imsm_entry_of(ctx, object);
        if (header == NULL)
                return ret;

        if ((header->version & 1) == 0)
                return ret;

        arena_base = (uintptr_t)imsm->slab.arena;
        arena_offset = (uintptr_t)object - arena_base;
        assert(arena_offset < (1UL << 36));

        encoded.global_index = imsm->global_index;
        encoded.arena_offset = (uintptr_t)object - arena_base;
        encoded.version = header->version >> 1;

        ret.bits = encoded.bits * IMSM_ENCODING_MULTIPLIER;
        return ret;
}

struct imsm *
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

static struct imsm_entry *
entry_of(const struct imsm *imsm, void *interior_pointer)
{
        uintptr_t arena_base = (uintptr_t)imsm->slab.arena;
        size_t element_size = imsm->slab.element_size;
        size_t offset = (uintptr_t)interior_pointer - arena_base;
        size_t header_offset = element_size * (offset / element_size);

        return (struct imsm_entry *)(arena_base + header_offset);
}

void *
imsm_deref(struct imsm_ref ref)
{
        union imsm_encoded_reference encoded = {
                .bits = ref.bits * IMSM_DECODING_MULTIPLIER
        };
        size_t offset;
        struct imsm *imsm;
        struct imsm_entry *header;
        void *ret;

        imsm = imsm_deref_machine(ref);
        if (imsm == NULL)
                return NULL;

        offset = encoded.arena_offset;
        if (offset >= imsm->slab.arena_size)
                return NULL;

        ret = (void *)((char *)imsm->slab.arena + offset);
        header = entry_of(imsm, ret);
        if ((header->version & 1) == 0 ||
            (header->version >> 1) != encoded.version)
                return NULL;

        return ret;
}

void
imsm_notify(struct imsm_ref ref, enum imsm_notification notification)
{

        if (ref.bits == 0)
                return;

        switch (notification) {
        case IMSM_NOTIFICATION_WAKE: {
                struct imsm *machine;
                void *decoded;
                struct imsm_entry *header;

                machine = imsm_deref_machine(ref);
                decoded = imsm_deref(ref);
                assert(machine != NULL);
                assert(decoded != NULL);

                header = entry_of(machine, decoded);
                header->wakeup_pending = 1;
                return;
        }

        case IMSM_NOTIFICATION_NONE:
        default:
                return;

        }
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
