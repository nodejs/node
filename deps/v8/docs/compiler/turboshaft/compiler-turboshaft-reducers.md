# Turboshaft Reducers: Composable Optimizations

This document explains the Reducer architecture in Turboshaft, which allows for highly composable and efficient optimization passes.

## The Reducer Pattern

Unlike traditional compilers that often have separate phases for each optimization (each iterating over the graph), Turboshaft uses a stack of **Reducers**.

A reducer is a component that intercepts the creation of IR operations. When an operation is being processed (e.g., during graph building or a copying phase), it passes through the stack of reducers. Each reducer has the opportunity to:
1.  **Modify** the operation (e.g., simplify it).
2.  **Replace** it with a different operation or a constant.
3.  **Ignore** it and pass it to the next reducer in the stack.

## Composing Phases

Reducers are composed using C++ templates (CRTP and variadic templates). A phase is defined by a specific list of reducers.

For example, a phase might be defined as:
```cpp
CopyingPhase<
  DeadCodeEliminationReducer,
  BranchEliminationReducer,
  MachineLoweringReducer
>::Run(data, zone);
```

This creates a stack where `DeadCodeEliminationReducer` is at the top, and `MachineLoweringReducer` is near the bottom.

## How a Reducer Works

A reducer is a template class that takes `Next` as a template parameter and inherits from it. This forms the stack.

> [!NOTE]
> The example below is simplified. In the actual V8 codebase, operations are often more generic (e.g., `WordBinop` instead of `Int32Add`) and take additional parameters like operation kind and representation.

```cpp
template <class Next>
class MyReducer : public Next {
 public:
  // Intercept the reduction of a WordBinop operation
  OpIndex ReduceWordBinop(OpIndex left, OpIndex right, WordBinopOp::Kind kind, WordRepresentation rep) {
    // Try to simplify or optimize (e.g., x + 0 => x)
    if (kind == WordBinopOp::Kind::kAdd && IsConstantZero(right)) return left;
    
    // If we can't optimize, pass it to the next reducer in the stack
    return Next::ReduceWordBinop(left, right, kind, rep);
  }
};
```

### Key Concepts

*   **`Next::Reduce[OpName]`**: Calls the next reducer in the stack. If no reducer overrides it, it eventually reaches the bottom of the stack where the operation is actually emitted into the output graph.
*   **`Asm()`**: A helper to access the full assembler interface, allowing a reducer to emit new operations if needed.

## The Copying Phase

Many Turboshaft phases are **Copying Phases**. They iterate over an existing "input graph" and use the reducer stack to build a new "output graph".

In `src/compiler/turboshaft/copying-phase.h`, `VisitOpNoMappingUpdate` uses a large switch statement to call `Asm().ReduceInputGraph[OpName]`:

```cpp
#define EMIT_INSTR_CASE(Name)                                             \
  case Opcode::k##Name:                                                   \
    if (MayThrow(Opcode::k##Name)) return OpIndex::Invalid();             \
    new_index = Asm().ReduceInputGraph##Name(index, op.Cast<Name##Op>()); \
    break;
```

Note that operations that may throw are handled separately and not directly reduced here.

This triggers the reducer stack for that operation.

## Benefits of the Reducer Architecture

*   **Single Pass**: Multiple optimizations can be performed in a single pass over the graph, reducing memory traffic and increasing speed.
*   **Modularity**: Each optimization is self-contained in a reducer and can be easily tested and reused in different phases.
*   **Flexibility**: It is easy to add, remove, or reorder optimizations by changing the template arguments of a phase.

---

## See Also
-   `src/compiler/turboshaft/reducer-traits.h`: Metaprogramming for reducers.
-   `src/compiler/turboshaft/copying-phase.h`: Implementation of the copying phase.
-   [Turboshaft](compiler-turboshaft.md): Overview of Turboshaft.
