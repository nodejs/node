# Hole Check Elimination in the Parser

This document details how V8 resolves variables, enforces Temporal Dead Zone (TDZ) rules, and uses caching mechanisms to optimize lookups without breaking source position calculations.



## Simple In-Function Access (The Baseline)

Before considering complex scope walks, the simplest scenario is accessing a variable within the exact same scope where it is declared.

For a simple local, same-scope access without nested closures, V8 compares the pure source location of the variable reference directly against the variable's `initializer_position`. If the access occurs lexically *before* or exactly at the initializer, a hole check is required. The concept of a propagated `access_position` is actually **only relevant for `eval` and nested closures (including classes)**. For simple in-function accesses, raw source positions suffice.

```javascript
function simple() {
  console.log(x); // access_position < initializer_position -> TDZ check required
  let x = 10;     // initializer_position
  console.log(x); // access_position > initializer_position -> No check needed
}
```

> **Note for Bytecode Generation**: The parser communicates this decision to the bytecode generator by marking the `VariableProxy` with a `HoleCheckMode` (`kElided` or `kRequired`). If marked as `kElided`, the bytecode generator skips emitting the hole check instruction entirely.

## Cross-Closure and Eval Accesses

The simple raw position approach breaks down as soon as we introduce JavaScript language features like function hoisting and `eval`. The concept of a propagated `access_position` becomes strictly relevant here.

### The Challenge of Nested Closures

Function declarations are hoisted to the top of their scope (per [FunctionDeclarationInstantiation](https://tc39.es/ecma262/#sec-functiondeclarationinstantiation)), while class declarations are not ([BlockDeclarationInstantiation](https://tc39.es/ecma262/#sec-blockdeclarationinstantiation)). This creates discrepancies between lexical order and execution order.

**Example: Function Declarations vs. Unhoisted Closures**
```javascript
function testHoisting() {         // Outer Scope (Starts at Pos 20)
  // let x = hole
  inner();
  let x = 10;                     // initializer_position = Pos 65

  // Function Declarations ARE hoisted.
  function inner() {              // Inner Scope (Starts at Pos 140)
    console.log(x);               // raw access_position for hole check = Pos 160
                                  // access_position after walk = Pos 20
  }

  // Unhoisted closures (like classes or function expressions) are NOT hoisted.
  class MyClass {                 // Class Scope (Starts at Pos 200)
    method() {
      console.log(x);             // raw access_position for hole check = Pos 240
                                  // access_position after walk = Pos 240
    }
  }
}
```

**The Problem with Raw Positions and Hoisting**
In the `testHoisting` example above, the function `inner` is hoisted. It is called at line 35, *before* `x` is initialized at line 36. However, the raw source position of the access to `x` inside `inner` (line 40, Pos 160) is lexically *after* the initialization of `x` (line 36, Pos 65).

If V8 relied solely on these raw source positions, it would incorrectly conclude that the access at Pos 160 happens after the initialization at Pos 65, and would elide the required hole check.

### The Solution: Propagating `access_position`

To fix this, when V8 encounters a variable access inside a nested closure, it resolves the reference by recursively walking the scope chain outward, updating an `access_position` along the way. This adjusted position projects the inner access onto the boundary of the outer scope.

**How `access_position` solves the hoisting problem:**
When V8 walks out of a scope, it checks if the scope boundary is hoisted. If it is, it updates the `access_position` to reflect that fact. Because a hoisted function can be called early, the access is effectively projected to the very beginning of the outer scope.

**Lookup Trace for `x` in `console.log(x)` inside `inner` (Hoisted):**
```text
[Scope Walk] Current Scope: `inner`
  - Target           : 'x'
  - access_position  : 160 (Raw source position)
  - Result           : Not found. Walking outward...

[Scope Walk] Crossing boundary (`function inner()`)
  - Boundary type    : Hoisted Function
  - access_position  : Updated 160 -> 20 (Projected to start of outer scope)

[Scope Walk] Current Scope: `testHoisting`
  - Result           : Found `let x` (initializer_position: 65)

[TDZ Check]  initializer_position (65) >= access_position (20)
  - Conclusion       : TRUE (Hole check required!)
```

**Lookup Trace for `x` in `console.log(x)` inside `MyClass` (Not Hoisted):**
```text
[Scope Walk] Current Scope: `MyClass`
  - Target           : 'x'
  - access_position  : 240 (Raw source position)
  - Result           : Not found. Walking outward...

[Scope Walk] Crossing boundary (`class MyClass`)
  - Boundary type    : Unhoisted Class
  - access_position  : Unchanged (Remains 240)

[Scope Walk] Current Scope: `testHoisting`
  - Result           : Found `let x` (initializer_position: 65)

[TDZ Check]  initializer_position (65) >= access_position (240)
  - Conclusion       : FALSE (No hole check needed)
```

> **Note for Bytecode Generation**: For references that *do* require a hole check (`kRequired`), the parser also communicates whether the target is in the **same or different closure**. This is because the bytecode compiler performs its own local control-flow analysis on variables that are locally defined (in the same closure) to get more accurate hole check elimination. While it could theoretically be extended to cross-closure variables, V8 currently limits this tracking to the same closure as an **implementation decision** (partially to keep the analysis lightweight and bounded to 64 bits).

## Advanced Closures: `is_hoisted_in_context`

While walking the physical scope chain handles hoisting naturally, this breaks down with two performance features:
1. **Lazy Compilation**: Compiling functions later must yield the same variable resolution as compiling them immediately.
2. **Scope Elision**: Removing intermediate scopes that don't need context allocation to reduce the context chain length.

**The Intersection Example:**
To see how these features collide, consider a hoisted function that returns an inner function:

```javascript
function outer() {
  // 1. We execute `hoisted()` and immediately call its return value (`inner()`).
  // This happens BEFORE `x` is initialized, because `hoisted` is hoisted to the top.
  hoisted()();

  let x = 10;

  function hoisted() {
    // 2. ELIDED SCOPE: V8 elides this scope because `hoisted` declares no
    // context-allocated variables of its own.

    return function inner() {
      // 3. LAZILY COMPILED UNIT: `inner` is compiled later when invoked.
      // When compiled, it resolves the reference to `x`.
      console.log(x); // ReferenceError (TDZ)
    };
  }
}
```

**The Problem**:
The scope of the hoisted function (`hoisted`) is elided by V8 to keep the context chain short. However, it contains an *inner* unit (`inner`) that is *lazily compiled* (compiled later). Because the outer function is hoisted, code inside this inner unit can execute early. When the inner unit is eventually compiled, any *references* it contains to variables outside the hoisted function (like `x`) must be resolved correctly.

**The Fix**:
Since the intermediate scope was elided, the physical boundary that would normally tell us to update the `access_position` for these references is gone. To fix this, V8 sets the `is_hoisted_in_context` flag to `true` on the scope of the lazily-compiled unit `inner`.

Since `inner` is a function expression it is *not* lexically hoisted within its own outer scope (`hoisted`). Still, because `hoisted`'s scope is elided, the runtime context chain connects `inner` directly to `outer`.

**The Runtime Context Chain (When `inner` is compiled):**
```text
[ inner() Scope ]  ========>  [ outer() Context ]
                       ^
                       | (The `hoisted()` scope is completely missing!)
```

In that *runtime context*, `inner` executes early because its parent was hoisted. The `is_hoisted_in_context` flag captures exactly this: it tells `inner` that while it isn't lexically hoisted itself, it *is* hoisted relative to the context it will see at runtime. This flag tells `inner` to update the `access_position` during its delayed compilation to the start position of the outer *context* — which, in this specific case, is the start position of `outer()`.

*(Note: This flag must also be stored on the `SharedFunctionInfo` (SFI). Uncompiled functions don't have their own `ScopeInfo` before compilation, but they still need to know how their inner scopes will behave compared to the outer context).*

## Standard Caching and the Stale Position Problem

Walking the scope chain for every single variable access is computationally expensive (O(N) per access). To optimize this, V8 caches the resolved scope on standard lexical variables crossing a compilation-unit boundary. This happens when an inner function is compiled lazily, and it references variables from an outer function that has already been compiled. V8 uses the `ScopeInfo` generated during the outer function's compilation to resolve these variables. This resolution is cached directly on that compilation-unit boundary using **`cache_scope`**.

### The Conflict: Stale `access_position`

When a scope walk is short-circuited by a `cache_scope` hit (because we find a cached resolution from a previous lookup or from the outer `ScopeInfo`), the recursive traversal is skipped. As a direct consequence, the `access_position` that gets passed down to the hole-check logic is "stale" — it has **not** been adjusted by the intermediate scopes because they were skipped.

If V8 relied on this stale `access_position` to recompute the hole check requirement (`var->initializer_position() >= access_position`), it would yield incorrect results.

### The Solution: `HoleCheckState` Caching

To avoid relying on a broken, un-adjusted `access_position`, V8 caches the *result* of the hole check computation directly on the variable (`HoleCheckState::kForce` or `kSkip`) during the *very first* full scope walk. Future lookups bypass the `access_position` calculation entirely.

## The Mechanics of `eval` Scopes

To understand why `eval` complicates hole check elision, it is helpful to review how variables declared *inside* an `eval` are scoped according to the ECMAScript specification:

### Direct `eval` (Sloppy Mode)
When `eval()` is called directly and strict mode is **not** active:
- **`var` declarations**: Hoisted out of the `eval` and added to the enclosing function's variable declaration scope (often landing in a `VarBlock` or the function scope itself, introducing `DynamicLocal` behavior).
- **`let` and `const` declarations**: Stay strictly inside the `eval` block's own scope. They do not leak to the outer function.

### Direct `eval` (Strict Mode)
When `eval()` is called directly and strict mode **is** active (either via `'use strict'` inside the eval or in the enclosing code):
- **All declarations** (`var`, `let`, `const`, and functions) stay strictly inside the `eval`'s local scope. They never leak to the outer function. (Yes, `var` can be declared in strict mode `eval`, but it remains local to the `eval` execution context).

### Indirect `eval`
When `eval` is invoked indirectly (e.g., `(0, eval)("...")` or assigning `eval` to a variable and calling it):
- The execution context is always the **global scope** (or module scope), not the local function scope.
- **Sloppy mode**: `var` declarations leak to the global object.
- **Strict mode**: All declarations stay local to the `eval` scope.

> **Note on Performance**: This behavior exists to enable **static `eval` detection**. If indirect `eval` calls could also pollute the local function scope, V8 would have to conservatively assume that *any* function call (which could be an aliased `eval`) might dynamically introduce variables! This would prevent almost all local variable optimizations. By restricting local scope pollution to statically detectable *direct* `eval` calls, V8 preserves global performance.

## Sloppy Eval and `DynamicLocal`

When a function body contains a sloppy `eval`, V8 cannot statically know what variables it will access or introduce at runtime. Because `eval` might dynamically introduce a shadowing variable as a context extension, lookups within that closure must be treated dynamically.

To handle this, V8 introduces the **`DynamicLocal`** variable mode. V8 caches the statically known lexical target underneath in a property called **`local_if_not_shadowed`** as a fallback.
*(Note: `with` statements also introduce dynamic scopes, but for `with`, V8 falls back to pure `Dynamic` variables and emits `LdaLookupSlot`, which unconditionally calls out to the runtime with no fast path!)*.

### Example 1: The Challenge of Eval

```javascript
function testEval() {
  eval("console.log(y)"); // Throws ReferenceError! 'y' is in the TDZ.
  let y = 20;
}
```

The source position of `console.log(y)` *inside* the eval string is `0` (relative to the string's start). If V8 were to use this raw position `0` when looking up `y` in the outer scope, it would almost always appear to be *before* the outer variable's declaration, triggering unnecessary hole checks (even if the `eval` was called *after* the variable was initialized!).

To solve this, when V8's scope walk crosses the boundary *out* of an `eval` scope, it simply swaps the raw inner `access_position` (which might be `0`) with the exact source position of the `eval()` call in the outer scope. From that point onward, the access is correctly evaluated as if it had physically occurred where `eval()` was invoked!

### Example 2: Visualizing `local_if_not_shadowed`

Let's "fake-add" the `DynamicLocal` concept into the JavaScript code to visualize how it links to the outer variable:

```javascript
function outer() {
  // let x = hole

  function inner(str) {
    eval(str); // might introduce a shadowing 'var x'

    // Conceptually, V8 sees 'x' here as:
    // Proxy(x) -> DynamicLocal [local_if_not_shadowed -> Variable(x in outer)]
    console.log(x);
  }

  let x = 10; // Statically resolved outer variable
}
```
In this scenario, the access to `x` inside `inner` becomes a `DynamicLocal`. V8 caches `x` from the `outer` scope inside `local_if_not_shadowed`. If `str` does not declare a `var x` at runtime, V8 falls back to reading the cached `x` from the outer scope.

---

### Can the underlying variable be in the same closure as the `eval`?
No. To understand why, we must first clarify where `DynamicLocal` variables are introduced: they are created in the **body's declaration scope** if that body contains an `eval` call within the closure. This is because the `eval` might dynamically introduce variables into that declaration scope at runtime, potentially shadowing existing variables.

Given that, we look at how V8 handles different variable modes in this scenario:

- **For `let` and `const`**: `eval` can only introduce `var` declarations into the outer scope (since `let` and `const` stay strictly local to the `eval` itself). If we have a `let` or `const` variable in that same closure scope, attempting to introduce a `var` with the same name via `eval` is a **SyntaxError** in JavaScript. Thus, a valid `DynamicLocal` representing a TDZ variable can never be shadowed by `eval` in the same closure.
- **For standard `var`**: If `eval` introduces a `var` that matches an existing `var` in the same closure, V8 simply resolves it directly or merges them rather than creating a `DynamicLocal`.
- **For `VarBlock` scopes**: A `VarBlock` is a block scope that acts as a declaration scope. V8 uses them for **class static blocks** and for **functions with non-simple parameters**. Since classes are always in strict mode, an `eval` inside a class static block cannot introduce dynamic variables, so `DynamicLocal` is never needed there. For functions with non-simple parameters, a `DynamicLocal` inside the `VarBlock` body *could* target a variable outside of it but within the same closure (like function parameters). However, these parameters are always `kCreatedInitialized` in V8, meaning they **never need a hole check**!

Therefore, for any variable that actually *requires a hole check* (i.e., `let` and `const` variables), the shadowed target must always be in a different, outer closure! Thus, `same_closure_scope` is guaranteed to be false for these lookups.
