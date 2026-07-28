# Concurrency and Background Compilation

This document explains how V8 achieves high performance by offloading compilation and parsing to background threads, and the mechanisms used to keep this concurrent execution safe.

## Background Compilation

To keep the main thread (which runs JavaScript and handles UI interaction) responsive, V8 offloads as much work as possible to background threads.

### 1. Background Parsing and Bytecode Generation
V8 can parse JavaScript source code and generate Ignition bytecode on background threads. This means that by the time a script is actually executed, its bytecode may already be prepared.

### 2. Background Optimization
Both Maglev and TurboFan (including its Turboshaft pipeline) perform their heavy optimization work on background threads.
*   **Main Thread**: Identifies hot functions, creates a compilation job, and triggers the background thread.
*   **Background Thread**: Reads the IR, applies optimizations, and generates machine code.
*   **Main Thread**: Finally installs the generated code.

## Isolation: `LocalIsolate` and `LocalHeap`

Background threads cannot safely access the main `Isolate` or `Heap` without heavy locking, which would destroy performance. Instead, V8 provides "local" equivalents:

*   **`LocalIsolate`**: A thread-local view of the Isolate. It provides access to a `LocalHeap` and handles background-safe operations.
*   **`LocalHeap`**: A thread-local heap that allows the background thread to allocate objects (in its own Linear Allocation Areas) and manage handles without locking the main heap.

## Safety: `JSHeapBroker` and `HeapRefs`

A major challenge in background optimization is that the background thread needs to know about the structure of JavaScript objects (Maps, shapes, constants) to optimize effectively. However, reading these objects directly from the background thread is unsafe because:
1.  The Garbage Collector might move them.
2.  The main thread might modify them concurrently.

To solve this, V8 uses the **`JSHeapBroker`** and a system of **`HeapRefs`**:

1.  **`HeapRefs`**: Instead of using direct pointers (`Tagged<Object>`), optimizing compilers use **Refs** (e.g., `ObjectRef`, `MapRef`, `JSFunctionRef`).
2.  **Concurrent Reading and Serialization**: Modern V8 allows `HeapRefs` to read the live heap concurrently for most kinds of data, rather than relying on a full snapshot.
    *   **Serialization** is only used for things that are actually unsafe to read concurrently.
    *   Many objects are marked as **`NEVER_SERIALIZED`** and are always read live from the background thread.
    *   Some data structures might be read live but using different algorithms to ensure safety (e.g., searching descriptor arrays linearly instead of using a binary search, to avoid issues with concurrent modifications).

This approach ensures that the background compiler sees a safe view of the heap data while minimizing the overhead of snapshotting.

## Synchronization: Safepoints and Parking

Even with isolation, the Garbage Collector occasionally needs to stop all threads to perform a collection. V8 uses a **Safepoint** mechanism to coordinate this.

### Safepoints
Threads running JavaScript or accessing the heap must regularly check if a safepoint has been requested (by calling `Safepoint()`). When requested, they pause execution until the GC is complete.

### Parking
Background threads (like a compiler thread) might spend a long time doing pure computation without needing to access the heap.
*   **`Parked` State**: A thread can explicitly "park" itself. Instead of creating a `ParkedScope` directly (its constructors are private), threads should use `ExecuteWhileParked` (or similar methods on `LocalHeap`), which provides a `const ParkedScope&` witness to the callback. While parked, the thread promises *not* to access the heap.
*   **GC Independence**: The GC does not need to wait for parked threads to reach a safepoint. It can proceed immediately, ignoring them.
*   **Unparking**: If a parked thread needs to access the heap again, it must "unpark". If a GC is currently in progress, the unpark call will block until the GC finishes.

This allows long-running background tasks to proceed without delaying garbage collection.

---

## See Also
-   `src/heap/local-heap.h`: Definition of `LocalHeap` and parking states.
-   `src/heap/parked-scope.h`: Definition of `ParkedScope` and related guards.
-   `src/compiler/js-heap-broker.h`: The heap broker for background optimization.
-   [Tiering](../runtime/tiering.md): How V8 decides to optimize functions.
