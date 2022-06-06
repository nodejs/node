# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Use this to run several variants of the tests.
ALL_VARIANT_FLAGS = {
  "assert_types": [["--assert-types"]],
  "code_serializer": [["--cache=code"]],
  "default": [[]],
  "future": [["--future"]],
  "gc_stats": [["--gc-stats=1"]],
  # Alias of exhaustive variants, but triggering new test framework features.
  "infra_staging": [[]],
  "interpreted_regexp": [["--regexp-interpret-all"]],
  "experimental_regexp":  [["--default-to-experimental-regexp-engine"]],
  "concurrent_inlining":  [["--concurrent-inlining"]],
  "jitless": [["--jitless"]],
  "sparkplug": [["--sparkplug"]],
  "always_sparkplug": [[ "--always-sparkplug", "--sparkplug"]],
  "minor_mc": [["--minor-mc"]],
  "no_lfa": [["--no-lazy-feedback-allocation"]],
  # No optimization means disable all optimizations. OptimizeFunctionOnNextCall
  # would not force optimization too. It turns into a Nop. Please see
  # https://chromium-review.googlesource.com/c/452620/ for more discussion.
  # For WebAssembly, we test "Liftoff-only" in the nooptimization variant and
  # "TurboFan-only" in the stress variant. The WebAssembly configuration is
  # independent of JS optimizations, so we can combine those configs.
  "nooptimization": [["--no-opt", "--liftoff", "--no-wasm-tier-up"]],
  "slow_path": [["--force-slow-path"]],
  "stress": [["--stress-opt", "--no-liftoff", "--stress-lazy-source-positions",
              "--no-wasm-generic-wrapper"]],
  "stress_concurrent_allocation": [["--stress-concurrent-allocation"]],
  "stress_concurrent_inlining": [["--stress-concurrent-inlining",
                                  "--concurrent-inlining"]],
  "stress_js_bg_compile_wasm_code_gc": [["--stress-background-compile",
                                         "--stress-wasm-code-gc"]],
  "stress_incremental_marking": [["--stress-incremental-marking"]],
  "stress_snapshot": [["--stress-snapshot"]],
  # Trigger stress sampling allocation profiler with sample interval = 2^14
  "stress_sampling": [["--stress-sampling-allocation-profiler=16384"]],
  # TODO(rmcilroy): Remove trusted variant once bots don't use it.
  "trusted": [[]],
  "no_wasm_traps": [["--no-wasm-trap-handler"]],
  "turboprop": [["--turboprop"]],
  "turboprop_as_toptier": [["--turboprop-as-toptier", "--turboprop"]],
  "instruction_scheduling": [["--turbo-instruction-scheduling"]],
  "stress_instruction_scheduling": [["--turbo-stress-instruction-scheduling"]],
  "top_level_await": [["--harmony-top-level-await"]],
  "wasm_write_protect_code": [["--wasm-write-protect-code-memory"]],
}

# Flags that lead to a contradiction with the flags provided by the respective
# variant. This depends on the flags specified in ALL_VARIANT_FLAGS and on the
# implications defined in flag-definitions.h.
INCOMPATIBLE_FLAGS_PER_VARIANT = {
  "jitless": ["--opt", "--always-opt", "--liftoff", "--track-field-types",
              "--validate-asm", "--sparkplug", "--always-sparkplug",
              "--regexp-tier-up"],
  "nooptimization": ["--always-opt"],
  "slow_path": ["--no-force-slow-path"],
  "stress_concurrent_allocation": ["--single-threaded-gc", "--predictable"],
  "stress_concurrent_inlining": ["--single-threaded", "--predictable", "--turboprop"],
  "turboprop": ["--stress_concurrent_inlining"],
  # The fast API tests initialize an embedder object that never needs to be
  # serialized to the snapshot, so we don't have a
  # SerializeInternalFieldsCallback for it, so they are incompatible with
  # stress_snapshot.
  "stress_snapshot": ["--turbo-fast-api-calls"],
  "stress": ["--always-opt", "--no-always-opt",
             "--max-inlined-bytecode-size=*",
             "--max-inlined-bytecode-size-cumulative=*", "--stress-inline",
             "--liftoff-only"],
  "sparkplug": ["--jitless"],
  "always_sparkplug": ["--jitless"],
  "code_serializer": ["--cache=after-execute", "--cache=full-code-cache",
                      "--cache=none"],
  "experimental_regexp": ["--no-enable-experimental-regexp-engine"],
  # There is a negative implication: --perf-prof disables
  # --wasm-write-protect-code-memory.
  "wasm_write_protect_code": ["--perf-prof"],
}

# Flags that lead to a contradiction under certain build variables.
# This corresponds to the build variables usable in status files as generated
# in _get_statusfile_variables in base_runner.py.
# The conflicts might be directly contradictory flags or be caused by the
# implications defined in flag-definitions.h.
INCOMPATIBLE_FLAGS_PER_BUILD_VARIABLE = {
  "lite_mode": ["--no-lazy-feedback-allocation", "--max-semi-space-size=*",
                "--stress-concurrent-inlining"]
               + INCOMPATIBLE_FLAGS_PER_VARIANT["jitless"],
  "predictable": ["--parallel-compile-tasks",
                  "--concurrent-recompilation",
                  "--stress-concurrent-allocation",
                  "--stress-concurrent-inlining"],
}

# Flags that lead to a contradiction when a certain extra-flag is present.
# Such extra-flags are defined for example in infra/testing/builders.pyl or in
# standard_runner.py.
# The conflicts might be directly contradictory flags or be caused by the
# implications defined in flag-definitions.h.
INCOMPATIBLE_FLAGS_PER_EXTRA_FLAG = {
  "--concurrent-recompilation": ["--predictable"],
  "--gc-interval=*": ["--gc-interval=*"],
  "--optimize-for-size": ["--max-semi-space-size=*"],
  "--stress_concurrent_allocation":
        INCOMPATIBLE_FLAGS_PER_VARIANT["stress_concurrent_allocation"],
  "--stress-concurrent-inlining":
        INCOMPATIBLE_FLAGS_PER_VARIANT["stress_concurrent_inlining"],
}

SLOW_VARIANTS = set([
  'stress',
  'stress_snapshot',
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
