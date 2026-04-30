# Zone Allocator in V8

The Zone allocator is a region-based memory manager used in V8 to support fast allocation of small chunks of memory. Chunks allocated in a Zone cannot be deallocated individually. Instead, the Zone supports deallocating all chunks in one fast operation.

This makes Zones ideal for holding temporary data structures that are needed only for a specific phase of execution, such as the Abstract Syntax Tree (AST) or compiler intermediate representations (IR), which can be discarded all at once after compilation is complete.

Notably, object pointers within a Zone can be relied on to stay live and available after allocation for as long as the Zone is live. This allows easier definition of complex data structures without having to worry about reachability, reference counting, or clean teardown of individual elements.

## How it Works

The Zone allocator works by requesting large segments of memory from the OS or a memory pool and then satisfying allocation requests by simply moving a pointer within the current segment.

### Segments

A `Zone` maintains a linked list of `Segment`s. When a `Zone` is created, it doesn't allocate any memory immediately. The first allocation triggers the allocation of a segment.
Segments are requested via the `AccountingAllocator`.

### Bump Pointer Allocation

Within a segment, allocation is extremely fast. It is a "bump pointer" allocation:
1. Check if the requested size fits in the remaining space of the current segment.
2. If it fits, return the current position and "bump" the position pointer forward by the requested size.
3. If it doesn't fit, expand the zone by allocating a new segment.

### Expansion Strategy

When the current segment is full, the Zone calls `Expand` to get a new segment.
*   The size of the new segment is typically double the size of the previous segment (high water mark strategy) to avoid excessive allocation overhead.
*   The size is bounded by a minimum (`kMinimumSegmentSize`, typically 8KB) and a maximum (`kMaximumSegmentSize`, typically 32KB).
*   New segments are prepended to the list of segments.

## Key Classes

*   **`Zone`**: The main class representing the allocator. It provides `Allocate`, `New`, and `AllocateArray` methods.
*   **`ZoneSnapshot`**: Stores the current allocation state of a Zone (position in the current segment and the list of segments).
*   **`ZoneScope`**: A RAII-style class that takes a snapshot of a Zone on creation and restores the Zone to that state on destruction, effectively freeing all memory allocated during the scope.
*   **`ZoneObject`**: A base class for objects that should be allocated in a Zone. It deletes the `delete` operator to prevent accidental individual deallocation.
    *   **CRITICAL NOTE**: Destructors of objects allocated in a Zone are **never** called when the Zone is discarded or reset. Objects allocated in a Zone must be robust against not being destructed (e.g., they must not own non-Zone resources like external heap memory, file handles, or standard containers like `std::vector` that manage their own memory).

## Zone Containers

To facilitate the use of Zones, V8 provides a set of containers that are specialized to use Zone allocation:
*   **`ZoneVector<T>`**: A custom vector implementation giving precise control over its implementation and performance.
*   **`ZoneMap`**, **`ZoneSet`**, **`ZoneDeque`**, etc.: Wrappers around standard C++ containers specialized with a `ZoneAllocator`.

These containers ensure that their internal nodes and backing stores are allocated within the Zone.

## Usage in the Codebase

The Zone allocator is used pervasively in the V8 compiler (`src/compiler/`).
*   **Compilation Phases**: Many compiler phases create a local Zone for their duration and discard it afterwards.
*   **Analysis Results**: Objects like `BytecodeAnalysis` are allocated in a Zone and contain Zone-based containers like `ZoneVector` and `ZoneMap` to store analysis data.
*   **AST**: The parser allocates AST nodes in a Zone, which is then passed to the compiler and finally discarded.

Note: The Zone implementation is inherently not thread-safe.
