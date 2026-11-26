---
name: minimize-reproducer
description: Use this when you need to minimize a complex or bloated JavaScript test case (e.g., from a fuzzer or external report) into a minimal, readable reproduction for debugging V8 issues.
---

# Minimize Reproducer

## Workflow

### 1. Identify the Failure Signature

Before minimizing, understand exactly what "reproduction" looks like.

- **Exact Command:** e.g., \`out/x64.release/d8 --fuzzing poc.js\`
- **Failure Type:** Crash (segfault, abort), Assertion failure, Sandbox
  violation, or Incorrect output.
- **Stability Check:** Initially check how stable the reproduction is. If it's
  flaky, you'll need to run it multiple times at every step. Use the bundled
  \`repro_check.sh\` script.

### 2. Mechanical Minimization First (`minimize_dd.py`)

Delegate brute-force line, block, and flag deletion to the bundled
delta-debugging script in one shell invocation instead of spending turns
deleting lines or testing flags one by one:

```bash
vpython3 agents/skills/minimize-reproducer/scripts/minimize_dd.py \
  --cmd "out/x64.release/d8 {flags} {file}" \
  --flags "--fuzzing --future --expose-gc" \
  --match "DCHECK failed: .*|AddressSanitizer" \
  --file poc.js \
  --out poc-min.js
```

- **Always Pass `--match`**: Match the exact crash/assertion signature on
  combined stdout/stderr so unrelated `SyntaxError`s or timeouts from partial
  deletions are rejected.
- **Umbrella Flags**: Strip umbrella fuzzer flags (`--fuzzing`,
  `--experimental-fuzzing`, `--jit-fuzzing`) first or test minimal leaf flags
  such as `--sandbox-testing`, `--turbofan`, or `--expose-gc`.
- **Flaky Reproducers**: If the bug fails probabilistically, wrap the command
  with `repro_check.sh` or stabilize the trigger with a loop before running
  `minimize_dd.py`.

### 3. Smart Semantic Minimization & Re-`ddmin`

After `minimize_dd.py` strips dead code, apply semantic simplifications that
line-level `ddmin` cannot discover on its own (and re-run `minimize_dd.py`
whenever a rewrite exposes newly dead helpers):

- **Replace Fuzzer Patterns with Direct V8 Idioms**: Replace CPU-spinning warmup
  loops, allocation sprays, or harness scaffolding with explicit intrinsics
  (`%PrepareFunctionForOptimization`, `%OptimizeFunctionOnNextCall`, `gc()`,
  `gc({ type: 'minor' })`) when investigating root causes.
- **Collapse Abstractions & Control Flow**: Inline single-use functions, remove
  unused parameters, replace complex class/Proxy/eval scaffolding with plain
  objects or direct calls, and unwrap non-essential `try-catch` blocks.
- **Simplify Expressions & Data**: Shrink large constants, strings, or arrays to
  the smallest values that preserve the failing shape/bounds, and split
  multi-statement lines when sub-expressions look irrelevant.
- **Stabilize Flaky Triggers**: If a reduced case still flakes, tighten the loop
  or add an explicit `gc()` around the trigger to make failure 100%
  deterministic.

### 4. Wasm Minimization

WebAssembly reproductions often start as opaque `Uint8Array` blobs. Converting
these to readable `WasmModuleBuilder` JS is critical for understanding and
minimizing the logic.

- **Convert Early with `wami`:** Use the `wami` tool to automatically generate
  `WasmModuleBuilder` code from a Wasm binary. This is much less error-prone
  than manual conversion.
  - **Command:** `out/x64.release/wami --mjsunit input.wasm > poc.js`
- **Handle `WasmModuleBuilder` Specifics:**
  - **Recursive Types:** Be careful with recursive type definitions; ensure they
    are correctly indexed.
  - **Reference Types:** When specifying indices for heap types (e.g., in
    `struct.get`), use `wasmRefNullType(typeIndex)` or similar helpers where the
    builder expects a type object rather than a raw integer.
  - **Signature Validation:** After conversion, verify that the module still
    reproduces the original issue. `WasmModuleBuilder` might require minor
    manual adjustments if the original binary had subtle validation errors.

### 5. Special API Handling (e.g., Sandbox API)

- **Offset Alignment:** Some APIs (like `Sandbox.markForCorruptionOnAccess`)
  require specific alignments (e.g., tagged-size-aligned offsets like 8, 12, or
  16).
- **Object Context:** Ensure the object being manipulated (e.g., a String) is
  large enough for the offset being targeted. Use `%DebugPrint(obj)` with
  `--allow-natives-syntax` to inspect the internal structure.

### 6. Clean and Polish

- **Rename Variables:** Obfuscated names like `__v_0` should be renamed to
  something descriptive like `str` or `targetObj`.
- **Readability > Size:** A 20-line file that is easy to understand is better
  than a 10-line file that is completely cryptic.
- **Self-Contained:** Ensure the final `poc-min.js` is completely self-contained
  and does not rely on external fuzzer libraries unless strictly necessary.

## Guidelines

- **Work on a copy:** Never modify the original reproduction file directly.
- **Validate EVERY change:** A single character change can stop a bug from
  reproducing. Always verify.
- **Keep a "Best Version":** Always update your \`-min.js\` file as soon as you
  find a smaller working version. This allows you to easily backtrack if a more
  aggressive minimization fails.
