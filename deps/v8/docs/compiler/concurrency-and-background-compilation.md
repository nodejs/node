# Concurrency & Background Compilation in V8

V8's compiler pipeline is designed to maximize throughput on modern multi-core hardware while minimizing main-thread pauses (jank). Because JavaScript execution and garbage collection (GC) mutate the V8 heap on the main thread, executing compilation passes on background worker threads requires strict synchronization protocols, safe object wrappers, and deferred dependency installation.

This document covers both the conceptual model of background compilation across Sparkplug, Maglev, and TurboFan/Turboshaft, and the concrete C++ invariants enforced in `src/baseline/`, `src/maglev/`, and `src/compiler/`.

---

## 1. Conceptual Overview: What Runs Where?

Across all three compiler tiers, V8 keeps main-thread involvement to the absolute minimum required for heap safety:

```
Main Thread (Mutator)                         Background Worker Thread
=====================                         ========================
1. Tier-up trigger (profiler / budget)
2. PublishMainThreadPendingAllocations()
3. Create CompilationJob + PersistentHandles
   ------------------------ Enqueue Job ----------------------->
                                              4. Attach LocalIsolate & LocalHeap
                                              5. Read heap via JSHeapBroker / ObjectRef
                                                 (on-demand background serialization)
                                              6. Build IR, run optimization passes,
                                                 record CompilationDependencies
                                              7. Select instructions & assemble machine code
                                                 into ThreadLocal / off-heap buffer
   <------------------- Notify Finalization --------------------
8. FinalizeJob():
   - Check & Commit(code) CompilationDependencies
   - Allocate Heap<Code> object & copy instructions
   - Install Code pointer into JSFunction / FeedbackVector
```

### Tier Breakdown

| Compiler Tier | Main-Thread Preparation | Background Thread Work | Main-Thread Finalization |
| :--- | :--- | :--- | :--- |
| **Concurrent Sparkplug** (`--concurrent-sparkplug`, `BaselineBatchCompiler`) | Collects `SharedFunctionInfo` handles into a `WeakFixedArray` batch (`compilation_queue_`). Spawns a `ConcurrentBaselineCompiler` job when the batch byte budget fills. | Runs `BaselineCompiler::GenerateCode` (both `PreVisit` and `Visit` passes) into `Factory::CodeBuilder` using a `LocalIsolate`. | `InstallBatch()` finalizes each `MaybeHandle<Code>` on the main thread and installs the baseline code onto the `SharedFunctionInfo`. |
| **Maglev** (`--concurrent-recompilation`, `MaglevConcurrentDispatcher`) | Calls `heap()->PublishMainThreadPendingAllocations()`, initializes `JSHeapBroker` (`InitializeAndStartSerializing`), and enqueues `MaglevCompilationJob`. | Runs `MaglevCompiler::Compile`: builds the Maglev CFG (`MaglevGraphBuilder`), runs inlining, phi untagging, register allocation, and assemblers (`GenerateCode`). | `FinalizeJobImpl` validates and commits `CompilationDependencies` (`deps->Commit(code)`) and installs the optimized `Code` object. |
| **TurboFan / Turboshaft** (`CompilerDispatcher` / `OptimizingCompileDispatcher`) | Calls `heap()->PublishMainThreadPendingAllocations()`, initializes `JSHeapBroker`, and hands off the `TurbofanCompilationJob`. | Runs the frontend (either **Turbolev** via `TurbolevFrontendPipeline` + `TurbolevGraphBuildingPhase`, or legacy Sea-of-Nodes bytecode graph building + scheduling), Turboshaft reducer phases, instruction selection, register allocation, and code assembly. | `FinalizeJobImpl` commits `CompilationDependencies`, links the `Code` object, and installs it. |

---

## 2. Thread-Local Execution: `LocalIsolate`, `LocalHeap`, and Safepoints

Background compiler threads do not have direct access to the main thread's `Isolate` or `Heap`. Instead, each worker creates or attaches a **`LocalIsolate`** (`src/execution/local-isolate.h`), which wraps a **`LocalHeap`** (`src/heap/local-heap.h`).

### GC Safepoints and Parking
Even though background compilers primarily allocate intermediate IR structures in arena-style **`Zone`** memory (`src/zone/zone.h`), they still hold handles to V8 heap objects via **`PersistentHandles`** (`src/handles/persistent-handles.h`) and occasionally allocate off-thread heap objects (via `LocalFactory`).

When the main thread initiates a stop-the-world GC pause, all active `LocalHeap` instances must reach a safepoint:
* **Running State**: While actively dereferencing handles or reading the V8 heap, the `LocalHeap` is in the `Running` state. It must periodically invoke `local_heap->Safepoint()` (for example, inside graph-building and optimization loops) to check whether a GC pause has been requested.
* **Parked State**: Before performing blocking operations (such as waiting on a mutex via `ParkedMutexGuardIf`) or long heap-independent computations, the background thread **parks** its `LocalHeap` (`LocalHeap::Park()`). While parked, the GC can proceed immediately without waiting for the worker thread, and the worker thread is strictly forbidden from dereferencing any `Handle` or raw heap pointer until it unparks.
* **Canonical `PersistentHandles` and GC String Shortcutting**: Although `JSHeapBroker::CanonicalPersistentHandle` deduplicates handle allocations per `HeapObject` at creation time, a non-internalized `String` (or flattened `ConsString`) can later transition to a `ThinString` on the main thread and have its `PersistentHandles` slot shortcutted by GC during a safepoint to an existing `InternalizedString` (or target `String`) that already has its own distinct `PersistentHandles` slot. Consequently, `h1.address() != h2.address()` (distinct handle slot locations) does **not** guarantee `*h1 != *h2` after a GC safepoint; passes that fold `HeapConstant` inequality (such as `MachineOptimizationReducer::TryCompareEqualHeapConstants`) must compare the dereferenced `Tagged<HeapObject>` values while unparked rather than relying solely on handle slot addresses.

---

## 3. `JSHeapBroker` and `ObjectRef` Hierarchy

In Maglev and TurboFan/Turboshaft, compiler passes never manipulate raw `Tagged<HeapObject>` pointers or `DirectHandle<T>` directly. Instead, all heap queries go through the **`JSHeapBroker`** (`src/compiler/js-heap-broker.h`) and typed **`ObjectRef`** wrappers (`MapRef`, `JSObjectRef`, `FeedbackVectorRef`, `SharedFunctionInfoRef`, `StringRef`, etc., declared in `src/compiler/heap-refs.h`).

### `RefSerializationKind` and `ObjectDataKind`
Historically, `JSHeapBroker` eagerly serialized the entire closure of reachable heap objects on the main thread before background compilation started. Modern V8 uses **direct concurrent heap reads** whenever safe, and performs **on-demand background serialization** only for object types whose fields or layout require snapshotting or mutex synchronization.

In `src/compiler/heap-refs.h`, every heap reference type generated by the `HEAP_BROKER_OBJECT_LIST_BASE` macro is tagged with a **`RefSerializationKind`**:

```cpp
// src/compiler/heap-refs.h
enum class RefSerializationKind {
  // Skips serialization entirely; ObjectData wraps the PersistentHandle directly.
  kNeverSerialized,
  // Wraps the object in a specialized HeapObjectData subclass that snapshots
  // or caches metadata on demand on the background thread.
  kBackgroundSerialized,
};
```

Internally (`src/compiler/heap-refs.cc`), the broker's `RefsMap` maps each object address to an `ObjectData*` tagged with one of five **`ObjectDataKind`** values:
1. **`kSmi`**: Small integer value stored inline without heap dereferencing.
2. **`kUnserializedReadOnlyHeapObject`**: Any `HeapObject` residing in `ReadOnlySpace` (`ReadOnlyHeap::SandboxSafeContains(*heap_object)`). Because `ReadOnlySpace` objects are immutable, immovable, and never collected, the broker stores a raw `Tagged<HeapObject>` without even allocating a `PersistentHandle` (`CanonicalPersistentHandle` is bypassed).
3. **`kNeverSerializedHeapObject`**: Types marked `kNeverSerialized` in `HEAP_BROKER_OBJECT_LIST_BASE` (such as `Code`, `SharedFunctionInfo`, `FeedbackVector`, `FeedbackCell`, `Context`, `ScopeInfo`, `Name`, `String`, `Symbol`, `Oddball`, `HeapNumber`, `AllocationSite`, `FunctionTemplateInfo`, `RegExpBoilerplateDescription`). Their `ObjectRef` methods read fields directly from the heap using atomic/acquire loads or sandbox-safe accessors.
4. **`kBackgroundSerializedHeapObject`**: Types marked `kBackgroundSerialized` (`JSReceiver`, `JSObject`, `JSFunction`, `JSTypedArray`, `JSPrimitiveWrapper`, `JSDataView`, `JSBoundFunction`, `Map`, `PropertyCell`, `FixedArrayBase`, `FixedArray`, `ScriptContextTable`, `BigInt`, `HeapObject`). When `TryGetOrCreateData` first encounters one of these objects (typically on the background thread), it allocates a specialized `HeapObjectData` subclass (`MapData`, `JSFunctionData`, `PropertyCellData`, `FixedArrayBaseData`, `BigIntData`, `JSTypedArrayData`) in the broker's zone:
   * Subclasses like `MapData` snapshot immutable metadata (`instance_type_`, `instance_size_`, `bit_field_`, `bit_field2_`, `bit_field3_`, `inobject_properties_`) in their constructor on demand.
   * `JSFunctionData::Cache(JSHeapBroker*)` and `PropertyCellData::Cache(JSHeapBroker*)` snapshot consistent multi-field views (such as `initial_map`, `prototype`, `shared`, `feedback_cell`, and `PropertyDetails` + `value`) on demand when first queried. To avoid deep recursion when caching chains of `JSFunction` objects (e.g., via initial maps and constructors), `JSHeapBroker::EnqueueJSFunctionDataForCaching` pushes uncached `JSFunctionData` instances onto `jsfunction_cache_worklist_` and drains them iteratively in `DrainJSFunctionCacheWorklist()`.
5. **`kUnserializedHeapObject`**: Used only when the broker is in `kDisabled` mode on the main thread.

> [!IMPORTANT]
> **`kBackgroundSerialized` does NOT mean upfront main-thread serialization.** Only the minimal root state (the target `JSFunction`, its `FeedbackVector`, and native context in `CollectArrayAndObjectPrototypes`) is touched during `InitializeAndStartSerializing` on the main thread. All other `kBackgroundSerialized` objects are wrapped and cached lazily on the background thread as the compiler traverses them.

---

## 4. The Uninitialized Object Publication Hazard & `TryMakeRef`

### Why Concurrent Heap Reads Can See Uninitialized Memory
On the main thread, V8 allocates young-generation objects by bumping a pointer in a Linear Allocation Area (LAB), writing the `Map` word, and then initializing the object's fields. For performance, the main thread does **not** emit a release memory fence after initializing every newly allocated object, nor does it atomically zero LAB memory on allocation.

On weakly-ordered architectures (like ARM64) or even across compiler reorderings, if the main thread allocates an object `O` and stores a reference to `O` into a reachable heap location (such as a `FeedbackVector` slot, a global `PropertyCell`, or a context slot) *without* a memory barrier, a background compiler thread reading that slot concurrently can observe the pointer to `O` before `O`'s `Map` word or payload fields are visible in memory, or while `O` still sits inside the main thread's active LAB (`Heap::IsPendingAllocation(object)`).

### Defense 1: `PublishMainThreadPendingAllocations()`
Before spawning any background compilation job (e.g., in `Pipeline::GenerateCodeForTesting`, `TurbofanCompilationJob`, or `MaglevCompilationInfo`) and before concurrent marking steps, the main thread invokes:

```cpp
isolate->heap()->PublishMainThreadPendingAllocations();
```

`PublishMainThreadPendingAllocations()` (`src/heap/heap.h`) makes all allocations performed up to that point visible to background threads by flushing the main thread's LABs (`MakeLinearAllocationAreasIterable()`) and issuing a `base::SeqCst_MemoryFence()`. Any object allocated *before* the compilation job was created is therefore guaranteed to be fully initialized and outside any active LAB.

### Defense 2: `JSHeapBroker::ObjectMayBeUninitialized`
Objects allocated on the main thread *after* the background compilation job starts can still be stored into live feedback vectors, descriptor arrays, or property cells while the compiler is running. Before creating an `ObjectData` entry for a newly discovered `HeapObject`, `JSHeapBroker::TryGetOrCreateData` checks:

```cpp
// src/compiler/js-heap-broker-inl.h
bool JSHeapBroker::ObjectMayBeUninitialized(Tagged<HeapObject> object) const {
  return !IsMainThread() && isolate()->heap()->IsPendingAllocation(object);
}
```

`Heap::IsPendingAllocation` checks whether the object's address falls inside an active main-thread LAB or a page boundary that has not yet been published. If it returns `true`, it is unsafe for the background thread to read the object's `Map` or fields.

### Contract of `TryMakeRef` vs. `MakeRef` vs. `MakeRefAssumeMemoryFence`
The functions in `src/compiler/js-heap-broker.h` provide three distinct contracts for turning a raw `Tagged<T>` or `Handle<T>` into a typed `Ref`:

| Helper | Flags Passed to `TryGetOrCreateData` | Return Type | When to Use & Failure Behavior |
| :--- | :--- | :--- | :--- |
| **`TryMakeRef(broker, object)`** | `{}` (default) | `OptionalRef<T>` | **Default for concurrent heap reads.** Returns `std::nullopt` if `ObjectMayBeUninitialized(object)` is `true` or if background caching (e.g., `PropertyCellData::Cache`) fails due to a concurrent race. **Callers must check `.has_value()` and gracefully bail out of the optimization on `nullopt`.** |
| **`MakeRef(broker, object)`** | `kCrashOnError` | `T` Ref (e.g., `MapRef`) | **Only for objects guaranteed to be published.** Calls `CHECK_WITH_MSG(!crash_on_error, "Ref construction failed")` if `TryGetOrCreateData` fails. Safe only for objects pinned before compilation started or known to reside in `ReadOnlySpace` / pre-existing handles. |
| **`MakeRefAssumeMemoryFence(broker, object)`** | `kAssumeMemoryFence \| kCrashOnError` | `T` Ref (e.g., `MapRef`) | **For pointers loaded with an acquire fence (`kAcquireLoad`) whose writer used a release store (`kReleaseStore`).** Because the release-store / acquire-load pair establishes happens-before ordering for the object's initialization, `TryGetOrCreateData` skips the `ObjectMayBeUninitialized` check. Used for fields like `HeapObject::map(kAcquireLoad)` and `JSFunction::raw_feedback_cell(kAcquireLoad)`. |

---

## 5. Broker Mutex Guards for Shared Data Structures

While immutable fields and atomic pointer slots can be read lock-free with acquire loads, certain mutable heap structures can be concurrently mutated in-place or reallocated by the main thread. `JSHeapBroker` provides recursive, LocalHeap-aware mutex guards (`src/compiler/js-heap-broker.h` and `src/compiler/js-heap-broker-inl.h`) that lock the corresponding `Isolate` mutex while automatically parking the background thread's `LocalHeap` if waiting is required:

* **`JSHeapBroker::MapUpdaterGuardIfNeeded`**: Acquires `isolate()->map_updater_access()` when reading or snapshotting `Map` transition trees, `DescriptorArray` contents, and field representations during `Map` updates.
* **`JSHeapBroker::BoilerplateMigrationGuardIfNeeded`**: Acquires `isolate()->boilerplate_migration_access()` when inspecting `AllocationSite` or object/array literal boilerplate trees in **`JSCreateLowering::TryAllocateFastLiteral`** (`src/compiler/js-create-lowering.cc`) and **`MaglevGraphBuilder::TryReadBoilerplateForFastLiteral`** (`src/maglev/maglev-graph-builder.cc`), preventing a race where the main thread migrates a boilerplate object's elements or map (`JSObject::MigrateInstance`) while the compiler is reading its properties. Both functions acquire `BoilerplateMigrationGuardIfNeeded` before reading `boilerplate.map(broker())`, record `DependOnObjectSlotValue(boilerplate, offsetof(HeapObject, map_), boilerplate_map)`, and verify `boilerplate.map_direct_read(broker())` still equals `boilerplate_map`.

Both inherit from `JSHeapBroker::RecursiveMutexGuardIfNeeded`, which tracks a per-broker recursion depth (`map_updater_mutex_depth_` or `boilerplate_migration_mutex_depth_`) so nested broker queries on the same thread do not deadlock.

---

## 6. Speculative Assumptions and `CompilationDependencies`

Optimizing compilers rely heavily on speculative assumptions about the heap state that hold at the moment of compilation (for example, that a `Map` remains stable (`!is_deprecated()` and no transitions added), a `PropertyCell` or `Context` slot retains its constant value, a prototype chain has no read-only or proxy interceptors, or an allocation site's pretenuring decision has not flipped).

Because the main thread continues executing JavaScript while the background compiler runs, **any of these assumptions can be invalidated before the compiled code is even installed**.

### Recording Dependencies on the Background Thread
During graph building and optimization (in both Maglev and TurboFan/Turboshaft), passes record assumptions via **`CompilationDependencies`** (`src/compiler/compilation-dependencies.h`, `.cc`):

* **`DependOnStableMap(MapRef)`**: Assumes the map stays stable (`CanTransition()` without new transitions).
* **`DependOnGlobalProperty(PropertyCellRef)` / `DependOnConstantInDictionaryPrototypeChain(...)`**: Assumes a global property cell or dictionary prototype property does not change value or cell type.
* **`DependOnProtector(PropertyCellRef)` / `DependOnElementsKind(AllocationSiteRef)` / `DependOnPretenureMode(AllocationSiteRef)`**: Assumes a V8 protector cell remains valid or an `AllocationSite` configuration does not change.
* **`DependOnFieldConstness(...)` / `DependOnFieldRepresentation(...)` / `DependOnFieldType(...)`**: Assumes an in-object or descriptor field's constness (`PropertyConstness::kConst`), representation (`Smi`, `Double`, `HeapObject`), or `FieldType` map is not widened by a subsequent store on the main thread.
* **`DependOnInitialMapInstanceSizePrediction(JSFunctionRef)`**: Assumes slack tracking does not change the predicted instance size of a constructor's initial map.
* **`TransitionDependencyOffTheRecord` / `FieldTypeDependencyOffTheRecord`**: Creates a `CompilationDependency const*` without immediately registering it, allowing an optimization pass to bundle and commit the dependency via `RecordDependency(dep)` only if the speculative optimization actually succeeds.

All recorded dependencies are stored in a deduplicated `ZoneUnorderedSet<const CompilationDependency*>` (`dependencies_`) inside the compilation zone. No heap mutations occur on the background thread when recording a dependency.

### Two-Phase Validation and Installation: `CompilationDependencies::Commit(Handle<Code>)`
When the background compilation job finishes and returns to the main thread for finalization, `CompilationDependencies::Commit(Handle<Code> code)` (`src/compiler/compilation-dependencies.cc`) executes a two-phase validation and installation protocol:

1. **Phase 1: `PrepareInstall()` (May Allocate on Heap)**
   Iterates over `dependencies_`. For each dependency:
   * Checks `dep->IsValid(broker_)`. If any assumption was violated on the main thread while the compiler was running in the background, `PrepareInstall()` immediately returns `false`.
   * Calls `dep->PrepareInstall(broker_)`. Most dependencies do nothing here, but `PrototypePropertyDependency::PrepareInstall` ensures that an uninitialized constructor function has its initial map allocated (`JSFunction::EnsureHasInitialMap`) so that a `DependentCode` array can be attached to it in Phase 2.
2. **Phase 2: Re-validation and Installation under `DisallowCodeDependencyChange`**
   Because `PrepareInstall()` in Phase 1 can allocate an initial map (which may itself deprecate or transition another map that the same compilation unit depended on via `StableMapDependency`), `Commit(code)` enters a `DisallowCodeDependencyChange` scope and iterates over `dependencies_` a second time:
   * Re-checks `dep->IsValid(broker_)`. If invalidated during `PrepareInstall()`, `Commit` aborts.
   * Calls `dep->Install(broker_, &pending_deps)`, which enqueues `(HeapObject, DependencyGroup)` pairs into a `PendingDependencies` table.
   * Calls `pending_deps.InstallAll(isolate, code)` to register the newly compiled `Code` object into each target heap object's `DependentCode` weak list.

If `Commit(code)` returns `false`, the newly compiled `Code` object is discarded (preventing stale speculative code from ever executing), and the function remains at its current tier until a future compilation attempt. If `Commit(code)` succeeds, any future main-thread mutation that violates one of the installed `DependentCode` groups will automatically mark the `Code` object for deoptimization (`Deoptimizer::DeoptimizeMarkedCode`).
