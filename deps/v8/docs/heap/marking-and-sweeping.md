# Marking and Sweeping in V8

This document provides a deep dive into the implementation of the Mark-Sweep-Compact garbage collection algorithm used in V8's Old Generation (Major GC).

## Overview

The Major GC in V8 follows three main phases:
1.  **Marking**: Identifying live objects.
2.  **Sweeping**: Reclaiming memory from dead objects.
3.  **Compaction**: Defragmenting the heap by moving live objects (optional, performed when needed).

## 1. Marking

V8 uses a bitmap-based marking approach combined with a worklist to achieve efficient and concurrent marking.

*   **Black (Processed)**: The object is live, and all its fields have been scanned. Its bit in the bitmap is `1`, and it is **not** in the worklist.

### Main Thread Marking and Weakness
While background threads do most of the marking, the main thread is responsible for:
1. **Marking Roots**: This includes the stack, global handles, and other roots that are not safe to access concurrently.
2. **Processing Weak References**: Objects pushed to the weak worklists (e.g., `WeakCell`) are processed at the end of marking on the main thread when the transitive closure is known.

### The Marking Bitmap
Defined in `src/heap/marking.h`, the `MarkingBitmap` is a sequence of cells containing bits. Each page in the heap has an associated marking bitmap.
*   Bits are mapped to tagged object addresses.
*   Atomic operations are used when setting bits concurrently.

### MarkingVisitor
The `MarkingVisitor` (defined in `src/heap/marking-visitor.h`) is responsible for traversing the object graph.
*   It pops grey objects from the worklist.
*   It iterates through the object's pointers (using `BodyDescriptor`).
*   For each pointed-to object, if it is not yet marked, the visitor marks it in the bitmap and pushes it to the worklist (making it grey).

### Concurrent Marking
To reduce pause times, V8 performs marking concurrently on background threads while the main thread continues to execute JavaScript.

#### Worklist and Work Stealing
*   **Segments**: The `MarkingWorklist` uses thread-local segments for pushing and popping objects to avoid synchronization overhead.
*   **Global Pool**: When a segment becomes full, it is published to a shared global pool. Other threads that run out of work can "steal" segments from this pool.

#### The Write Barrier
To support both generational GC and concurrent marking, V8 uses a **Combined Write Barrier**. Whenever a pointer is written to a heap object (`object.field = value`), the barrier checks:
1. **Generational/Shared Barrier**: If the `object` is in the old generation and the `value` is in the young generation (or shared heap), the slot is recorded in a remembered set.
2. **Marking Barrier**: If incremental marking is active, and the `value` is a white object (unmarked), it is atomically marked grey and pushed to the marking worklist.

*   *Optimization*: The fast path of the write barrier checks if the host object's page requires tracking pointers from it, and if the value object's page requires tracking pointers to it, avoiding deeper checks in most cases.

#### On-Hold Worklist
Concurrent marking introduces data races if a background thread reads an object while the main thread modifies it.
*   To avoid races with the mutator allocating in the Linear Allocation Area (LAB) of the new space, background threads **bail out** of visiting objects that are located in the current LAB.
*   These objects are pushed to an **On-Hold Worklist**.

### Forwarding Pointers and Compaction
During the compaction phase, live objects are moved to new locations.
*   To update pointers to moved objects, V8 leaves a **forwarding pointer** in the original object's map word.
*   The forwarding pointer encodes the new address of the object.
*   For more details on compaction, see [Compaction in V8](compaction.md).
*   The on-hold worklist is processed by the main thread or merged back to the shared worklist when it is safe (e.g., at a safepoint where LABs are flushed).
*   *Note*: The term "Bailout Worklist" is used in `cppgc` (V8's C++ garbage collector) for objects that cannot be traced concurrently, but in the V8 JS heap, it is referred to as the on-hold worklist.

#### Handling Object Layout Changes and Transitions
If the main thread changes an object's layout (e.g., transitioning a field from tagged to untagged) or transitions a string while a background thread is scanning it, it could cause a crash or inconsistent state.
*   For certain objects like strings that may transition concurrently, V8 uses **Object Locks** (see `ObjectLockGuard`) to ensure exclusive access during scanning by background threads.
*   For other complex transitions, V8 ensures synchronization through careful ordering of operations and memory barriers, rather than a generic bailout mechanism for all layout changes.

## 2. Sweeping

After marking completes, all objects with a `0` in the marking bitmap are considered dead.

The `Sweeper` (defined in `src/heap/sweeper.cc`):
*   Iterates through the pages of the Old Generation.
*   For each page, it identifies contiguous ranges of dead objects.
*   It fills these ranges with `Filler` objects (dead space) and adds them to the space's **Free List**.
*   Free lists are used by the allocator to find space for new objects.

Like marking, sweeping is heavily parallelized and can run concurrently with JavaScript execution.

## 3. Compaction (Evacuation)

Over time, the Old Generation can become fragmented. Sweeping leaves free spaces, but they might be too small for large objects.

To combat this, V8 may choose to compact certain pages during a Major GC:
1.  **Evacuation**: Live objects on a page selected for compaction are copied to a new page.
2.  **Pointer Updating**: All pointers pointing to the moved objects must be updated to point to the new location. This requires recording the locations of all pointers to the evacuated page (remembered sets are also used here).

Compaction is expensive because it involves copying objects and updating pointers, so V8 only performs it on heavily fragmented pages.

---

## See Also
-   [Garbage Collection](garbage-collection.md)
-   [Pointer Tagging](pointer-tagging.md)
