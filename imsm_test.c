#include <assert.h>
#include <stddef.h>

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
        struct echo_state *state;

        state = IMSM_GET(&echo);
        assert(state->in_count == 0);
        return;
}

int
main()
{

        init();
        slab_get_put();
        return 0;
}
