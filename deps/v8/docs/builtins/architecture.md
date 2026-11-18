# V8 Builtins Architecture

Builtins are chunks of executable machine code maintained by V8 and pre-compiled at build time by `mksnapshot` into the read-only embedded blob (`EmbeddedData`). They implement JavaScript standard library functions (`Array.prototype.push`, `Promise.prototype.then`, `Object.keys`), Inline Cache (IC) handlers, Ignition bytecode handlers, runtime entry/exit trampolines (`JSEntry`, `CEntry`), and compiler support stubs (allocations, write barriers, lazy compilation, and deoptimization exits).

Every Isolate holds a fixed-size `Builtins` table (`IsolateData::builtin_entry_table()` and `builtin_tier0_entry_table()`) indexed by the `Builtin` enum (`src/builtins/builtins-definitions.h`), allowing generated code to call any builtin via `kRootRegister`-relative addressing or PC-relative jumps within the embedded blob.

---

## 1. Builtin Kinds (`BUILTIN_LIST`)

Builtins are declared in `src/builtins/builtins-definitions.h` (and emitted by Torque into `out/<build>/gen/torque-generated/builtin-definitions.h`) using one of the following macro kinds:

| Macro Kind | Implementation Mechanism | Linkage | Description |
|---|---|---|---|
| `CPP(Name, Argc)` | C++ (`BUILTIN(Name)` in `src/builtins/builtins-*.cc`) | JS Linkage (via `AdaptorWithBuiltinExitFrame`) | Executed in C++ on the host CPU. Convenient for cold or complex specification logic, but incurs a JS-to-C++ `BUILTIN_EXIT` frame transition. |
| `TFJ(Name, Argc, ...)` | `CodeStubAssembler` (CSA) or Torque (`transitioning javascript builtin`) | JS Linkage | Compiled via `CodeAssemblerTurboshaftCompilationJob` (`TurboshaftPipelineKind::kCSA`). Can be installed directly on `SharedFunctionInfo` as a JS-callable function. |
| `TFJ_TSA(Name, Argc, ...)` | `TurboshaftBuiltinsAssembler` (TSA) | JS Linkage | Emits Turboshaft IR directly (`BuildWithTurboshaftAssemblerJS`, `TurboshaftPipelineKind::kTSABuiltin`) with JS linkage. |
| `TFC(Name, Descriptor)` | `CodeStubAssembler` (CSA) | Stub Linkage (`CallInterfaceDescriptor`) | Internal helper stub with custom register/stack parameters specified by `Descriptor` (`Descriptor##Descriptor`). |
| `TFC_TSA(Name, Descriptor)` | `TurboshaftBuiltinsAssembler` (TSA) | Stub Linkage (`CallInterfaceDescriptor`) | Direct Turboshaft internal stub (`BuildWithTurboshaftAssemblerCS`) with custom `CallInterfaceDescriptor`. |
| `TFS(Name, ...)` | `CodeStubAssembler` (CSA) or Torque (`builtin` / `macro`) | Stub Linkage (auto-generated descriptor) | Internal CSA/Torque stub where all declared parameters are `Tagged<Object>` (auto-generates a `Name##Descriptor`). |
| `TFH(Name, Descriptor)` | `CodeStubAssembler` (CSA) | Stub Linkage (`CallInterfaceDescriptor`) | Inline Cache (IC) handler stub compiled via CSA/Turboshaft. |
| `TFH_TSA(Name, Descriptor)` | `TurboshaftBuiltinsAssembler` (TSA) | Stub Linkage (`CallInterfaceDescriptor`) | Direct Turboshaft Inline Cache handler stub. |
| `BCH(Name, OperandScale, Bytecode)` | `InterpreterAssembler` (CSA subclass) | `InterpreterDispatchDescriptor` | Ignition bytecode handler (`IGNITION_HANDLER`) compiled via `CodeAssemblerTurboshaftCompilationJob`. |
| `BCH_TSA(Name, OperandScale, Bytecode)` | `TurboshaftBuiltinsAssembler` (TSA) | `InterpreterDispatchDescriptor` | Ignition bytecode handler (`V_TSA`) compiled directly via `Pipeline::NewBytecodeHandlerCompilationJobTSA`. |
| `ASM(Name, Descriptor)` | Platform `MacroAssembler` (`Builtins::Generate_Name`) | Stub or JS Linkage (`Descriptor`) | Hand-written per-architecture assembly for low-level trampolines (`JSEntry`, `InterpreterEntryTrampoline`, `DeoptimizationEntry_*`, OSR, stack switching). |

---

## 2. Calling Conventions: JS Linkage vs Stub Linkage

### JavaScript Linkage (`TFJ`, `TFJ_TSA`, `CPP`, and `ASM` with `JSTrampolineDescriptor`)
Builtins with JS linkage follow the standard V8 JavaScript calling convention (`Linkage::GetJSCallDescriptor` in `src/compiler/linkage.cc`) so callers do not need to know whether a `JSFunction` is backed by a builtin, Ignition bytecode, or JIT-compiled code:
- **Stack arguments**: The receiver (`this`) and all positional arguments (`arg0 .. arg_{n-1}`) are pushed onto the stack.
- **Fixed register parameters**:
  - `kJavaScriptCallTargetRegister` (`rdi` on x64, `x1` on arm64): The called `JSFunction` closure.
  - `kJavaScriptCallNewTargetRegister` (`rdx` on x64, `x3` on arm64): `new.target` (or `undefined` for non-construct calls).
  - `kJavaScriptCallArgCountRegister` (`rax` on x64, `x0` on arm64): Actual number of arguments passed on the stack (excluding receiver, or including receiver depending on descriptor flags).
  - `kJavaScriptCallDispatchHandleRegister` (`r15` on x64, `x4` on arm64): The `JSDispatchHandle` entry in the `JSDispatchTable` (when `V8_JS_LINKAGE_INCLUDES_DISPATCH_HANDLE_BOOL` is enabled).
  - `kContextRegister` (`rsi` on x64, `x27` on arm64): The current `Context`.
- **Variadic vs fixed arity**:
  - If a `TFJ`/`TFJ_TSA` builtin is declared with `kDontAdaptArgumentsSentinel`, it inspects `kJavaScriptCallArgCountRegister` directly using `CodeStubArguments` (`TorqueStructArguments`) without frame adaptation.
  - If it declares a fixed formal parameter count `Argc`, the prologue automatically handles under-application (filling missing arguments with `undefined`) and cleans up the receiver + `max(actual_argc, formal_argc)` stack slots upon return.
- **`CPP` Builtin Trampoline**: `BuildAdaptor` (`src/builtins/setup-builtins-internal.cc`) wraps each `CPP` C++ function (`Address Builtin_Name(int argc, Address* args, Isolate* isolate)`) in a tiny stub that tail-calls `AdaptorWithBuiltinExitFrame`, constructing a `BUILTIN_EXIT` frame and passing a `BuiltinArguments` view over the stack.

### CodeStub Linkage (`TFC`, `TFC_TSA`, `TFS`, `TFH`, `TFH_TSA`, `BCH`, `BCH_TSA`, `ASM`)
Internal helpers and fast paths avoid stack-argument pushing and JS argument count checks by using **Stub Linkage** (`Linkage::GetStubCallDescriptor`):
- A `CallInterfaceDescriptor` (`src/codegen/interface-descriptors.h`) specifies:
  - How many parameters are passed in registers (`GetRegisterParameterCount()`) and which exact machine registers they use (`GetRegisterParameter(i)`).
  - How many excess parameters are passed on the stack (`GetStackParameterCount()`) and their stack order (`StackArgumentOrder`).
  - Whether a `Context` register is implicitly passed (`HasContextParameter()`).
- Many fast-path builtins split their implementation into a thin `TFJ` JS-linkage wrapper and a `TFS`/`TFC` stub-linkage core so that both JavaScript callers and internal CSA/Torque/Maglev/Turboshaft callers can invoke the core logic with register arguments.

---

## 3. Circular Builtin Compilation During `mksnapshot`

Builtins frequently call or tail-call one another (and can be mutually recursive, e.g. `InterpreterEntryTrampoline`, IC handlers, type-conversion stubs, and runtime trampolines). When `mksnapshot` builds the initial Isolate (`SetupIsolateDelegate::SetupBuiltinsInternal` in `src/builtins/setup-builtins-internal.cc`), it resolves circular dependencies and compiles builtins in parallel across three phases:

```
Phase 1: PopulateWithPlaceholders(isolate)
   │  Populates every slot in Builtins table with a minimal placeholder Code
   │  object carrying its Builtin ID so any builtin can reference any other.
   ▼
Phase 2: Parallel Compilation via BuiltinCompilationScheduler
   │  1. ASM and CPP adaptor builtins are assembled synchronously.
   │  2. BCH/BCH_TSA, TFJ/TFJ_TSA, TFC/TFC_TSA, TFS, and TFH/TFH_TSA jobs are
   │     submitted to BuiltinCompilationScheduler:
   │       - PrepareJob() runs on the main thread.
   │       - ExecuteJob() (Turboshaft optimization + codegen) runs in parallel
   │         on background worker threads in batches.
   │       - FinalizeJob() runs on the main thread in strict finalize_order
   │         for deterministic snapshot heap layouts.
   ▼
Phase 3: ReplacePlaceholders(isolate)
      Walks RelocInfo (CODE_TARGET, RELATIVE_CODE_TARGET, EMBEDDED_OBJECT) of
      every compiled builtin and patches references to placeholder Code objects
      with the final compiled Code / instruction_start().
```

### Why `BuiltinCompilationScheduler` Enforces `finalize_order`
`BuiltinCompilationScheduler` (`src/compiler/code-assembler.{h,cc}`) overlaps main-thread graph creation with background-thread Turboshaft optimization and register allocation (`--concurrent-builtin-generation`). Because V8 snapshots must be bit-for-bit deterministic across builds, background worker threads are forbidden from allocating on the V8 heap; instead, completed jobs are queued and `FinalizeJob` is executed on the main thread in the exact deterministic sequence `finalize_order_` in which jobs were created.

---

## 4. Builtin Profile-Guided Optimization (PGO) and Gearbox

### Builtin PGO (`ProfileDataFromFile`)
Because builtins are compiled ahead of time during `mksnapshot`, they cannot rely on runtime feedback vectors to distinguish hot paths from cold paths. Instead, V8 supports **Builtin PGO** (`src/builtins/profile-data-reader.{h,cc}` and `tools/builtins-pgo/`):
1. **Collection (`--turbo-profiling` / `--turbo-profiling-output`)**: An instrumented build inserts basic-block execution counters into every CSA/TSA/Torque builtin (`BasicBlockProfiler`) and records cross-builtin call counts (`BasicBlockCallGraphProfiler`).
2. **Validation (`GetProfileForBuiltin` in `src/compiler/turboshaft/pipelines.cc`)**: During a PGO-enabled `mksnapshot` build, `ProfileDataFromFile::TryRead(name)` loads the recorded profile for each builtin and checks `profile->hash() == ComputeInitialGraphHash(graph)`. If the builtin's control-flow graph changed since the profile was recorded, the stale profile is safely rejected.
3. **Application (`ProfileApplicationPhase` & `BlockPositioningPhase`)**: Validated block weights mark cold blocks as deferred, guide Turboshaft's `BlockPositioningPhase` to lay out hot paths as straight-line fallthroughs, and allow `EmbeddedFileWriter` (`--reorder-builtins`) to pack frequently co-executed builtins into adjacent OS pages to minimize instruction TLB and I-cache misses.

### Gearbox Multi-Versioned Builtins (`V8_ENABLE_GEARBOX`)
On x64 machines, V8 may be compiled with a conservative baseline ISA requirement while running on modern CPUs that support newer instruction set extensions (such as `SSE4_1` or `AVX`). **Gearbox** (`V8_ENABLE_GEARBOX` in `src/builtins/builtins-definitions.h` and `src/builtins/setup-builtins-internal.cc`) compiles two specialized variants of hot builtins declared with `WITH_GEARBOX(KIND, NAME, ...)`:
- **`NAME_Generic`** (`kGearboxGenericBuiltinIdOffset = -2`): Compiled with the baseline instruction set.
- **`NAME_ISX`** (`kGearboxISXBuiltinIdOffset = -1`): Compiled inside an `InstructionSetExtensionScope` (`CpuFeatures::SetSupported(SSE4_1)`), enabling wider SIMD or specialized instructions in the assembler.
- **`NAME` (Dispatch Slot)**: At Isolate startup (`Builtins::InitializeIsolateDataTables`), V8 checks the host CPU's capabilities and populates the `NAME` entry in the Isolate's `builtin_entry_table` with the entrypoint of either `NAME_ISX` (if supported) or `NAME_Generic`.

---

## 5. Key Files Reference

| File | Purpose |
|---|---|
| `src/builtins/builtins-definitions.h` | Master `BUILTIN_LIST` macro defining `CPP`, `TFJ`, `TFJ_TSA`, `TFC`, `TFC_TSA`, `TFS`, `TFH`, `TFH_TSA`, `BCH`, `BCH_TSA`, `ASM`, and `WITH_GEARBOX` builtins. |
| `src/builtins/builtins.{h,cc}` | `Builtins` table management, linkage queries, and `CodeSandboxingMode` lookup (`Builtins::SandboxingModeOf`). |
| `src/builtins/setup-builtins-internal.cc` | `SetupIsolateDelegate::SetupBuiltinsInternal`, `PopulateWithPlaceholders`, `ReplacePlaceholders`, and Gearbox compilation. |
| `src/builtins/builtins-*-gen.{h,cc}` | Hand-written `CodeStubAssembler` and `TurboshaftBuiltinsAssembler` (`*-tsa.cc`) builtin implementations. |
| `src/builtins/*.tq` | Torque builtin and runtime stub implementations. |
| `src/builtins/builtins-*.cc` | `CPP` builtin implementations (`BUILTIN(Name)`). |
| `src/builtins/<arch>/builtins-<arch>.cc` | Architecture-specific `ASM` builtins (`Builtins::Generate_*`). |
| `src/builtins/profile-data-reader.{h,cc}` | `ProfileDataFromFile` reader for Builtin PGO during `mksnapshot`. |
