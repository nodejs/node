# TurboFan & The Top-Level Optimizing Pipeline

TurboFan (`src/compiler/`) is V8's top-tier optimizing compiler, responsible for generating peak-performance machine code for hot JavaScript and WebAssembly functions.

Over the past several years, V8's top-tier compilation architecture has evolved from a pure **Sea-of-Nodes (SoN)** pipeline into a hybrid pipeline centered around **Turboshaft** (a cache-friendly Control-Flow Graph IR) and **Turbolev** (which uses Maglev's fast graph builder and mid-tier passes as the frontend for Turboshaft).

---

## 1. Conceptual Overview: The Modern Top-Tier Pipeline

Today, when a JavaScript function tiers up to TurboFan (`Pipeline::GenerateCode` in `src/compiler/pipeline.cc`), V8 has two frontend paths that both converge into the **Turboshaft** backend:

```
                        [ Ignition Bytecode + FeedbackVector ]
                                         |
                    +--------------------+--------------------+
                    |                                         |
     Default: Turbolev (--turbolev)            Legacy: Sea-of-Nodes (--no-turbolev)
     ==============================            ====================================
     1. TurbolevFrontendPipeline:              1. JSGraphBuilder (BytecodeGraphBuilder)
        - MaglevGraphBuilderPhase              2. Inlining & Early SoN Reducers
        - InlinerPhase                            (Typer, TypedLowering, EscapeAnalysis,
        - LoopPeelerPhase                          SimplifiedLowering)
        - TruncationPhase                      3. ComputeScheduledGraph()
        - PrePhiUntagging & PhiUntagging          (Schedule + Sea-of-Nodes Graph)
        - RangeAnalysis & PostOptimizer                       |
        - EscapeAnalysisPhase                                 v
        - PostHocPhase & DeadNodeSweeping      4. BuildGraphPhase
                    |                             (Schedule -> Turboshaft CFG)
                    v                                         |
     2. TurbolevGraphBuildingPhase                            |
        (Maglev CFG -> Turboshaft CFG)                        |
                    |                                         |
                    +--------------------+--------------------+
                                         |
                                         v
                    [ Turboshaft CFG Pipeline (Mandatory) ]
                    =======================================
                    - MachineLoweringPhase / FastApiCallLowering
                    - LoopUnrollingPhase / StoreStoreEliminationPhase
                    - OptimizePhase (Structural & Value Reducers)
                    - TypedOptimizations / DecompressionOptimization
                    - InstructionSelectionPhase (InstructionSelector)
                    - Register Allocation & Code Assembly
```

### Critical Architectural Realities

1. **Turbolev Does NOT Use Sea of Nodes**:
   When `--turbolev` is active, V8 does **not** convert Maglev IR into a Sea-of-Nodes graph. Instead, `TurbolevFrontendPipeline` (`src/compiler/turboshaft/turbolev-frontend-pipeline.cc`) builds and optimizes a Maglev CFG directly, and `TurbolevGraphBuildingPhase` (`src/compiler/turboshaft/turbolev-graph-builder.cc`) translates the Maglev CFG directly into a Turboshaft CFG, bypassing Sea of Nodes completely.
2. **Turboshaft Is Mandatory for Top-Tier Compilation**:
   `--no-turboshaft` implies `--no-turbofan` in V8's flag definitions (`src/flags/flag-definitions.h`). The backend (`InstructionSelector` in `src/compiler/backend/instruction-selector.cc`) operates **exclusively** on Turboshaft `Graph`s (`OpIndex` and `Block`); the legacy Sea-of-Nodes instruction selector path has been removed.
3. **Sea of Nodes Is Legacy Frontend Only**:
   Sea of Nodes (`src/compiler/turbofan-graph.h`, `src/compiler/node.h`) is now used only when `--no-turbolev` is passed (or for specific internal/testing pipelines), and only for early frontend phases up to `PipelineImpl::ComputeScheduledGraph()`, where it is immediately lowered into Turboshaft via `turboshaft::BuildGraphPhase`.

---

## 2. Entry Points in `src/compiler/pipeline.cc`

Top-tier compilation jobs (`TurbofanCompilationJob` in `src/compiler/pipeline.cc`) follow a three-stage lifecycle managed by `PipelineImpl`:

1. **`PrepareJobImpl(Isolate*)` (Main Thread)**:
   Initializes the `JSHeapBroker`, pins root handles, records compilation dependencies, and configures `PipelineData` (`src/compiler/turboshaft/phase.h`).
2. **`ExecuteJobImpl(RuntimeCallStats*, LocalIsolate*)` (Background Thread)**:
   Dispatches based on `v8_flags.turbolev`:
   * **Turbolev Path (`PipelineImpl::CreateGraphWithMaglev()`)**:
     Runs `turboshaft::TurbolevFrontendPipeline` (producing an optimized `maglev::Graph*`), then runs `turboshaft::TurbolevGraphBuildingPhase` to populate `data->graph()` (the Turboshaft `Graph`), followed by `OptimizeTurboshaftGraph()` and `AssembleCode()`.
   * **Sea-of-Nodes Path (`PipelineImpl::CreateGraph()`)**:
     Runs the Sea-of-Nodes phases (`GraphBuilderPhase`, `InliningPhase`, `TyperPhase`, `TypedLoweringPhase`, `LoopPeelingPhase`, `LoadEliminationPhase`, `EscapeAnalysisPhase`, `SimplifiedLoweringPhase`), calls `ComputeScheduledGraph()` (`SchedulePhase`), converts the scheduled SoN graph to Turboshaft via `turboshaft::BuildGraphPhase`, and then runs the shared Turboshaft optimization and backend phases.
3. **`FinalizeJobImpl(Isolate*)` (Main Thread)**:
   Validates and commits `CompilationDependencies` (`deps->Commit(code)`) and installs the generated `Code` object.

---

## 3. Legacy Sea-of-Nodes Frontend Concepts (`--no-turbolev`)

When investigating or maintaining the legacy Sea-of-Nodes frontend (`src/compiler/`), several core data structures and invariants apply. (For why V8 migrated away from Sea of Nodes to CFG IRs, see [Why CFG (`why-cfg.md`)](../why-cfg.md).)

### Value, Effect, and Control Edges (`src/compiler/node.h`)
In a Sea-of-Nodes `TFGraph`, basic blocks are not explicit containers. Every operation is a `Node*` connected by three distinct kinds of directed edges in a single unified graph:
* **Value Edges**: Data dependencies (e.g., the two numeric operands of `SpeculativeSafeIntegerAdd`).
* **Effect Edges**: Ordering constraints for operations that read or write mutable state (heap loads, stores, calls, checks). Effect edges form an explicit effect chain (`EffectPhi` at control merges) so pure value nodes can float freely while stateful operations cannot be reordered across interfering side effects.
* **Control Edges**: Execution dependencies (`Start`, `Branch`, `IfTrue`, `IfFalse`, `Merge`, `Loop`, `End`).

### Key Sea-of-Nodes Frontend Phases
1. **`BytecodeGraphBuilder` (`src/compiler/bytecode-graph-builder.cc`)**:
   Walks the `BytecodeArray`, maintaining an `Environment` mapping Ignition registers and the accumulator to `Node*` values, alongside the current `effect` and `control` nodes, emitting `JS*` operators (`JSCall`, `JSLoadNamed`, `JSAdd`).
2. **`JSTypedLowering` & `JSInliningHeuristic` (`src/compiler/js-inlining-heuristic.cc`)**:
   Uses `JSHeapBroker` feedback (`JSCallReducer`, `JSNativeContextSpecialization`) to inline callees and lower generic `JS*` nodes into `Simplified` operators (`CheckMaps`, `LoadField`, `SpeculativeNumberAdd`).
3. **`TyperPhase` (`src/compiler/turbofan-typer.cc`)**:
   Computes lattice `Type` bounds (ranges, heap constants, NaN/MinusZero flags) for every `Node*`.
4. **`LoadElimination` & `EscapeAnalysis` (`src/compiler/load-elimination.cc`, `turbofan-escape-analysis.cc`)**:
   Eliminates redundant field/element loads along the effect chain and scalar-replaces non-escaping allocations (`Allocate` -> `FinishRegion`) into virtual state attached to `FrameState` nodes.
5. **`SimplifiedLoweringPhase` (`src/compiler/simplified-lowering.cc`)**:
   Runs a three-stage representation selection pass (`PropagatePhase` backward, `RetypePhase` forward, `LowerPhase` forward) using `RepresentationChanger` to convert `Simplified` nodes into `Machine` operators (`Int32Add`, `Float64Add`, `ChangeInt32ToFloat64`).
6. **`ComputeScheduledGraph()` (`src/compiler/scheduler.cc`)**:
   Runs `Scheduler::ComputeSchedule`, which places floating Sea-of-Nodes operations into explicit basic blocks (`Schedule`) using dominance, post-dominance, and loop nesting analysis, preparing the graph for `turboshaft::BuildGraphPhase`.

---

## 4. Related Compiler Documentation

* [Why CFG (`why-cfg.md`)](../why-cfg.md): Detailed comparison of Sea of Nodes vs. CFG IRs in Maglev and Turboshaft.
* [Concurrency & Background Compilation (`concurrency-and-background-compilation.md`)](../concurrency-and-background-compilation.md): `JSHeapBroker`, `RefSerializationKind`, `TryMakeRef`, and `CompilationDependencies`.
* [Maglev Compiler (`../maglev/compiler-maglev.md`)](../maglev/compiler-maglev.md): Maglev's CFG IR, `KnownNodeAspects`, and optimization passes (also shared with Turbolev).
* [Turboshaft Architecture (`../turboshaft/compiler-turboshaft.md`)](../turboshaft/compiler-turboshaft.md): `TurbolevFrontendPipeline`, Turboshaft phases, and `InstructionSelector`.
* [Turboshaft IR (`../turboshaft/compiler-turboshaft-ir.md`)](../turboshaft/compiler-turboshaft-ir.md): `OperationBuffer`, `OpIndex`, `V<T>`, and `THROWING_OP_BOILERPLATE`.
* [Turboshaft Reducers (`../turboshaft/compiler-turboshaft-reducers.md`)](../turboshaft/compiler-turboshaft-reducers.md): Reducer stacks, `Assembler`, and control-flow macros.
* [Turboshaft Copying Approach (`../turboshaft/compiler-turboshaft-copying-approach.md`)](../turboshaft/compiler-turboshaft-copying-approach.md): `GraphVisitor`, `SnapshotTable`, `VariableReducer`, and block cloning.
