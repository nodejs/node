# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/bucket-defaults.star", "bucket_defaults")
load("//lib/builders.star", "v8_builder")
load("//lib/gclient.star", "GCLIENT_VARS")
load("//lib/lib.star", "CQ")
load("//lib/siso.star", "SISO")

def with_cancel(cq_properties):
    result = dict(cq_properties)
    result["cancel_stale"] = True
    return result

#TODO(almuthanna): get rid of kwargs and specify default values
def trybot_pair(
        name,
        cq_properties = CQ.OPTIONAL,
        cq_branch_properties = CQ.OPTIONAL,
        cq_compile_only_properties = CQ.OPTIONAL,
        cq_branch_compile_only_properties = CQ.OPTIONAL,
        experiments = {},
        description = None,
        build_timeout = None,
        total_timeout = None,
        disable_resultdb_exports = True,
        **kwargs):
    # Compilator names are constructed based on orchestrator names with an
    # infix like: v8_linux_rel -> v8_linux_compile_rel.
    prefix, suffix = name.rsplit("_", 1)
    compilator_name = prefix + "_compile_" + suffix

    orchestrator_description = None
    if description:
        orchestrator_description = dict(description)
        orchestrator_description["compiles_with"] = compilator_name

    # Generate orchestrator trybot.
    v8_builder(
        bucket_defaults["try.triggered"],
        name = name,
        bucket = "try",
        execution_timeout = total_timeout,
        executable = "recipe:v8/orchestrator",
        cq_properties = with_cancel(cq_properties),
        cq_branch_properties = with_cancel(cq_branch_properties),
        in_list = "tryserver",
        experiments = experiments,
        description = orchestrator_description,
        properties = {"compilator_name": compilator_name},
        disable_resultdb_exports = disable_resultdb_exports,
    )

    # Generate compilator trybot.
    v8_builder(
        bucket_defaults["try"],
        name = compilator_name,
        bucket = "try",
        execution_timeout = build_timeout,
        executable = "recipe:v8/compilator",
        cq_properties = with_cancel(cq_compile_only_properties),
        cq_branch_properties = with_cancel(cq_branch_compile_only_properties),
        in_list = "tryserver",
        experiments = experiments,
        description = description,
        disable_resultdb_exports = disable_resultdb_exports,
        use_siso = SISO.CHROMIUM_UNTRUSTED,
        **kwargs
    )

trybot_pair(
    name = "v8_android_arm64_p7_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"target_platform": "android", "target_arch": "arm"},
)

trybot_pair(
    name = "v8_linux64_arm64_asan_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"target_arch": "arm", "target_bits": 64},
    gclient_vars = [GCLIENT_VARS.LINUX_ARM64_SYMBOLIZER],
)

trybot_pair(
    name = "v8_linux64_arm64_dbg",
    cq_properties = CQ.on_files(".*generated-code-validator.*"),
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"target_arch": "arm", "target_bits": 64},
)

trybot_pair(
    name = "v8_linux64_arm64_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"target_arch": "arm", "target_bits": 64},
)

trybot_pair(
    name = "v8_linux64_arm64_no_pointer_compression_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_asan_rel",
    cq_compile_only_properties = CQ.BLOCK,
    cq_properties = CQ.OPTIONAL,
    cq_branch_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_asan_sandbox_testing_rel",
    cq_compile_only_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"default_targets": ["v8_clusterfuzz"]},
)

trybot_pair(
    name = "v8_linux64_cfi_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    build_timeout = 3600,
)

trybot_pair(
    name = "v8_linux64_cppgc_non_default_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_css_dbg",
    cq_properties = CQ.OPTIONAL,
    cq_compile_only_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_no_sandbox_dbg",
    cq_properties = CQ.OPTIONAL,
    cq_branch_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_dbg",
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_full_dbg",
    cq_properties = CQ.OPTIONAL,
    cq_compile_only_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_no_shared_cage_dbg",
    cq_properties = CQ.OPTIONAL,
    cq_branch_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_dict_tracking_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_runtime_call_stats_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_dumpling_rel",
    cq_properties = CQ.OPTIONAL,
    cq_compile_only_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_gc_stress_custom_snapshot_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_gc_stress_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_fuzzilli_rel",
    cq_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_fyi_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_icu4x_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_lower_limits_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_msan_rel",
    cq_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    gclient_vars = [GCLIENT_VARS.INSTRUMENTED_LIBRARIES],
)

trybot_pair(
    name = "v8_linux64_nodcheck_rel",
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_official_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    build_timeout = 3600,
)

trybot_pair(
    name = "v8_linux64_perfetto_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_pku_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_pku_dbg",
    cq_properties = CQ.OPTIONAL,
    cq_compile_only_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_no_metagen_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_no_pointer_compression_rel",
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_no_sandbox_rel",
    cq_properties = CQ.OPTIONAL,
    cq_branch_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_sandbox_testing_rel",
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    properties = {"default_targets": ["v8_clusterfuzz"]},
)

trybot_pair(
    name = "v8_linux64_single_generation_dbg",
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_sticky_mark_bits_dbg",
    cq_properties = CQ.OPTIONAL,
    cq_compile_only_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_rel",
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    gclient_vars = [GCLIENT_VARS.GCMOLE],
)

trybot_pair(
    name = "v8_linux64_predictable_rel",
    cq_properties = CQ.OPTIONAL,
    cq_branch_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_riscv64_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_riscv64_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_riscv64_pointer_compression_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_loong64_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_tsan_rel",
    cq_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_tsan_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_tsan_no_cm_rel",
    cq_properties = CQ.on_files(
        "src/compiler/js-heap-broker.(h|cc)",
        "src/compiler/heap-refs.(h|cc)",
    ),
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_tsan_isolates_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_ubsan_rel",
    cq_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_undefined_double_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux64_verify_csa_rel",
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux_arm64_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux_arm64_gc_stress_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux_arm64_rel",
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    gclient_vars = [GCLIENT_VARS.GCMOLE],
)

trybot_pair(
    name = "v8_linux_arm_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux_arm_lite_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux_arm_rel",
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    gclient_vars = [GCLIENT_VARS.GCMOLE],
)

trybot_pair(
    name = "v8_linux_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux_full_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux_gc_stress_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux_nodcheck_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux_noi18n_rel",
    cq_properties = CQ.on_files(".*intl.*", ".*test262.*"),
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux_rel",
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.BLOCK,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    gclient_vars = [GCLIENT_VARS.GCMOLE],
)

trybot_pair(
    name = "v8_linux_optional_rel",
    cq_properties = CQ.on_files(
        "src/codegen/shared-ia32-x64/macro-assembler-shared-ia32-x64.(h|cc)",
        "src/codegen/x64/(macro-)?assembler-x64.(h|cc)",
        "src/codegen/x64/sse-instr.h",
        "src/compiler/backend/x64/code-generator-x64.cc",
        "src/wasm/baseline/x64/liftoff-assembler-x64.h",
    ),
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_linux_verify_csa_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_mac64_dbg",
    total_timeout = 7200,
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Mac", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_mac64_asan_rel",
    total_timeout = 7200,
    cq_properties = CQ.OPTIONAL,
    # TODO(almuthanna): add this to Branch CQ after current milestone + 3
    # (i.e. M100).
    cq_compile_only_properties = CQ.OPTIONAL,
    dimensions = {"os": "Mac", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_mac64_noopt_dbg",
    total_timeout = 7200,
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Mac", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_mac64_rel",
    total_timeout = 7200,
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.BLOCK,
    dimensions = {"os": "Mac", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_mac_arm64_gc_stress_dbg",
    total_timeout = 7200,
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Mac", "cpu": "arm64"},
)

trybot_pair(
    name = "v8_mac_arm64_rel",
    total_timeout = 7200,
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.BLOCK,
    dimensions = {"os": "Mac", "cpu": "arm64"},
)

trybot_pair(
    name = "v8_mac_arm64_dbg",
    total_timeout = 7200,
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Mac", "cpu": "arm64"},
)

trybot_pair(
    name = "v8_mac_arm64_full_dbg",
    total_timeout = 7200,
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Mac", "cpu": "arm64"},
)

trybot_pair(
    name = "v8_mac_arm64_no_pointer_compression_dbg",
    total_timeout = 7200,
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Mac", "cpu": "arm64"},
)

trybot_pair(
    name = "v8_numfuzz_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_numfuzz_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_numfuzz_tsan_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_win64_cet_shadow_stack_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Windows-10", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_win64_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Windows-10", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_win64_drumbrake_dbg",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Windows-10", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_win64_nodcheck_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Windows-10", "cpu": "x86-64"},
    total_timeout = 7200,
)

trybot_pair(
    name = "v8_win64_rel",
    cq_properties = CQ.BLOCK,
    cq_branch_properties = CQ.BLOCK,
    dimensions = {"os": "Windows-10", "cpu": "x86-64"},
    total_timeout = 7200,
)

trybot_pair(
    name = "v8_win_dbg",
    cq_properties = CQ.OPTIONAL,
    cq_compile_only_properties = CQ.BLOCK,
    cq_branch_compile_only_properties = CQ.BLOCK,
    dimensions = {"os": "Windows-10", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_win_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Windows-10", "cpu": "x86-64"},
)

trybot_pair(
    name = "v8_win64_asan_rel",
    cq_properties = CQ.OPTIONAL,
    dimensions = {"os": "Windows-10", "cpu": "x86-64"},
)
