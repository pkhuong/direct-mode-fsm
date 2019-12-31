#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "imsm.h"

#define ACCEPT_BUFFER 32

#define BUF_SIZE 200

enum io_result {
        IO_RESULT_DONE = 0,
        IO_RESULT_RETRY,
        IO_RESULT_ABORT
};

struct echo_state {
        struct imsm_entry header;
        int fd;
        uint8_t in_index;
        uint8_t newline_index;
        uint8_t out_index;
        char buf[BUF_SIZE];
};

static int accept_fd;
static int epoll_fd;

static IMSM(, struct echo_state) echo;

/*
 * Allow up to 128 concurrent echo state machines.
 */
static struct echo_state backing[128];

static void
echo_state_init(void *vstate)
{
        struct echo_state *state = vstate;

        state->fd = -1;
        state->in_index = 0;
        state->newline_index = 0;
        state->out_index = 0;
        return;
}

static void
echo_state_deinit(void *vstate)
{
        struct echo_state *state = vstate;

        if (state->fd >= 0)
                close(state->fd);

        echo_state_init(state);
        return;
}

/*
 * Attaches the accept fd to the epoll fd.
 */
static void
attach_accept_fd(void)
{
        struct epoll_event event = {
                .events = EPOLLIN,
                .data.u64 = 0,
        };
        int r;

        r = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, accept_fd, &event);
        if (r < 0) {
                perror("epoll_ctl");
                abort();
        }

        return;
}

/*
 * Registers `fd` with the epoll fd, without waiting on any event in
 * particular.
 */
static void
epoll_register(int fd)
{
        struct epoll_event event = { 0 };
        int r;

        r = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
        if (r < 0) {
                perror("epollctl");
                abort();
        }

        return;
}

/*
 * Arms `fd`'s state in the epoll fd to wait for exactly one
 * occurrence of `events`; the ref is attached as user data to let the
 * epoll handling loop wake the corresponding echo state machine.
 */
static void
epoll_arm(struct imsm_ref ref, int fd, uint32_t events)
{
        struct epoll_event event = {
                .events = events | EPOLLONESHOT,
                .data.u64 = ref.bits,
        };
        int r;

        r = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
        if (r < 0) {
                perror("epoll_ctl");
                abort();
        }

        return;
}

static void
handle_io_result(struct echo_state **done, struct imsm_ctx *ctx,
    struct echo_state *current, uint32_t events, enum io_result result)
{
        IMSM_CTX_PTR(ctx);

        switch (result) {
                case IO_RESULT_DONE:
                        imsm_list_push(done, current, 0);
                        break;

                case IO_RESULT_RETRY:
                        epoll_arm(IMSM_REFER(current), current->fd, events);
                        break;

                case IO_RESULT_ABORT:
                default:
                        IMSM_PUT(&echo, current);
                        break;
        }

        return;
}

#define echo_list_map(IN, EVENTS, VAR, EXPRESSION)                      \
        ({                                                              \
                struct echo_state *const *list_in_ = (IN);              \
                struct echo_state **list_out_ =                         \
                        IMSM_LIST_GET(struct echo_state,                \
                                      imsm_list_size(list_in_));        \
                                                                        \
                                                                        \
                imsm_list_foreach(VAR, list_in_)                        \
                        handle_io_result(list_out_, IMSM_CTX_PTR_VAR,   \
                            VAR, (EVENTS), (EXPRESSION));               \
                list_out_;                                              \
        })

/*
 * Accepts up to `batch_limit` new connections and returns them as an
 * imsm_list of echo states.
 */
static struct echo_state **
accept_new_connections(struct imsm_ctx *ctx, size_t batch_limit)
{
        struct echo_state **ret;
        IMSM_CTX_PTR(ctx);

        ret = IMSM_LIST_GET(struct echo_state, batch_limit);
        for (size_t i = 0; i < batch_limit; i++) {
                struct echo_state *state;
                int new_connection;
                bool success;

                state = IMSM_GET(&echo);
                if (state == NULL)
                        break;

                new_connection = accept4(accept_fd, NULL, NULL,
                    SOCK_NONBLOCK | SOCK_CLOEXEC);
                if (new_connection < 0) {
                        if (errno != EAGAIN && errno != EWOULDBLOCK)
                                perror("accept4");
                        IMSM_PUT(&echo, state);
                        break;
                }

                epoll_register(new_connection);
                state->fd = new_connection;
                state->in_index = 0;
                state->newline_index = 0;
                state->out_index = 0;
                success = imsm_list_push(ret, state, 0);
                assert(success && "imsm_list_push failed.");
        }

        return ret;
}

/*
 * Attempts to read a line (or 200 characters) for this echo state
 * machine.
 */
static enum io_result
read_one_line(struct echo_state *state)
{
        void *dst;
        char *newline;
        size_t to_read;
        ssize_t num_recv;

        if (state->in_index >= sizeof(state->buf)) {
                state->newline_index = sizeof(state->buf);
                return IO_RESULT_DONE;
        }

        dst = &state->buf[state->in_index];
        to_read = sizeof(state->buf) - state->in_index;
        num_recv = recv(state->fd, dst, to_read, MSG_DONTWAIT);
        if (num_recv < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                        return IO_RESULT_RETRY;

                perror("recv");
                return IO_RESULT_ABORT;
        }

        state->in_index += num_recv;
        newline = memchr(dst, '\n', num_recv);
        if (newline != NULL) {
                state->newline_index = newline - &state->buf[0];
                return IO_RESULT_DONE;
        }

        if (num_recv == 0 || state->in_index == sizeof(state->buf)) {
                state->newline_index = state->in_index;
                return IO_RESULT_DONE;
        }

        return IO_RESULT_RETRY;
}

/*
 * Reads data for echo state machines until their buffer size is
 * exhausted, or a newline character is found.
 *
 * `accepted` state machines are added to the set of echo state
 * machines waiting to read the first line.
 *
 * Returns a list of state machines that have fully read their
 * input line and are ready to spit it back to the peer.
 */
static struct echo_state **
read_first_line(struct imsm_ctx *ctx, struct echo_state **accepted)
{
        IMSM_CTX_PTR(ctx);

        IMSM_REGION("read_first_line");
        return echo_list_map(IMSM_STAGE("ready_to_read", accepted, 0),
            EPOLLIN | EPOLLRDHUP, current, read_one_line(current));
}

/*
 * Attemps to write (the rest of) the echo state machine's line to the
 * remote peer.
 */
static enum io_result
write_one_line(struct echo_state *state)
{
        void *buf = &state->buf[state->out_index];
        size_t to_write;
        ssize_t sent;

        if (state->out_index >= state->newline_index)
                return IO_RESULT_DONE;

        to_write = state->newline_index - state->out_index;
        sent = send(state->fd, buf, to_write, MSG_DONTWAIT | MSG_NOSIGNAL);
        if (sent < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                        return IO_RESULT_RETRY;

                perror("send");
                return IO_RESULT_ABORT;
        }

        if (sent == 0)
                return IO_RESULT_DONE;

        state->out_index += sent;
        if (state->out_index < state->newline_index)
                return IO_RESULT_RETRY;

        return IO_RESULT_DONE;
}

/*
 * Spits back the echo line to the remote for all echo state machines
 * that are ready to write.
 *
 * `fully_read` are added to the the list of state machines ready to
 * write.
 *
 * Returns a list of all state machines have fully written their echo
 * line.
 */
static struct echo_state **
echo_line(struct imsm_ctx *ctx, struct echo_state **fully_read)
{
        IMSM_CTX_PTR(ctx);

        IMSM_REGION("echo_line");
        return echo_list_map(IMSM_STAGE("ready_to_write", fully_read, 0),
            EPOLLOUT | EPOLLRDHUP, current, write_one_line(current));
}

static enum io_result
print_one_newline(struct echo_state *state)
{
        static const char buf[1] = "\n";
        ssize_t sent;

        sent = send(state->fd, buf, sizeof(buf), MSG_DONTWAIT | MSG_NOSIGNAL);
        if (sent < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                        return IO_RESULT_RETRY;

                perror("send");
                return IO_RESULT_ABORT;
        }

        return IO_RESULT_DONE;
}

static struct echo_state **
print_newline(struct imsm_ctx *ctx, struct echo_state **fully_written)
{
        IMSM_CTX_PTR(ctx);

        IMSM_REGION("print_newline");
        return echo_list_map(IMSM_STAGE("ready_to_newline", fully_written, 0),
            EPOLLOUT | EPOLLRDHUP, current, print_one_newline(current));
}

/*
 * Fully scans the echo state machine once.
 */
static void
echo_fn(struct imsm_ctx *ctx)
{
        struct echo_state **accepted, **fully_read, **echoed, **done;
        IMSM_CTX_PTR(ctx);

        accepted = accept_new_connections(ctx, ACCEPT_BUFFER);
        fully_read = read_first_line(ctx, accepted);
        echoed = echo_line(ctx, fully_read);
        done = print_newline(ctx, echoed);

        IMSM_PUT_N(&echo, done, imsm_list_size(done));
        return;
}

static void
run_echo_loop(void)
{
        struct imsm_ctx ctx = {
                .imsm = &echo.imsm,
        };

        for (;;) {
                struct epoll_event events[32];
                int r;

                r = epoll_wait(epoll_fd, events, sizeof(events) / sizeof(events[0]),
                               1000);
                if (r < 0 && errno != EINTR) {
                        perror("epoll");
                        abort();
                }

                if (r > 0) {
                        for (size_t i = 0, n = r; i < n; i++)
                                imsm_notify((struct imsm_ref){events[i].data.u64});
                }

                echo.imsm.poll_fn(&ctx);
                imsm_list_cache_recycle(&ctx.cache);
                ctx.position = (struct imsm_ppoint_record) { 0 };
        }

        imsm_list_cache_deinit(&ctx.cache);
        return;
}

static int
make_accept_fd(int port)
{
        struct sockaddr_in sock = { 0 };
        int one = 1;
        int fd;

        fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (fd < 0) {
                perror("socket");
                abort();
        }

        sock.sin_family = AF_INET;
        sock.sin_port = htons(port);
        sock.sin_addr.s_addr = htonl(INADDR_ANY);

        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
                perror("setsockopt");
                abort();
        }

        if (bind(fd, (struct sockaddr *)&sock, sizeof(sock)) < 0) {
                perror("bind");
                abort();
        }

        if (listen(fd, ACCEPT_BUFFER) < 0) {
                perror("listen");
                abort();
        }

        printf("Listening on port %i\n", port);
        return fd;
}

int
main(int argc, char **argv)
{

        if (argc < 2)
                return -1;

        accept_fd = make_accept_fd(atoi(argv[1]));
        epoll_fd = epoll_create1(EPOLL_CLOEXEC);
        if (epoll_fd < 0) {
                perror("epoll_create");
                abort();
        }

        attach_accept_fd();

        IMSM_INIT(&echo, header, backing, sizeof(backing),
            echo_state_init, echo_state_deinit, echo_fn);
        run_echo_loop();
        return 0;
}
