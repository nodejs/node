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

### 2. Command-Line Flag Minimization

Find the minimal set of V8 flags required to reproduce the issue.

- **Batch Removal:** Start by removing larger batches of flags (e.g., all flags
  starting with \`--no-\` or all optimization flags).
- **One-by-One:** Progressively remove individual flags until only the essential
  ones remain.
- **Common Critical Flags:** Pay attention to flags like \`--sandbox-fuzzing\`,
  \`--memory-corruption-via-watchpoints\`, or \`--turbofan\`, as these often
  control the underlying feature being tested.
- **Validation:** Test reproduction after every step. If it stops failing,
  restore the last flag removed.

### 3. File Minimization (The Iterative Phase)

Reduce the input JavaScript file systematically. Always work on a copy (e.g.,
\`poc-copy.js\`) and maintain a \`poc-min.js\` as your "best known" version.

- **Remove Large Blocks:** Start by removing entire functions, classes, or large
  blocks of code (e.g., fuzzer boilerplate, unused libraries).
- **Binary Search:** If a large block is present, try removing half. If it still
  fails, that half was unnecessary.
- **Remove Individual Lines:** Once large blocks are gone, try removing
  individual lines.
- **Inline Functions:** If a function is called only once, inline its content
  and remove the definition.
- **Simplify Wrappers:** Remove \`try-catch\` blocks but retain the code inside
  the \`try\` block.
- **Stabilize with Loops:** If a reproduction is flaky (e.g., it only fails 10%
  of the time), add a loop around the triggering logic to increase the failure
  probability to 100%.

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
