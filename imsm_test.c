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
slab_get_put(void)
{
        struct echo_state *state[32];
        struct imsm_ctx ctx = {
                &echo.imsm,
        };

        for (size_t i = 0; i < 32; i++) {
                state[i] = IMSM_GET(&ctx, &echo);

                assert(state[i]->in_count == 0);
                printf("%p\n", state[i]);
        }

        for (size_t i = 0; i < 32; i++)
                IMSM_PUT(&ctx, &echo, state[i]);
        return;
}

void
slab_get_put_tight(void)
{
        static struct echo_imsm small_echo;
        static struct echo_state buf[2];
        struct imsm_ctx ctx = {
                &small_echo.imsm,
        };

        struct echo_state *state0, *state1, *state2;

        /* Make sure we handle small arenas. */
        IMSM_INIT(&small_echo, header, buf, sizeof(buf),
                  echo_poll);

        state0 = IMSM_GET(&ctx, &small_echo);
        state1 = IMSM_GET(&ctx, &small_echo);
        state2 = IMSM_GET(&ctx, &small_echo);
        printf("%p %p %p\n", state0, state1, state2);

        assert(state0 != NULL);
        assert(state1 != NULL);
        assert(state2 == NULL);

        IMSM_PUT(&ctx, &small_echo, state2);
        IMSM_PUT(&ctx, &small_echo, state1);

        state2 = IMSM_GET(&ctx, &small_echo);
        assert(state2 == state2);

        IMSM_PUT(&ctx, &small_echo, state2);
        IMSM_PUT(&ctx, &small_echo, state0);

        state0 = IMSM_GET(&ctx, &small_echo);
        state1 = IMSM_GET(&ctx, &small_echo);
        state2 = IMSM_GET(&ctx, &small_echo);
        printf("%p %p %p\n", state0, state1, state2);

        assert(state0 != NULL);
        assert(state1 != NULL);
        assert(state2 == NULL);
        return;
}

void
slab_get_empty(void)
{
        static struct echo_imsm small_echo;
        static char buf[sizeof(struct echo_state) - 1];
        struct imsm_ctx ctx = {
                &small_echo.imsm,
        };

        struct echo_state *state;

        /* Make sure we handle empty arenas. */
        IMSM_INIT(&small_echo, header, buf, sizeof(buf),
                  echo_poll);

        state = IMSM_GET(&ctx, &small_echo);
        printf("%p\n", state);
        assert(state == NULL);
        return;
}

static void
ppoint_rec(struct imsm_ctx *ctx)
{

        IMSM_REGION(ctx, "ppoint_rec");

        printf("ppoint_rec: %zu\n", IMSM_INDEX(ctx, "rec"));
        return;
}

static void
ppoint_rec2(struct imsm_ctx *ctx)
{

        WITH_IMSM_REGION(ctx, "ppoint_rec") {
                printf("ppoint_rec: %zu\n", IMSM_INDEX(ctx, "rec"));
        }

        return;
}

void
ppoint(void)
{
        struct imsm_ctx ctx = {
                &echo.imsm,
        };

        printf("entry: %zu\n", IMSM_INDEX(&ctx, "entry"));
        printf("next: %zu\n", IMSM_INDEX(&ctx, "next"));

        for (size_t i = 0; i < 10; i++) {
                for (size_t j = 0; j < 2; j++) {
                        printf("loop: %zu\n", IMSM_INDEX(&ctx, "loop", i));
                }
        }

        for (size_t i = 0; i < 2; i++)
                ppoint_rec(&ctx);

        for (size_t i = 0; i < 2; i++)
                ppoint_rec2(&ctx);

        printf("out: %zu\n", IMSM_INDEX(&ctx, "out"));
}

int
main()
{

        init();
        slab_get_put();
        slab_get_put_tight();
        slab_get_empty();
        ppoint();
        return 0;
}
