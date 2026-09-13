# Handles in V8

Handles are the primary mechanism in V8 for referencing heap objects in C++ code in a way that is safe for garbage collection. Because V8's garbage collector moves objects during compaction, raw C++ pointers to heap objects would become invalid. Handles solve this by adding a layer of indirection or by collaborating with conservative stack scanning.

This document focuses on **internal** handles used within the V8 implementation. For external handles used by embedders, see the public API (e.g., `v8::Local` and `v8::Persistent`).

## Types of Handles

V8 provides two main types of internal handles: **Indirect Handles** (`Handle<T>` or explicitly `IndirectHandle<T>`) and **Direct Handles** (`DirectHandle<T>`).

### 1. Indirect Handles (`Handle<T>`)

Indirect handles are the traditional handle type in V8.
*   **Representation**: A `Handle<T>` is a pointer to a location (usually in a `HandleScope`) that contains the actual tagged pointer to the `HeapObject`.
*   **GC Safety**: When the GC moves an object, it updates the pointer in the `HandleScope` slot. All `Handle` instances pointing to that slot automatically see the new location.
*   **Usage**: Used widely across the codebase.

### 2. Direct Handles (`DirectHandle<T>`)

Direct handles are an optimization intended to reduce the overhead of indirection.
*   **Representation**:
    *   When `V8_ENABLE_DIRECT_HANDLE` is enabled: A `DirectHandle<T>` is a direct wrapper around a tagged pointer to a heap object or Smi. It does NOT use a slot in a `HandleScope`.
    *   When `V8_ENABLE_DIRECT_HANDLE` is NOT enabled: It is a wrapper around an `IndirectHandle<T>`, serving as a fallback. (Note: `DirectHandle<T>` is often used interchangeably with `Handle<T>` in API signatures that accept both).
*   **GC Safety**: When enabled, direct handles rely on **Conservative Stack Scanning (CSS)**. The GC scans the C++ stack to find potential pointers to heap objects. If a direct handle is on the stack, the GC will find it and update it if the object moves. DirectHandles are *only* valid thanks to CSS finding them on the stack.
*   **Usage**: They should only be used on the stack (enforced by C++ type traits in debug builds). They must not be stored in heap objects or global structures.

## Handle Scopes (`HandleScope`)

Local handles are managed in the context of a `HandleScope`.
*   **Indirect Handles**: A `HandleScope` accumulates slots for handles created within its scope. When the `HandleScope` is destroyed (typically at the end of a C++ function scope), all handles created within it become invalid because their slots are reclaimed. This prevents memory leaks from handles accumulating indefinitely.
*   **Direct Handles**: While direct handles do not use `HandleScope` slots, they are still conceptually local. They must only live on the stack and are valid as long as the object they point to is kept alive by some other means (e.g., an indirect handle in a scope) or if they are tracked by CSS.
*   **GC Interaction**: `HandleScope` instances are considered roots by the GC. The GC walks all active `HandleScope` slots during the main thread pause to update pointers to moved objects.

## Global and Eternal Handles

For objects that need to live beyond the scope of a function or a single turn, V8 provides global handles.

### Global Handles (`GlobalHandles`)
*   Independent of stack state.
*   Can be strong or weak (with callbacks/finalizers).
*   Used for embedding V8 in applications (e.g., keeping a reference to a JS object from C++).

### Eternal Handles (`EternalHandles`)
*   Handles that never go away.
*   Cheaper than global handles because they don't need to be tracked for destruction.

## Maybe Handles

V8 also provides `MaybeHandle<T>` and `MaybeDirectHandle<T>`.
*   These represent a handle that may be empty (null).
*   They force validation (checking for emptiness) before they can be converted to regular handles and used.
*   This is similar to `std::optional` or a nullable pointer, but specifically for handles.

## Migration to Direct Handles

V8 is in the process of migrating to `DirectHandle` where appropriate to improve performance.
*   Direct handles avoid the memory overhead of allocating slots in `HandleScope`.
*   They avoid the indirection cost when accessing the object.
*   **Default Status**: Handles are now by default direct handles if pointer compression is enabled (which it is by default on 64-bit platforms), and V8 is moving towards making them the only kind of handle, except where indirect handles are explicitly required.
*   **Rule**: Use `DirectHandle` for local variables on the stack. Code should be written to use `DirectHandle` where possible, as it will fallback to `Handle` if direct handles are not enabled.
