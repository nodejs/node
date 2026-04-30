# Roots in V8

Roots are a set of well-known objects that are always reachable by the garbage collector. They serve as the entry points for heap traversal during GC and provide fast access to frequently used objects and constants.

## Categories of Roots

V8 categorizes roots into several lists, defined primarily in `src/roots/roots.h` using X-macros:

1.  **Read-Only Roots**: These are objects that are allocated during isolate initialization and never change. They are placed in the `ReadOnlySpace`.
    *   **Constants**: `undefined`, `null`, `true`, `false`, `the_hole_value`.
        *   *Note on Hole Values*: V8 uses multiple specific "hole" values (e.g., `the_hole_value`, `hash_table_hole_value`, `uninitialized_value`) as sentinels in different places. Their bodies are often deliberately unmapped or placed in protected memory so that attempting to read them (for example, as part of a type confusion exploit) will cause a crash, preventing security vulnerabilities.
    *   **Maps**: Maps for basic types like `ByteArrayMap`, `FixedArrayMap`, `SymbolMap`, etc.
    *   **Empty Collections**: `EmptyFixedArray`, `EmptyPropertyArray`, etc.
2.  **Mutable Immovable Roots**: These objects can be modified but their location in memory is fixed (immortal and immovable).
    *   **Protectors**: Objects used to invalidate optimizations when certain assumptions change (e.g., `ArraySpeciesProtector`).
    *   **Caches**: Caches for string splitting, regexps, etc.
3.  **Mutable Movable Roots**: These are roots that can be modified and moved by the GC.
    *   **Public Symbol Table**: The table of registered symbols.
    *   **Script List**: List of all scripts in the isolate.
4.  **Smi Roots**: Small integers stored directly in the roots table for convenience, such as the last script ID.

## RootsTable and ReadOnlyRoots

*   **`RootsTable`**: This class (defined in `src/roots/roots.h`) encapsulates the array of all roots. It provides accessors to get handles to specific roots.
*   **`ReadOnlyRoots`**: A lighter-weight class that provides access *only* to the read-only roots. It can be used even when the full `Isolate` is not available (e.g., on background threads using `LocalIsolate`) because read-only roots are shared or at least stable.

## Static Roots Optimization

When `v8_enable_static_roots = true` is enabled (supported under pointer compression, since then read-only pointers are cage-relative), V8 uses a fixed layout for the read-only heap to achieve compile-time constant addresses for core objects.

### How it Works
*   **Cage Placement**: The read-only heap is placed at the very beginning of every Pointer Compression Cage. This ensures that the 32-bit compressed offset of any read-only object is constant across all isolates in a process.
*   **Deterministic Layout**: The `mksnapshot` tool deterministically creates a bit-identical read-only heap.
*   **Two-Stage Build (Conceptual)**: To use these addresses in C++ code (`libv8`), V8 needs to know them at compile time.
    1.  `mksnapshot` (or specialized generator tools) is run to generate the addresses and write them to configuration-specific headers (e.g., `src/roots/static-roots-intl-wasm.h`), which are included by `src/roots/static-roots.h` based on build flags.
    2.  The main V8 binary is compiled, using these hardcoded offsets (e.g., `StaticReadOnlyRoot::kUndefinedValue`).
    *   *Note*: In practice, these generated headers are checked into the repository and only validated as part of normal compilation. If the read-only heap layout changes, the headers have to be regenerated with the `tools/dev/gen-static-roots.py` script.

### Benefits
*   **Faster Checks**: Instead of loading the address of `undefined` from the roots table, V8 can directly compare the compressed pointer to a constant (e.g., checking if it equals `StaticReadOnlyRoot::kUndefinedValue`).
*   **Range Checks**: Common objects are grouped together. For example, all string maps are adjacent, allowing V8 to check if an object is a string by verifying if its map's address falls within a specific range.
*   **Smaller Code**: Reduces the size of generated machine code by avoiding indirection through the roots table.

## See Also
-   [Objects and Maps](objects-and-maps.md)
-   [Garbage Collection](garbage-collection.md)
