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
};

/*
 * Initializes a base `imsm` struct.  Use
 *  `IMSM_INIT(imsm, path_to_imsm_entry_field, arena, arena_size, poll_fn)`
 * for a type-safe IMSM.
 */
void imsm_init(struct imsm *, void *arena, size_t arena_size, size_t elsize,
    void (*poll_fn)(struct imsm_ctx *));
