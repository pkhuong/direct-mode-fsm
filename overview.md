Batched event loops, in direct style C
======================================

Immediate mode GUIs show us one way out of callback hell: instead of
representing the current state / continuation as a function in a
language like C, without support for that operation, keep running the
same code and pull events to the continuation point.

I've used that pattern for one-off state machines that did not require
high throughput, but had to be easy to audit: essentially, enter the
same function from the top, and look at the current state to determine
what the next step should be.

We can pull off a similar trick at higher throughput.  Straightforward
designs like microthreads, and even regular threads for that matter,
attempt to efficiently jump to the correct program location given a
state data structure (struct).  We'll instead go through each possible
program location--we are only making state machines easier to write--
and find the set of state structs that are associated with the current
location.  This approach might be impractical if we used it to
dispatch individual state structs.  However, by transposing the
execution order and only traversing the program states once for each
batch of state structs, we amortise the overhead into virtually
nothing, especially as the load, and thus the number of concurrently
executing state machines, grows.

Cool pitch, sell me more
------------------------

I'm trying to provide an alternative to threads and evented callback
hell for high concurrency / high throughput systems with soft latency
goals.  My reasoning is that, at low concurrency levels, threads are
probably the best option, and hard latency constraints should be
handled by real-time operating systems.

I see three discrete components in this alternative.

1. A slab allocator for versioned type stable storage, with
   cache-friendly allocation patterns.
2. Program point descriptors.
3. Signaling hooks.

The slab allocator helps improve performance when batching, if we make
sure to process allocated state structs in linear order.  More
importantly, it makes it possible to inspect all live instances of a
state type and dump their status.  Finally, the versioning and type
stability mean we can safely share references to other subsytems
(e.g., other state machines).  One of the hardest things about evented
systems is cancellation: any stale "closure" reference can lead to
mysterious use-after-free bugs.  Versioning lets us detect
cancellation after the fact, and type stability gives us more
lock-free options to do so efficiently.

Program point descriptors are the only novel (for event-driven
systems) idea.  At first, I'd think of them as simple `static`
allocated data structures; we can push a list of state struct to a
program point descriptor, and extract a list of "potentially active"
state struct from each program point descriptor.

Pushing to a program point is equivalent to transitioning to a new
state machine state. "Potentially active" captures the idea of change
triggering with spurious wake-ups.  The first time a state struct
enters a program point, it is potentially active.  After that, it can
only be activeted externally, through signaling hooks.

Batching operations in lists encourages programmers to think in term
of discrete phases, each with specific resource requirements, like
SEDA queues.

We also want to let multiple worker threads operate on the same set of
program point descriptors (same state machine), with some
specialisation.  For example, a program point('s continuation) could
be annotated as CPU intensive, and another as IO-bound, with different
threads assigned to each type of workload.

Signaling hooks are the only way to kick existing state structs
forward.  They're a pair of a callback and a typed / versioned
reference.  The callback runs on arbitrary threads, and receives a
locked mutable reference to the state struct, along with the caller's
arguments; the callback is responsible for updating the state struct
with the arguments, and determining whether the state should be woken
up and marked as active.

The driver loop
---------------

One of the interesting ideas about imgui is that we can re-execute the
same code to get to the same program points.  We'll do the same here:
each state machine has a `void (*)(void)` entry point, and any number
of worker threads can execute that function to start pulling work from
program points.

The driver loop must determine when some state might have been woken
up.  At first, we can simply poll, maybe with exponential backoff when
we fail to find any work to do.  Less naively, I'd have a global
change counter to track the number of time state structs have been
woken up, and even shard it into multiple counters by workload type.
The driver loop could snapshot the counter values before entering the
entry point function, and wait until they changed.

We also need a way to allocate new state structs (terminating one is
simple deallocation, especially since we can get away with lazy
cancellation); more importantly, we need a way to determine when to do
so.  We can piggyback on the same global change counter system, but
now also count changes that are not associated with any state struct
in particular.  That'll trigger a re-execution of the entry point,
which will be able to `accept(2)` new connections, grab work from an
in-memory queue, etc., and allocate new state structs.

We also want the ability to run multiple driver loops in the same
thread.  The best way to multiplex the wake-ups might be... `eventfd`
:\ We'll have to think about it, but I don't think it impacts the
programming model.

Nested state machines
---------------------

Imgui has the ability to modularise UI components (with, e.g., loops
and function calls) by associating elements with a name instead of a
specific program point.  I find that a bit error-prone, but we can
achieve comparable flexibility by tracking an abstract view of the
call stack context in thread-local storage (dynamic scope).
Programmers will explicitly opt into state tracking, and must
guarantee that the same states will always be visited
deterministically by the entry point function.  The stack of contexts
handles function calls (and even recursion); we can augment stack
entries with a single integer counter to also capture loop iterations.

The hard part will be to efficiently map the context stack to sets of
waiting and active state structs.  I suggest we construct an
append-only trie, without efficient random access: we already ask the
programmer to make sure traversal is deterministic, so we only need to
detect variance in traversal order.

Parallel state machines
-----------------------

There is no special support for parallelism or concurrency within a
state machine.  Programmers should spawn new state machines and add a
signaling hook for their completion.  We will help track dependencies
by pointing each state machine struct to its parent, which will also
help detect indirect cancellation, and dump the full state of the
system.  The programmer is responsible for tracking the mapping from
parent to children.

What it'd look like
-------------------

Let's say we have a subsystem where work units come through an
in-memory queue, followed by an HTTP request, and a CPU-bound phase.
The entry point function could look like:

    void
    entry_point(void)
    {
        /* List allocations are implicitly freed on exit. */
        struct foo_state_list entry_list;
        struct foo_state_list http_done_list;

        {
            size_t n = queue_available(&entry_queue);
            
            foo_state_list_init(&entry_list, n);
            for (size_t i = 0; i < n; i++) {
                struct queue_entry entry;

                queue_pop(&entry_queue, &entry);
                /* Populate `entry_list.values[i]`. */
            }
        }

        /*
         * The string does not matter; we have file / lineno / counter.
         * The second argument is the iteration identifier, which could
         * default to zero if missing...
         */
        WITH_CONTEXT("initial_http_call", 0) {
            struct http_state_list http_list;
            
            /* Consumes `entry_list`. */
            PROJECT_LIST(&http_list, &entry_list, .http_state);
            /*
             * This FSM subroutine consumes `http_list` and
             * may be called from multiple contexts.
             */
            http_list = make_http_calls(&http_list);
            UNPROJECT_LIST(&http_done_list, &http_list, .http_state);
        }
        
        for (size_t i = 0, n = http_done_list.size; i < n; i++) {
            double result;
            
            /* Compute CPU-bound result for `http_done_list.values[i]`. */
            COMPLETE(http_done_list.values[i].done, result);
        }
    }

Inefficiency not worth optimising
---------------------------------

Nested contexts also give us an easy way to skip scanning for states
that we know have no work to do.  We could try to quickly scan the
data structure for wake-ups, and not execute a `WITH_CONTEXT` block
when empty.  However, that introduces more variation in execution
path, so maybe not; just use a child state machine if that matters.
