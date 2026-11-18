# Turboshaft Reducers & Assembler Architecture

In Turboshaft (`src/compiler/turboshaft/`), all graph construction, lowering, and optimization passes are implemented as composable C++ class templates called **Reducers**, stacked together inside an **`Assembler`** (`src/compiler/turboshaft/assembler.h`, `reducer-traits.h`, `define-assembler-macros.inc`).

---

## 1. Conceptual Overview: The Reducer Stack

Instead of running separate passes that mutate a single graph in place through virtual method dispatch, a Turboshaft phase instantiates a single concrete **`Assembler<Reducers...>`** type whose inheritance hierarchy forms a static compile-time pipeline:

```
  Top of Stack (Entry Point: Asm().ReduceFoo(...) or Asm().ReduceInputGraphFoo(...))
  +-------------------------------------------------------------------+
  | GraphVisitor<Next>                (if running inside CopyingPhase)|
  +-------------------------------------------------------------------+
  | VariableReducer<Next>             ( + RequiredOptimizationReducer)|
  +-------------------------------------------------------------------+
  | Custom Reducer 1 (e.g., MachineLoweringReducer<Next>)             |
  +-------------------------------------------------------------------+
  | Custom Reducer 2 (e.g., FastApiCallLoweringReducer<Next>)         |
  +-------------------------------------------------------------------+
  | Custom Reducer 3 (e.g., MachineOptimizationReducer<Next>)         |
  +-------------------------------------------------------------------+
  | EmitProjectionReducer<Next>       (auto-inserted before GVN)      |
  +-------------------------------------------------------------------+
  | ValueNumberingReducer<Next>       (optional GVN)                  |
  +-------------------------------------------------------------------+
  | AssemblerOpInterface<Next>        (auto-inserted convenience API) |
  +-------------------------------------------------------------------+
  | ReducerBase<Next>                 (auto-inserted default handler) |
  +-------------------------------------------------------------------+
  | GraphEmitter<Next>                (auto-inserted bottom emitter)  |
  +-------------------------------------------------------------------+
  | StackBottom<Assembler, ReducerList>                               |
  +-------------------------------------------------------------------+
```

Because each reducer is a template `template <class Next> class MyReducer : public Next`, calling `Next::ReduceFoo(...)` is a direct, devirtualized C++ method call that the compiler inlines across the entire stack.

---

## 2. How `BuildReducerStack` and `Assembler` Work

When a phase declares `Assembler<GraphVisitor, MyReducer1, MyReducer2>` (or `CopyingPhase<MyReducer1, MyReducer2>`), **`detail::BuildReducerStack`** (`src/compiler/turboshaft/assembler.h`) automatically splices in the required infrastructure layers using template metaprogramming (`src/compiler/turboshaft/reducer-traits.h`):

1. **`GraphVisitor<AfterNext>`** (when using `CopyingPhase`):
   Inherits from `OutputGraphAssembler<GraphVisitor<AfterNext>, VariableReducer<AfterNext>>`, which automatically pulls in **`VariableReducer`** and **`RequiredOptimizationReducer`** right below `GraphVisitor`.
2. **User Reducers (`Reducers...`)**:
   Instantiated in the exact order listed in the phase definition.
3. **`EmitProjectionReducer`**:
   Automatically inserted immediately *before* `ValueNumberingReducer` (if `ValueNumberingReducer` is in the stack) or else right before `AssemblerOpInterface`. Whenever an operation with multiple outputs (`op.outputs_rep().size() > 1`, such as `OverflowCheckedBinopOp` or a multi-return `CallOp`) is emitted, `EmitProjectionReducer` automatically emits the trailing `ProjectionOp`s for each output and wraps them in a `MakeTupleOp` (`WrapInTupleIfNeeded`). Placing it above `ValueNumberingReducer` allows `ValueNumberingReducer` to GVN both the multi-output operation and its individual `ProjectionOp`s.
4. **Bottom Infrastructure Trio (`AssemblerOpInterface`, `ReducerBase`, `GraphEmitter`)**:
   * **`AssemblerOpInterface`**: Provides hundreds of ergonomic helper methods on `Asm()` (such as `__ Word32Add(a, b)`, `__ SmiConstant(x)`, `__ LoadField(obj, field)`). It resolves `ConstOrV<T>` arguments into `ConstantOp`s and dispatches to the top of the reducer stack (`Asm().ReduceWordBinop(...)`).
   * **`ReducerBase`**: Provides default fallback implementations for every `ReduceInputGraphFoo` and `ReduceFoo` method, plus control-flow helpers (`Bind`, `Goto`, `Branch`) and `CatchScope` wiring.
   * **`GraphEmitter`**: Sits at the very bottom of the stack (`Next` is `StackBottom`). Its `ReduceFoo(args...)` method finally allocates the `FooOp` in `output_graph().Allocate(...)` inside `OperationBuffer`.

### `TURBOSHAFT_REDUCER_BOILERPLATE(Name)`
Every reducer class begins with `TURBOSHAFT_REDUCER_BOILERPLATE(Name)` (`src/compiler/turboshaft/assembler.h`), which expands to:

```cpp
// src/compiler/turboshaft/assembler.h
#define TURBOSHAFT_REDUCER_GENERIC_BOILERPLATE(Name)                    \
  using ReducerList = typename Next::ReducerList;                       \
  using assembler_t = typename Next::assembler_t;                       \
  assembler_t& Asm() { return *static_cast<assembler_t*>(this); }       \
  template <class T>                                                    \
  using ScopedVar = compiler::turboshaft::ScopedVar<T, assembler_t>;    \
  using CatchScope = compiler::turboshaft::CatchScopeImpl<assembler_t>; \
  static constexpr auto& ReducerName() { return #Name; }

#define TURBOSHAFT_REDUCER_BOILERPLATE(Name)   \
  TURBOSHAFT_REDUCER_GENERIC_BOILERPLATE(Name) \
  using node_t = typename Next::node_t;        \
  using block_t = typename Next::block_t;
```

Key mechanics provided by the boilerplate:
* **`Asm()`** downcasts `this` via `static_cast<assembler_t*>(this)` to the concrete outer `Assembler` type. Inside reducer code (where `#define __ Asm().` is active from `define-assembler-macros.inc`), writing `__ Word32Add(a, b)` invokes `Asm().Word32Add(a, b)`, sending the newly synthesized operation through the **entire** reducer stack from the top, whereas calling `Next::ReduceWordBinop(...)` forwards only to the reducers **below** the current reducer.
* **`ReducerName()`** provides the compile-time string name used by `--turboshaft-trace-intermediate-reductions`.
* **`ScopedVar<T>`** and **`CatchScope`** bind the `Var` and exception-scope types to the concrete `assembler_t`.

---

## 3. `REDUCE_INPUT_GRAPH(Foo)` vs. `REDUCE(Foo)`

Turboshaft provides two distinct interception hooks for every operation `Foo` (`src/compiler/turboshaft/define-assembler-macros.inc`):

```
  [ Input Graph: FooOp at OpIndex ig_index ]
                       |
                       v
       Asm().ReduceInputGraphFoo(ig_index, op)
       +---------------------------------------------------+
       | Reducer 1::ReduceInputGraphFoo(ig_index, op)      |
       |   (Pre-mapping: op.inputs() are still old-graph   |
       |    OpIndices; ig_index is valid in input_graph!)  |
       +---------------------------------------------------+
                       | (falls through to ReducerBase::ReduceInputGraphFoo)
                       v
       GraphVisitor::AssembleOutputGraphFoo(op)
       (Calls op.Explode: maps every input via MapToNewGraph)
                       |
                       v
       Asm().ReduceFoo(mapped_input_0, mapped_input_1, options...)
       +---------------------------------------------------+
       | Reducer 1::ReduceFoo(mapped_args...)              | <-- Also called when a
       | Reducer 2::ReduceFoo(mapped_args...)              |     reducer synthesizes
       |   (Post-mapping: all inputs are output_graph      |     `__ Foo(...)`!
       |    V<T> / OpIndices!)                             |
       +---------------------------------------------------+
                       |
                       v
       GraphEmitter::ReduceFoo(mapped_args...)
       (Allocates FooOp in output_graph OperationBuffer)
```

### Detailed Comparison

| Hook Macro | Signature | Input Graph vs. Output Graph State | When to Use |
| :--- | :--- | :--- | :--- |
| **`REDUCE_INPUT_GRAPH(Foo)`** (`ReduceInputGraphFoo`) | `V<Rep> REDUCE_INPUT_GRAPH(Foo)(V<Rep> ig_index, const FooOp& op)` | **`ig_index` and `op.inputs()` belong to `input_graph()`!** If you need the corresponding `output_graph` value of an input, you must call `__ MapToNewGraph(op.input(i))`. | Use **only** when a reducer needs: (1) the original `ig_index` to query an pre-computed analysis side table on `input_graph()` (e.g., `BranchEliminationReducer`, `LoopUnrollingReducer`), or (2) to inspect/skip the raw `FooOp` before its inputs are mapped. **Note:** `ReduceInputGraphFoo` is *not* called for new `Foo` operations synthesized on the fly via `__ Foo(...)`! |
| **`REDUCE(Foo)`** (`ReduceFoo`) | `V<Rep> REDUCE(Foo)(V<Arg0> a, V<Arg1> b, Options... options)` | **All `V<...>` arguments already belong to `output_graph()`!** | **Default for almost all optimizations and lowerings.** Called both when copying an operation from `input_graph` (after `AssembleOutputGraphFoo` maps its inputs) *and* whenever an upstream reducer synthesizes a new `Foo` operation via `__ Foo(...)`. |

### Debug Verification in `REDUCE` and `REDUCE_INPUT_GRAPH`
In `#ifdef DEBUG` (`src/compiler/turboshaft/define-assembler-macros.inc`):
* `REDUCE(Foo)` evaluates `std::is_same_v<FooOp, FooOp>` at compile time so a typo in `REDUCE(WordBinoptypo)` fails to compile immediately instead of silently defining an unused dead function.
* It wraps `ReduceFooHelper` with indentation-aware tracing when `--turboshaft-trace-intermediate-reductions` is enabled.

### `UniformReducerAdapter`
If a reducer wants to intercept *every* operation uniformly (for example, `ValueNumberingReducer`, `EmitProjectionReducer`, or a tracing/verification reducer), it can inherit from **`UniformReducerAdapter<Derived, Next>`** (`src/compiler/turboshaft/uniform-reducer-adapter.h`) and implement:
```cpp
template <Opcode opcode, typename Continuation, typename... Args>
OpIndex ReduceOperation(Args... args) {
  // Pre-processing...
  OpIndex res = Continuation{this}.Reduce(args...);
  // Post-processing...
  return res;
}
```

---

## 4. Control-Flow Macros & Scoped Variables (`define-assembler-macros.inc`)

Inside any reducer file that includes `"src/compiler/turboshaft/define-assembler-macros.inc"` (and `"src/compiler/turboshaft/undef-assembler-macros.inc"` at the end), reducers can emit structured CFG control flow that looks like natural C++ code:

### 1. `IF` / `IF_NOT` / `ELSE`
```cpp
IF ( __ Word32Equal(x, 0) ) {
  // Emitted into true block
} ELSE {
  // Emitted into false block
}
// Automatically binds merge block if either branch falls through!
```
* **How the Macro Works**: Because C++ `if`/`else` only executes one branch at compile time, the `IF(...)` macro (`src/compiler/turboshaft/define-assembler-macros.inc:129-143`) expands into a 3-iteration `for (int iteration = 0, bound = false; iteration < 3; ...)` loop around a `ControlFlowHelper_IfState`:
  * `iteration == 0`: Calls `Asm().ControlFlowHelper_BindIf(cond, &state)` to emit the `BranchOp`, bind the `if_true` block, execute the C++ `if` body, and jump to the merge block (`FinishIfBlock`).
  * `iteration == 1`: If an `ELSE` clause is present, calls `ControlFlowHelper_BindElse(&state)` to bind the `if_false` block, execute the `ELSE` body, and jump to the merge block.
  * `iteration == 2`: Calls `ControlFlowHelper_EndIf(&state)` to bind the merge block (if reachable from either arm).

### 2. `Label<Ts...>`, `LoopLabel<Ts...>`, `GOTO`, `GOTO_IF`, `BIND`, `BIND_LOOP`
For multi-way branches, early exits, or loops with explicit SSA phi parameters:
* **`Label<Word32, Object> done(this)`**: Declares an unbound merge block expecting `V<Word32>` and `V<Object>` values along every incoming edge.
  * `GOTO(done, result_int, result_obj)` or `GOTO_IF(cond, done, result_int, result_obj)` records the incoming values and jumps to `done`.
  * `BIND(done, phi_int, phi_obj)` binds the block and uses C++17 structured bindings to unpack the automatically materialized `PhiOp`s (or single-predecessor pass-through values).
* **`LoopLabel<Word32> loop(this)`** and **`BIND_LOOP(loop, index)`**:
  * A `LoopLabel` manages both a `loop_header` block and a single `backedge` merge block.
  * The first `GOTO(loop, initial_index)` jumps to `loop_header`.
  * `BIND_LOOP(loop, index)` binds `loop_header`, emits `PendingLoopPhiOp`s for each loop parameter, runs the C++ loop body once, and in its `for`-loop post-step (`EndLoop`) binds the backedge block, emits the backedge `GotoOp` to `loop_header`, and calls `FixLoopPhis` to replace each `PendingLoopPhiOp` in-place with a 2-input `PhiOp`.

### 3. `ScopedVar<T>` vs. `Var<T>`
Instead of manually passing SSA values as arguments to `Label<Ts...>`, reducers can declare mutable CFG variables backed by `VariableReducer` (`src/compiler/turboshaft/assembler.h:1024-1093`):
```cpp
ScopedVar<Word32> count(this, 0);
IF (cond) {
  count = __ Word32Add(count, 1);
}
V<Word32> final_count = count; // Automatically inserts a PhiOp at the merge if modified!
```
* **`ScopedVar<T, Assembler>`** (aliased as `ScopedVar<T>` inside reducers) inherits from `Var<T, Assembler>`. When `ScopedVar` goes out of C++ scope, its destructor automatically calls `assembler_.SetVariable(*this, OpIndex::Invalid())`, removing the variable from `VariableTable::active_loop_variables` so subsequent loops do not wastefully allocate `PendingLoopPhiOp`s for a dead variable.

### 4. `LABEL_BLOCK(no_change)`
When writing pattern-matching reductions in `REDUCE(Foo)` that have multiple bailout conditions, `LABEL_BLOCK(no_change)` (`src/compiler/turboshaft/assembler.h:1106`) lets you write the fallback `Next::ReduceFoo(...)` call once at the top of the function:
```cpp
V<Word> REDUCE(WordBinop)(V<Word> left, V<Word> right, WordBinopOp::Kind kind, WordRepresentation rep) {
  LABEL_BLOCK(no_change) {
    return Next::ReduceWordBinop(left, right, kind, rep);
  }
  if (!ShouldOptimize(kind)) goto no_change;
  ...
}
```
It expands to `for (; false; UNREACHABLE()) no_change:`, so normal execution skips the block on entry, and any `goto no_change;` jumps into the block and returns `Next::ReduceWordBinop(...)`.
