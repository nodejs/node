# Garbage Collection in V8 (Orinoco)

V8's garbage collector, codenamed **Orinoco**, is a generational, incremental, concurrent, and parallel collector designed to minimize main-thread pause times while maintaining high throughput and compact memory usage. It also coordinates directly with **Oilpan (`CppHeap`)** as a **Unified Heap** for C++/DOM objects and manages cross-isolate **Shared Space GC**.

---

## 1. Generational Architecture

V8 exploits the **generational hypothesis** (most allocated objects die shortly after creation) by dividing the heap into two generations with distinct collectors:

| Collector | Target Space(s) | Primary Strategy | Trigger |
| :--- | :--- | :--- | :--- |
| **Scavenger** (Default Minor GC) | `SemiSpaceNewSpace` (`NEW_SPACE`), `NewLargeObjectSpace` | Parallel semi-space copying (`From-Space` to `To-Space` or promotion to `OldSpace`). | `NewSpace` allocation failure. |
| **Minor Mark-Sweep (`MinorMS`)** | `PagedNewSpace` (`--minor-ms`) or `StickySpace` (`--sticky-mark-bits`) | Concurrent/parallel marking + non-copying page sweeping (with on-demand evacuation). | Young generation allocation limit. |
| **Major Mark-Sweep-Compact (`MarkCompactCollector`)** | Entire Isolate Heap (`NewSpace`, `OldSpace`, `CodeSpace`, `TrustedSpace`, `LargeObjectSpace`s, `CppHeap`) | Incremental/concurrent tri-color marking + parallel sweeping + selective page evacuation. | Old generation allocation limit, external memory pressure, idle tasks, or last-resort OOM recovery. |
| **Shared Space GC** | `SharedSpace`, `SharedTrustedSpace`, `SharedLargeObjectSpace`, `SharedTrustedLargeObjectSpace` | Stop-the-world `GlobalSafepoint` across all client `Isolate`s in the `IsolateGroup` + Mark-Sweep-Compact. | `Heap::CollectGarbageShared` (`CollectSharedGarbage`) on shared allocation limit. |

---

## 2. Young Generation Collection: Scavenger vs. MinorMS

### 1. Scavenger (`src/heap/scavenger.h`)
The default young-generation collector uses Cheney-style copying collection parallelized across worker threads (`ScavengerCollector`):
1. **Semi-Space Flip**: `SemiSpaceNewSpace` swaps `To-Space` and `From-Space`. All live objects currently reside in `From-Space`.
2. **Root and Remembered-Set Scanning**: Parallel `Scavenger` tasks scan stack roots (via Conservative Stack Scanning if active), handles, global handles, and the `OLD_TO_NEW` / `OLD_TO_NEW_BACKGROUND` remembered sets recorded on `MutablePage`s in `OldSpace`.
3. **Evacuation & Promotion**:
   * **First Survival**: An object in `From-Space` that has not yet crossed the semi-space `age_mark_` is copied to `To-Space`.
   * **Second Survival (Promotion)**: An object that has already survived one Scavenge (its address is below `age_mark_`), or when `To-Space` fills past a high-water threshold, is promoted directly into `OldSpace` (into thread-local `CompactionSpace` LABs).
   * **Forwarding**: The original object's `MapWord` in `From-Space` is atomically CAS-updated (`relaxed_compare_and_swap_map_word_forwarded`) to a forwarding pointer (`kForwardingTag = 0` in the low 2 bits). If another worker thread races to copy the same object, the loser abandons its copy and uses the winner's forwarding address.
   * **Quarantined Pages (Conservative Stack Scanning)**: When a `From-Space` page contains an object directly pointed to from the C++ stack (`Pinning`), that object cannot move. Rather than copying individual pinned objects, the entire 256KB `NormalPage` is **quarantined** (relinked in-place into `To-Space` or promoted to `OldSpace` when `WillBePromoted` is set).
   * **Young Large Objects (`NewLargeObjectSpace`)**: Never copied; surviving `LargePage`s are promoted in-place to `OldLargeObjectSpace`.

### 2. Minor Mark-Sweep (`MinorMS`, `src/heap/minor-mark-sweep.h`)
When `--minor-ms` is enabled, `NewSpace` uses `PagedNewSpace` (or `StickySpace` under `--sticky-mark-bits`) instead of two copying semi-spaces:
* **Marking**: Marks live young objects (concurrently and in parallel) starting from roots and `OLD_TO_NEW` remembered sets. Also records young objects holding `ExternalPointerTable` entries in the `SURVIVOR_TO_EXTERNAL_POINTER` remembered set.
* **Page Promotion & Sweeping**: Pages with high live byte counts are promoted in-place to `OldSpace` by relinking their `NormalPage` metadata (and evacuating their `SURVIVOR_TO_EXTERNAL_POINTER` entries from the young EPT space to the old EPT space). Remaining young pages are swept in the background to rebuild free lists.

---

## 3. Write Barriers (`src/heap/WRITE_BARRIER.md` & `src/heap/heap-write-barrier.h`)

Whenever a pointer field inside a `HeapObject` (`host`) is mutated to reference another `HeapObject` (`value`), V8 emits a **Write Barrier** after the store to preserve GC invariants:
1. **Generational Invariant**: Record old-to-young pointers in `OLD_TO_NEW` (or `OLD_TO_NEW_BACKGROUND`).
2. **Incremental/Concurrent Marking Invariant (Dijkstra Write Barrier)**: Shade `value` grey if marking is active and `host` is already black.
3. **Compaction Invariant**: Record slots pointing to `EVACUATION_CANDIDATE` pages in `OLD_TO_OLD` (or `TRUSTED_TO_TRUSTED`, `TRUSTED_TO_CODE`).
4. **Shared Space Invariant**: Record client-to-shared pointers in `OLD_TO_SHARED` (or `TRUSTED_TO_SHARED_TRUSTED`).

### The 4 Stages of the Standard Write Barrier (`src/heap/WRITE_BARRIER.md`)
To keep code size small and fast-path execution minimal, the standard `Tagged` write barrier is split into four stages from hottest to coldest:

```
[1. Inline Fast Path]  (Emitted at every store site with write barrier)
   |-- Is `value` a Smi? --------------------------> Done
   |-- Check `host` MemoryChunk flags:
       Is `POINTERS_FROM_HERE_ARE_INTERESTING` == 0? -> Done (Common case for Young host when not marking)
       |
       v (Unlikely branch)
[2. Deferred Out-of-Line Path]  (At the end of the JIT function)
   |-- Check `value` MemoryChunk flags:
       Is `POINTERS_TO_HERE_ARE_INTERESTING` == 0? ---> Return to inline code
       |
       v
[3. Shared CSA Builtin]  (`RecordWriteSaveFP` / `RecordWriteIgnoreFP` in CodeStubAssembler)
   |-- Handles incremental marking (marks `value` grey if white)
   |-- Inserts slot into `OLD_TO_NEW` `SlotSet` if bucket already exists
       |
       v (If new `SlotSet` bucket allocation or slow marking worklist push is needed)
[4. C++ Slow Path]  (`Heap::InsertIntoRememberedSetFromCode`)
   |-- Allocates `SlotSet` buckets, handles background/shared remembered sets
```

> [!IMPORTANT]
> **Compiler Fast-Path Difference (`TurboFan` / `Turboshaft` / `Maglev` vs. `Sparkplug`)**:
> * In **TurboFan/Turboshaft** (`ArchStoreWithWriteBarrier`) and **Maglev** (`CheckAndEmitDeferredWriteBarrier`), the **inline fast path (#1)** checks whether `value` is a Smi and tests `POINTERS_FROM_HERE_ARE_INTERESTING` on the **`host`** `MemoryChunk` only. The `POINTERS_TO_HERE_ARE_INTERESTING` check on **`value`**'s `MemoryChunk` is emitted in the **deferred out-of-line block (#2)** at the end of the function to save instruction cache footprint at every store site.
> * Only **Sparkplug** (`MacroAssembler::RecordWrite`, which has no deferred code blocks) emits both `host` and `value` flag checks inline.

### All Specialized Write Barriers (`src/heap/heap-write-barrier.h`)
Because V8 segregates pointers across sandboxes, indirect pointer tables, and CppHeap, `WriteBarrier` provides specialized static entry points:

| Write Barrier Method | Field / Slot Type | What It Does |
| :--- | :--- | :--- |
| **`WriteBarrier::ForValue`** | `TaggedMember<T>` (`ObjectSlot` / `MaybeObjectSlot`) | Standard 4-stage barrier for generational (`OLD_TO_NEW`), shared (`OLD_TO_SHARED`), marking, and evacuation (`OLD_TO_OLD`) tracking. |
| **`WriteBarrier::ForTrustedPointer`** | `TrustedPointerMember<T, Tag>` (`TrustedPointerSlot`) | Marks the target `ExposedTrustedObject` AND its `TrustedPointerTable` entry during incremental marking (does not require host page flag checks; checks `IsMarking`). |
| **`WriteBarrier::ForProtectedPointer`** | `ProtectedTaggedMember<T>` (`ProtectedPointerSlot`) | Marks the target `TrustedObject` during incremental marking and records `TRUSTED_TO_TRUSTED`, `TRUSTED_TO_CODE`, or `TRUSTED_TO_SHARED_TRUSTED` remembered-set slots. |
| **`WriteBarrier::ForJSDispatchHandle`** | `JSDispatchHandleMember` (`JSDispatchHandle`) | Marks the `JSDispatchTable` entry and its target `Code` object when a dispatch handle is stored into a `JSFunction`, `SharedFunctionInfo`, or `FeedbackCell`. |
| **`WriteBarrier::ForExternalPointer`** | `ExternalPointerMember<Tag>` (`ExternalPointerSlot`) | Marks the `ExternalPointerTable` entry during major marking and notifies EPT compaction if the entry lives in a compacting EPT segment. |
| **`WriteBarrier::ForCppHeapPointer`** | `CppHeapPointerSlot` | Marks the referenced Oilpan object via `CppHeap` and calls `CppHeap::RememberCrossHeapReferenceIfNeeded` for generational Oilpan GC. |
| **`WriteBarrier::ForEphemeronHashTable`** | `EphemeronHashTable` (`ObjectSlot` value) | Records key-to-value ephemeron dependencies (`ephemeron_remembered_set_`) when an old-generation `EphemeronHashTable` entry is populated with a young key. |
| **`WriteBarrier::ForRelocInfo`** | `InstructionStream` + `RelocInfo*` | Records embedded pointers and code targets inside JIT machine code into the page's `TypedSlotSet` and marks target objects during incremental marking. |
| **`WriteBarrier::ForArrayBufferExtension`** | `JSArrayBuffer` + `ArrayBufferExtension*` | Marks the `ArrayBufferExtension` for sweeping/accounting of off-heap backing store bytes. |

---

## 4. Remembered Sets (`RememberedSetType`, `SlotSet`, `TypedSlotSet`)

Each `MutablePage` (`src/heap/mutable-page.h`) maintains arrays of `SlotSet*` and `TypedSlotSet*` indexed by **`RememberedSetType`** (`NUMBER_OF_REMEMBERED_SET_TYPES = 8`):

| `RememberedSetType` | Recorded Direction | Purpose |
| :--- | :--- | :--- |
| **`OLD_TO_NEW`** | Old/Trusted/Shared -> Young (`NEW_SPACE` / `NEW_LO_SPACE`) | Recorded by main-thread write barriers (`RecordWrite`). Scanned as roots during Minor GC (Scavenger / MinorMS). |
| **`OLD_TO_NEW_BACKGROUND`** | Old -> Young (from background threads) | Recorded by background threads (e.g., background compilation/deserialization) to avoid atomic contention with the main-thread `OLD_TO_NEW` fast path. |
| **`OLD_TO_OLD`** | Any space -> `EVACUATION_CANDIDATE` page in `OldSpace` / `CodeSpace` | Populated during Major GC marking so `MarkCompactCollector` can update all pointers to evacuated objects. |
| **`OLD_TO_SHARED`** | Client Isolate space -> `SHARED_SPACE` / `SHARED_LO_SPACE` | Populated by client isolates; scanned during Shared Space GC to find live shared objects and update moved pointers. |
| **`TRUSTED_TO_CODE`** | `TRUSTED_SPACE` (`Code`) -> `CODE_SPACE` (`InstructionStream`) | Tracks references from trusted `Code` metadata objects to executable `InstructionStream` pages selected for evacuation. |
| **`TRUSTED_TO_TRUSTED`** | `TRUSTED_SPACE` -> `EVACUATION_CANDIDATE` in `TRUSTED_SPACE` | Tracks protected pointers (`ProtectedTaggedMember`) between trusted objects for `TrustedSpace` compaction. |
| **`TRUSTED_TO_SHARED_TRUSTED`** | Per-Isolate `TRUSTED_SPACE` -> `SHARED_TRUSTED_SPACE` | Tracks protected pointers from client isolate trusted objects into shared trusted space. |
| **`SURVIVOR_TO_EXTERNAL_POINTER`** | Surviving young page -> Young `ExternalPointerTable` space | Populated during MinorMS marking for young objects owning external pointers; used when a page is promoted in-place to `OldSpace` to migrate its EPT entries to `old_space`. |

### `SlotSet` vs. `TypedSlotSet` (`src/heap/slot-set.h`)
* **`SlotSet` (`BasicSlotSet<kTaggedSize>`)**: Used for normal `kTaggedSize`-aligned object fields on a 256KB `NormalPage` (or chunked on a `LargePage`). Organized as a two-level sparse bitmap: an array of `Bucket*` pointers, where each allocated `Bucket` holds `32`-bit atomic cells (`1 bit = 1 slot` on the page), paired with a `PossiblyEmptyBuckets` bitmap so empty buckets can be quickly freed after sweeping/updating.
* **`TypedSlotSet`**: Used when a pointer is embedded inside JIT machine code (`InstructionStream` via `RelocInfo`) where the slot may be unaligned or encoded in instruction immediates. Stored as a linked list of `Chunk`s containing 64-bit `TypedSlot` entries (`SlotType` enum such as `kEmbeddedObjectCompressed`, `kEmbeddedObjectFull`, `kCodeEntry`, `kConstPoolEmbeddedObjectCompressed` + 29-bit byte offset on the page).

---

## 5. Unified Heap (`CppHeap` / Oilpan in `src/heap/cppgc-js/cpp-heap.h`)

Embedders like Blink allocate C++ DOM and wrapper objects on **Oilpan** (`cppgc`), represented inside V8 by **`CppHeap`** (`src/heap/cppgc-js/cpp-heap.h`, accessible via `heap->cpp_heap()`). Because JavaScript objects reference C++ wrappers (via `CppHeapPointerTable` slots) and C++ objects reference JavaScript objects (via `v8::TracedReference<T>` backed by `TracedHandles`), neither collector can collect cycles in isolation.

The **Unified Heap** integrates both collectors into a single garbage collection cycle:
1. **Unified Marking**: When V8 starts incremental/concurrent marking (`IncrementalMarking::Start`), it starts `CppHeap` marking (`CppHeap::StartMarking`).
   * When V8 marks a `JSObject` with a `CppHeapPointer` wrapper slot (or when `WriteBarrier::ForCppHeapPointer` triggers), V8 passes the C++ object pointer to `CppMarkingState::MarkAndPush` so Oilpan's concurrent marker traces the C++ subgraph.
   * When Oilpan marks a C++ object containing a `v8::TracedReference<T>`, it invokes the `UnifiedHeapMarkingVisitor` to push the target V8 `HeapObject` onto V8's `MarkingWorklists`.
2. **Transitive Closure & Atomic Pause**: During the final atomic pause (`MarkCompactCollector::MarkLiveObjects`), V8 and `CppHeap::AdvanceMarking` drain both worklists in a fixpoint loop until neither heap has remaining grey objects, after which V8 sweeps/compacts the JS heap and `CppHeap` sweeps/compacts the Oilpan heap.
3. **Generational CppHeap**: When young generation Oilpan is enabled, `WriteBarrier::ForCppHeapPointer` calls `CppHeap::RememberCrossHeapReferenceIfNeeded`, and `TracedHandles` tracks young `TracedNode`s so minor GCs can collect young JS+C++ cycles without tracing the old generation.

---

## 6. Shared Space GC (`CollectGarbageShared` & `GlobalSafepoint`)

In multi-isolate configurations (`IsolateGroup`), worker `Isolate`s share objects in `SharedSpace`, `SharedLargeObjectSpace`, `SharedTrustedSpace`, and `SharedTrustedLargeObjectSpace`, all owned by a dedicated `shared_space_isolate()` (`src/heap/safepoint.h`, `src/heap/heap.cc`):

1. **Triggering (`Heap::CollectGarbageShared`)**: When an allocation in a shared space fails or hits the shared allocation limit on any client `Isolate`, it calls `Heap::CollectGarbageShared`, which requests a major GC on the `shared_space_isolate()`.
2. **`GlobalSafepoint` (`src/heap/safepoint.h`)**:
   * While a regular GC only pauses threads within a single isolate using `IsolateSafepointScope`, a Shared Space GC enters a **`GlobalSafepointScope`** (`GlobalSafepoint::EnterGlobalSafepointScope`).
   * `GlobalSafepoint` locks `clients_mutex_` and brings the `shared_space_isolate()` plus **every client `Isolate`** (and all of their background `LocalHeap` threads) to a simultaneous safepoint.
3. **Cross-Isolate Root & Remembered-Set Scanning**:
   * Using `GlobalSafepoint::IterateClientIsolates`, the collector scans the stack, handles, and **`OLD_TO_SHARED`** / **`TRUSTED_TO_SHARED_TRUSTED`** remembered sets of every client `Isolate` as roots into the shared spaces.
   * If shared pages are compacted or strings are internalized/forwarded, the collector updates the `OLD_TO_SHARED` slots across all client isolates before releasing the `GlobalSafepointScope`.
