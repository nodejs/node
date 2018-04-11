# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Use this to run several variants of the tests.
ALL_VARIANT_FLAGS = {
  "code_serializer": [["--cache=code"]],
  "default": [[]],
  "future": [["--future"]],
  # Alias of exhaustive variants, but triggering new test framework features.
  "infra_staging": [[]],
  "liftoff": [["--liftoff"]],
  "minor_mc": [["--minor-mc"]],
  # No optimization means disable all optimizations. OptimizeFunctionOnNextCall
  # would not force optimization too. It turns into a Nop. Please see
  # https://chromium-review.googlesource.com/c/452620/ for more discussion.
  "nooptimization": [["--noopt"]],
  "slow_path": [["--force-slow-path"]],
  "stress": [["--stress-opt", "--always-opt"]],
  "stress_background_compile": [["--background-compile", "--stress-background-compile"]],
  "stress_incremental_marking":  [["--stress-incremental-marking"]],
  # Trigger stress sampling allocation profiler with sample interval = 2^14
  "stress_sampling": [["--stress-sampling-allocation-profiler=16384"]],
  "trusted": [["--no-untrusted-code-mitigations"]],
  "wasm_traps": [["--wasm_trap_handler", "--invoke-weak-callbacks", "--wasm-jit-to-native"]],
  "wasm_no_native": [["--no-wasm-jit-to-native"]],
}

ALL_VARIANTS = set(ALL_VARIANT_FLAGS.keys())
