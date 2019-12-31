#pragma once

#include <stddef.h>
#include <stdint.h>

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
