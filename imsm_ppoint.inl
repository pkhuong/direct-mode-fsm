/* -*- mode: C -*- */

#pragma once

inline struct imsm_unwind_record
imsm_region_push(struct imsm_ctx *ctx, struct imsm_ppoint_record record)
{

        /*
         * Clear out the current position to make sure the next
         * program point gets a fresh index.
         */
        ctx->position.ppoint = NULL;
        record.index = (size_t)IMSM_PPOINT_ACTION_POP;
        return (struct imsm_unwind_record) {
                .position = record,
                .context = ctx,
        };
}

inline void
imsm_region_pop(const struct imsm_unwind_record *unwind)
{

        /*
         * We only need to clear the current position.
         */
        unwind->context->position.ppoint = NULL;
        return;
}

inline size_t
imsm_index(struct imsm_ctx *ctx, struct imsm_ppoint_record record)
{
        uintptr_t ppoint_diff
            = (uintptr_t)ctx->position.ppoint ^ (uintptr_t)record.ppoint;
        uint64_t iteration_diff_high
            = (uint64_t)(ctx->position.iteration >> 64)
            ^ (uint64_t)(record.iteration >> 64);
        uint64_t iteration_diff_lo =
            (uint64_t)(ctx->position.iteration) ^ (uint64_t)(record.iteration);
        /* Only increment if the record differs in ppoint or iteration. */
        size_t inc = !!(ppoint_diff | iteration_diff_high | iteration_diff_lo);

        ctx->position.iteration = record.iteration;
        ctx->position.ppoint = record.ppoint;
        ctx->position.index += inc;
        /*
         * If we just incremented, undo that to simulate
         * post-increment; if we did not increment, subtract 1 to
         * override the post-increment.
         */
        return ctx->position.index - 1;
}
