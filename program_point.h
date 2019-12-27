#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#define PP_STATE pp_state

struct ppoint {
        const char *name;
        const char *function;
        const char *file;
        size_t lineno;
        size_t unique;
};

enum ppoint_state_mode {
        PPOINT_STATE_MODE_RECORD = 0,
        PPOINT_STATE_MODE_COMPARE,
        PPOINT_STATE_MODE_FAST,
};

enum ppoint_state_action {
        PPOINT_STATE_ACTION_PUSH = -1,
        PPOINT_STATE_ACTION_POP = -2,
};

struct ppoint_state_record {
        const struct ppoint *ppoint;
        size_t iteration;
        /*
         * negative values are mapped to ppoint_state_action
         */
        ssize_t state_index;
};

struct ppoint_state {
        enum ppoint_state_mode mode;
        size_t state_index_counter;
        const struct ppoint *previous_ppoint;
        size_t previous_iteration;

        /*
         * `next_record` indexes in `reference` in COMPARE mode and in
         * `records` in RECORD mode.
         */
        size_t next_record;
        size_t capacity;
        const struct ppoint_state *reference;
        struct ppoint_state_record *records;
};

/*
 * Track where we are in a program in order to figure out what state
 * structs are associated with that program state.
 */
#define PPOINT(NAME) PPOINT_(__COUNTER__, NAME)
#define PPOINT_(COUNTER, NAME)                                          \
        ({                                                              \
                static const struct ppoint ppoint_ = {                  \
                        .name = (NAME),                                 \
                        .function = __PRETTY_FUNCTION__,                \
                        .file = __FILE__,                               \
                        .lineno = __LINE__,                             \
                        .unique = COUNTER,                              \
                };                                                      \
                &ppoint_;                                               \
        })


struct ppoint_target {
        struct ppoint_state *state;
        const struct ppoint *ppoint;
        size_t iteration;
        size_t scratch;
};

#define PPOINT_TARGET(STATE, NAME, ITER) (struct ppoint_target) {       \
                .state = (STATE),                                       \
                .ppoint = PPOINT(NAME),                                 \
                .iteration = (ITER),                                    \
        }

/*
 * Returns the index for this program point.  Repeated calls for the
 * same target and program point will return the same index.
 *
 * Returns SIZE_MAX if there is no such index.
 */
inline size_t ppoint_index(struct ppoint_target);

#define PPOINT_INDEX(PP_STATE, NAME, ITER)                      \
        ppoint_index(PPOINT_TARGET(PP_STATE, NAME, ITER))

inline struct ppoint_target ppoint_push(struct ppoint_target);
inline void ppoint_pop(struct ppoint_target *);

#define PPOINT_CONTEXT(NAME, ITER)                                      \
        struct ppoint_target pp_context_##NAME                          \
        __attribute__((cleanup(ppoint_pop)))                            \
                = ppoint_push(PPOINT_TARGET(PP_STATE, #NAME, ITER))

#define WITH_PPOINT_CONTEXT(NAME, ITER)         \
        for (PPOINT_CONTEXT(NAME, ITER);        \
             pp_context_##NAME.scratch == 0;    \
             pp_context_##NAME.scratch = 1)

bool ppoint_state_validate(const struct ppoint_state *);

size_t ppoint_index_slow(const struct ppoint_target *target, size_t predicted);

inline size_t
ppoint_index(struct ppoint_target target)
{
        size_t ret;

        if (target.state->previous_ppoint == target.ppoint &&
            target.state->previous_iteration == target.iteration)
                return target.state->state_index_counter;

        target.state->previous_ppoint = target.ppoint;
        target.state->previous_iteration = target.iteration;
        ret = ++target.state->state_index_counter;
        if (__builtin_expect(target.state->mode == PPOINT_STATE_MODE_FAST, 1))
                return ret;
        return ppoint_index_slow(&target, ret);
}

void ppoint_push_slow(const struct ppoint_target *target);

inline struct ppoint_target
ppoint_push(struct ppoint_target target)
{

        target.state->previous_ppoint = NULL;
        target.state->previous_iteration = 0;
        if (__builtin_expect(target.state->mode != PPOINT_STATE_MODE_FAST, 0))
                ppoint_push_slow(&target);
        return target;
}

void ppoint_pop_slow(const struct ppoint_target *target);

inline void
ppoint_pop(struct ppoint_target *target)
{

        target->state->previous_ppoint = NULL;
        target->state->previous_iteration = 0;
        if (__builtin_expect(target->state->mode == PPOINT_STATE_MODE_FAST, 1))
                return;
        ppoint_pop_slow(target);
}
