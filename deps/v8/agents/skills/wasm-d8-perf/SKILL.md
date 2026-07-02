---
name: wasm-d8-perf
description: Use this skill to reproduce, profile, or analyze WebAssembly (Wasm) execution performance using d8 shell.
---

# WebAssembly Performance Analysis in d8

Reproduce, profile, and analyze Wasm performance using the V8 developer shell
(`d8`).

## 1. Reproduction and Establishing a Baseline

- **Build Configurations**:
  - **CRITICAL**: Always use a **`release`** build (e.g., `out/x64.release/d8`)
    for benchmarking.
  - Use **`optdebug`** (e.g., `out/x64.optdebug/d8`) for disassembling/debugging
    (preserves symbols).
- **Measurement Metrics**:
  - **Wall time**: Measure execution time (e.g., JS `console.time()`).
    - *LLM Tip (Statistical Rigor & Warmup)*: Run 3-5 times; take the
      median/average. Ensure code runs in a loop to trigger Turboshaft tier-up
      (warmup) before measuring.
  - **Resource/CPU**: Use `/usr/bin/time -v` (Wall time, memory) and
    `perf stat -d` (cycles, instructions, IPC, branch misses).
    - *LLM Tip (perf Fallback)*: If `perf` fails due to permissions (e.g.
      `kernel.perf_event_paranoid`), notify the user to adjust settings or
      switch to a PMU-enabled environment (gLinux/Cloudtop). Fall back to
      `d8 --prof` if needed.
  - **GC Overhead**: Use `--trace-gc` to trace garbage collection pauses,
    especially if WasmGC is used.
- **Tier Isolation**: Compare performance between execution tiers to understand
  the optimization effect.
  - Liftoff (Baseline): `d8 --liftoff-only script.js`
  - Turboshaft (Optimized): `d8 --no-liftoff script.js`
    - *Note*: `--no-liftoff` disables speculative inlining since it skips
      Liftoff execution and thus doesn't collect feedback.

## 2. Hotspot Identification

Find where time is spent, what the hottest functions, loops, blocks, or
instructions are, so we focus on that:

- **Linux Perf**: Run `./tools/profiling/linux-perf-d8.py` to get a profile with
  JIT symbols resolved, then analyze with `perf report`.
- **V8 Profiler**: Run `d8 --prof script.js` and analyze `v8.log` with
  `./tools/linux-tick-processor`.
- **Categorize Hotspots**: Wasm user code, JS code, V8 C++
  Runtime/GC/Compilation, or JS/Wasm boundary (wrappers).

## 3. Deep Dive Analysis

### Compilation Overhead

- **Identify Slow to Compile Functions**: Use
  `d8 --trace-wasm-compilation-times` and `d8 --turbo-stats-wasm`.
- **Inspect Bytecode**: Build `wami` (`tools/dev/gm.py x64.release wami`) and
  disassemble: `out/x64.release/wami --single-wat=<func_index> <path_to_wasm>`

### Wasm Binary Quality & Toolchain

Unoptimized Wasm binary patterns can hurt V8 compilation and execution.

- Ask the user if source code (C++/Rust) is available.
- Check for: huge functions, excessive locals, many branches, or missed static
  optimizations.
- Ensure binary is a release build (compiled with `-O3`/`-Os`).

### Execution Performance & Optimization (Deep Dives)

Perform deep dives on hot functions:

- **Turboshaft IR Traces**: Run
  `d8 --trace-turbo --trace-turbo-filter=<func_name>`.
  - *Context Window*: To keep the context windows clean use `jq`
    programmatically:
    - List phases: `jq '.phases[].name' turbo-*.json`
    - Count nodes:
      `jq '.phases[] | {name: .name, node_count: (if (.data | type) == "object" and .data.nodes then (.data.nodes | length) else 0 end)}' turbo-*.json`
    - Find ops:
      `jq '.phases[] | select(.name == "TurboshaftTypeAssertionsPhase") | select((.data | type) == "object" and .data.nodes) | .data.nodes[] | select(.title | contains("BoundsCheck"))' turbo-*.json`
- **Assembly Profiling**: Run `perf annotate -i file.perf.data.jitted --stdio`
  to map hot instructions to bytecode.
  - *Instruction Skid*: Account for cycle misattribution; inspect surrounding
    instructions and data dependencies.
- **Disassembly & Comments**: Run
  `out/x64.optdebug/d8 --print-wasm-code-function-index=<idx> --code-comments`.
  - *LLM Tip*: Redirect disassembly output to a file (`> disasm.txt`) to avoid
    flooding the terminal.
- **Optimization & Tracing Flags**:
  - Inlining: `--trace-wasm-inlining`, `--trace-turbo-inlining`
  - Loop Peeling and Unrolling: Enable/disable `--wasm-loop-peeling`,
    `--wasm-loop-unrolling` to see an effect of those optimizations.
  - Other Optimizations: `--turboshaft-trace-load-elimination`,
    `--turboshaft-trace-peeling`, `--turboshaft-trace-unrolling`
  - Typing: `--trace-wasm-typer`
- **Inlining & Tiering Budgets**: Scalability tuning flags to adjust
  optimizations:
  - Tiering up earlier or later: `--wasm-tiering-budget=<value>`.
  - Inline more or less aggressively: `--wasm-inlining-budget=<value>`.
- **V8 Flag Discovery**: Search for context-specific flags using
  `d8 --help | grep -i <keyword>` or by looking up definitions in
  `src/flags/flag-definitions.h` and via `grep`. Refer to `v8-commands` skill
  for general usage.

## 4. Synthesis and Reporting

Provide a clear report summarizing:

1. **Hottest Functions**: Contribution and compilation tier (Liftoff vs
   Turboshaft).
2. **Inlining Status**: Report Wasm-to-Wasm and JS/Wasm wrapper and body
   inlining.
3. **Wasm Input Binary Properties**: Release vs debug, missed static
   optimizations, very large functions, or otherwise noteworthy code patterns.
4. **Disassembly & Assembly Hotspots**: Highlight hot instructions and map them
   to Wasm bytecode.
5. **Turboshaft Optimization**: Key phases affecting the CFG and any failed
   optimizations.
6. **Code Skeleton**: Provide a simplified snippet of the hot block/loop at the
   most useful level (source, bytecode, IR, or machine code).

## 5. Experimentation & Verification

Only proceed to this step after your initial report and explicit approval by the
user.

- **Ablation Studies**: Toggle optimizations to measure wall-time impact (e.g.
  `--no-wasm-loop-unrolling`, `--no-turboshaft-wasm-load-elimination`,
  `--no-wasm-inlining`, `--no-turbo-inline-js-wasm-calls`).
  - *LLM Tip*: Present results in a **Before vs. After table**.
- **Wasm Binary/Static Optimization**: Use `wasm-opt -O3` (or `-Os`) if
  available on `PATH`, or ask the user to rebuild with different flags.
- **Hypotheses**: Align on potential optimization opportunities with the user
  before implementing fixes.
