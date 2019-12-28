#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "imsm.h"

struct echo_state;

static IMSM(echo_imsm, struct echo_state) echo;

struct echo_state {
        struct imsm_entry header;
        size_t in_count;
        size_t out_count;
        char *buf;
};

static void echo_poll(struct imsm_ctx *ctx)
{

        (void)ctx;
        return;
}

void
init(void)
{
        static struct echo_state buf[128];
        
        IMSM_INIT(&echo, header, buf, sizeof(buf),
                  echo_poll);
        return;
}

void
slab_get_put()
{
        struct echo_state *state[32];

        for (size_t i = 0; i < 32; i++) {
                state[i] = IMSM_GET(&echo);

                assert(state[i]->in_count == 0);
                printf("%p\n", state[i]);
        }

        for (size_t i = 0; i < 32; i++)
                IMSM_PUT(&echo, state[i]);
        return;
}

void
slab_get_put_tight()
{
        static struct echo_imsm small_echo;
        static struct echo_state buf[2];
        struct echo_state *state0, *state1, *state2;

        /* Make sure we handle small arenas. */
        IMSM_INIT(&small_echo, header, buf, sizeof(buf),
                  echo_poll);

        state0 = IMSM_GET(&small_echo);
        state1 = IMSM_GET(&small_echo);
        state2 = IMSM_GET(&small_echo);
        printf("%p %p %p\n", state0, state1, state2);

        assert(state0 != NULL);
        assert(state1 != NULL);
        assert(state2 == NULL);

        IMSM_PUT(&small_echo, state2);
        IMSM_PUT(&small_echo, state1);

        state2 = IMSM_GET(&small_echo);
        assert(state2 == state2);

        IMSM_PUT(&small_echo, state2);
        IMSM_PUT(&small_echo, state0);

        state0 = IMSM_GET(&small_echo);
        state1 = IMSM_GET(&small_echo);
        state2 = IMSM_GET(&small_echo);
        printf("%p %p %p\n", state0, state1, state2);

        assert(state0 != NULL);
        assert(state1 != NULL);
        assert(state2 == NULL);
        return;
}

void
slab_get_empty()
{
        static struct echo_imsm small_echo;
        static char buf[sizeof(struct echo_state) - 1];
        struct echo_state *state;

        /* Make sure we handle empty arenas. */
        IMSM_INIT(&small_echo, header, buf, sizeof(buf),
                  echo_poll);

        state = IMSM_GET(&small_echo);
        printf("%p\n", state);
        assert(state == NULL);
        return;
}

int
main()
{

        init();
        slab_get_put();
        slab_get_put_tight();
        slab_get_empty();
        return 0;
}
