# Scopes and ScopeInfos in V8

V8 manages variable scopes at both compile time and runtime using different data structures. This document explains how `Scope` and `ScopeInfo` work together.

## Compile-Time: `Scope`

During parsing and AST generation, V8 uses the `Scope` class (defined in `src/ast/scopes.h`) to represent lexical scopes.

### Purpose
*   **Variable Tracking**: It tracks all variables declared in a scope.
*   **Variable Resolution**: It resolves variable references to their declarations.
*   **Allocation Decision**: It determines whether a variable can live on the stack (fast) or must be allocated in a `Context` on the heap (slow, needed for closures).

### Scope Types
V8 supports various scope types defined in `src/common/globals.h`:
*   `SCRIPT_SCOPE`: The top-level scope for a script or a top-level eval.
*   `REPL_MODE_SCOPE`: The top-level scope for a repl-mode script.
*   `CLASS_SCOPE`: The scope introduced by a class.
*   `EVAL_SCOPE`: The top-level scope for an eval source.
*   `FUNCTION_SCOPE`: The top-level scope for a function.
*   `MODULE_SCOPE`: The scope introduced by a module literal.
*   `CATCH_SCOPE`: The scope introduced by catch.
*   `BLOCK_SCOPE`: The scope introduced by a new block.
*   `WITH_SCOPE`: The scope introduced by with.
*   `SHADOW_REALM_SCOPE`: Synthetic scope for ShadowRealm NativeContexts.

## Runtime: `ScopeInfo`

When V8 compiles a function, it serializes the compile-time `Scope` information into a `ScopeInfo` object (defined in `src/objects/scope-info.h`).

### Purpose
`ScopeInfo` is a compressed heap object that retains the necessary information about a scope for execution and debugging.

*   **Context Layout**: It tells V8 how many context slots are needed and what variables go into which slots.
*   **Debugger Support**: It allows the debugger to inspect variables and understand the scope structure of running code.
*   **Deoptimization**: Used to reconstruct stack frames during deoptimization.

### Contents of ScopeInfo
*   **Flags**: Scope type, language mode, receiver info, etc.
*   **Counts**: Number of parameters, context-allocated locals.
*   **Inlined Local Names**: Names of variables, allowing lookups by name (primarily for debugging and `eval`).
*   **Outer Scope Info**: A link to the `ScopeInfo` of the enclosing scope, forming a chain.

## Runtime: `Context`

While `ScopeInfo` is read-only metadata, the `Context` is the actual runtime object (a variable-sized heap object, conceptually similar to a `FixedArray`) that holds the values of context-allocated variables.

*   Each `ScopeInfo` that requires a context will have a corresponding `Context` object created at runtime when the scope is entered.
*   Contexts are linked in a chain (via the `previous` pointer), mirroring the `ScopeInfo` chain.

### Context Types
Similar to scope types, V8 has different context types (see `src/objects/contexts.h`):
*   **Native Context**: The top-level context for a specific realm. Contains many fixed slots for builtins and global objects.
*   **Function Context**: Created for function executions that have context-allocated variables.
*   **Block Context**: Created for block scopes with block-scoped declarations (`let`, `const`).
*   **Catch Context**: Created for `catch` blocks to hold the exception object.
*   **With Context**: Created for `with` statements.
*   **Eval Context**: Created for `eval` executions.
*   **Module Context**: Created for ES modules.


## Example: Context Allocation (Closures)

To understand how `ScopeInfo` and `Context` work in practice, consider a closure that captures a variable:

```javascript
function makeCounter() {
  let count = 0;
  return function() {
    return ++count;
  };
}
const counter = makeCounter();
counter(); // Returns 1
counter(); // Returns 2
```

### What V8 does:
1.  **Compile-Time**: The parser sees that the inner anonymous function references `count`, which is declared in the outer `makeCounter` function. The compiler decides that `count` cannot be stack-allocated and must be **context-allocated**.
2.  **ScopeInfo**: The `ScopeInfo` for `makeCounter` will record that it needs a context with at least one slot for `count`.
3.  **Runtime Execution**:
    *   When `makeCounter()` is called, V8 creates a new `FunctionContext` for it.
    *   The `count` variable is stored in a slot in this context, initialized to `0`.
    *   The inner function is created as a `JSFunction` object. This object contains a reference to the current `Context` (the `makeCounter` context).
    *   When `makeCounter` returns, its stack frame is destroyed, but the `FunctionContext` lives on in the heap because the inner function holds a reference to it.
    *   When `counter()` is called, V8 sets the current context to the one referenced by the closure. The `++count` operation reads and writes directly to the slot in that heap context.

## Summary

1.  **Parser** creates `Scope` objects.
2.  **Compiler** uses `Scope` to allocate variables and generates `ScopeInfo`.
3.  **Runtime** uses `ScopeInfo` to create `Context` objects and access variables.

## See Also
-   [Objects and Maps](../heap/objects-and-maps.md)
