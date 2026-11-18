# Copyright 2024 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# These settings enable overwriting variables in V8's DEPS file.
GCLIENT_VARS = struct(
    BENCHMARKS = {"checkout_benchmarks": "True"},
    CENTIPEDE = {"checkout_centipede_deps": "True"},
    COVERAGE = {"checkout_clang_coverage_tools": "True"},
    GCMOLE = {"download_gcmole": "True"},
    INSTRUMENTED_LIBRARIES = {"checkout_instrumented_libraries": "True"},
    ITTAPI = {"checkout_ittapi": "True"},
    JSFUNFUZZ = {"download_jsfunfuzz": "True"},
    LINUX_ARM64_SYMBOLIZER = {"download_prebuilt_arm64_llvm_symbolizer": "True"},
    NO_BENCHMARKS = {"checkout_benchmarks": "False"},
    V8_HEADER_INCLUDES = {"check_v8_header_includes": "True"},
)

def gclient_vars_properties(props):
    # We check out benchmarks on all bots.
    gclient_vars = dict(GCLIENT_VARS.BENCHMARKS)
    for prop in props:
        gclient_vars.update(prop)
    if gclient_vars:
        return {"gclient_vars": gclient_vars}
    else:
        return {}
