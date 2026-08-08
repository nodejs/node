---
name: workflow-perf
description: "Workflow for performance and memory evaluation in V8. Use when tasked with improving the performance or memory usage of a workload in V8. Do not use when debugging a crash or functionality issue."
---

# Workflow: Performance Evaluation

Use this skill when tasked with improving the performance or memory usage of a workload in V8. This workflow focuses on identifying bottlenecks and applying V8-side optimizations.

## Activation Criteria

- User requests to optimize a specific benchmark or script.
- Goal is to reduce execution time, CPU cycles, or memory footprint.

## Core Principles

1.  **Data-Driven**: Always base optimization decisions on profiling data, not intuition.
2.  **V8-Centric**: For V8 engineers, performance work usually means changing V8 to better handle the JS pattern, rather than changing the JS itself (though both are valid for general users).
3.  **Holistic View**: Look for general efficiency improvements, not just the single hottest function.
4.  **Contextual Interpretation**: Assume unknown terms in performance tasks are likely benchmark names or domain-specific concepts, not environmental terms (e.g., "WSL" is likely a JetStream story, not the OS). Verify before assuming. If in doubt, ask the user.
5.  **Mandatory Orchestration**: The agent executing this workflow MUST act as an Orchestrator. Delegate execution of benchmarks, profiles, or searches to subagents to maximize parallelism.
6.  **Local Experiment Baseline**: Whenever asked to perform a local experiment, always compile the baseline first and store it in a separate directory like `x64.release-baseline` for better `gm.py` integration. This avoids recompiling at later stages.

## Workflow

### Planning

For performance analysis, you do NOT need to create a full `implementation_plan.md` until you are actually _fixing_ the performance issue you've detected. Instead, maintain an **Analysis Plan** (e.g., in `task.md` or as a list of questions to answer) to guide the investigation.

### 1. Parallel Track Initialization

Initialize the following tracks concurrently:

- **Track A: Profiling & Tracing**:
  - Run the workload with the v8-profiling skill (e.g. linux-perf and/or pprof).
  - Use V8 tracing flags to gather specific runtime telemetry.
- **Track B: V8 Log Analysis**
  - Use the v8-log skill to extract v8.log and analyze internal state of V8
- **Track C: JS Source Analysis**:
  - Study the JavaScript benchmark to understand the core operations and potential hotspots.
- **Track D: Static V8 Research**:
  - Search for known optimization patterns or issues related to the observed JS patterns in the V8 codebase.

### 3. Running Benchmarks / Pages in Chrome with with Crossbench

- **Generate Performance Logs and Profiles**: use the crossbench skill to
  gather v8.log, detailed perfetto traces and sampling profiles from
  pages or benchmarks.
- **Probes**:
  - Use `--probe=profiling` for full-browser or d8 profiles
  - Use `--probe=perfetto` for detailed perfetto traces
  - Use `--probe=v8.log` for extracting internal v8 logs from chrome

### 4. Alternative: Running Benchmarks with jsb_run_bench

In the `jetski` environment, you can also use the `jsb_run_bench` tool from `v8-utils` as an alternative for quick runs.

- **Run for Scores**: Call `jsb_run_bench` with paths to `d8` binaries to compare performance.
- **Profile**: Supports `record: "perf"` and `record: "v8log"`.

### 5. Profile Analysis & Tick Processor

- **Generate and Analyze Profile**: Use the v8-profiling skill to generate
  and analyze linux-perf and tickprocessor profiles. Note that you can use
  crossbench for generating chromium-level profiles.
- **Cross Reference**:
  - Correlate JS sources with v8.log to understand what V8 is doing
    when executing the JS sources.
  - Use the v8.log to further drill down on internal v8 state to understand
    bottlenecks.
- **Interpretation**:
  - Look at the C++ entry points and JS functions taking the most ticks.
  - Check if time is spent in runtime functions vs. generated code.
  - Identify if specific builtins are taking significant time.
  - Identify inefficient JS code patterns in the workloads
  - Suggest builtins, C++ code that can be optimized

### 6. Tracing Compiler Graphs (Turbolizer)

For peak performance, it is often necessary to inspect the intermediate representations (IR) of the optimizing compiler (TurboFan or Turboshaft).

- **Generate Graph Data**: Run `d8 --trace-turbo script.js` or pass flags in Crossbench/jsb_run_bench. This generates JSON files containing the graph state at various optimization phases (e.g., `turbo-*.json`).
- **Visualize with Turbolizer**: Use the Turbolizer tool (available internally at `go/turbolizer` or in the V8 repository under `tools/turbolizer`).
- **Analysis**:
  - Inspect the graph at different phases to see how nodes are simplified, combined, or eliminated.
  - Look for missed optimizations, such as redundant checks that were not hoisted or allocations that failed to be eliminated by escape analysis.
  - Identify unexpected deoptimization points.

### 7. Identifying General Efficiency Improvements

Beyond hotspots, look for areas where V8 can be improved to handle patterns better:

- **Reducing Allocations**: High GC overhead implies frequent allocations. Investigate if V8 can optimize allocation folding, escape analysis, or if the allocations are unavoidable.
- **Optimizing Hot Loops**: Ensure loops are not deoptimizing in V8. Check if checks can be hoisted by the compiler or if loop peeling is effective in the VM.
- **Hidden Class (Map) Stability**: Understand how object shapes evolve and cause polymorphic or megamorphic IC states. Investigate if V8 can be optimized to handle these transitions better.

### 8. Analysis & Reprioritization

- Analyze profile results (e.g., flamegraphs, top functions).
- **Dynamic Reprioritization**:
  - **High GC Time**: If profile shows significant time in GC, pivot to allocation analysis and reducing memory churn. Also explain which JS code is causing frequent allocations.
  - **High IC Misses**: If `--log-ic` shows frequent misses, pivot to investigating object layout and stabilizing hidden classes.
  - **Dominant Hotspot**: If a single function dominates execution time, focus all efforts on that component.
  - **Pattern Identification**: If a V8 change is identified that could improve the pattern generally, prioritize implementing and testing it over further analysis.

### 9. Optimization & Verification

- Propose a V8 change to improve performance (e.g., specialized builtin, improved optimization pass).
- **Verification on Pinpoint (Preferred)**:
  - Commit changes to a local branch.
  - Use the automation script to upload a CL and start a Pinpoint job:
    ```bash
    scripts/upload_and_pinpoint.py \
      --benchmark=<benchmark_name> \
      --bot=<bot_name> \
      --message="Experiment: My performance optimization"
    ```
  - Use `./cb.py pinpoint help` to understand the available options.
- **Local Verification**:
  - Re-run the benchmark locally if Pinpoint is unavailable.
  - Compare `perf` stats (cycles, instructions).
  - Ensure no regressions in correctness or other benchmarks.
