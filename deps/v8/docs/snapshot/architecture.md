# V8 Snapshot Architecture (`src/snapshot/`)

Constructing a fresh JavaScript environment (`Isolate` and `NativeContext`) from scratch requires compiling hundreds of JavaScript and Torque builtins, allocating core `Map`s (`meta_map`, `fixed_array_map`, oddballs), and setting up all global intrinsics (`Object`, `Array`, `Promise`, `Intl`, etc.). Doing this at runtime would take tens or hundreds of milliseconds.

Instead, V8 runs **`mksnapshot`** at build time to initialize an `Isolate` and `Context`, serialize the resulting heap graphs into a binary **Snapshot Blob**, and link or map that blob into the binary so runtime startup (`Snapshot::Initialize`) deserializes a ready-to-run `Isolate` in a few milliseconds.

---

## 1. Snapshot Layers and Serializers

A V8 snapshot blob (`src/snapshot/snapshot.h`) is divided into four modular sections, each produced by a dedicated serializer and restored by a matching deserializer:

```
Snapshot Blob Layout
+-----------------------------------------------------------------------+
| 1. ReadOnly Snapshot     (ReadOnlySerializer / ReadOnlyDeserializer)  |
|    - Raw memory segments of ReadOnlyPage(s) + optional relocation bits|
+-----------------------------------------------------------------------+
| 2. SharedHeap Snapshot   (SharedHeapSerializer / Deserializer)        |
|    - Shared space objects (e.g., shared internalized strings)         |
+-----------------------------------------------------------------------+
| 3. Startup Snapshot      (StartupSerializer / StartupDeserializer)    |
|    - Per-Isolate roots, Builtins constants, initial OldSpace/CodeSpace|
|    - Split into Untrusted Section + TrustedSectionSerializer          |
+-----------------------------------------------------------------------+
| 4. Context Snapshot(s)   (ContextSerializer / ContextDeserializer)    |
|    - NativeContext, JSGlobalObject, intrinsics, initial Map trees     |
|    - Split into Untrusted Section + TrustedSectionSerializer          |
+-----------------------------------------------------------------------+
```

### 1. `ReadOnlySerializer` & `ReadOnlyDeserializer` (`src/snapshot/read-only-serializer.h`)
Unlike the other serializers, **`ReadOnlySerializer` does NOT serialize objects one by one using serializer bytecodes** (`ReadOnlySerializer::SerializeObjectImpl` is literally `UNREACHABLE()`).

Instead, `ReadOnlySerializer` performs a **segment-based `memcpy` serialization** (`src/snapshot/read-only-serializer.cc`):
1. **`ReadOnlySegmentForSerialization`**: Copies each contiguous object area of every `ReadOnlyPage` into an off-heap byte buffer via `MemCopy`.
2. **`ObjectPreProcessor`**: Walks the objects in the off-heap copy in lockstep with the live `ReadOnlyPage` to scrub process-specific pointers:
   * External pointers in `AccessorInfo`, `InterceptorInfo`, `FunctionTemplateInfo`, and `JSExternalObject` are encoded into compact 32-bit `ro::EncodedExternalReference` indices (`tag`, `is_from_api`, `index`).
   * Builtin `Code` objects in `ReadOnlySpace` have their runtime `instruction_start` and source position tables cleared.
3. **Relocation Bitset (`ro::BitSet tagged_slots`) vs. Static Roots (`V8_STATIC_ROOTS_BOOL`)**:
   * **When `V8_STATIC_ROOTS_BOOL` is enabled (Default on 64-bit compressed builds)**: Every `ReadOnlyPage` is deterministic and always mapped at the exact same offset at the bottom of the 4GB pointer compression cage. Therefore, `ReadOnlySerializer` does **not** even encode tagged slots; `ReadOnlyDeserializer` simply `memcpy`s the raw page segments directly into `ReadOnlySpace` and decodes the few preprocessed external pointer slots.
   * **When `V8_STATIC_ROOTS_BOOL` is disabled**: `EncodeRelocationsVisitor` encodes every tagged slot into a `ro::EncodedTagged(page_index, offset)` and records a `1` bit in `tagged_slots`, which `ReadOnlyDeserializer` uses to relocate pointers after allocating the `ReadOnlyPage`s.

### 2. `SharedHeapSerializer` & `SharedHeapDeserializer` (`src/snapshot/shared-heap-serializer.h`)
* Serializes objects residing in `SharedSpace` (such as shared string table entries when `--shared-string-table` is active) that are initialized once per `IsolateGroup` (`shared_space_isolate()`).

### 3. `StartupSerializer` & `StartupDeserializer` (`src/snapshot/startup-serializer.h`)
* Serializes per-`Isolate` state reachable from the mutable sections of `RootsTable` (`STRONG_MUTABLE_IMMOVABLE_ROOT_LIST` and `STRONG_MUTABLE_MOVABLE_ROOT_LIST`) and the `StartupObjectCache`.
* References to objects in `ReadOnlySpace` are emitted as compact `kReadOnlyHeapRef` backreferences rather than re-serializing the object.

### 4. `ContextSerializer` & `ContextDeserializer` (`src/snapshot/context-serializer.h`)
* Serializes a complete `NativeContext` (or custom embedder context created via `v8::SnapshotCreator`).
* References to per-`Isolate` objects already serialized by `StartupSerializer` are emitted as `kStartupObjectCache` indices; references to `ReadOnlySpace` use `kReadOnlyHeapRef`.
* Enables `Context::New` to instantiate additional realms (`<iframe>`s or worker contexts) in microseconds by running `ContextDeserializer` without re-running `StartupDeserializer`.

### 5. `CodeSerializer` (`src/snapshot/code-serializer.h`)
* Used at runtime (not just build time) to serialize compiled `SharedFunctionInfo` and `BytecodeArray` / baseline code into an on-disk **Code Cache** (`v8::ScriptCompiler::CachedData`).

---

## 2. Sandbox Security: Untrusted vs. Trusted Snapshot Sections

Under the [V8 Sandbox](../sandbox/architecture.md), snapshots and cached data are **not** treated as a single monolithically trusted byte stream. If an untrusted or corrupted snapshot stream could emit arbitrary external reference or trusted-space bytecodes while writing into in-sandbox objects, or vice versa, deserialization could be abused to corrupt out-of-sandbox memory.

V8 enforces a strict separation between the **Untrusted Section** (in-sandbox objects) and the **Trusted Section** (`TrustedSectionSerializer` in `src/snapshot/trusted-section-serializer.h`).

```
Untrusted Section (In-Sandbox Objects)               Trusted Section (TrustedSectionSerializer)
+-----------------------------------------+          +------------------------------------------+
| Allocates only in NEW/OLD/SHARED_SPACE  |          | Allocates TrustedObjects outside sandbox |
| Forbids IsTrustedBytecode(bytecode)     |          | + External References Table              |
|                                         |          |                                          |
| [kTrustedPointerPrefix]                 |          | Export[n]:                               |
| [kTrustedExportRef (n)] ----------------+--------->|   [kNewObject (TrustedSpace)]            |
|                                         |          |   [kInitializeSelfTrustedPointer]        |
| [kNewObject (OldSpace)] <---------------+----------|   [kUntrustedRef (backref)]              |
|                                         |          |                                          |
| [kTrustedExternalRef (m)] --------------+--------->| ExternalReferences[m]:                   |
+-----------------------------------------+          |   EncodedExternalReference(index, tag)   |
                                                     +------------------------------------------+
```

### 1. Bytecode Classification (`SerializerDeserializer::IsTrustedBytecode`)
In `src/snapshot/serializer-deserializer.h`, `SerializerDeserializer::IsTrustedBytecode(Bytecode)` exhaustively classifies every serializer bytecode (with no `default:` case so any newly added bytecode must be explicitly reviewed):
* **Trusted-Only Bytecodes (`IsTrustedBytecode == true`)**:
  * `kApiReference`, `kExternalReference`, `kSandboxedApiReference`, `kSandboxedExternalReference`, `kSandboxedRawExternalReference`
  * `kInitializeSelfTrustedPointer`
  * `kAllocateJSDispatchEntry`
  * `kProtectedPointerPrefix`
* When `Deserializer` is deserializing an **untrusted section**, encountering any bytecode where `IsTrustedBytecode(bytecode)` is `true` is rejected, ensuring the untrusted stream cannot write outside the sandbox or forge raw external pointers/trusted table entries.

### 2. `TrustedSectionSerializer` and Cross-Section Reference Bytecodes (`src/snapshot/trusted-section-serializer.h`)
When the untrusted serializer (`StartupSerializer`, `ContextSerializer`, or `CodeSerializer`) encounters a reference from an in-sandbox object to an out-of-sandbox resource, it delegates to `TrustedSectionSerializer`:
* **`kTrustedExportRef`**: When an in-sandbox object references a `TrustedObject` (for example, via `kTrustedPointerPrefix`), the untrusted serializer calls `trusted_serializer->Export(obj)` to assign an export index `n` and emits `kTrustedExportRef(n)` into the untrusted stream. `TrustedSectionSerializer::SerializeExports()` later serializes all exported `TrustedObject`s into the trusted section.
* **`kUntrustedRef`**: Conversely, if a `TrustedObject` inside the trusted section contains a regular tagged pointer back to an in-sandbox `HeapObject`, `TrustedSectionSerializer::SerializeUntrustedReference(obj)` serializes the in-sandbox object inside the `untrusted_serializer_` and emits `kUntrustedRef` (referencing its untrusted backref index) in the trusted stream.
* **`kTrustedExternalRef`**: When an in-sandbox object contains an `ExternalPointerSlot`, the untrusted serializer registers the `(ExternalReferenceEncoder::Value, ExternalPointerTag)` pair in the trusted section via `trusted_serializer->AddExternalReference(reference, tag)` and emits `kTrustedExternalRef(m)` in the untrusted stream.

---

## 3. Core Serializer / Deserializer Bytecodes (`src/snapshot/serializer-deserializer.h`)

The snapshot bytecode stream is a compact state-machine DSL interpreted by `Deserializer::ReadData`:

* **Object Allocation & Backreferences**:
  * `kNewObject (0x00..0x03)`: Encodes the target `SnapshotSpace` (`kOld`, `kCode`, `kMap`, `kTrusted`) in the low 2 bits, followed by the object's size in `kTaggedSize` words, allocates the object, pushes it to the `back_refs_` vector, and recursively deserializes its body.
  * `kBackref (0x04)` & `kFixedBackref (0x90..0xcf)`: Emits a reference to the `n`-th previously deserialized object (`back_refs_[n]`), handling cyclic object graphs.
  * `kRegisterPendingForwardRef` / `kResolvePendingForwardRef`: Used when an object must be referenced before its own allocation has completed (e.g., complex cyclic map/transition structures).
  * `kNewContextlessMetaMap` / `kNewContextfulMetaMap` / `kExtendedMapBitFieldEx`: Specialized bytecodes for bootstrapping meta-maps and initializing `ExtendedMap::bit_field_ex_` immediately after the map word so the GC knows the extended map's size before visiting its fields.
* **Cross-Snapshot & Root References**:
  * `kRootArray` & `kRootArrayConstants (0x40..0x5f)`: Loads a root directly from `Isolate::roots_table()[index]`.
  * `kReadOnlyHeapRef`: Resolves a `(chunk_index, offset)` into `ReadOnlySpace`.
  * `kStartupObjectCache` / `kSharedHeapObjectCache` / `kAttachedReference`: Indexes into the startup cache, shared heap cache, or code-serializer attached objects list.
* **Raw Data & Repeats**:
  * `kFixedRawData (0x60..0x7f)` & `kVariableRawData`: Copies `N` uninterpreted `kTaggedSize` words (Smi fields, numbers, strings, bytecode bytes) directly into the current object.
  * `kFixedRepeatRoot (0x80..0x8f)` & `kVariableRepeatRoot`: Fills `N` consecutive slots with the same root value (e.g., initializing a `FixedArray` with `undefined` or `the_hole`).
* **Sandbox & Dispatch Table Bytecodes**:
  * `kTrustedPointerPrefix`: Prefixes the next object reference so it is stored as a `TrustedPointerHandle` into a `TrustedPointerSlot`.
  * `kInitializeSelfTrustedPointer`: Initializes `ExposedTrustedObject::self_indirect_pointer_` in the `TrustedPointerTable` right after the `map_` word so inner backreferences can already use the handle.
  * `kProtectedPointerPrefix`: Prefixes a reference between two `TrustedObject`s (`ProtectedTaggedMember`).
  * `kAllocateJSDispatchEntry` & `kJSDispatchEntry`: Allocates a new entry in the `JSDispatchTable` (with parameter count and `Code` object) or back-references the `n`-th already-allocated dispatch entry.

---

## 4. Rehashing (`RehashBasedOnMap`)

To prevent HashDoS attacks, V8 randomizes its string and hash-table hash seed (`v8_flags.hash_seed`) per `Isolate` at startup. Because hash tables (`NameDictionary`, `SwissNameDictionary`, `NumberDictionary`, `RegisteredSymbolTable`, etc.) baked into the snapshot were built using the `mksnapshot` hash seed, `ReadOnlyDeserializer`, `StartupDeserializer`, and `ContextDeserializer` track deserialized tables (`to_rehash_`) and call **`HeapObject::RehashBasedOnMap`** at the end of deserialization to recompute hash codes and re-insert entries whenever the runtime hash seed differs from the snapshot seed.
