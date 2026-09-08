---
name: v8-regression-testing
description: Use this skill when designing, writing, or minimizing regression tests (e.g., mjsunit) for V8 bug fixes
---

# Skill: V8 Regression Testing

Use this skill to design, write, minimize, and refine high-quality regression
tests in the V8 codebase (typically under `test/mjsunit/` or its
component-specific subfolders like `compiler/`, `turboshaft/`, `maglev/`, etc.).

## Core Mandates

- **Mandatory Reproducer Rule**: Unless explicitly specified otherwise by the
  user, **every single bug fix in V8 MUST be uploaded with a working reproducer
  (regression test)**. Uploading just the fix alone is NEVER acceptable.
- **Semantic Quality**: V8 regression tests must be **semantic
  (root-cause-driven)** rather than just **syntactic (fuzzer-driven)**. A
  high-quality test case is a clear, human-readable, and robust demonstration of
  the specific logical bug or compiler invariant under test, rather than a
  fragile, black-box crash script.
- **CRITICAL REPRODUCER CONSTRAINT**: **NEVER** provide a reproducer that does
  not actually trigger the issue to the user without **explicitly** telling them
  about it.

______________________________________________________________________

## The "From-Scratch" Reproducer Philosophy

When constructing a reproducer for a bug, **always prefer writing it from
scratch based on a deep conceptual understanding of the issue, rather than
starting from a fuzzer-generated crash script and trying to minimize it.**

### Why the Post-Hoc Fuzzer Minimization Trap Fails:

1. **The Black Box Dilemma**: Starting with a fuzzer-generated block of code
   means you are constantly guessing which strange construct is necessary. You
   are highly likely to retain useless nesting (`__wrapTC`), redundant
   try-catches, and magic numbers because you are afraid to break the fragile
   reproduction state.
2. **Fragility**: Fuzzer scripts often rely on incidental timings, engine
   heuristics, or side effects to trigger a crash. If optimization heuristics
   change slightly in a future V8 release, the fuzzer reproducer will silently
   break and stop testing the intended bug path.

### The "From-Scratch" Synthesis Approach:

1. **Identify the Root Cause**: Analyze the compiler phase graph, the crash
   backtrace, and the failing `DCHECK` or invariant to understand the exact
   abstract state machine failure (e.g., "an incorrect type representation
   selection, a specific Map transition mismatch, or a loop-carried
   side-effect").
2. **Begin with a Blank File**: Open an empty editor. Do not copy-paste the
   fuzzer script.
3. **Target the State Directly**: Write the absolute minimum JavaScript
   necessary to construct that logical state:
   - Use clean, structured object definitions and allocations to establish
     target internal structures (such as distinct Maps or Shapes).
   - Control the IC feedback vector cleanly by warming it up with specific
     types.
   - Explicitly trigger the compiler pass with intrinsic calls (e.g.,
     `%PrepareFunctionForOptimization`, `%OptimizeFunctionOnNextCall`).
4. **Result**: You produce a clean, robust, perfectly commented, 15-line
   reproducer that explicitly tests the underlying logic. It is
   self-documenting, easy to maintain, and impervious to unrelated engine
   optimization tweaks.

______________________________________________________________________

## Debugging a Failing Reproducer (The Scientific Loop)

When writing a reproducer from scratch, it is common for the first attempt to
**not** trigger the crash. When this happens, do not give up or fall back to
blindly copying the fuzzer script. Instead, treat the reproducer's failure as a
debugging task and enter a scientific feedback loop.

### The Feedback Loop:

1. **Trace the Execution**: Use V8's extensive set of tracing flags to inspect
   the JIT/engine pipeline (eg, `--trace-turbo-graph`, `--trace-ic`,
   `--trace-deopt`...), and insert custom instruction (eg, using `PrintF`) as
   needed if the existing flags are too coarse.
2. **Detect the Detour**: Identify exactly where the execution path diverged
   from your expectation. For instance:
   - *Did the function deoptimize prematurely due to an unexpected type feedback
     change?*
   - *Did load elimination optimize away a read that was supposed to trigger a
     side effect?*
   - *Did a map transition occur too early?*
3. **Analyze the Cause**: Figure out *why* V8 chose that detour.
   - What invariant, heuristic, or state in your JavaScript code caused the
     compiler to take a different optimization path?
4. **Heal and Steering**: Modify your JavaScript reproducer to block the detour
   and steer the compiler back onto the target path (e.g., altering map setups,
   introducing fake side effects to prevent load-elimination, or adjusting
   argument warming patterns).

[!IMPORTANT] **The Ultimate Conceptual Checkpoint**: If you repeatedly fail to
construct a clean, working reproducer from scratch, **this almost always means
your conceptual understanding of the bug is either incorrect or too shallow.**
Do not waste days trying to brute-force random JavaScript tweaks. When this
checkpoint is hit, you must **go back to the drawing board**, challenge what you
think you know about the bug, and deepen your understanding.

______________________________________________________________________

## Synctatic Principles of a Good Regression Test

### 1. Minimal & "Leaf" Compiler Flags

- **Strict Flag Minimization**: Every flag included in `// Flags: ...` must be
  thoroughly understood and strictly necessary to reproduce the issue. Remove
  all useless or redundant flags.
- **Prefer "Leaf" Flags over "Compound" Flags**:
  - **Bad**: Using `--jit-fuzzing` or `--fuzzing` because it implies a massive,
    ever-changing bundle of underlying behaviors.
  - **Good**: Identifying and specifying the exact leaf flags responsible (e.g.,
    `--no-lazy-feedback-allocation`, `--homomorphic-ic`,
    `--stress-concurrent-inlining-attach-code`).

### 2. Human-Friendly Naming

Avoid default fuzzer variable and function names (`__f_0`, `__v_10`, `v17`,
`a6`, `a7`).

- Use **descriptive names** for semantic concepts (`large_arr`, `global_var`,
  `testGenerator`).
- Use **clean, standard placeholders** for simple generic logic (`foo`, `bar`,
  `obj`, `f`, `x`, `y`).
- Maintain cognitive load at an absolute minimum.

### 3. Absolute Minimization of Code & Boilerplate

- Remove every parameter, variable, block, or helper that does not directly
  contribute to reproducing the bug.
- Never leave unused parameters in function signatures, unless absolutely
  required to reproduce the issue.
- Do not include catch-all blocks (`catch (e) {}`) or dummy wrapper functions
  (`__wrapTC`) unless they are semantically required to trigger the crash path.

### 4. Deterministic Optimization Control

- Avoid spinning CPU-burning loops (e.g., `for (let i = 0; i < 100000; i++)`) to
  trigger JIT tier-up, unless the bug specifically relates to loop-scaling or
  profiling feedback.
- Use V8 intrinsic compiler controls to force deterministic optimization:
  - `%PrepareFunctionForOptimization(foo);`
  - `%OptimizeFunctionOnNextCall(foo);` or `%OptimizeMaglevOnNextCall(foo);`
- Make sure `--allow-natives-syntax` is included in the Flags header when using
  percent (`%`) intrinsics.
