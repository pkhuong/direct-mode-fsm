#pragma once

/*
 * Base type-erased implementation.
 */
#include <stddef.h>
#include <stdint.h>

#include "imsm_internal.h"
#include "imsm_list.h"
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

/*
 * Allocates one object from the `imsm`'s slab, or NULL if the slab
 * has no free object.  The second argument is redundant, but serves
 * as a witness that the context is for the correct imsm.  This
 * function will abort on mismatches.
 *
 * See IMSM_GET for a type-safe version.
 */
inline struct imsm_entry *imsm_get(struct imsm_ctx *, struct imsm *);

/*
 * Deallocates one object back to the `imsm`'s slab.  Safe to call on
 * NULL pointers, but will abort on any other pointer not allocated
 * by the `imsm`.
 *
 * See IMSM_PUT for a type-safe version.
 */
inline void imsm_put(struct imsm_ctx *, struct imsm *, struct imsm_entry *);

/*
 * Augments the imsm context with a LIFO program point context record.
 * The augmentation should be unwound with `imsm_region_pop` in LIFO
 * order.
 *
 * See IMSM_REGION(CTX, NAME, ITER?) and WITH_IMSM_REGION(CTX, NAME, ITER?)
 * for convenient wrappers.
 */
inline struct imsm_unwind_record imsm_region_push(struct imsm_ctx *,
    struct imsm_ppoint_record);

inline void imsm_region_pop(const struct imsm_unwind_record *);

/*
 * Exposed only for testing.
 */

/*
 * Returns the state machine encoded in the reference, if any.
 */
struct imsm *imsm_deref_machine(struct imsm_ref);

/*
 * Accepts an interior pointer to an element of the `imsm_ctx`'s slab,
 * and returns a pointer to the entry header, or NULL if there is no
 * such element in the slab.
 */
inline struct imsm_entry *imsm_entry_of(struct imsm_ctx *, void *);

/*
 * Returns a pointer to the `i`th element of `imsm_ctx`'s slab, or
 * NULL if there is no such element, or the element is inactive.
 */
inline struct imsm_entry *imsm_traverse(struct imsm_ctx *, size_t i);

/*
 * Returns the state index for this program point record.  This value
 * increases monotonically, unless we revisit the same program point
 * twice in a row.
 *
 * See IMSM_INDEX(CTX, NAME, ITER?) for a convenient wrapper.
 */
inline size_t imsm_index(struct imsm_ctx *, struct imsm_ppoint_record);

#include "imsm_inl.h"
