# Pointer Compression in V8

Pointer compression is a technique used in V8 to reduce the memory footprint of the heap on 64-bit architectures. It achieves this by representing 64-bit pointers as 32-bit offsets from a base address, effectively cutting the size of pointer fields in half.

## How it Works

When pointer compression is enabled, V8 allocates a large contiguous region of virtual memory called a **Cage** (typically 4GB).

### Cage Types

V8 supports two main configurations for the pointer compression cage:
*   **Isolate Cage**: Each isolate has its own 4GB cage. The base address is unique per isolate.
*   **Shared Cage (Process-wide)**: A single cage is shared across all isolates in a process. This is required for the **V8 Sandbox** (where the cage effectively serves as the sandbox boundary). This mode simplifies sharing of read-only objects and improves performance by avoiding cage base reloads.

### Compression

To compress a 64-bit pointer to a 32-bit value:
1.  The pointer is verified to be within the designated cage.
2.  The pointer is truncated to its lower 32 bits.

```cpp
// Conceptually
Tagged_t CompressObject(Address tagged) {
  return static_cast<Tagged_t>(tagged);
}
```

### Decompression

To decompress a 32-bit value back to a full 64-bit pointer:
1.  The 32-bit value is zero-extended to 64 bits.
2.  The zero-extended value is added to the 64-bit **Cage Base** address.

*Note on Smis*: In their uncompressed representation on 64-bit architectures with pointer compression, Smis explicitly have their top 32 bits undefined (neither guaranteed to be zero nor sign-extended). Operations on Smis *must* truncate to the lower 32 bits and ignore these top bits. This allows decompression to be identical whether the tagged pointer is a Smi or a HeapObject reference (since both just need the lower 32 bits added to the cage base).

```cpp
// Conceptually
Address DecompressTagged(Tagged_t raw_value) {
  Address cage_base = base(); // Get the current cage base
  return cage_base + static_cast<Address>(raw_value);
}
```

## Cages

V8 uses different cages for different types of objects to optimize memory layout and security.

### Main Cage
Used for most JavaScript objects and heap metadata. The base address is typically stored in a dedicated register (`r14` on x64, `x28` on arm64) for fast access during execution. In C++ code, it is accessed via a global variable (if shared cage is enabled) or thread-local storage.

### Trusted Cage
When the V8 Sandbox is enabled, trusted objects are placed in a separate trusted cage outside the sandbox to prevent them from being corrupted by untrusted code.

-   **Trusted Objects**: These are objects that are trusted to not have been modified in a malicious way. Examples include `BytecodeArray` and `Code`.
-   **Exposed Trusted Objects**: Some trusted objects need to be referenced from untrusted objects inside the sandbox. These inherit from `ExposedTrustedObject` and are referenced via **indirect pointers** (indices into a pointer table) to guarantee memory safety. Both `Code` and `BytecodeArray` are exposed trusted objects.
-   **Protected Pointers**: Pointers between objects within the trusted cage can be direct compressed pointers, as they cannot be modified by an attacker in the sandbox.

### External Code Space
When `V8_EXTERNAL_CODE_SPACE` is enabled, the executable instructions of code objects are placed in a separate cage, specifically in `InstructionStream` objects. This allows the code range to be allocated closer to the executable text section of the process, which is sometimes required by operating systems or for performance (shorter jump distances).

The `Code` object itself (which contains metadata) lives in the Trusted Cage (or Main Cage if sandbox is disabled), while the `InstructionStream` object (which contains the actual machine code) lives in the External Code Space.

This cage has a slightly more complex decompression scheme to allow it to cross 4GB boundaries. Instead of just adding the 32-bit truncated value to the cage base, it computes the difference between the truncated value and the truncated cage base. If the difference is negative, it adds 4GB to the result. This ensures that the decompressed address is always within the correct 4GB window relative to the cage base.

```cpp
// Conceptually
Address DecompressExternalCode(Tagged_t raw_value) {
  Address cage_base = ExternalCodeCompressionScheme::base();
  uint32_t compressed_value = static_cast<uint32_t>(raw_value);
  uint32_t compressed_base = static_cast<uint32_t>(cage_base);
  
  intptr_t diff = static_cast<intptr_t>(compressed_value) - static_cast<intptr_t>(compressed_base);
  if (diff < 0) {
    diff += 4LL * 1024 * 1024 * 1024; // Add 4GB
  }
  return cage_base + diff;
}
```

## Interaction with Pointer Tagging

Pointer compression works orthogonally to pointer tagging. The 32-bit compressed value still contains the tag bits in its least significant bits:
-   **Smi**: Least significant bit is `0`.
-   **HeapObject**: Least significant bit is `1`.

Decompression preserves these tags because adding the cage base (which is aligned to the cage size, typically at least 4GB, so its lower 32 bits are zero) does not affect the lower bits where the tags reside.

## See Also
-   [Pointer Tagging](pointer-tagging.md) for details on pointer tagging.
