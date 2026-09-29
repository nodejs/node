# Pointer Tagging in V8

V8 uses pointer tagging to efficiently represent different types of values in a single word (pointer-sized). This avoids the overhead of separate type tags for every value, which is especially important for small integers (Smis) and heap object references.

## The Tagging Scheme

V8 uses the least significant bits of a pointer-sized word to encode the type of the value.

### Small Integers (Smi)

Small integers are encoded directly in the pointer word.

-   **Tag**: The least significant bit is `0`.
-   **Value**: The remaining bits represent the integer value (shifted).

#### Smi Representation Details

The exact representation of Smis depends on the architecture and whether pointer compression is enabled:

-   **32-bit architectures and 64-bit architectures with pointer compression**:
    -   Smis are 31-bit signed integers.
    -   They are shifted left by 1 bit.
    -   The representation is `[31 bits value][0]`.
-   **64-bit architectures without pointer compression**:
    -   Smis are 32-bit signed integers.
    -   They are shifted left by 32 bits.
    -   The representation is `[32 bits value][32 bits zeros]`.
    -   This allows for fast operations on 64-bit systems where the upper 32 bits can be treated as a 32-bit integer directly in some instructions. Specifically, when loading the value from a field, we can increase the load offset by 4 bytes to load just the 32-bit integer payload directly.
    -   *Note*: This was the default representation on x64 until recently, and is still used if pointer compression is disabled.

### Heap Objects

References to objects on the heap are tagged with a non-zero value in the least significant bits.

-   **Strong Reference**: The last two bits are `01`.
-   **Weak Reference**: The last two bits are `11`.
    *   *Note on Weakness*: The distinction between strong and weak references (bit 1 being 0 vs 1) only exists in the slot on the heap. When a weak pointer is loaded into a register to be used as a normal pointer, the weak bit is typically cleared.
-   **Constants**:
    -   `kHeapObjectTag = 1`
    -   `kWeakHeapObjectTag = 3`
    -   `kHeapObjectTagMask = 3`

### Compressed Pointers

When pointer compression is enabled (the default on 64-bit platforms), pointers are stored as 32-bit values. These compressed pointers are relative to the **isolate's cage base address** (which is often stored in a dedicated register, e.g., `r13` on x64). To decode a compressed pointer to a full 64-bit address, V8 adds it to the cage base.

Compressed pointers are represented in C++ with the `Tagged_t` type. `Tagged_t` is `uint32_t` when pointer compression is enabled, and `Address` (system-pointer-sized) otherwise.

#### Forwarding Pointers (GC)

During garbage collection, V8 uses the least significant bits `00` in the object header (MapWord) to indicate a forwarding pointer to the new location of the object. While this looks like a Smi (LSB is 0), it is only valid in the context of the object header during GC.

## C++ Representation: `Tagged<T>`

V8 C++ code uses the `Tagged<T>` template class (defined in `src/objects/tagged.h`) to represent tagged pointers. This provides type safety and encapsulates the tagging operations.

-   `Tagged<Object>`: Can represent any tagged value (Smi or HeapObject).
-   `Tagged<Smi>`: Represents a small integer.
-   `Tagged<HeapObject>`: Represents a pointer to any object on the heap.
-   `Tagged<MyType>`: Represents a pointer to a specific heap object type (e.g., `Tagged<JSArray>`).

The `Tagged` class handles untagging automatically when dereferencing (e.g., via `operator->`) or when accessing the raw address via `address()`.

## Address Calculation

To get the actual memory address of a heap object from a tagged pointer, the tag must be subtracted:

```cpp
Address address() const { return this->ptr() - kHeapObjectTag; }
```

Conversely, to create a tagged pointer from an address, the tag is added:

```cpp
Tagged(reinterpret_cast<Address>(ptr) + kHeapObjectTag)
```

For weak pointers, the weak bit is bitwise-ORed:

```cpp
return Tagged<WeakOf<T>>(value.ptr() | kWeakHeapObjectTag);
```

## See Also
-   [Pointer Compression](pointer-compression.md) for how pointer tagging interacts with pointer compression.
