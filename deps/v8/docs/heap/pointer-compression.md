# Pointer Compression in V8

Pointer compression (`V8_COMPRESS_POINTERS`) reduces the size of tagged pointers on 64-bit architectures from 64 bits (`8` bytes) to 32 bits (`4` bytes, `Tagged_t`). Because tagged fields make up the majority of V8 heap memory, pointer compression saves roughly 30% to 40% of total heap memory and significantly improves CPU cache utilization and memory bandwidth.

---

## 1. Core Concept: 4GB Cage Alignment

On a 64-bit architecture, pointer compression reserves a contiguous **4GB (`2^32` bytes) virtual address cage** aligned to a 4GB boundary (`kPtrComprCageBaseAlignment = 4GB`):

```
64-bit Full Address:
+----------------------------------+----------------------------------+
|    Upper 32 bits (Cage Base)     |   Lower 32 bits (Tagged_t)       |
+----------------------------------+----------------------------------+
|<--- Identical for all objects -->|<--- Stored in heap fields ------>|
      in the same 4GB cage
```

Because the cage base is aligned to `4GB`, the lower 32 bits of the cage base address are all `0`.
* **Compression (`CompressObject`)**: Truncates a 64-bit `Address` to its lower 32 bits (`static_cast<Tagged_t>(address)`).
* **Decompression (`DecompressTagged`)**: Zero-extends the 32-bit `Tagged_t` to 64 bits and adds the 64-bit cage base (`base + static_cast<Address>(static_cast<uint32_t>(raw_value))`).
  * For **Smis** (which are 31-bit signed integers with LSB `0` under pointer compression), decompression either sign-extends the 32-bit Smi or leaves the upper 32 bits as the cage base when the Smi is only inspected as a 32-bit value.

### Dedicated Cage Base Registers
In JIT-compiled code and builtins, the main pointer compression cage base is kept permanently in a dedicated register (`kPtrComprCageBaseRegister`) so decompression requires no memory loads:
* **x64 (`src/codegen/x64/register-x64.h`)**: `r14` (`kPtrComprCageBaseRegister`), alongside `r13` (`kRootRegister`).
* **ARM64 (`src/codegen/arm64/register-arm64.h`)**: `x28` (`kPtrComprCageBaseRegister`), alongside `x26` (`kRootRegister`).

On x64, decompressing and loading a field from a compressed pointer can be folded into a single indexed addressing mode: `movl rax, [r14 + rbx*1 + offset - 1]`.

---

## 2. Multiple Compression Schemes (`src/common/ptr-compr.h`)

Modern V8 uses separate pointer compression schemes for different memory regions to enforce [V8 Sandbox](../sandbox/architecture.md) isolation and support executable code ranges:

### 1. `V8HeapCompressionScheme` (Main Cage)
* Used for all standard in-sandbox heap spaces (`NewSpace`, `OldSpace`, `SharedSpace`, `ReadOnlySpace`).
* When `V8_ENABLE_SANDBOX` is enabled, the 4GB main pointer compression cage is placed at the start of the 1TB V8 Sandbox reservation (`IsolateGroup::GetPtrComprCageBase()`).
* Standard `TaggedMember<T>` fields use `V8HeapCompressionScheme` by default.

### 2. `TrustedSpaceCompressionScheme` (`TrustedCage`)
* When the V8 Sandbox is enabled (`V8_ENABLE_SANDBOX`), security-sensitive V8 metadata (`TrustedObject`s such as `BytecodeArray`, `Code`, `TrustedFixedArray`, `ProtectedFixedArray`, and Wasm internal structures) must reside **outside** the sandbox so an attacker with arbitrary in-sandbox write cannot corrupt them.
* These objects are allocated in `TrustedSpace` and `SharedTrustedSpace` inside a separate 4GB virtual memory reservation called the **`TrustedCage`** (`IsolateGroup::GetTrustedPtrComprCage()`).
* Pointers between trusted objects (`ProtectedTaggedMember<T>`, stored in `TrustedObject`s) are 32-bit compressed offsets compressed and decompressed using `TrustedSpaceCompressionScheme::base()`. Because both the pointer and the target live in `TrustedCage`, they cannot be forged from inside the sandbox.

### 3. `ExternalCodeCompressionScheme` (`V8_EXTERNAL_CODE_SPACE`)
Executable machine code (`InstructionStream` objects in `CodeSpace` and `CodeLargeObjectSpace`) is often allocated in a separate `CodeRange` outside the main 4GB data cage (or near the binary's `.text` section to allow fast PC-relative `call`/`jmp` instructions).

Unlike the data cage and trusted cage, **the external code range does not need to be 4GB-aligned**. Instead, `ExternalCodeCompressionScheme` only requires that the code range does not cross a 4GB boundary (`kMinExpectedOSPageSize` alignment):

1. **`PrepareCageBaseAddress(code_range_base)`**: Rounds `code_range_base` **down** to the nearest 4GB boundary:
   ```cpp
   base = RoundDown<kPtrComprCageBaseAlignment>(code_range_base);
   ```
2. **Constraint**: As long as the entire `CodeRange` (typically 128MB to 512MB) fits inside a single `[N * 4GB, (N + 1) * 4GB)` window, every `InstructionStream` in that range shares the same upper 32 bits (`base`).
3. **Smi Disambiguation**: If `code_range_base` happens to land directly on a 4GB boundary (`Lower32(code_range_base) == 0`), an offset of `0` with bit 0 = `0` could collide with `Smi::zero()`. To ensure `DecompressTagged` can still distinguish Smis from `InstructionStream` pointers, V8 reserves a small redzone at the start of the 4GB window so valid `InstructionStream` offsets are never zero.

---

## 3. Contiguous Compressed Read-Only Space (`CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL`)

Frequently, V8 and the garbage collector need to check whether a `HeapObject` resides in `ReadOnlySpace` (`HeapLayout::InReadOnlySpace(obj)`):
* Write barriers and GC marking skip read-only objects because they are immortal and immutable.
* Under the V8 Sandbox, reading the `MemoryChunk` flags of an in-sandbox page to check `READ_ONLY_HEAP` would both incur a memory load and trust an in-sandbox header.

When `CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL` is enabled (`src/common/globals.h`, `src/heap/memory-chunk-inl.h`), V8 eliminates the `MemoryChunk` load entirely using virtual address layout geometry:

1. **Bottom-of-Cage Reservation**:
   * All `ReadOnlySpace` pages are allocated exclusively within the first **`kContiguousReadOnlyReservationSize`** bytes at offset `0` of the main pointer compression cage (`V8_CONTIGUOUS_COMPRESSED_RO_SPACE_SIZE_MB * MB`, a power of two such as `8MB`).
   * All mutable spaces (`NewSpace`, `OldSpace`, `SharedSpace`) are allocated at offsets `>= kContiguousReadOnlyReservationSize`.
   * Crucially, V8 also reserves an inaccessible **red zone** of size `kContiguousReadOnlyReservationSize` at offset `0` of every other cage (`TrustedCage` and the `ExternalCodeCompressionScheme` window).
2. **Single-Instruction Bitmask Check**:
   * V8 defines the compile-time mask (`src/common/globals.h`):
     ```cpp
     constexpr uintptr_t kContiguousReadOnlySpaceMask =
         (kPtrComprCageBaseAlignment - 1) ^
         (kContiguousReadOnlyReservationSize - 1);
     // For a 4GB cage and 8MB RO space: 0x00000000'ff800000
     ```
   * Any object in `ReadOnlySpace` has bits `[23..31]` equal to `0`. Because all other cages have an unmapped red zone in their first `8MB`, **no non-read-only object in any cage can ever have those bits all zero**.
   * `MemoryChunk::InReadOnlySpace()` (`src/heap/memory-chunk-inl.h`) simply executes:
     ```cpp
     return (reinterpret_cast<uintptr_t>(this) & kContiguousReadOnlySpaceMask) == 0;
     ```
   * Furthermore, `FastInReadOnlySpaceOrSmallSmi(Tagged_t obj)` checks `((obj & kContiguousReadOnlySpaceMask) == 0)` on a raw 32-bit compressed value: in a single `test` instruction, it returns `true` for every object in `ReadOnlySpace` as well as every non-negative Smi up to `2^22 - 1`, allowing GC marking and write barriers to fast-path both cases simultaneously.

---

## 4. Isolate Groups (`V8_COMPRESS_POINTERS_IN_MULTIPLE_CAGES` vs. Shared Cage)

An **`IsolateGroup`** (`src/init/isolate-group.h`) owns the pointer compression cages (`main` cage, `TrustedCage`, and `metadata_pointer_table`):
* **`V8_COMPRESS_POINTERS_IN_SHARED_CAGE`**: All `Isolate`s in the process belong to a single default `IsolateGroup` and share the same 4GB pointer compression cage (and 1TB sandbox). This allows `SharedReadOnlySpace` and `SharedSpace` objects to be referenced directly by any `Isolate`, and enables `V8_STATIC_ROOTS_BOOL` (where read-only root offsets are compile-time constants).
* **`V8_COMPRESS_POINTERS_IN_MULTIPLE_CAGES`**: Embedders can create multiple independent `IsolateGroup`s within a single process, each with its own 4GB pointer compression cage (`IsolateGroup::current()`).

---

## 5. Summary of Compression Schemes

| Compression Scheme | Target Space(s) | Base Alignment | Field Wrapper | Sandbox Location |
| :--- | :--- | :--- | :--- | :--- |
| **`V8HeapCompressionScheme`** | `NewSpace`, `OldSpace`, `SharedSpace`, `ReadOnlySpace` | 4GB (`kPtrComprCageBaseAlignment`) | `TaggedMember<T>` | Inside Sandbox (4GB cage at start of 1TB sandbox) |
| **`TrustedSpaceCompressionScheme`** | `TrustedSpace`, `SharedTrustedSpace` | 4GB (`kPtrComprCageBaseAlignment`) | `ProtectedTaggedMember<T>` | Outside Sandbox (`TrustedCage`) |
| **`ExternalCodeCompressionScheme`** | `CodeSpace`, `CodeLargeObjectSpace` | Within a single 4GB window (`PrepareCageBaseAddress`) | Instruction stream slots | Outside Sandbox (`CodeRange`) |
