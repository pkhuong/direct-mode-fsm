#include "program_point.h"

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

extern size_t ppoint_index(struct ppoint_target target);
extern struct ppoint_target ppoint_push(struct ppoint_target target);
extern void ppoint_pop(struct ppoint_target *target);

static void
push_record(struct ppoint_state *state,
            const struct ppoint *ppoint,
            size_t iteration,
            size_t index)
{
        const struct ppoint_state_record record = {
                .ppoint = ppoint,
                .iteration = iteration,
                .state_index = index,
        };

        if (state->next_record >= state->capacity) {
                size_t new_capacity = 2 * state->capacity;

                if (new_capacity < 4)
                        new_capacity = 4;
                /* XXXalloc */
                state->records = realloc(state->records,
                    new_capacity * sizeof(record));
                state->capacity = new_capacity;
        }

        state->records[state->next_record++] = record;
        return;
}

static bool
compare_record(struct ppoint_state *state,
               const struct ppoint *ppoint,
               size_t iteration,
               size_t index)
{
        const struct ppoint_state_record *reference;

        if (state->next_record >= state->reference->next_record)
                return false;

        reference = &state->reference->records[state->next_record++];
        return reference->ppoint == ppoint &&
            reference->iteration == iteration &&
            (size_t)reference->state_index == index;
}

size_t
ppoint_index_slow(const struct ppoint_target *target,
                  size_t predicted)
{

        switch (target->state->mode) {
        case PPOINT_STATE_MODE_RECORD:
                push_record(target->state, target->ppoint, target->iteration,
                    predicted);
                return predicted;

        case PPOINT_STATE_MODE_COMPARE:
                if (compare_record(target->state, target->ppoint, target->iteration,
                    predicted))
                        return predicted;
                return SIZE_MAX;

        case PPOINT_STATE_MODE_FAST:
        default:
                return predicted;
        }
}

void
ppoint_push_slow(const struct ppoint_target *target)
{
        
        switch (target->state->mode) {
        case PPOINT_STATE_MODE_RECORD:
                push_record(target->state, target->ppoint, target->iteration,
                    PPOINT_STATE_ACTION_PUSH);
                return;

        case PPOINT_STATE_MODE_COMPARE:
                assert(compare_record(target->state, target->ppoint,
                    target->iteration, PPOINT_STATE_ACTION_PUSH) &&
                    "Reference trace must match current execution.");
                return;

        case PPOINT_STATE_MODE_FAST:
        default:
                return;
        }
}

void
ppoint_pop_slow(const struct ppoint_target *target)
{
        
        switch (target->state->mode) {
        case PPOINT_STATE_MODE_RECORD:
                push_record(target->state, target->ppoint, target->iteration,
                    PPOINT_STATE_ACTION_POP);
                return;

        case PPOINT_STATE_MODE_COMPARE:
                assert(compare_record(target->state, target->ppoint,
                    target->iteration, PPOINT_STATE_ACTION_POP) &&
                    "Reference trace must match current execution.");
                return;

        case PPOINT_STATE_MODE_FAST:
        default:
                return;
        }
}

bool
pppoint_state_validate(const struct ppoint_state *state)
{

        /* XXX: implement something useful. */
        (void)state;
        return true;
}
