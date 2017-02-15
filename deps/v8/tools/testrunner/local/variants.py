# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Use this to run several variants of the tests.
ALL_VARIANT_FLAGS = {
  "default": [[]],
  "stress": [["--stress-opt", "--always-opt"]],
  "turbofan": [["--turbo"]],
  "turbofan_opt": [["--turbo", "--always-opt"]],
  "nocrankshaft": [["--nocrankshaft"]],
  "ignition": [["--ignition"]],
  "ignition_staging": [["--ignition-staging"]],
  "ignition_turbofan": [["--ignition-staging", "--turbo"]],
  "preparser": [["--min-preparse-length=0"]],
  "asm_wasm": [["--validate-asm"]],
}

# FAST_VARIANTS implies no --always-opt.
FAST_VARIANT_FLAGS = {
  "default": [[]],
  "stress": [["--stress-opt"]],
  "turbofan": [["--turbo"]],
  "nocrankshaft": [["--nocrankshaft"]],
  "ignition": [["--ignition"]],
  "ignition_staging": [["--ignition-staging"]],
  "ignition_turbofan": [["--ignition-staging", "--turbo"]],
  "preparser": [["--min-preparse-length=0"]],
  "asm_wasm": [["--validate-asm"]],
}

ALL_VARIANTS = set(["default", "stress", "turbofan", "turbofan_opt",
                    "nocrankshaft", "ignition", "ignition_staging",
                    "ignition_turbofan", "preparser", "asm_wasm"])
