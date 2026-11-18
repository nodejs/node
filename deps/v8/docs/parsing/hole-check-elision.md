# Temporal Dead Zone (TDZ) Hole Check Elision Pipeline

## Conceptual Overview

In ECMAScript (`let`, `const`, `class`, and `using` declarations, as well as derived class `this` bindings before `super()`), a variable exists in its lexical scope from the very start of the block, but cannot be read or written until its declaration statement has been evaluated. The region between entering the scope and evaluating the initializer is called the **Temporal Dead Zone (TDZ)**:

```javascript
{
  console.log(x); // Must throw ReferenceError: Cannot access 'x' before initialization
  let x = 42;
  console.log(x); // Valid: evaluates to 42
}
```

At runtime, V8 represents an uninitialized lexical variable (or unallocated derived constructor `this` receiver) by initializing its storage slot (bytecode register, `Context` slot, `ContextCell`, or `ScriptContextTable` slot) with a dedicated internal sentinel: **`tdz_hole_value`** (`TdzHole` in `ReadOnlyRoots`; see [Holes and Sentinels](../objects/holes-and-sentinels.md)).

Whenever code reads a variable that might still be in its TDZ (or assigns to a variable before initialization), V8 emits a **TDZ Hole Check**:
- In **Ignition bytecode**: `ThrowReferenceErrorIfTdzHole` (or `ThrowSuperNotCalledIfTdzHole` / `ThrowSuperAlreadyCalledIfNotTdzHole` for derived `this`).
- In **Maglev**: `ThrowReferenceErrorIfTdzHole`, `ThrowSuperNotCalledIfTdzHole`, or `ThrowSuperAlreadyCalledIfNotTdzHole` (elided via `ValueNode::IsTdzHole()` / `IsHole(RootIndex::kTdzHoleValue)`).
- In **TurboFan / Turboshaft**: `ReferenceEqual(val, TdzHoleConstant())` + `BuildHoleCheckAndThrow` in TurboFan, and `RootEqual(val, RootIndex::kTdzHoleValue)` in Turboshaft.

Because lexical variables (`let` and `const`) are used everywhere in modern JavaScript, checking for `tdz_hole` on every single variable read and write would add substantial overhead. Therefore, V8 implements a **three-stage Hole Check Elision Pipeline** that eliminates redundant TDZ initialization and checks across the **Parser**, **BytecodeGenerator**, and **Optimizing Compilers (Maglev & TurboFan)**.

---

## The Three-Stage Elision Pipeline

```mermaid
flowchart TD
    Source["JavaScript Source (let / const / class / using)"] --> Stage1

    subgraph Stage1 ["Stage 1: Parser & Scope Resolution (Static Position Analysis)"]
        S1["Scope::ResolveTo -> UpdateNeedsHoleCheck (src/ast/scopes.cc)"]
        S1_Desc["1. Compares Variable::initializer_position() vs VariableProxy::position()<br/>2. Eliminates checks when declaration lexically precedes access in same closure<br/>3. Tracks ForceHoleInitializationFlag to avoid initializing variables with 0 hole checks!"]
        S1 --- S1_Desc
    end

    Stage1 --> Stage2

    subgraph Stage2 ["Stage 2: Ignition BytecodeGenerator (Control-Flow Bitmap Elision)"]
        S2["BytecodeGenerator::HoleCheckElisionScope & HoleCheckElisionMergeScope"]
        S2_Desc["1. Tracks 64-bit Variable::HoleCheckBitmap (up to 63 variables per function)<br/>2. Elides subsequent checks in the same basic block after the 1st check/init<br/>3. Intersects bitmaps across converging if/else and switch branches"]
        S2 --- S2_Desc
    end

    Stage2 --> Stage3

    subgraph Stage3 ["Stage 3: Optimizing Compilers (Maglev & TurboFan SSA Dataflow)"]
        S3["Maglev ValueNode::IsTdzHole() / KnownNodeAspects & TurboFan/Turboshaft"]
        S3_Desc["1. Folds away ThrowReferenceErrorIfTdzHole when ValueNode::IsTdzHole() is kFalse<br/>2. Lowers via TdzHoleConstant() / RootEqual(val, RootIndex::kTdzHoleValue)<br/>3. Carefully invalidates cached context loads on throwing exception edges!"]
        S3 --- S3_Desc
    end
```

---

## Detailed Stage Documentation

Each stage of the pipeline has its own invariants, data structures, and subtle edge cases:

1. **[Stage 1: Parser & Scope Analysis (`hole-check-elimination-parser.md`)](hole-check-elimination-parser.md)**
   - How `UpdateNeedsHoleCheck` (`src/ast/scopes.cc`) uses `initializer_position()` vs `access_position` across closure boundaries.
   - How `ScopeInfo::kMaxVariablePositionDistance` enables cross-closure hole check elimination even for lazily compiled inner functions.
   - How `Variable::ForceHoleInitializationFlag` avoids emitting `LdaTdzHole` initializations altogether when no access requires a hole check.
2. **[Stage 2: Ignition Bytecode Generation (`hole-checks-bytecode.md`)](hole-checks-bytecode.md)**
   - How `BytecodeGenerator` uses a 64-bit `Variable::HoleCheckBitmap` (`hole_check_bitmap_`) to track up to 63 variables (`1..63`, with bit `0` reserved for uncacheable variables).
   - How `HoleCheckElisionScope` saves/restores the bitmap across non-merging branches (`if` without `else`, loops, `try`/`catch`) and how `HoleCheckElisionMergeScope` intersects bitmaps (`merge_value_ &= *bitmap_`) across converging branches (`if`/`else`, `?:`, `switch`).
3. **[Stage 3: Optimizing Compilers: Maglev & TurboFan (`hole-checks-optimized.md`)](hole-checks-optimized.md)**
   - How Maglev (`VisitThrowReferenceErrorIfTdzHole`, `ValueNode::IsTdzHole()`, `KnownNodeAspects`) and TurboFan / Turboshaft (`BytecodeGraphBuilder`, `TurbolevGraphBuilder`) eliminate redundant checks across basic blocks and loops.
   - The **"Inverted Generator" / `try-catch` Context Load Caching bug and fix**: why caching an uninitialized `LoadContextSlotNoCells` (`kNotAssigned`) before `ThrowReferenceErrorIfTdzHole` can poison a `catch` block's `KnownNodeAspects` unless temporarily cleared across the throwing edge.
4. **[Holes and Sentinels Reference (`../objects/holes-and-sentinels.md`)](../objects/holes-and-sentinels.md)**
   - Why `tdz_hole_value` (`TdzHole`) is segregated from array `the_hole_value` (`TheHole`), how `--unmap-holes` guards hole objects in memory, and how two-phase literal initialization works.
