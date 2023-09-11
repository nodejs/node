# Oilpan: C++ Garbage Collection

Oilpan is an open-source garbage collection library for C++ that can be used stand-alone or in collaboration with V8's JavaScript garbage collector.
Oilpan implements mark-and-sweep garbage collection (GC) with limited compaction (for a subset of objects).

**Key properties**

- Trace-based garbage collection;
- Incremental and concurrent marking;
- Incremental and concurrent sweeping;
- Precise on-heap memory layout;
- Conservative on-stack memory layout;
- Allows for collection with and without considering stack;
- Non-incremental and non-concurrent compaction for selected spaces;

See the [Hello World](https://chromium.googlesource.com/v8/v8/+/main/samples/cppgc/hello-world.cc) example on how to get started using Oilpan to manage C++ code.

Oilpan follows V8's project organization, see e.g. on how we accept [contributions](https://v8.dev/docs/contribute) and [provide a stable API](https://v8.dev/docs/api).

## Threading model

Oilpan features thread-local garbage collection and assumes heaps are not shared among threads.
In other words, objects are accessed and ultimately reclaimed by the garbage collector on the same thread that allocates them.
This allows Oilpan to run garbage collection in parallel with mutators running in other threads.

References to objects belonging to another thread's heap are modeled using cross-thread roots.
This is even true for on-heap to on-heap references.

Oilpan heaps may generally not be accessed from different threads unless otherwise noted.

## Heap partitioning

Oilpan's heaps are partitioned into spaces.
The space for an object is chosen depending on a number of criteria, e.g.:

- Objects over 64KiB are allocated in a large object space
- Objects can be assigned to a dedicated custom space.
  Custom spaces can also be marked as compactable.
- Other objects are allocated in one of the normal page spaces bucketed depending on their size.

## Precise and conservative garbage collection

Oilpan supports two kinds of GCs:

1. **Conservative GC.**
A GC is called conservative when it is executed while the regular native stack is not empty.
In this case, the native stack might contain references to objects in Oilpan's heap, which should be kept alive.
The GC scans the native stack and treats the pointers discovered via the native stack as part of the root set.
This kind of GC is considered imprecise because values on stack other than references may accidentally appear as references to on-heap object, which means these objects will be kept alive despite being in practice unreachable from the application as an actual reference.

2. **Precise GC.**
A precise GC is triggered at the end of an event loop, which is controlled by an embedder via a platform.
At this point, it is guaranteed that there are no on-stack references pointing to Oilpan's heap.
This means there is no risk of confusing other value types with references.
Oilpan has precise knowledge of on-heap object layouts, and so it knows exactly where pointers lie in memory.
Oilpan can just start marking from the regular root set and collect all garbage precisely.

## Atomic, incremental and concurrent garbage collection

Oilpan has three modes of operation:

1. **Atomic GC.**
The entire GC cycle, including all its phases (e.g. see [Marking](#Marking-phase) and [Sweeping](#Sweeping-phase)), are executed back to back in a single pause.
This mode of operation is also known as Stop-The-World (STW) garbage collection.
It results in the most jank (due to a single long pause), but is overall the most efficient (e.g. no need for write barriers).

2. **Incremental GC.**
Garbage collection work is split up into multiple steps which are interleaved with the mutator, i.e. user code chunked into tasks.
Each step is a small chunk of work that is executed either as dedicated tasks between mutator tasks or, as needed, during mutator tasks.
Using incremental GC introduces the need for write barriers that record changes to the object graph so that a consistent state is observed and no objects are accidentally considered dead and reclaimed.
The incremental steps are followed by a smaller atomic pause to finalize garbage collection.
The smaller pause times, due to smaller chunks of work, helps with reducing jank.

3. **Concurrent GC.**
This is the most common type of GC.
It builds on top of incremental GC and offloads much of the garbage collection work away from the mutator thread and on to background threads.
Using concurrent GC allows the mutator thread to spend less time on GC and more on the actual mutator.

## Marking phase

The marking phase consists of the following steps:

1. Mark all objects in the root set.

2. Mark all objects transitively reachable from the root set by calling `Trace()` methods defined on each object.

3. Clear out all weak handles to unreachable objects and run weak callbacks.

The marking phase can be executed atomically in a stop-the-world manner, in which all 3 steps are executed one after the other.

Alternatively, it can also be executed incrementally/concurrently.
With incremental/concurrent marking, step 1 is executed in a short pause after which the mutator regains control.
Step 2 is repeatedly executed in an interleaved manner with the mutator.
When the GC is ready to finalize, i.e. step 2 is (almost) finished, another short pause is triggered in which step 2 is finished and step 3 is performed.

To prevent a user-after-free (UAF) issues it is required for Oilpan to know about all edges in the object graph.
This means that all pointers except on-stack pointers must be wrapped with Oilpan's handles (i.e., Persistent<>, Member<>, WeakMember<>).
Raw pointers to on-heap objects create an edge that Oilpan cannot observe and cause UAF issues
Thus, raw pointers shall not be used to reference on-heap objects (except for raw pointers on native stacks).

## Sweeping phase

The sweeping phase consists of the following steps:

1. Invoke pre-finalizers.
At this point, no destructors have been invoked and no memory has been reclaimed.
Pre-finalizers are allowed to access any other on-heap objects, even those that may get destructed.

2. Sweeping invokes destructors of the dead (unreachable) objects and reclaims memory to be reused by future allocations.

Assumptions should not be made about the order and the timing of their execution.
There is no guarantee on the order in which the destructors are invoked.
That's why destructors must not access any other on-heap objects (which might have already been destructed).
If some destructor unavoidably needs to access other on-heap objects, it will have to be converted to a pre-finalizer.
The pre-finalizer is allowed to access other on-heap objects.

The mutator is resumed before all destructors have ran.
For example, imagine a case where X is a client of Y, and Y holds a list of clients.
If the code relies on X's destructor removing X from the list, there is a risk that Y iterates the list and calls some method of X which may touch other on-heap objects.
This causes a use-after-free.
Care must be taken to make sure that X is explicitly removed from the list before the mutator resumes its execution in a way that doesn't rely on X's destructor (e.g. a pre-finalizer).

Similar to marking, sweeping can be executed in either an atomic stop-the-world manner or incrementally/concurrently.
With incremental/concurrent sweeping, step 2 is interleaved with mutator.
Incremental/concurrent sweeping can be atomically finalized in case it is needed to trigger another GC cycle.
Even with concurrent sweeping, destructors are guaranteed to run on the thread the object has been allocated on to preserve C++ semantics.

Notes:

* Weak processing runs only when the holder object of the WeakMember outlives the pointed object.
If the holder object and the pointed object die at the same time, weak processing doesn't run.
It is wrong to write code assuming that the weak processing always runs.

* Pre-finalizers are heavy because the thread needs to scan all pre-finalizers at each sweeping phase to determine which pre-finalizers should be invoked (the thread needs to invoke pre-finalizers of dead objects).
Adding pre-finalizers to frequently created objects should be avoided.
