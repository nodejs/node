# V8 WebAssembly Architecture

V8's WebAssembly subsystem (`src/wasm/`) provides a high-performance, sandbox-hardened execution environment supporting core WebAssembly, **WasmGC**, **Exception Handling**, **JS Promise Integration (JSPI)**, **Memory64**, and **Relaxed SIMD**.

Unlike JavaScript, WebAssembly modules are statically typed and validated before execution. V8 exploits this through an off-heap code management architecture (`NativeModule`) shared across Isolates, a two-tier compilation pipeline (**Liftoff** baseline compiler + **Turboshaft** optimizing compiler), and feedback-driven speculative inlining with deoptimization support.

---

## 1. Compilation Pipeline: Lazy Validation, Liftoff, and Turboshaft

### Lazy Compilation by Default (`--wasm-lazy-compilation`)
By default, V8 enables **lazy compilation** (`--wasm-lazy-compilation`) for WebAssembly modules:
1. **Streaming Decoding and Validation (`StreamingDecoder` / `ModuleCompiler`)**: When `WebAssembly.compileStreaming` (or `WebAssembly.compile`) receives module wire bytes, background worker threads decode the module header and **validate** all function bodies (`ValidateSequentially` / `CompileLazy` jobs in `src/wasm/module-compiler.cc`), but do **not** compile every function upfront.
2. **`GetDefaultTiersPerModule` and `WasmCompileLazy`**: In `src/wasm/module-compiler.cc`, `GetDefaultTiersPerModule(native_module, is_in_debug_state, lazy_module)` returns `{ExecutionTier::kNone, ExecutionTier::kNone}` when `lazy_module` is true. Every function's slot in the `NativeModule`'s jump table is initialized to point to the `WasmCompileLazy` builtin.
3. **On-Demand Baseline Compilation**: The first time function `func_index` is called, execution enters `WasmCompileLazy`, which invokes `CompileLazy` (`src/wasm/module-compiler.cc`) to compile that single function with **Liftoff** (`GetLazyCompilationTiers`), patches the jump table slot with the compiled entrypoint, and jumps to it.

### Two Compilation Tiers: Liftoff and Turboshaft (`ExecutionTier::kTurbofan`)

```
Wasm Wire Bytes (Validated during module creation)
   │
   ▼  (On first call via WasmCompileLazy)
Tier 1: Liftoff Baseline Compiler    (src/wasm/baseline/liftoff-compiler.cc)
   │  Single-pass structured-control-flow code generator using an abstract
   │  register/stack value state (LiftoffAssembler::CacheState).
   │  - Emits fast machine code in a single pass (~10-20 MB/s).
   │  - Decrements per-function tiering_budget_array on calls and loops.
   │  - Collects call-site type feedback (direct, call_ref, call_indirect)
   │    into WasmTrustedInstanceData::feedback_vectors.
   ▼  (When tiering_budget reaches 0 -> TriggerTierUp)
Tier 2: Turboshaft Optimizing Tier   (src/wasm/turboshaft-graph-interface.cc)
      (Represented in C++ by ExecutionTier::kTurbofan in src/wasm/wasm-tier.h)
      - TransitiveTypeFeedbackProcessor processes collected feedback.
      - InliningTree selects direct and speculative indirect callees to inline.
      - BuildTSGraph (TurboshaftGraphBuildingInterface) decodes Wasm wire bytes
        directly into a Turboshaft IR graph.
      - Runs Turboshaft Wasm optimization phases (WasmGCOptimizePhase,
        WasmOptimizePhase, DeadCodeElimination, InstructionSelection, RegAlloc)
        and publishes optimized WasmCode into NativeModule.
```

For a detailed guide to Liftoff's single-pass decoder interface, `CacheState` register allocation, control-flow merges, stack frame layout, GC safepoints, and debugging/deoptimization invariants, see [liftoff.md](liftoff.md).

> **Note on Naming (`ExecutionTier::kTurbofan`)**: The optimizing compiler for WebAssembly is now **100% Turboshaft** (`compiler::turboshaft::BuildTSGraph` in `src/wasm/turboshaft-graph-interface.cc` and `GenerateWasmCodeFromTurboshaftGraph` in `src/compiler/pipeline.cc`). However, for historical reasons, the C++ enum value in `src/wasm/wasm-tier.h` is still named `ExecutionTier::kTurbofan`.

### Debugging Tier (`WasmEngine::EnterDebuggingForIsolate`)
Only Liftoff can generate code with breakpoint support and value inspection side tables (`DebugSideTable`, mapping each PC offset to the exact registers and stack slots holding Wasm locals and operand stack values).

When DevTools attaches or a breakpoint is set, `WasmEngine::EnterDebuggingForIsolate(Isolate*)` (`src/wasm/wasm-engine.cc`):
1. Marks each `NativeModule` in the Isolate as `kDebugging` (`native_module->SetDebugState(kDebugging)`).
2. Calls `native_module->RemoveCompiledCode(NativeModule::RemoveFilter::kRemoveNonDebugCode)`. This deletes **all** non-debug compiled code in the module (both optimized Turboshaft `kTurbofan` code **and** regular non-debug Liftoff code) and resets the jump tables back to `WasmCompileLazy`.
3. As functions are subsequently executed, `CompileLazy` (`src/wasm/module-compiler.cc`) recompiles them on demand with Liftoff in `ForDebugging::kForDebugging` mode (emitting trapping-instruction and OOL-trap safepoints, full stack-slot spilling at stack checks, and OSR hooks; `DebugSideTable`s are generated on demand during inspection via `GenerateLiftoffDebugSideTable`, and breakpoints trigger recompilation in `kWithBreakpoints` / `kForStepping` mode via `RecompileLiftoffWithBreakpoints`). If Liftoff bails out (for example due to a missing CPU feature), `CompileLazy` (and `ExecuteCompilationUnits`) falls back to a Turboshaft (`ExecutionTier::kTurbofan`) `WasmCompilationUnit` while keeping `WasmCode::for_debugging() == kForDebugging` so `NativeModule` installs the code in debug state; `WasmCode::is_inspectable()` (`is_liftoff() && for_debugging()`) distinguishes actual inspectable Liftoff debug code from such fallback Turboshaft code. When debugging ends, `LeaveDebuggingForIsolate` removes debug code (`kRemoveDebugCode`) and restores normal dynamic tiering.

### Code Caching (`WasmSerializer` / `WasmDeserializer`)
Because Liftoff code depends on runtime instance state and is fast to regenerate on demand, V8's Wasm code caching (`src/wasm/wasm-serialization.{h,cc}`, Used by IndexedDB/HTTP caching in Chromium) serializes only optimized `ExecutionTier::kTurbofan` (Turboshaft) `WasmCode` objects along with module wire bytes. Unserialized functions simply start at `WasmCompileLazy` upon deserialization.

---

## 2. Wasm Inlining, Type Feedback, and Deoptimization

Because WasmGC and object-oriented languages compiled to Wasm rely heavily on indirect calls (`call_ref` and `call_indirect`) to small functions, V8 performs **feedback-directed speculative inlining** accompanied by **Wasm Deoptimization** (`--wasm-deopt`).

### 1. Liftoff Call-Site Feedback Collection
When `--wasm-inlining` is enabled, Liftoff instruments every `call`, `return_call`, `call_ref`, `return_call_ref`, `call_indirect`, and `return_call_indirect` instruction:
- Per-instance feedback is stored in `WasmTrustedInstanceData::feedback_vectors()` (`FixedArray` of per-function `FixedArray` vectors).
- As documented in `TransitiveTypeFeedbackProcessor::ProcessFunction` (`src/wasm/module-compiler.cc`) and `UpdateCallRefOrIndirectIC` (`src/builtins/wasm.tq`), each call site occupies `FeedbackConstants::kSlotsPerInstruction = 2` slots (after 1 header slot storing the total function invocation count):
  - **Direct calls**: `(Smi(count), Smi(0))`.
  - **`call_ref` / `call_indirect`**: Transitions from **uninitialized** `(Smi(0), Smi(0))` -> **monomorphic** `(WasmFuncRef / Smi(target), Smi(count))` -> **polymorphic** `(FixedArray of up to kMaxPolymorphism = 4 target/count pairs, Undefined)` -> **megamorphic** `(megamorphic_symbol, Undefined)`.

### 2. `TransitiveTypeFeedbackProcessor` and `InliningTree`
When a function's `tiering_budget_array` entry reaches zero (`TriggerTierUp` in `src/wasm/module-compiler.cc`):
1. `TransitiveTypeFeedbackProcessor::Process(isolate, trusted_instance_data, func_index)` (`src/wasm/module-compiler.cc`) reads the on-heap `feedback_vectors` (validating bounds with `SBXCHECK` because `feedback_vectors` reside in the untrusted main heap) and converts them into off-heap `CallSiteFeedback` arrays stored in `WasmModule::type_feedback.feedback_for_function`, transitively processing any newly discovered callees. To prevent deopt loops across multiple instances, when `--wasm-deopt` is enabled, new feedback is merged with any previously recorded `CallSiteFeedback` for that function.
2. Before Turboshaft graph building starts, `InliningTree::CreateRoot` (`src/wasm/inlining-tree.h`) runs `FullyExpand()` using a priority queue ordered by `score() = relative_call_count_ / wire_byte_size_`, greedily marking call sites as `is_inlined()` until `kMaxInlinedCount` (`60`), `kMaxInliningNestingDepth` (`7`), or the wire-byte budget (`SmallEnoughToInline`) is reached.
3. During `BuildTSGraph`, `TurboshaftGraphBuildingInterface` (`src/wasm/turboshaft-graph-interface.cc`) consults the `InliningTree`:
   - For a direct `call`, it inlines the callee's Turboshaft graph directly.
   - For a monomorphic or polymorphic `call_ref` / `call_indirect`, it emits a chain of target checks (`func_ref == expected_target` or dispatch table entry check), inlines each speculative candidate on its matching branch, and emits a **deoptimization exit** (`DeoptIfNot` / `Bailout` when `--wasm-deopt` is enabled, or a fallback indirect call if megamorphic/non-inlineable targets were seen).

### 3. Wasm Deoptimization (`--wasm-deopt` & `LiftoffFrameDescriptionForDeopt`)
When an inlined speculative `call_ref` or `call_indirect` encounters a target not seen during profiling:
- **Deopt Metadata (`WasmDeoptData` and `WasmDeoptView` in `src/wasm/wasm-deopt-data.h`)**: Stored on the optimized `WasmCode`. Each `WasmDeoptEntry` records the `bytecode_offset` in the Wasm wire bytes and a `translation_index` into the shared `FrameTranslations` array describing how to reconstruct the unoptimized frame(s) (including inlined frames).
- **Reconstructing Liftoff Frames (`LiftoffFrameDescriptionForDeopt`)**: When the deoptimizer (`Deoptimizer::ComputeOutputFrames` in `src/deoptimizer/deoptimizer.cc`) triggers a Wasm deopt, it invokes Liftoff (`CompileWithLiftoffAndGetDeoptInfo`) on each frame in the inlining stack to obtain a `LiftoffFrameDescriptionForDeopt` containing the exact `pc_offset` at the `wire_bytes_offset`, `total_frame_size`, `trusted_instance` register, and `std::vector<LiftoffVarState> var_state` (describing which physical register or stack slot Liftoff expects each local and operand stack value to inhabit at that deopt point).
- The deoptimizer materializes the new Liftoff frames on the stack, updates the function's feedback, and resumes execution in Liftoff right at the failing `call_ref` / `call_indirect`.

---

## 3. WasmGC: Type Canonicalization, Subtyping, and On-Heap Objects

WasmGC allows WebAssembly modules to allocate and manipulate garbage-collected structs, arrays, and function references directly on the V8 heap (`src/wasm/wasm-objects.h`).

### 1. Isorecursive Type Canonicalization (`TypeCanonicalizer`)
In WasmGC, two recursive type groups (`rec` groups) defined in different modules are type-equivalent if and only if they are structurally isomorphic (`src/wasm/canonical-types.h`):
- **`TypeCanonicalizer` (Process-wide Singleton)**: When a module's type section is decoded, `TypeCanonicalizer::AddRecursiveGroup` hashes and canonicalizes each recursive group against all previously registered groups across the process, assigning each `ModuleTypeIndex` a global `CanonicalTypeIndex` (`module->isorecursive_canonical_type_ids`).
- **`PrepareForCanonicalTypeId`**: Allocates canonical RTT `Map`s on the Isolate (`wasm_canonical_rtts`), enabling zero-copy passing of WasmGC objects across independent module instances.

### 2. Subtyping (`src/wasm/wasm-subtyping.h` and `WasmTypeInfo`)
- **Compile-time validation**: `IsSubtypeOf` and `ValidSubtypeDefinition` (`src/wasm/wasm-subtyping.h`) verify declared supertype hierarchies across abstract heap types (`any`, `eq`, `i31`, `struct`, `array`, `func`, `extern`, `none`, `nofunc`, `noextern`) and indexed canonical types.
- **Runtime casts (`ref.test`, `ref.cast`, `br_on_cast`)**: Every WasmGC `Map` carries a `WasmTypeInfo` (`src/wasm/wasm-objects.h`) in its constructor/backpointer slot. `WasmTypeInfo` embeds the type's `canonical_type_` and a flexible array `supertypes[]` containing the `CanonicalTypeIndex` (or RTT `Map`) of every ancestor up to `kMinimumSupertypeArraySize = 8`. Consequently, subtype checks up to depth 8 compile to a single constant-offset array load and compare without loops.

### 3. On-Heap Object Representations (`src/wasm/wasm-objects.h`)
| Class | Inheritance | Representation Details |
|---|---|---|
| `WasmStruct` | `WasmObject` -> `JSReceiver` | Fields are packed at fixed offsets (`WasmStruct::FieldOffset`) immediately after the 1-word `Map` + 1-word `properties_or_hash` `JSReceiver` header. `WasmStruct::BodyDescriptor` consults `WasmStruct::GcSafeType(map)` during GC marking to visit only tagged fields while skipping raw scalar (`i8`/`i16`/`i32`/`i64`/`f32`/`f64`/`v128`) fields. Custom Descriptors also use `WasmCustomMap` (`Map` subclass). Under `--wasm-custom-descriptors`, `WasmObject`s exposed to JS behave like sealed empty JS objects (with an optional custom prototype). |
| `WasmArray` | `WasmObject` -> `JSReceiver` | Stores `uint32_t length_` followed by packed elements (1, 2, 4, 8, or 16 bytes, or tagged slots). Shared `WasmArray`s pad their header to 8-byte alignment (`WasmArray::HeaderSize`) to support 64-bit atomic accesses. |
| `WasmFuncRef` & `WasmInternalFunction` | `HeapObject` / `ExposedTrustedObject` | In-sandbox `WasmFuncRef` holds only a `TrustedPointerMember<WasmInternalFunction>`. The trusted `WasmInternalFunction` lives in `TrustedSpace` and stores the `WasmCodePointer call_target_`, `protected_implicit_arg_` (`WasmTrustedInstanceData` or `WasmImportData`), `function_index_`, and `CanonicalSig*`. |
| `WasmNull` | `HeapObject` | Dedicated root (`roots.wasm_null()`) representing Wasm `ref.null`. When static roots are enabled (`V8_STATIC_ROOTS_BOOL`), `WasmNull::kSize` is **64 KB**, and its payload pages after the `Map` word are mapped `PROT_NONE` (inaccessible) and registered with the trap handler (`RegisterCoveredMemory`). This allows `struct.get` / `struct.set` on small field offsets (`<= kMaxStructFieldIndexForImplicitNullCheck`) to omit explicit null checks and trap via hardware page fault on `wasm_null`. |

---

## 4. V8 Sandbox Isolation for WebAssembly

WebAssembly execution is a prime target under the V8 Sandbox threat model because Wasm code performs raw native memory loads/stores and indirect calls. V8 enforces three core isolation boundaries:

### 1. Off-Heap `NativeModule` and `WasmCodeManager` (`src/wasm/wasm-code-manager.h`)
Compiled `WasmCode` objects do **not** live on the V8 GC heap. `WasmCodeManager` manages process-wide virtual address space reservations (`WasmCodeAllocator`) for `NativeModule`s:
- Each `NativeModule` owns off-heap executable memory pages protected with hardware W^X (`MemoryProtectionKey` / PKEYs where supported, or `mprotect` via `ThreadIsolation`).
- `NativeModule` is shared across all `WasmInstanceObject`s and across `Worker` Isolates via `std::shared_ptr<NativeModule>` wrapped in `CppGCManaged<wasm::NativeModule>` (in-sandbox) and `TrustedManaged<wasm::NativeModule>` (in `TrustedSpace`).

### 2. `WasmCodePointerTable` (`src/wasm/wasm-code-pointer-table.h`)
Raw 64-bit native code pointers are **never** stored in Wasm tables or heap objects. Instead, every indirect call target is referenced via a 32-bit `WasmCodePointer` handle indexing into the global `WasmCodePointerTable` (`GetProcessWideWasmCodePointerTable()`):
- Each `WasmCodePointerTableEntry` stores `(std::atomic<Address> entrypoint_, uint64_t signature_hash_)` and the table memory is hardware write-protected (`CFIMetadataWriteScope`).
- Indirect calls (`call_indirect` and `call_ref`) index into the `WasmCodePointerTable`, verify `signature_hash_` for forward-edge CFI, and branch to `entrypoint_`.

### 3. Split Instance Architecture: `WasmInstanceObject` vs `WasmTrustedInstanceData`
To prevent an attacker who corrupts the in-sandbox JS heap from hijacking linear memory pointers, global buffers, or dispatch tables:
- **`WasmInstanceObject` (Untrusted, in-sandbox `JSObject`)**: Exposed to JavaScript as `WebAssembly.Instance`. Contains only `module_object_`, `exports_object_`, and a `TrustedPointerMember<WasmTrustedInstanceData> trusted_data_`.
- **`WasmTrustedInstanceData` (Trusted, in `TrustedSpace`)**: Inherits from `ExposedTrustedObject` (`src/wasm/wasm-objects.h`), allocated and preserved across GC compaction with `kDoubleAligned` (8-byte) alignment so hot pointer-sized fields do not straddle a cache-line boundary. Holds all security-critical raw pointers and protected pointers:
  - `memory0_start_` (`uint8_t*`) and `memory0_size_or_address_` (plus `memory_bases_and_sizes_` in `TrustedFixedAddressArray` for multi-memory).
  - `jump_table_start_` (`Address`) and `tiering_budget_array_` (`std::atomic<uint32_t>*`).
  - `dispatch_table0_` (`WasmDispatchTable`), `dispatch_tables_` (`ProtectedFixedArray`), and `dispatch_table_for_imports_` (`WasmDispatchTableForImports`), all allocated in `TrustedSpace` and linked via `ProtectedPointer`s (`ProtectedTaggedMember`).
- **Frame invariant**: Running Wasm code keeps `WasmTrustedInstanceData` pinned in `kWasmImplicitArgRegister` (`rsi` on x64, `x7` on arm64, `r3` on arm, `esi` on ia32) and spilled in a dedicated stack frame slot (`WasmFrameConstants::kWasmInstanceDataOffset` / `WasmLiftoffFrameConstants::kInstanceDataOffset`). Wasm code **never** reloads `WasmTrustedInstanceData` through an untrusted in-sandbox object.

---

## 5. JS-Wasm Interop, JSPI Stack Switching, and Hardware Trap Handling

### 1. JS-to-Wasm and Wasm-to-JS Wrappers & Inlining (`--wasm-in-js-inlining-body`)
Calls across the JS/Wasm boundary require ABI adaptation (unboxing/boxing JS values, converting numbers/BigInts/WasmGC references, switching the context/instance register (`rsi` on x64 is `kContextRegister` in JS and `kWasmImplicitArgRegister` in Wasm), and managing `CentralStack` transitions):
- **`JSToWasmWrapper` (`WasmExportedFunction`)**: Starts with the generic `JSToWasmWrapper` builtin (interpreting the signature via `packed_args_size`) and tiers up (`wrapper_budget_`) to a signature-specialized Turboshaft wrapper (`CompileJSToWasmWrapper` in `src/wasm/wrappers.cc`, cached per `CanonicalTypeIndex` on the Isolate).
- **`WasmToJSWrapper` (`WasmImportData`)**: Adapts Wasm calls to imported JS callables (`CompileWasmToJSWrapper`).
- **JS-to-Wasm Inlining (`--wasm-in-js-inlining` and `--wasm-in-js-inlining-body`)**: When Turboshaft compiles a hot JavaScript function that calls a `WasmExportedFunction`:
  1. `JSInliningHeuristic` inlines the `JSToWasmWrapper` directly into the JS caller (`WasmInJSInliningPhase`).
  2. With `--wasm-in-js-inlining-body` enabled (`src/compiler/turboshaft/wasm-in-js-inlining-phase.cc`), Turboshaft further inlines the **actual Wasm function body** (including small WasmGC struct/array operations and numeric computations) directly into the surrounding JavaScript Turboshaft graph, eliminating the JS-to-Wasm call frame entirely.

### 2. JavaScript Promise Integration (JSPI) & Stack Switching
JSPI (`WebAssembly.Suspending` and `WebAssembly.promising`, plus the core Stack Switching `cont` proposal) allows synchronous Wasm code to suspend when calling an asynchronous JS import that returns a `Promise`, and resume when that `Promise` resolves:
- **Secondary Stacks (`wasm::StackMemory` and `JumpBuffer` in `src/wasm/stacks.h`)**: When a `promising` export is invoked, `WasmSuspend` / stack-switching builtins allocate or reuse an off-heap `StackMemory` (composed of dynamically growable `StackSegment`s when `--wasm-growable-stacks` is enabled) and save the parent stack's `sp`, `fp`, `pc`, and `stack_limit` in `JumpBuffer` (tracking `StackState`: `Active`, `Suspended`, `Inactive`, `Retired` for sandbox safety).
- **`WasmSuspenderObject` (`ExposedTrustedObject` in `src/wasm/wasm-objects.h`)**: Lives in `TrustedSpace` and tracks the active `StackMemory*`, parent `WasmSuspenderObject`, `promise_`, and `resume_`/`reject_` callbacks (`WasmResumeData`).
- **Central Stack Switching for JS/Runtime Calls**: While executing on a secondary `StackMemory`, any Wasm-to-JS import call or C++ runtime call switches `sp` back to the Isolate's main OS stack (`CentralStack`, recorded in `StackMemory::StackSwitchInfo`) so JavaScript and C++ code always run on the central stack. On Windows native builds, `StackMemory` also saves and restores the central stack's dynamically committed `TEB->StackLimit` (`SwitchStackBoundsFromCentral` / `RestoreCentralStackBounds` / `SetCurrentThreadStackBounds`) across all stack switches, segment growth/shrinking, and exception unwinding.

### 3. Hardware Bounds-Check Trap Handling (`src/trap-handler/`)
On 64-bit platforms (`x64`, `arm64`, `riscv64`), V8 eliminates explicit bounds-check branches for 32-bit Wasm linear memories (`memory32`):
1. **10 GB Virtual Reservation with Guard Regions**: Each 32-bit `WasmMemoryObject` backing store reserves up to 8 GB (plus guard regions before and after) of virtual address space (`RegisterCoveredMemory` in `src/trap-handler/trap-handler.h`), with pages beyond the current memory size mapped `PROT_NONE`. Since any 32-bit `index + 32-bit offset` is `< 8 GB`, any out-of-bounds access is guaranteed to land inside the unmapped guard region and trigger an OS signal/exception (`SIGSEGV` / `SIGBUS` on POSIX, Vectored Exception Handler on Windows, Mach exception port on macOS).
2. **`TrappingInstructionData` Registration**: During Liftoff or Turboshaft code generation, every protected memory load/store (and implicit `WasmNull` dereference) records its instruction offset in a `TrappingInstructionData` table registered via `trap_handler::RegisterHandlerData` (`src/trap-handler/handler-outside.cc`).
3. **Signal Handler Recovery (`TryHandleSignal` in `src/trap-handler/handler-inside.cc`)**: When a fault occurs, the lock-free signal handler verifies that:
   - The faulting address lies inside a registered V8 Sandbox (`RegisterV8Sandbox`) and covered memory region (`RegisterCoveredMemory`), and
   - The faulting `rip`/`pc` matches a registered `TrappingInstructionData` entry in a `WasmCode` object.
   If both checks pass, the signal handler rewrites the thread context's instruction pointer (`pc`) to the global `landing_pad` (`SetLandingPad` -> `WasmTrapHandlerThrowTrap` builtin), which transitions cleanly into the V8 runtime to throw a `WebAssembly.RuntimeError` without executing a single bounds-check branch in the hot path.

---

## 6. Key Files Reference

| File | Purpose |
|---|---|
| `src/wasm/wasm-engine.{h,cc}` | Process-wide `WasmEngine`, Isolate/NativeModule tracking, and `EnterDebuggingForIsolate` / `LeaveDebuggingForIsolate`. |
| `src/wasm/module-compiler.{h,cc}` | `GetDefaultTiersPerModule`, lazy compilation (`CompileLazy`), `TriggerTierUp`, and `TransitiveTypeFeedbackProcessor`. |
| `src/wasm/baseline/liftoff-compiler.{h,cc}` | Single-pass **Liftoff** baseline compiler, call-site feedback collection, and `GenerateLiftoffCodeForDeopt`. |
| `src/wasm/turboshaft-graph-interface.{h,cc}` | `TurboshaftGraphBuildingInterface` (`BuildTSGraph`) decoding Wasm wire bytes into Turboshaft IR and performing speculative inlining. |
| `src/wasm/inlining-tree.h` | `InliningTree` feedback-driven call-site prioritization and wire-byte budget calculation. |
| `src/wasm/wasm-deopt-data.{h,cc}` | `WasmDeoptData`, `WasmDeoptEntry`, `WasmDeoptView`, and `LiftoffFrameDescriptionForDeopt`. |
| `src/wasm/canonical-types.{h,cc}` | `TypeCanonicalizer` for isorecursive WasmGC type canonicalization across modules. |
| `src/wasm/wasm-subtyping.{h,cc}` | `IsSubtypeOf`, `ValidSubtypeDefinition`, and WasmGC type hierarchy rules. |
| `src/wasm/wasm-objects.{h,cc}` | `WasmInstanceObject`, `WasmTrustedInstanceData`, `WasmDispatchTable`, `WasmStruct`, `WasmArray`, `WasmFuncRef`, `WasmNull`, and `WasmSuspenderObject`. |
| `src/wasm/wasm-code-manager.{h,cc}` | Off-heap `NativeModule`, `WasmCode`, and `WasmCodeManager`. |
| `src/wasm/wasm-code-pointer-table.{h,cc}` | Write-protected `WasmCodePointerTable` enforcing forward-edge CFI for indirect Wasm calls. |
| `src/wasm/wrappers.{h,cc}` | Turboshaft `JSToWasmWrapper` and `WasmToJSWrapper` compilers. |
| `src/compiler/turboshaft/wasm-in-js-inlining-phase.{h,cc}` | `WasmInJSInliningPhase` inlining Wasm function bodies directly into JS callers (`--wasm-in-js-inlining-body`). |
| `src/wasm/stacks.{h,cc}` | `StackMemory`, `StackSegment`, and `JumpBuffer` for JSPI and Wasm stack switching. |
| `src/trap-handler/trap-handler.h` | Hardware bounds-check and implicit-null-check trap handler (`RegisterHandlerData`, `RegisterCoveredMemory`, `SetLandingPad`). |
