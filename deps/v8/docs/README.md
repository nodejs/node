# V8 Architecture and Engineering Documentation

Welcome to the V8 codebase architecture documentation. These guides are designed for **both human engineers onboarding to V8** (providing a clear conceptual model at the top of each document) and **experienced V8 developers and AI coding agents** (providing exact C++ class names, bitfield layouts, cross-tier invariants, and sharp edges verified against `src/`).

*(Note: The top-level `.md` files directly inside `docs/` are legacy copies of historical [v8.dev/docs](https://v8.dev/docs) articles. The structured subsystem and cross-component documentation lives in the subdirectories indexed below.)*

---

## 1. Parsing, Scopes, and TDZ Hole Check Elision (`docs/parsing/`)

How JavaScript source text is scanned, parsed into an AST, analyzed for variable allocation, and checked for Temporal Dead Zone (TDZ) invariants across tiers:

- **[Parser, Scanner, and AST](parsing/parser-and-ast.md)**: `Scanner` (1-character `c0_` lookahead + 4-token `TokenDesc` ring buffer), off-heap `AstRawString` / `AstConsString` deduplication in `AstValueFactory` (internalized in `Parser::PostProcessParseResult`), `ParserBase<Impl>` CRTP, single-pass ambiguity resolution via `ExpressionScope`, vtable-free `AstNode` bit-packing, `DeclarationScope::Analyze`, and `Rewriter::Rewrite` (`.result`).
- **[Lazy Parsing, `PreParser`, and `PreparseData`](parsing/lazy-parsing-and-preparser.md)**: Eager vs lazy compilation heuristics (IIFE/PIFE detection), `ParserTypes<PreParser>` (`PreParserExpression` 32-bit bitfields), why `PreParser` tracks outer variable references (`VariableProxy`), and how `ProducedPreparseData` / `ConsumedPreparseData` avoid re-preparsing nested functions.
- **[TDZ Hole Check Elision Overview (Cross-Component Pipeline)](parsing/hole-check-elision.md)**: End-to-end overview of how V8 represents lexical TDZ bindings with `tdz_hole_value` (`TdzHole`) and eliminates redundant checks across three compiler stages:
  - **[Stage 1: Parser & Scope Hole Check Elimination](parsing/hole-check-elimination-parser.md)**: Static position analysis in `UpdateNeedsHoleCheck` (`src/ast/scopes.cc`), `ScopeInfo::kMaxVariablePositionDistance`, module import & `kDynamic`/`kDynamicLocal` edge cases, and eliding `LdaTdzHole` via `ForceHoleInitializationFlag`.
  - **[Stage 2: Bytecode Hole Check Elision](parsing/hole-checks-bytecode.md)**: Ignition's 64-bit `Variable::HoleCheckBitmap` (tracking up to 63 variables per function in bits `1..63`), `HoleCheckElisionScope`, and branch intersection via `HoleCheckElisionMergeScope`.
  - **[Stage 3: Optimizing Compiler Hole Check Elision](parsing/hole-checks-optimized.md)**: Maglev (`ValueNode::IsTdzHole()`, `ThrowReferenceErrorIfTdzHole`, `KnownNodeAspects`) and TurboFan / Turboshaft (`TdzHoleConstant`, `RootEqual`) hole check elimination, including how Maglev prevents `LoadContextSlotNoCells` (`loaded_context_constants_`) in the "Inverted Generator" pattern from poisoning `catch` block merges before `ThrowReferenceErrorIfTdzHole`.

---

## 2. Object Model, Hidden Classes, and Sentinels (`docs/objects/`)

How JavaScript values, strings, hidden classes (`Map`s), properties, elements, and internal sentinels are laid out in the V8 heap:

- **[Holes and Internal Sentinels (Cross-Component Guide)](objects/holes-and-sentinels.md)**: Why V8 segregates `ReadOnlyRoots` holes (`TheHole` strictly for array/dictionary elements, `TdzHole` for lexical TDZ & derived `this`, `PropertyCellHole`, `HashTableHole`, `PromiseHole`, `UninitializedHole`, `ExceptionHole`, `TerminationException`, `OptimizedOut`, etc.), `--unmap-holes` 64 KB hardware guard pages (`Hole::kPayloadSize`), `kHoleNanInt64` vs user NaN canonicalization (`DOUBLE_LITERAL` vs `HOLEY_DOUBLE_LITERAL`), and two-phase literal boilerplate initialization (`JSDataObjectBuilder` / `FoldedMutableHeapNumberAllocation` / `gc-observable` stores).
- **[Strings: Representations, Encoding, and Hashing](objects/strings.md)**: `InstanceType` low 7-bit string encoding (`kStringRepresentationMask`, `kIsIndirectStringMask`, `kStringEncodingMask`, `kUncachedExternalStringMask`, Bit 5 `kSharedStringMask`, and Bit 6 `kIsNotInternalizedMask`), `SeqString`, `ConsString` in-place flattening and GC shortcutting, `SlicedString`, `ExternalString` / `UncachedExternalString`, `ThinString`, and `Name::raw_hash_field_` (`kHash`, `kIntegerIndex`, `kForwardingIndex` in `StringForwardingTable`, `kEmpty`).
- **[DescriptorArrays and TransitionArrays](objects/descriptor-and-transition-arrays.md)**: `DescriptorArray` header (`number_of_all_descriptors`, `number_of_descriptors`, slack descriptors, concurrent marking `raw_gc_state`, `EnumCache`), 3-slot entry layout, `FieldType` encoding (`None`, `Any`, `Weak<Map>`), `DescriptorPointer` binary search, `TransitionsAccessor::Encoding` (6 inline/out-of-line states), `TransitionArray`, and `SideStepTransition` (`kCloneObject`, `kObjectAssign`).
- **[Fields, Elements, and Map Generalization](objects/fields-and-elements.md)**: In-object fields vs `PropertyArray`, why `Representation::Double()` fields box values in mutable `HeapNumber`s under pointer compression, `SwissNameDictionary` (SIMD SSE2/NEON H2 probing + insertion-order `meta_table`), `ElementsKind` transition sequence (`kFastElementsKindSequence` vs enum order warning), and `MapUpdater` generalization.
- **[`LookupIterator`: Property Resolution State Machine](objects/lookup-iterator.md)**: `receiver_` vs `lookup_start_object_` (`super.prop`), `holder_`, the `LookupIterator::State` machine (`ACCESS_CHECK`, `INTERCEPTOR`, `JSPROXY`, `TYPED_ARRAY_INDEX_NOT_FOUND`, `DATA`, `ACCESSOR`, `TRANSITION`), two-pass masking vs non-masking `InterceptorState`, and write transitions (`PrepareForDataProperty`, `PrepareTransitionToDataProperty`).

---

## 3. Execution Runtime, ICs, Tiering, and Deoptimization (`docs/runtime/`)

How V8 executes functions, caches property accesses, walks the call stack, unwinds exceptions, tiers up across compilers, and deoptimizes back to bytecode:

- **[Function Architecture (`JSFunction`, `SharedFunctionInfo`, `Context`, `FeedbackVector`)](runtime/function-architecture.md)**: Separation of per-definition `SharedFunctionInfo` (dual `trusted_function_data_` and `untrusted_function_data_` slots) from per-closure `JSFunction` (`context()` invariant pointing to the creation scope's `Context`), `JSDispatchTable` dispatch (`dispatch_handle_`), and the three-stage `FeedbackCell` lifecycle (`Undefined -> ClosureFeedbackCellArray -> FeedbackVector`).
- **[Scopes, Contexts, `ScopeInfo`, and `ContextCell`](runtime/scopes-and-scope-infos.md)**: Compile-time `Scope` analysis vs persistent `ScopeInfo` bit-packing, runtime `Context` chains, and `ContextCell` state specialization (`kConst`, `kSmi`, unboxed `kInt32`, unboxed `kFloat64`, and `kDetached` with `DependentCode` deoptimization).
- **[Hidden Classes and Inline Caches (ICs)](runtime/hidden-classes-and-ics.md)**: IC state lattice (`UNINITIALIZED -> MONOMORPHIC -> POLYMORPHIC -> MEGAMORPHIC`), `LoadHandler` and `StoreHandler` Smi bitfields, variable-length `DataHandler` (`smi_handler_`, `validity_cell_`, `data1..data5`), prototype validity cells, and the three per-Isolate `StubCache`s (`load_stub_cache_`, `store_stub_cache_`, `define_own_stub_cache_` with `primary_` and `secondary_` tables).
- **[Hidden Classes Step-by-Step Tutorial](runtime/hidden-classes-tutorial.md)**: Walkthrough of hidden-class transitions, shared `DescriptorArray` prefixes, branching `TransitionArray`s, and `PropertyConstness::kConst` constant folding (`DependentCode::kFieldConstGroup`).
- **[Execution Tiers and the `TieringManager`](runtime/tiering.md)**: How `FeedbackCell::interrupt_budget_` drives `TieringManager::OnInterruptTick`, feedback allocation + Sparkplug batch compilation on the 1st interrupt tick, IC stabilization (`NotifyICChanged`), `JSDispatchTable::SetTieringRequest` (`TieringBuiltin`), and On-Stack Replacement (`FeedbackVector::osr_urgency`, `JumpLoop` `loop_depth`, and Maglev-to-TurboFan deopt-to-OSR).
- **[Deoptimization Architecture](runtime/deoptimization.md)**: `DeoptimizeKind::kEager` vs `kLazy`, trusted-space `DeoptimizationData` (`DeoptimizationLiteralArray` vs `ProtectedDeoptimizationLiteralArray`), `DeoptimizationFrameTranslation` opcodes (`INTERPRETED_FRAME_WITH_RETURN` vs `WITHOUT_RETURN`, `DOUBLE_LITERAL` vs `HOLEY_DOUBLE_LITERAL`, `CAPTURED_OBJECT`), and two-phase no-GC stack rewriting followed by GC-allowed heap materialization.
- **[Exception Handling](runtime/exception-handling.md)**: `Isolate::pending_exception` vs `scheduled_exception`, `TerminationException`, `HandlerTable` encodings (`kRangeBasedEncoding` with `CatchPrediction` + `HandlerWasUsed` vs `kReturnAddressBasedEncoding`), `kNoHandlerFound = -1` vs `kLazyDeopt = HandlerOffsetField::kMax`, how Sparkplug handles exceptions directly via `BytecodeArray`'s `HandlerTable` without deoptimizing, and `finally` token re-dispatch.
- **[Stack Walking and Frame Layouts](runtime/stack-walking.md)**: Frame pointer (`fp`) chains, `Context` vs `Smi` `StackFrame::Type` markers (`INTERPRETED`, `BASELINE`, `MAGLEV`, `TURBOFAN_JS`, `ENTRY`, `EXIT`, `BUILTIN_CONTINUATION`, `WASM`), `c_entry_fp` bridging, `StackFrameIterator` vs signal-safe `StackFrameIteratorForProfiler` (and why it must never be `V8_EXPORT_PRIVATE`), and Conservative Stack Scanning (`CSS`) with `SafepointTable`s.

---

## 4. Interpreter, Compilers, Codegen, Builtins, and Torque

- **Ignition Interpreter (`docs/interpreter/`)**:
  - [Ignition Interpreter & Bytecode Architecture](interpreter/interpreter-ignition.md)
- **Sparkplug, Maglev, TurboFan, and Turboshaft (`docs/compiler/`)**:
  - [Sparkplug Baseline Compiler](compiler/sparkplug/compiler-sparkplug.md)
  - [Maglev Mid-Tier Optimizing Compiler](compiler/maglev/compiler-maglev.md)
  - [Maglev Register Allocator](compiler/maglev/compiler-maglev-regalloc.md)
  - [TurboFan Sea-of-Nodes Compiler](compiler/turbofan/compiler-turbofan.md)
  - [Why Control-Flow Graphs (CFG) over Sea-of-Nodes](compiler/why-cfg.md)
  - [Turboshaft Architecture Overview](compiler/turboshaft/compiler-turboshaft.md)
  - [Turboshaft IR Operations & Types](compiler/turboshaft/compiler-turboshaft-ir.md)
  - [Turboshaft Reducers & Optimization Phases](compiler/turboshaft/compiler-turboshaft-reducers.md)
  - [Turboshaft Graph-Copying Approach](compiler/turboshaft/compiler-turboshaft-copying-approach.md)
  - [Concurrency, Background Compilation, and `HeapBroker`](compiler/concurrency-and-background-compilation.md)
- **Code Generation & Assemblers (`docs/codegen/`)**:
  - [Assembler & MacroAssembler Architecture](codegen/assembler-architecture.md)
  - [CodeStubAssembler (CSA) & Turboshaft Builtins Assembler (TSA)](codegen/code-stub-assembler.md)
  - [CodeStubAssembler Tutorial](codegen/csa-tutorial.md)
- **Builtins & Torque (`docs/builtins/`, `docs/torque/`)**:
  - [V8 Builtins Architecture](builtins/architecture.md)
  - [Torque Language Tutorial](builtins/torque-tutorial.md)
  - [Torque Compiler Architecture](torque/architecture.md)
  - [Torque User Manual](torque/user-manual.md)

---

## 5. Heap, Memory Management, Sandbox, Snapshot, Wasm, RegExp, and Security

- **Heap & Garbage Collection (`docs/heap/`)**:
  - [Heap Spaces & Memory Layout Overview](heap/heap-overview.md)
  - [Heap Objects, Maps, and C++ Object Macros](heap/objects-and-maps.md)
  - [Pointer Tagging (`Smi`, `HeapObject`, `MaybeObject`)](heap/pointer-tagging.md)
  - [Pointer Compression (`PtrComprCageBase`)](heap/pointer-compression.md)
  - [Garbage Collection & Orinoco Architecture](heap/garbage-collection.md)
  - [Tri-Color Marking, Write Barriers, and Sweeping](heap/marking-and-sweeping.md)
  - [Evacuation, Compaction, and Slot Updates](heap/compaction.md)
  - [Handles (`DirectHandle`, `IndirectHandle`, `LocalHandles`, `TracedHandles`)](heap/handles.md)
  - [Roots (`ReadOnlyRoots`, `RootsTable`, Static Roots)](heap/roots.md)
  - [Zone Arena Allocator (`Zone`, `AccountingAllocator`)](heap/zone-allocator.md)
- **V8 Sandbox & Startup Snapshots (`docs/sandbox/`, `docs/snapshot/`)**:
  - [V8 Sandbox, Trusted Space, and Pointer Tables](sandbox/architecture.md)
  - [Startup Snapshot Serialization & Deserialization](snapshot/architecture.md)
- **WebAssembly, RegExp, and Security (`docs/wasm/`, `docs/regexp/`, `docs/security/`)**:
  - [WebAssembly Compilation & Runtime Architecture](wasm/architecture.md)
  - [Liftoff WebAssembly Baseline Compiler](wasm/liftoff.md)
  - [Irregexp Regular Expression Engine Architecture](regexp/architecture.md)
  - [V8 Security Bug Triaging Guide](security/triaging.md)
  - [Reproducing Security Bugs](security/reproducing-bugs.md)
