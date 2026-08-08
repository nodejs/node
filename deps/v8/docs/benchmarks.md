---
title: 'Running benchmarks locally'
description: 'This document explains how to run benchmark suites in d8.'
---
V8 performance is typically tracked using industry-standard benchmark suites that reflect real-world usage.

For modern V8 performance tracking, we recommend using:
- **[Speedometer](https://browserbench.org/Speedometer3.1/)**: The current industry standard for measuring web application responsiveness.
- **[JetStream](https://browserbench.org/JetStream/)**: Focuses on advanced web applications and includes JavaScript and WebAssembly.

## CPU Configuration

Before running benchmarks, ensure your CPU is configured for consistent results by setting the frequency scaling governor to `performance`.

```bash
sudo tools/cpu.sh fast
```

## Internal Microbenchmarks

V8 maintains a suite of microbenchmarks in `test/js-perf-test` for tracking the performance of specific language features. These are run using `tools/run_perf.py`:

```bash
tools/run_perf.py --arch x64 --binary-override-path out/x64.release/d8 test/js-perf-test/JSTests.json
```

For more details, see [the testing guide](/docs/test#running-microbenchmarks).

## Useful `d8` Flags for Benchmarking

### Compiler Control

- `--no-opt`: Disables all optimizing compilers (TurboFan, Maglev).
- `--no-sparkplug`: Disables the Sparkplug baseline compiler.
- `--no-maglev`: Disables the Maglev mid-tier compiler.
- `--no-turbofan`: Disables the TurboFan top-tier optimizing compiler.
- `--trace-opt`: Trace optimized compilation events.
- `--trace-deopt`: Trace deoptimization events.
- `--trace-ic`: Trace inline cache behavior.
- `--print-opt-code`: Print the generated machine code for optimized functions.
- `--trace-turbo`: Trace the TurboFan compilation pipeline (generates `turbo-*.json` and `turbo-*.dot` files).
- `--trace-turbo-filter=<filter>`: Only trace specific functions that match the filter.
- `--trace-maglev`: Same as `--trace-turbo` but for the Maglev compiler.

### Profiling

- `--prof`: Log statistical profiling information for CPU profiling. Use `tools/linux-tick-processor` (on Linux) to process the resulting `v8.log`.
- `--log-timer-events`: Log timer events (including `console.time` and internal V8 events).

### Memory & Garbage Collection

- `--trace-gc`: Print a trace line for each garbage collection.
- `--max-semi-space-size=<MB>`: Adjusts the young generation (nursery) heap size.
- `--max-old-space-size=<MB>`: Adjusts the old generation heap size.
