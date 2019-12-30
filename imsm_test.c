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

        IMSM_CTX_PTR(&ctx);

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
slab_get_put_tight(void)
{
        static struct echo_imsm small_echo;
        static struct echo_state buf[2];
        struct imsm_ctx ctx = {
                &small_echo.imsm,
        };

        struct echo_state *state0, *state1, *state2;

        IMSM_CTX_PTR(&ctx);
        /* Make sure we handle small arenas. */
        IMSM_INIT(&small_echo, header, buf, sizeof(buf),
                  echo_poll);

        state0 = IMSM_GET(&small_echo);
        state1 = IMSM_GET(&small_echo);
        state2 = IMSM_GET(&small_echo);
        printf("%p %i %p %i %p\n",
               state0, state0->header.version,
               state1, state1->header.version,
               state2);

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
        printf("%p %i %p %i %p\n",
               state0, state0->header.version,
               state1, state1->header.version,
               state2);

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

        IMSM_CTX_PTR(&ctx);

        state = IMSM_GET( &small_echo);
        printf("%p\n", state);
        assert(state == NULL);
        return;
}

static void
ppoint_rec(struct imsm_ctx *IMSM_CTX_PTR_VAR)
{

        IMSM_REGION("ppoint_rec");

        printf("ppoint_rec: %zu\n", IMSM_INDEX("rec"));
        return;
}

static void
ppoint_rec2(struct imsm_ctx *IMSM_CTX_PTR_VAR)
{

        WITH_IMSM_REGION("ppoint_rec") {
                printf("ppoint_rec: %zu\n", IMSM_INDEX("rec"));
        }

        return;
}

void
ppoint(void)
{
        struct imsm_ctx ctx = {
                &echo.imsm,
        };

        IMSM_CTX_PTR(&ctx);
        printf("entry: %zu\n", IMSM_INDEX("entry"));
        printf("next: %zu\n", IMSM_INDEX("next"));

        for (size_t i = 0; i < 10; i++) {
                for (size_t j = 0; j < 2; j++) {
                        printf("loop: %zu\n", IMSM_INDEX("loop", i));
                }
        }

        for (size_t i = 0; i < 2; i++)
                ppoint_rec(&ctx);

        for (size_t i = 0; i < 2; i++)
                ppoint_rec2(&ctx);

        printf("out: %zu\n", IMSM_INDEX("out"));
}

void
stage_io(void)
{
        struct echo_state **in, **out;
        struct imsm_ctx ctx = {
                &echo.imsm,
        };

        IMSM_CTX_PTR(&ctx);
        in = IMSM_LIST_GET(struct echo_state, 2);
        imsm_list_push(in, IMSM_GET(&echo), 0);
        imsm_list_push(in, IMSM_GET(&echo), 0);

        in[0]->in_count = 1;
        in[1]->in_count = 2;

        for (size_t rep = 0; rep < 2; rep++) {
                if (rep > 0)
                        in = NULL;

                out = IMSM_STAGE("test", in, 0);
                for (size_t i = 0; i < imsm_list_size(out); i++)
                        printf("%zu %p %zu\n", i, out[i], out[i]->in_count);
        }

        return;
}

int
main()
{

        init();
        slab_get_put();
        slab_get_put_tight();
        slab_get_empty();
        ppoint();
        stage_io();
        return 0;
}
