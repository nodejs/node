# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Use this to run several variants of the tests.
ALL_VARIANT_FLAGS = {
  "default": [[]],
  "stress": [["--stress-opt", "--always-opt"]],
  "turbofan": [["--turbo"]],
  "turbofan_opt": [["--turbo", "--always-opt"]],
  "noturbofan": [["--no-turbo"]],
  "noturbofan_stress": [["--no-turbo", "--stress-opt", "--always-opt"]],
  "fullcode": [["--nocrankshaft", "--no-turbo"]],
  # No optimization actually means no profile guided optimization -
  # %OptimizeFunctionOnNextCall still works.
  "nooptimization": [["--nocrankshaft"]],
  "asm_wasm": [["--validate-asm", "--fast-validate-asm", "--stress-validate-asm", "--suppress-asm-messages"]],
  "wasm_traps": [["--wasm_guard_pages", "--wasm_trap_handler", "--invoke-weak-callbacks"]],
}

# FAST_VARIANTS implies no --always-opt.
FAST_VARIANT_FLAGS = {
  "default": [[]],
  "stress": [["--stress-opt"]],
  "turbofan": [["--turbo"]],
  "noturbofan": [["--no-turbo"]],
  "noturbofan_stress": [["--no-turbo", "--stress-opt"]],
  "fullcode": [["--nocrankshaft", "--no-turbo"]],
  # No optimization actually means no profile guided optimization -
  # %OptimizeFunctionOnNextCall still works.
  "nooptimization": [["--nocrankshaft"]],
  "asm_wasm": [["--validate-asm", "--fast-validate-asm", "--stress-validate-asm", "--suppress-asm-messages"]],
  "wasm_traps": [["--wasm_guard_pages", "--wasm_trap_handler", "--invoke-weak-callbacks"]],
}

ALL_VARIANTS = set(["default", "stress", "turbofan", "turbofan_opt",
                    "noturbofan", "noturbofan_stress", "fullcode",
                    "nooptimization", "asm_wasm", "wasm_traps"])
