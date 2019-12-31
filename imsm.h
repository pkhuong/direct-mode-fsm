#pragma once

/*
 * Base type-erased implementation.
 */
#include <stddef.h>
#include <stdint.h>

#include "imsm_list.h"
#include "imsm_ppoint.h"
#include "imsm_slab.h"
#include "imsm_wrapper.h"

struct imsm_ctx;

/*
 * The first field in any IMSM state struct must be a `imsm_entry`.
 */
struct imsm_entry {
        /* Version is even if inactive, odd if active. */
        uint32_t version;
        /* UINT16_MAX means no queue. */
        uint16_t queue_id;
        /*
         * XXX: needs more range here... make this 16 bytes?  Could
         * also use the bytes for timeouts / exponential backoff.
         */
        uint8_t offset;
        uint8_t wakeup_pending;
};

/*
 * We can encode a reference to an imsm state object in 64 bits,
 * including version information.  This encoding is only safe for
 * potentially spurious wake-ups, as the version data is minimal.
 *
 * All 0 is a special NULL reference.
 */
struct imsm_ref {
        uint64_t bits;
};

/*
 * This base struct hold the global information for one immediate mode
 * state machine.  Use the IMSM(IMSM_TYPE_NAME, STATE_TYPE_NAME) macro
 * to declare a type-safe version of this struct.
 */
struct imsm {
        size_t global_index;
        struct imsm_slab slab;
        void (*poll_fn)(struct imsm_ctx *);
};

/*
 * Poll loops center their work around a `struct imsm_ctx` type-erased
 * context struct. It could also be a thread-local variable, but it
 * seems simpler to explicitly thread it around as an argument.
 */
struct imsm_ctx {
        struct imsm *imsm;
        struct imsm_ppoint_record position;
        struct imsm_list_cache cache;
};

/*
 * Initializes a base `imsm` struct.  Use
 *  `IMSM_INIT(imsm, path_to_imsm_entry_field, arena, arena_size, poll_fn)`
 * for a type-safe IMSM.
 *
 * The arena should be zero-initialized.
 */
void imsm_init(struct imsm *, void *arena, size_t arena_size, size_t elsize,
    void (*poll_fn)(struct imsm_ctx *));

/*
 * Returns a packed reference to an imsm and a pointer managed by that
 * state machine, or a NULL reference on failure.
 *
 * Packing all that data in 64 bits makes it easier to interface with
 * epoll or kqueue.
 */
struct imsm_ref imsm_refer(struct imsm_ctx *, void *);

/*
 * Returns the object encoded in the reference, if any.
 */
struct imsm_entry *imsm_deref(struct imsm_ref);

/*
 * Wakes the imsm managed object in the reference if any.
 *
 * Returns true iff the reference was valid or NULL, false if
 * definitely corrupt.
 */
bool imsm_notify(struct imsm_ref);

/*
 * Adds all records in `imsm_list_in` where the auxiliary value equals
 * `aux_match` to the queue identified by the current program point,
 * and returns a list of all the values associated with that queue
 * that (may) have been woken.
 *
 * A NULL list is empty.
 */
void **imsm_stage_io(struct imsm_ctx *, struct imsm_ppoint_record,
     void **list_in, uint64_t aux_match);

#include "imsm_ppoint.inl"
#include "imsm_slab.inl"
