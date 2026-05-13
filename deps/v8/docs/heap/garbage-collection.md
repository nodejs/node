# Garbage Collection in V8

V8 uses a sophisticated garbage collection (GC) system to automatically manage memory. It is a generational collector, meaning it divides the heap into different generations based on object lifetime.

## Generational Hypothesis

The GC is based on the generational hypothesis: most objects die young. By dividing the heap into young and old generations, V8 can optimize GC pauses by collecting the young generation more frequently and quickly.

## The Young Generation

The Young Generation is where newly allocated objects are placed. It is typically small (a few megabytes to tens of megabytes) and is designed to be collected quickly.

### Scavenger (Minor GC)

The default collector for the young generation is the **Scavenger**. It uses a semi-space design:
1.  The young generation is divided into two halves: **To-Space** and **From-Space**.
2.  New objects are allocated in To-Space.
3.  When To-Space is full, a Scavenge cycle begins.
4.  At the start of the cycle, the roles of To-Space and From-Space are swapped. The space containing the new objects becomes **From-Space**, and the empty space becomes **To-Space**.
5.  The Scavenger traces live objects in From-Space and copies them to To-Space (or promotes them to the Old Generation if they have survived a previous cycle).

This is a copying collector, which compacts memory as it goes, leaving no fragmentation in the young generation.

### Minor Mark-Sweep (Minor MS) (Not enabled by default)

**Minor Mark-Sweep** is an alternative young generation collector in V8, enabled with the `--minor-ms` flag. Unlike the Scavenger, which is a copying collector, Minor MS uses a mark-sweep approach.

*   **Goal**: Avoid the overhead of copying objects and reduce memory fragmentation in certain workloads.
*   **Mechanism**:
    *   **Marking**: It marks objects reachable from roots and remembered sets (`OLD_TO_NEW`). It supports concurrent and parallel marking.
    *   **Non-Moving**: It generally does not move objects (unlike the Scavenger).
    *   **Page Promotion**: Instead of copying individual objects to the old generation, Minor MS can promote **entire pages** to the old generation if they have a high survival rate.
    *   **Sweeping**: Pages that are not promoted are swept to reclaim memory from dead objects. Sweeping can also be done concurrently.
*   **File**: `src/heap/minor-mark-sweep.cc`

## The Old Generation

Objects that survive a certain number of Scavenge cycles in the young generation are promoted (tenured) to the **Old Generation**. This space is much larger and contains long-lived objects.

### Mark-Sweep-Compact (Major GC)

The Old Generation is collected by the **Mark-Sweep-Compact** collector. This is a much more involved process:

1.  **Marking**: The collector traverses the object graph starting from roots (stack, global objects, etc.) and marks all reachable objects as live. V8 uses **Incremental Marking** and **Concurrent Marking** to do most of this work while the main JavaScript thread is running, minimizing pause times.
2.  **Sweeping**: The collector scans the heap and adds memory occupied by dead objects (unmarked) to free lists, making it available for future allocations. This can also be done concurrently.
3.  **Compaction**: Over time, the old generation can become fragmented. The collector may choose to compact certain pages by moving live objects together and updating pointers, to create large contiguous free spaces.

## Write Barriers

To support generational GC and incremental marking without scanning the entire heap, V8 uses **Write Barriers**. A write barrier is code emitted for heap stores like `host.field = value` to inform the GC about changes to the object graph.

The write barrier serves three main purposes (as detailed in `src/heap/WRITE_BARRIER.md`):
1.  **Generational Barrier (Old-to-New)**: Records references from the Old Generation to the Young Generation in a **Remembered Set** (`OLD_TO_NEW`). This allows the Scavenger to find live young objects without scanning the whole old generation. This barrier is always enabled.
2.  **Marking Barrier**: Prevents "black-to-white" references during incremental or concurrent marking. If a black (already traced) object is updated to point to a white (untraced) object, the marking barrier ensures the target object is marked or pushed to the worklist.
3.  **Evacuation Barrier (Old-to-Old)**: Records references between old objects when the target resides on an evacuation candidate page, to update pointers after compaction.

### Eliminating Write Barriers
Write barriers add overhead, so V8 eliminates them when safe:
*   **Smi or Read-Only**: Storing a Smi or a pointer to a read-only object never needs a barrier.
*   **Initializing Stores**: Storing into the most recently allocated young object (before any potential GC point) does not need a barrier.

## Concurrent and Parallel GC

To achieve low latency, V8 heavily utilizes background threads:
-   **Parallel**: The main thread and worker threads do GC work at the same time (e.g., Parallel Scavenge).
-   **Concurrent**: Worker threads do GC work while the main thread is running JavaScript (e.g., Concurrent Marking and Sweeping).

## See Also
-   [Pointer Tagging](pointer-tagging.md)
-   [Objects and Maps](objects-and-maps.md)
-   [Marking and Sweeping](marking-and-sweeping.md)
-   [Heap Compaction](compaction.md)
