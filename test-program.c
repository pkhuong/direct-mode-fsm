#include <stdio.h>

#include "notification.h"
#include "program_point.h"
#include "slab.h"

struct foo {
        struct {
                struct slab_entry header;
        } test;
        int x;
};

static SLAB_HEAD(, struct foo) slab_head;
static char buf[1024];

/* void */
/* init(void) */
/* { */
/*         SLAB_INIT(slab_head, test.header, buf, sizeof(buf)); */
/*         SLAB_RELEASE(slab_head, SLAB_ALLOC(slab_head)); */
/*         SLAB_OBJECT_OF(slab_head, SLAB_ALLOC(slab_head), x); */
/* } */

NOTIFICATION(request, const char *data, size_t size);

static const struct request_notifier notifier;

static void
poll_loop(struct ppoint_state *pp_state, size_t n)
{
        PPOINT_CONTEXT(entry, 0);
        for (size_t i = 0; i < 10; i++)
                WITH_PPOINT_CONTEXT(loop_iter, i) {
                        for (size_t j = 0; j < n; j++)
                                printf("index: %zu\n",
                                       PPOINT_INDEX(pp_state, "test", 0));
                }

        printf("\n");
        return;
}

int
main()
{
        struct ppoint_state state = {
                .mode = PPOINT_STATE_MODE_RECORD,
        };

        poll_loop(&state, 1);
        struct ppoint_state copy = {
                .mode = PPOINT_STATE_MODE_COMPARE,
                .reference = &state,
        };
        poll_loop(&copy, 2);

        return 0;
}
