# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Use this to run several variants of the tests.
ALL_VARIANT_FLAGS = {
  "code_serializer": [["--cache=code"]],
  "default": [[]],
  "future": [["--future"]],
  "gc_stats": [["--gc-stats=1"]],
  # Alias of exhaustive variants, but triggering new test framework features.
  "infra_staging": [[]],
  "interpreted_regexp": [["--regexp-interpret-all"]],
  "jitless": [["--jitless"]],
  "minor_mc": [["--minor-mc"]],
  # No optimization means disable all optimizations. OptimizeFunctionOnNextCall
  # would not force optimization too. It turns into a Nop. Please see
  # https://chromium-review.googlesource.com/c/452620/ for more discussion.
  # For WebAssembly, we test "Liftoff-only" in the nooptimization variant and
  # "TurboFan-only" in the stress variant. The WebAssembly configuration is
  # independent of JS optimizations, so we can combine those configs.
  "nooptimization": [["--no-opt", "--liftoff", "--no-wasm-tier-up"]],
  "slow_path": [["--force-slow-path"]],
  "stress": [["--stress-opt", "--always-opt", "--no-liftoff",
              "--no-wasm-tier-up"]],
  "stress_background_compile": [["--stress-background-compile"]],
  "stress_incremental_marking":  [["--stress-incremental-marking"]],
  # Trigger stress sampling allocation profiler with sample interval = 2^14
  "stress_sampling": [["--stress-sampling-allocation-profiler=16384"]],
  "trusted": [["--no-untrusted-code-mitigations"]],
  "no_wasm_traps": [["--no-wasm-trap-handler"]],
}

SLOW_VARIANTS = set([
  'stress',
  'nooptimization',
])

FAST_VARIANTS = set([
  'default'
])


def _variant_order_key(v):
  if v in SLOW_VARIANTS:
    return 0
  if v in FAST_VARIANTS:
    return 100
  return 50

ALL_VARIANTS = sorted(ALL_VARIANT_FLAGS.keys(),
                      key=_variant_order_key)

# Check {SLOW,FAST}_VARIANTS entries
for variants in [SLOW_VARIANTS, FAST_VARIANTS]:
  for v in variants:
    assert v in ALL_VARIANT_FLAGS
