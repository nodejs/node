# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "presubmit_builder", "try_builder", "v8_builder")
load("//lib/gclient.star", "GCLIENT_VARS")
load("//lib/lib.star", "CQ")
load("//lib/siso.star", "SISO")

presubmit_builder(
    "v8_presubmit",
    "try",
    cq_properties = CQ.BLOCK_NO_REUSE,
    cq_branch_properties = CQ.BLOCK_NO_REUSE,
    cq_chromium_branch_properties = CQ.BLOCK_NO_REUSE,
)

try_builder(
    name = "v8_android_arm_compile_rel",
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"target_platform": "android", "target_arch": "arm"},
)

try_builder(
    name = "v8_android_arm_verify_deterministic_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"target_platform": "android", "target_arch": "arm", "default_targets": ["verify_deterministic_mksnapshot"]},
    always_isolate_targets = ["snapshot_set"],
)

try_builder(
    name = "v8_android_arm64_compile_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"target_platform": "android", "target_arch": "arm"},
)

try_builder(
    name = "v8_android_arm64_d8_compile_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"default_targets": ["d8"], "target_platform": "android", "target_arch": "arm"},
)

try_builder(
    name = "v8_fuchsia_compile_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"target_platform": "fuchsia"},
)

try_builder(
    name = "v8_full_presubmit",
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.BLOCK,
    executable = "recipe:v8/presubmit",
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

try_builder(
    name = "v8_ios_simulator",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Mac", "cpu": "x86-64"},
    execution_timeout = 3600,
    properties = {"$depot_tools/osx_sdk": {"sdk_version": "16c5032a"}, "target_platform": "ios"},
    caches = [
        swarming.cache(
            path = "osx_sdk",
            name = "osx_sdk",
        ),
    ],
)

try_builder(
    name = "v8_linux64_bazel",
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.OPTIONAL,
    executable = "recipe:v8/bazel",
    dimensions = {"host_class": "bazel", "os": "Ubuntu", "cpu": "x86-64"},
    execution_timeout = 3600,
)

try_builder(
    name = "v8_linux64_bigsleep",
    cq_properties = CQ.OPTIONAL,
    executable = "recipe:v8/bigsleep",
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    gclient_vars = [GCLIENT_VARS.NO_BENCHMARKS],
)

try_builder(
    name = "v8_linux64_coverage_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    gclient_vars = [GCLIENT_VARS.COVERAGE],
    properties = {
        "enable_swarming": False,
        "coverage": "llvm",
    },
    execution_timeout = 7200,
)

try_builder(
    name = "v8_linux64_coverage_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    gclient_vars = [GCLIENT_VARS.COVERAGE],
    properties = {
        "enable_swarming": False,
        "coverage": "llvm",
    },
)

try_builder(
    name = "v8_linux64_native_arm64_no_pointer_compression_compile_rel",
    cq_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"target_arch": "arm", "target_bits": 64},
)

try_builder(
    name = "v8_linux64_verify_builtins_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"default_targets": ["verify_all_builtins_hashes"]},
)

try_builder(
    name = "v8_linux64_verify_deterministic_rel",
    cq_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"default_targets": ["verify_deterministic_mksnapshot"]},
    always_isolate_targets = ["snapshot_set"],
)

try_builder(
    name = "v8_linux64_header_includes_dbg",
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    gclient_vars = [GCLIENT_VARS.V8_HEADER_INCLUDES],
)

try_builder(
    name = "v8_linux64_no_wasm_compile_rel",
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

try_builder(
    name = "v8_linux64_arm64_asan_fuzzilli_sandbox_testing_compile_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"default_targets": ["v8_clusterfuzz"]},
)

try_builder(
    name = "v8_linux64_arm64_fuzzilli_sandbox_testing_compile_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"default_targets": ["v8_clusterfuzz"]},
)

try_builder(
    name = "v8_linux64_arm64_no_wasm_compile_dbg",
    cq_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"target_arch": "arm", "target_bits": 64},
)

try_builder(
    name = "v8_linux64_asan_centipede_compile_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    gclient_vars = [GCLIENT_VARS.CENTIPEDE],
    properties = {"default_targets": ["v8_fuzztests"]},
)

try_builder(
    name = "v8_linux64_asan_centipede_compile_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    gclient_vars = [GCLIENT_VARS.CENTIPEDE],
    properties = {"default_targets": ["v8_fuzztests"]},
)

try_builder(
    name = "v8_linux64_asan_fuzzilli_sandbox_testing_compile_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"default_targets": ["v8_clusterfuzz"]},
)

try_builder(
    name = "v8_linux64_asan_undefined_double_compile_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"default_targets": ["v8_clusterfuzz"]},
)

try_builder(
    name = "v8_linux64_fuzzilli_imports_compile_rel",
    cq_properties = CQ.OPTIONAL,
    cq_branch_properties = CQ.OPTIONAL,
    executable = "recipe:v8/compilator",
    properties = {"default_targets": ["d8"]},
    dimensions = {"os": "Ubuntu-22.04"},
)

try_builder(
    name = "v8_linux64_fuzzilli_sandbox_testing_compile_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"default_targets": ["v8_clusterfuzz"]},
)

try_builder(
    name = "v8_linux64_shared_compile_rel",
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

try_builder(
    name = "v8_linux_arm_lite_compile_dbg",
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

try_builder(
    name = "v8_linux_blink_rel",
    cq_properties = CQ.OPTIONAL,
    executable = "recipe:chromium_trybot",
    dimensions = {"host_class": "chromium", "os": "Ubuntu-22.04", "cpu": "x86-64"},
    execution_timeout = 4400,
    build_numbers = True,
    disable_resultdb_exports = True,
)

try_builder(
    name = "v8_linux_chromium_gn_rel",
    cq_properties = CQ.BLOCK,
    executable = "recipe:chromium_trybot",
    dimensions = {"host_class": "chromium", "os": "Ubuntu-22.04", "cpu": "x86-64"},
    execution_timeout = 4200,
    build_numbers = True,
    use_siso = SISO.CHROMIUM_UNTRUSTED_J500,
    disable_resultdb_exports = True,
)

try_builder(
    name = "v8_linux_mips64el_compile_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

try_builder(
    name = "v8_linux_noi18n_compile_dbg",
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

try_builder(
    name = "v8_linux_shared_compile_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

try_builder(
    name = "v8_linux_torque_compare",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"default_targets": ["compare_torque_runs"]},
)

try_builder(
    name = "v8_linux_vtunejit",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    gclient_vars = [GCLIENT_VARS.ITTAPI],
)

try_builder(
    name = "v8_test_tools",
    cq_properties = CQ.on_files("tools/clusterfuzz/js_fuzzer/.+"),
    executable = "recipe:v8/test_tools",
    dimensions = {"host_class": "docker", "os": "Ubuntu-22.04", "cpu": "x86-64"},
)

try_builder(
    name = "v8_win_arm64_compile_dbg",
    cq_properties = CQ.OPTIONAL,
    cq_branch_properties = CQ.OPTIONAL,
    dimensions = {"host_class": "default", "os": "Windows-10", "cpu": "x86-64"},
)

try_builder(
    name = "v8_flako",
    bucket = "try.triggered",
    executable = "recipe:v8/flako",
    execution_timeout = 14400,
)

try_builder(
    name = "v8_verify_flakes",
    bucket = "try.triggered",
    executable = "recipe:v8/verify_flakes",
    execution_timeout = 16200,
    schedule = "with 3h interval",
)

try_builder(
    name = "v8_mega_cq_launcher",
    executable = "recipe:v8/mega_cq_launcher",
    execution_timeout = 16200,
)

try_builder(
    name = "v8_linux_pgo_compile_rel",
    cq_properties = CQ.OPTIONAL,
    cq_branch_properties = CQ.OPTIONAL,
    executable = "recipe:v8/compilator",
    properties = {"default_targets": ["d8_pgo"]},
    dimensions = {"os": "Ubuntu-22.04"},
)

try_builder(
    name = "v8_linux64_pgo_compile_rel",
    cq_properties = CQ.OPTIONAL,
    cq_branch_properties = CQ.OPTIONAL,
    executable = "recipe:v8/compilator",
    properties = {"default_targets": ["d8_pgo"]},
    dimensions = {"os": "Ubuntu-22.04"},
)

try_builder(
    name = "v8_win_pgo_compile_rel",
    cq_properties = CQ.OPTIONAL,
    cq_branch_properties = CQ.OPTIONAL,
    executable = "recipe:v8/compilator",
    properties = {"default_targets": ["d8_pgo"]},
    dimensions = {"os": "Windows-10"},
)

try_builder(
    name = "v8_win64_pgo_compile_rel",
    cq_properties = CQ.OPTIONAL,
    cq_branch_properties = CQ.OPTIONAL,
    executable = "recipe:v8/compilator",
    properties = {"default_targets": ["d8_pgo"]},
    dimensions = {"os": "Windows-10"},
)
