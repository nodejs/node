# V8 Sandbox Architecture (`src/sandbox/`)

The **V8 Sandbox** (`V8_ENABLE_SANDBOX`) is an in-process software fault isolation (SFI) mechanism designed to prevent memory corruption vulnerabilities inside V8 from escaping into arbitrary code execution or arbitrary read/write in the host process.

---

## 1. Threat Model and Security Boundary

### Attacker Model
The V8 Sandbox assumes that an attacker (via a bug in the JIT compilers, runtime builtins, or garbage collector) has already achieved **arbitrary, concurrent read and write access to all memory inside the V8 Sandbox address space** (for example, using a worker thread mutating `SharedArrayBuffer` or in-sandbox heap memory concurrently with the main thread).

### Security Goal
Even with full read/write control over every byte inside the sandbox, the attacker **must not** be able to:
1. Corrupt memory outside the sandbox virtual address space (no out-of-sandbox writes), or
2. Divert control flow to arbitrary machine code instructions (`RIP`/`PC` control).

All code paths crossing from in-sandbox data to out-of-sandbox memory or execution must therefore treat in-sandbox bytes as untrusted user input, enforced by hardware guard regions, pointer indirection tables, and runtime assertions (`SBXCHECK`).

---

## 2. Virtual Address Space Layout, Guard Regions, and `BoundedSize`

On 64-bit systems, the V8 Sandbox reserves a contiguous **1TB (`2^40` bytes, `kSandboxSize`)** virtual address region surrounded by **32GB (`kSandboxGuardRegionSize`)** inaccessible guard regions at both ends (`src/sandbox/sandbox.h`, `include/v8-internal.h`):

```
|<-- 32GB Guard -->|<----------------- 1TB V8 Sandbox (2^40 B) ----------------->|<-- 32GB Guard -->|
+------------------+------------------------------------+------------------------+------------------+
| PROT_NONE        | 4GB Pointer Compression Cage       | Backing Stores,        | PROT_NONE        |
| (Unmapped)       | (ROSpace, NewSpace, OldSpace, ...) | ArrayBuffers, Wasm Mem | (Unmapped)       |
+------------------+------------------------------------+------------------------+------------------+
                   ^
                   sandbox->base() == kPtrComprCageBaseRegister (r14 on x64, x28 on arm64)
```

### Why 1TB + 32GB Guard Regions?
1. **In-Sandbox Heap Objects (`V8HeapCompressionScheme`)**: Placed in the first 4GB of the sandbox. Any 32-bit compressed `Tagged_t` pointer decompressed against `kPtrComprCageBaseRegister` (`r14` on x64, `x28` on arm64) is guaranteed by 32-bit unsigned arithmetic to land inside `[base, base + 4GB)`.
2. **`SandboxedPointer_t` (`src/sandbox/sandboxed-pointer.h`)**: Raw pointers to backing stores inside the 1TB sandbox (such as `JSArrayBuffer` backing stores or `ExternalString` data) are stored as 40-bit offsets shifted left by 24 bits (`kSandboxedPointerShift = 64 - 40 = 24`). Decoding shifts right by 24 and adds `sandbox_base`, guaranteeing the resulting pointer lies within `[base, base + 1TB)` with no branch instructions.
3. **`BoundedSize` (`src/sandbox/bounded-size.h`)**: Sizes and byte lengths of in-sandbox buffers (`JSArrayBuffer::byte_length`, `JSTypedArray::byte_length`, `JSDataView::byte_offset`) are stored shifted left by `kBoundedSizeShift = 29` (`kMaxSafeBufferSizeForSandbox = 32GB - 1`). Reading a `BoundedSize` shifts right by 29 bits, guaranteeing the decoded `size_t` is strictly `< 32GB`. Therefore, any indexed access `sandboxed_ptr + bounded_offset` is mathematically guaranteed to stay within the 1TB sandbox or hit the trailing **32GB `PROT_NONE` guard region** and crash safely with `SIGSEGV`.

---

## 3. Trusted Space, `TrustedObject`, and `ExposedTrustedObject`

Any V8 heap object containing raw machine code, bytecode, native pointers, or invariants that must not be forged by an attacker cannot live inside the sandbox. V8 places these objects outside the sandbox in **`TrustedSpace`** and **`SharedTrustedSpace`** inside a separate 4GB **`TrustedCage`** (`TrustedSpaceCompressionScheme`).

### Class Hierarchy (`src/objects/trusted-object.h`)

```
HeapObject
  +-- TrustedObject (Lives in TrustedSpace / SharedTrustedSpace outside the sandbox)
        |-- ProtectedFixedArray, ProtectedWeakFixedArray, TrustedFixedArray, TrustedByteArray
        +-- ExposedTrustedObject (Has `self_indirect_pointer_` in TrustedPointerTable)
              |-- Code, BytecodeArray, InterpreterData, UncompiledData
              |-- RegExpData, DebugInfo
              +-- WasmTrustedInstanceData, WasmDispatchTable, WasmInternalFunction, ...
```

1. **`TrustedObject`**:
   * Allocated outside the sandbox in `TrustedSpace` or `SharedTrustedSpace`.
   * Cannot be referenced directly from inside the sandbox.
   * Can reference other `TrustedObject`s directly using **`ProtectedTaggedMember<T>`** (`TaggedMember<T, TrustedSpaceCompressionScheme>`), which compresses 32-bit offsets relative to `TrustedCage`. Because both the host object and target object live outside the sandbox, `ProtectedTaggedMember<T>` cannot be corrupted by an attacker.
2. **`ExposedTrustedObject`**:
   * Subclass of `TrustedObject` for objects that need to be referenced **from inside the sandbox**.
   * Contains a **`TrustedPointerHandle self_indirect_pointer_`** immediately after its `map_` word. When allocated, the object registers itself in the out-of-sandbox **`TrustedPointerTable` (`TPT`)** (`src/sandbox/trusted-pointer-table.h`) with a type-specific `TrustedPointerTag`.
   * In-sandbox objects store a 32-bit **`TrustedPointerMember<T, Tag>`** (`src/objects/trusted-pointer.h`) holding the `TrustedPointerHandle`.

### Why Generic Containers Must NEVER Inherit from `ExposedTrustedObject`
Generic container classes in `TrustedSpace` (such as **`ProtectedFixedArray`**, **`TrustedFixedArray`**, **`TrustedByteArray`**, and **`ProtectedWeakFixedArray`**) inherit directly from `TrustedObject`, **never** `ExposedTrustedObject`:
* If `TrustedFixedArray` were exposed in the `TrustedPointerTable` with a single `kTrustedFixedArrayTag`, an attacker with in-sandbox write access could take a `TrustedPointerHandle` for a 2-element `TrustedFixedArray` used by subsystem A and overwrite a field in subsystem B that expects a 100-element `TrustedFixedArray` with different element types (**handle substitution attack**).
* Instead, only domain-specific objects with self-contained invariants (such as `BytecodeArray`, `Code`, `RegExpData`, `WasmTrustedInstanceData`) inherit from `ExposedTrustedObject`, and they hold any auxiliary `TrustedFixedArray` or `ProtectedFixedArray` tables via `ProtectedTaggedMember<T>`.

### The `InitDontPublish` / `Publish` / `Unpublish` Lifecycle (`kUnpublishedTrustedPointerTag`)
Some `ExposedTrustedObject`s (such as `BytecodeArray` or complex multi-object graphs) must be validated before they are safe to expose to the sandbox:
* **`ExposedTrustedObject::InitDontPublish(isolate)`**: Allocates the object's `TrustedPointerTable` entry tagged with **`kUnpublishedTrustedPointerTag` (`0x7c`)** (`src/sandbox/trusted-pointer-tag.h`). While unpublished, any attempt by in-sandbox code to resolve the handle with its normal tag (`kBytecodeArrayTrustedPointerTag`) fails the tag check and returns an inaccessible pointer.
* **`ExposedTrustedObject::Publish<kTag>(isolate)`**: After verification succeeds (e.g., `BytecodeVerifier::Verify`), transitions the `TrustedPointerTable` entry's tag from `kUnpublishedTrustedPointerTag` to `kTag`, making it live.
* **`TrustedPointerPublishingScope` (`src/sandbox/trusted-pointer-scope.h`)**: Tracks all `ExposedTrustedObject`s created within a scope; if initialization fails (`MarkFailure()`), it overwrites all tracked TPT entries back to `kUnpublishedTrustedPointerTag` (`Unpublish`) so partially constructed trusted objects can never be reached from the sandbox.

---

## 4. Indirect Pointer Tables

All references crossing from inside the sandbox to out-of-sandbox objects or memory go through bounds-checked, type-tagged **Indirect Pointer Tables** (`SegmentedTable` in `src/common/segmented-table.h`). Because a handle is a 32-bit index shifted left (`index = handle >> kShift`), indexing into a pre-reserved maximum-capacity table array (`base[index]`) is inherently bounds-safe without branches.

### 1. `TrustedPointerTable` (`src/sandbox/trusted-pointer-table.h`)
* Maps `TrustedPointerHandle`s to `ExposedTrustedObject`s in `TrustedSpace` (per-Isolate table) or `SharedTrustedSpace` (shared table).
* **Entry Layout (`64 bits`)**: Packs a **7-bit `TrustedPointerTag`**, **1 GC markbit**, and the **untagged `ExposedTrustedObject` address** (`56-bit` payload on x64 LA57; `48-bit` payload + `8` top bits on ARM64).
* **Fast Untagging (`IsFastTrustedPointerTagRange`)**: Frequently accessed tags (`kWasmTrustedInstanceDataTrustedPointerTag`, `kCodeTrustedPointerTag`, etc.) are assigned power-of-two bit values so resolving a handle masks off the expected tag bit and markbit in a single bitwise `AND`. If an attacker substitutes a handle with a different tag, the remaining tag bit stays set in the upper pointer bits, producing a **non-canonical address** that immediately faults in hardware on dereference.

### 2. `JSDispatchTable` (`src/sandbox/js-dispatch-table.h`)
Calling a JavaScript function (`JSFunction`) is one of the hottest operations in V8 and a primary target for control-flow hijacking. Rather than resolving `JSFunction -> SharedFunctionInfo -> Code -> entrypoint` through multiple indirect loads, every `JSFunction`, `SharedFunctionInfo`, and `FeedbackCell` holds a 32-bit **`JSDispatchHandle`** (`JSDispatchHandleMember`) indexing into the **`JSDispatchTable` (`JDT`)**:

```
JSDispatchEntry on 48-bit Address Architectures (e.g. ARM64, x64 without LA57): 16 bytes
+-------------------------------------------------------------------+
| Word 0 (64-bit): entrypoint_  (Direct instruction start address)  |
+-------------------------------------------------------------------+
| Word 1 (64-bit): [Code object pointer (47b) | mark (1b) | argc(16b)]
+-------------------------------------------------------------------+

JSDispatchEntry on x64 with LA57 (V8_TARGET_ARCH_X64 && V8_SUPPORT_LA57): 32 bytes
+-------------------------------------------------------------------+
| Word 0 (64-bit): entrypoint_  (Direct instruction start address)  |
+-------------------------------------------------------------------+
| Word 1 (64-bit): code_object_ (Full 64-bit Code object address)   |
+-------------------------------------------------------------------+
| Word 2 (64-bit): [Unused (47b, bits 63..17) | mark (1b) | argc(16b)]
+-------------------------------------------------------------------+
| Word 3 (64-bit): padding_     (Maintains power-of-two 32B size)   |
+-------------------------------------------------------------------+
```

* **Single-Indirection Calls**: Calling a `JSFunction` loads `entrypoint_` and the 16-bit `parameter_count_` directly from `JDT[dispatch_handle >> kJSDispatchHandleShift]` in a single indexed memory access.
* **Parameter Count Enforcement**: Because `parameter_count_` (formal argument count) and `entrypoint_` are stored together inside the same out-of-sandbox `JSDispatchEntry`, an attacker cannot cause stack-frame corruption by mismatching a function's formal parameter count with its compiled `Code` object (which determines how many arguments the callee pops from the stack).
* **Instant Tiering**: Tiering a function up (Sparkplug -> Maglev -> TurboFan) or deoptimizing it updates the `Code` pointer and `entrypoint_` inside its `JSDispatchEntry` in place (`SetCodeNoWriteBarrier`).
* **Hardware Write Protection**: The `JSDispatchTable` is protected with hardware memory protection keys (`CFIMetadataWriteScope` / PKEYs where supported).

### 3. `ExternalPointerTable` (`src/sandbox/external-pointer-table.h`)
* Protects off-heap C++ pointers (`v8::External`, native callbacks in `AccessorInfo`/`FunctionTemplateInfo`, `WaiterQueueNode`, etc.) referenced from in-sandbox objects via `ExternalPointerHandle` (`ExternalPointerSlot`).
* **Type Tagging (`ExternalPointerTag` in `src/sandbox/external-pointer-tag.h`)**: Each 64-bit entry encodes the C++ pointer alongside a tag in the upper bits (`kExternalPointerTagMask`) and a GC markbit. Resolving an entry checks that the tag lies within the expected `ExternalPointerTagRange`.
* **Generational Spaces**: Partitioned into three `ExternalPointerTable::Space` instances per `Isolate`:
  1. `young_external_pointer_space()`: For handles allocated by objects in `NewSpace` (swept during Minor GC).
  2. `old_external_pointer_space()`: For handles owned by tenured objects in `OldSpace` (supports **EPT Compaction** via `kExternalPointerEvacuationEntryTag`; see [Compaction](../heap/compaction.md)).
  3. `read_only_external_pointer_space()`: For handles in `ReadOnlySpace`.
* **Use-After-Free Protection (`ManagedResource`)**: Off-heap C++ objects that may be freed before their owning V8 wrapper dies inherit from `ExternalPointerTable::ManagedResource`. Their destructor calls `ZapExternalPointerTableEntry()`, immediately invalidating the EPT entry with `kExternalPointerZappedEntryTag` so any subsequent dereference crashes safely.

### 4. `CppHeapPointerTable` (`src/sandbox/cppheap-pointer-table.h`)
* Dedicated table (using 15-bit `CppHeapPointerTag`s) for references from V8 `JSObject` wrappers to Oilpan (`CppHeap`) C++ objects.

---

## 5. Bytecode Verification (`BytecodeVerifier` in `src/sandbox/bytecode-verifier.h`)

Ignition `BytecodeArray` objects live in `TrustedSpace`, and the interpreter (`Ignition`) trusts bytecode instructions to index into registers on the C++ machine stack without per-instruction bounds checks. Whenever a `BytecodeArray` is created or deserialized, **`BytecodeVerifier::Verify`** validates it before calling `Publish(isolate)`:
1. **`VerifyLight` (Default)**: Validates control flow (every jump target lands on a valid instruction boundary within the bytecode array) and verifies that all stack/register operand indices (`Register`) stay within the frame size declared by `BytecodeArray::frame_size()` and `parameter_count()`, preventing bytecode from reading or writing out-of-bounds C++ stack slots.
2. **`VerifyFull` (`--verify-bytecode-full`)**: Additionally verifies constant pool indices and handler table offsets.

---

## 6. The Four Common V8 Sandbox Bug Classes

When writing or reviewing C++, CSA, Torque, or compiler code in V8, watch for these four recurring sandbox vulnerability patterns:

### 1. TOCTOU (Time-of-Check to Time-of-Use) Double-Fetch
* **Bug**: Reading a field from an in-sandbox object (`JSArray::length`, `SeqString::length`, `Map`, or a typed array offset), validating it (`CHECK_LT(obj->length(), max)`), and then **re-reading** `obj->length()` from the in-sandbox object when performing a memory copy or indexing an out-of-sandbox buffer. A concurrent attacker thread can mutate the in-sandbox field between the first and second load.
* **Fix**: Load the in-sandbox value into a local C++ variable once (using `Relaxed_Load` if concurrent mutation is possible), run **`SBXCHECK`** on the local variable, and pass that local variable to all downstream operations.

### 2. Signed 32-Bit Size / Index Integer Overflow
* **Bug**: Computing byte sizes or buffer offsets using signed 32-bit `int` or `int32_t` (for example, `int byte_len = length * sizeof(Element) + kHeaderSize;`). If `length` is corrupted inside the sandbox to a large value (e.g., `0x40000000`), the 32-bit multiplication overflows into a negative `int` (bypassing `if (byte_len > capacity)`) or sign-extends to `0xffffffff80000000` when converted to `size_t`, jumping past the 32GB sandbox guard region.
* **Fix**: Use **`SafeHeapObjectSize`** (`uint32_t`), **`BoundedSize`**, or `size_t` / `uint64_t` for all size and offset calculations, and assert bounds with `SBXCHECK_LE` / `SBXCHECK_LT`.

### 3. Trusting In-Sandbox `MemoryChunk` Flags or `Map`s
* **Bug**: Branching on `MemoryChunk::MainThreadFlags` (`untrusted_main_thread_flags_`) or trusting an in-sandbox object's `Map` / `InstanceType` to authorize an out-of-sandbox write, or using `Cast<T>` / `TrustedCast<T>` to treat an in-sandbox pointer as a `TrustedObject`.
* **Fix**:
  * For security-sensitive page properties (`IsTrusted`, `IsReadOnly`, `IsWritableSharedSpace`, `owner`, `heap`), always query the out-of-sandbox **`BasePage` / `MutablePage`** via `MemoryChunk::Metadata()` (or use `CONTIGUOUS_COMPRESSED_READ_ONLY_SPACE_BOOL` bitmask checks for `InReadOnlySpace`).
  * `Cast<T>` statically rejects `TrustedObject` targets; never use `TrustedCast<T>` or `UncheckedCast<T>` on pointers read from inside the sandbox.

### 4. Unverified Trusted Objects or Handle Substitution Across Contexts
* **Bug**:
  1. Exposing a `TrustedObject` to the sandbox before its fields are initialized or verified, or
  2. Sharing a single `TrustedPointerTag` across multiple `ExposedTrustedObject` subclasses and downcasting with `TrustedCast<SubType>` instead of `SbxCast<SubType>` / `TryCast<SubType>`, or
  3. Reading `parameter_count` from an in-sandbox `SharedFunctionInfo` while loading `Code` from a separate handle (allowing an attacker to mismatch caller/callee stack frame argument counts).
* **Fix**:
  * Use `InitDontPublish` + validation + `Publish` (`TrustedPointerPublishingScope`) for multi-step trusted object initialization.
  * Whenever a `TrustedPointerTagRange` covers multiple types (1:N tag mapping), always narrow with **`SbxCast<T>`** or **`TryCast<T>`**.
  * Load both `entrypoint` and `parameter_count` from the same **`JSDispatchTable`** entry (`JSDispatchHandle`).
