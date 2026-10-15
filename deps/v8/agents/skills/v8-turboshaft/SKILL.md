---
name: v8-turboshaft
description: Turboshaft C++ coding conventions. Use when writing, modifying, or reviewing code in src/compiler/turboshaft/.
---

# Turboshaft Coding Conventions

- Prefer `V<T>` and `OptionalV<T>` over raw `OpIndex` and `OptionalOpIndex`
  (e.g., `V<Word32>`, `V<JSReceiver>`, `V<Any>`, `V<None>`). Use
  `V<T>::Invalid()` instead of `OpIndex::Invalid()`.

- Use the most precise `V<T>` type possible (e.g., `V<JSArray>` over
  `V<JSReceiver>` over `V<Object>`). Define new union types when needed:
  `Union<...>` for tagged types and `UntaggedUnion<...>` for untagged types.

- Use `V<T>::Cast` only when downcasting another `V<U>`. Never use `V<T>::Cast`
  on `OpIndex` or for upcasting (`V<T>` is implicitly constructible from
  `OpIndex` and from `V<SubType>`). For calls, use `__ Call<T>(...)` instead of
  `V<T>::Cast(__ Call(...))`.

- Prefer structured control flow (`IF` / `IF_NOT` / `ELSE`, using `ScopedVar<T>`
  to merge values across branches) over `Label<T>` with `GOTO` / `GOTO_IF` /
  `GOTO_IF_NOT` when possible.

- With `ScopedVar<T>`, use direct assignment (`var = ...`) and implicit reads
  instead of `.Set()` / `.Get()`.

- Avoid explicit `__ Word32Constant(...)` / `__ Float64Constant(...)` calls:
  most functions taking untagged value inputs use `ConstOrV<T>` parameters (and
  new helpers should too), which accept raw C++ constants directly.

- When matching operations, prefer a single `op.TryCast<Op>()` (with
  `Opmask::k...` when checking `Kind`/operands) over `op.Is<Op>()` +
  `op.Cast<Op>()`.

- Use named, typed input getters on operations (e.g., `op.left()`, `op.input()`)
  instead of generic indexed `op.input(0)` (and define them on new operations in
  `operations.h`).

- Prefer `BuiltinCallDescriptor` with `__ CallBuiltin<builtin::Foo>(...)` over
  manual `Call` descriptors.

- In `REDUCE` methods, use early returns or
  `LABEL_BLOCK(no_change) { return Next::Reduce...(...); }` with
  `goto no_change;` to avoid deep nesting or repeated `Next::Reduce...` calls.

- Only prefix Assembler/reducer-stack calls with `__`; call private helper
  methods on the current reducer directly (without `__`).

- When defining operations in `operations.h`, inherit from
  `FixedArityOperationT<N, Op>` if possible, rely on its default
  `options()`-based `operator==`/`hash_value`/`PrintOptions`, omit empty
  `Validate()` methods, and expose typed helpers in `assembler.h` rather than
  calling `ReduceIfReachable*` directly.

- Operations that can throw JS exceptions must be added to
  `TURBOSHAFT_JS_THROWING_OPERATION_LIST` with `THROWING_OP_BOILERPLATE`, and
  wrapped in a `ThrowingScope` when emitted in `GraphBuilder` /
  `TurbolevGraphBuilder`.

- To minimize binary size and compile time, reuse existing phases
  (`CopyingPhase<...>`) rather than introducing new phases when possible, and
  avoid branching at runtime between two `CopyingPhase` instantiations with
  different reducer lists (include the reducer unconditionally and check flags
  inside it).

- Optimization reducers should check `ShouldSkipOptimizationStep()` (at least at
  the beginning of `REDUCE` / `REDUCE_INPUT_GRAPH` methods) before performing
  non-mandatory transformations.
