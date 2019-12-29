#pragma once

/*
 * Base type-erased implementation.
 */
#include <stddef.h>
#include <stdint.h>

#include "imsm_internal.h"
#include "imsm_wrapper.h"

struct imsm_ctx;

/*
 * The first field in any IMSM state struct must be a `imsm_entry`.
 */
struct imsm_entry {
        uint32_t version;
        uint16_t queue_id;
        uint8_t wakeup_pending;
};

/*
 * We use statically allocated imsm program points to map queue
 * operations to program states.
 *
 * Only their pointer identity matters; the fields only exist for
 * pretty printing and debugging.
 *
 * Obtain a reference to one with IMSM_PPOINT(NAME).
 */
struct imsm_ppoint {
        /* The `name` should be a domain-specific description. */
        const char *name;
        /* `function` should be populated with `__PRETTY_FUNCTION__`. */
        const char *function;
        /* `file` should be populated with `__FILE__`. */
        const char *file;
        /* `line` should be populated with `__LINE__`. */
        size_t lineno;
        /*
         * `unique` can be used to make program points on the same
         * line look different.
         */
        size_t unique;
};

/*
 * In addition to regular program points, we can push and pop context.
 * These values are encoded in the program point index.
 */
enum imsm_ppoint_action {
        IMSM_PPOINT_ACTION_PUSH = -1,
        IMSM_PPOINT_ACTION_POP = -2,
};

/*
 * We can update the state index visiting a new program point.
 *
 * Obtain one (by value) with IMSM_PPOINT_RECORD(NAME, ITERATION?).
 */
struct imsm_ppoint_record {
        __uint128_t iteration;
        const struct imsm_ppoint *ppoint;
        size_t index;
};

/*
 * We can also augment the program point data with additional "program
 * region" context information.  This struct encapsulates the
 * information needed to subtract region from the context.
 *
 * The useful side-effect of a region is that returning into the same
 * program point in a new region yields a distinct index.
 */
struct imsm_unwind_record {
        struct imsm_ppoint_record position;
        struct imsm_ctx *context;
        size_t scratch;
};

/*
 * This base struct hold the global information for one immediate mode
 * state machine.  Use the IMSM(IMSM_TYPE_NAME, STATE_TYPE_NAME) macro
 * to declare a type-safe version of this struct.
 */
struct imsm {
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
};

/*
 * Initializes a base `imsm` struct.  Use
 *  `IMSM_INIT(imsm, path_to_imsm_entry_field, arena, arena_size, poll_fn)`
 * for a type-safe IMSM.
 */
void imsm_init(struct imsm *, void *arena, size_t arena_size, size_t elsize,
    void (*poll_fn)(struct imsm_ctx *));

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
 * See IMSM_GET for a type-safe version.
 */
inline void imsm_put(struct imsm_ctx *, struct imsm *, struct imsm_entry *);

/*
 * Returns the state index for this program point record.  This value
 * increases monotonically, unless we revisit the same program point
 * twice in a row.
 *
 * See IMSM_INDEX(CTX, NAME, ITER?) for a convenient wrapper.
 */
inline size_t imsm_index(struct imsm_ctx *, struct imsm_ppoint_record);

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

#include "imsm_inl.h"
