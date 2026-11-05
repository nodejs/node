# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "v8_builder")
load("//lib/gclient.star", "GCLIENT_VARS")
load("//lib/lib.star", "BARRIER", "ci_pair_factory", "greedy_batching_of_1", "in_console")
load("//lib/siso.star", "SISO")

builder_pair = ci_pair_factory(v8_builder)

def clusterfuzz_builder(properties = None, barrier = BARRIER.TREE_CLOSER, default_target = "v8_clusterfuzz", **kwargs):
    properties = dict(properties or {})
    properties["builder_group"] = "client.v8.clusterfuzz"
    properties["default_targets"] = [default_target]
    return v8_builder(
        bucket = "ci",
        barrier = barrier,
        properties = properties,
        triggered_by = ["v8-trigger"],
        triggering_policy = greedy_batching_of_1,
        use_siso = SISO.CHROMIUM_TRUSTED,
        experiments = {"v8.resultdb": 100},
        **kwargs
    )

in_category = in_console("clusterfuzz")

in_category(
    "Windows",
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Win64 ASAN - release builder",
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "win64-release/d8-asan-win64-release-v8-component", "bucket": "v8-asan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Win64 ASAN - debug builder",
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "win64-debug/d8-asan-win64-debug-v8-component", "bucket": "v8-asan"}},
    ),
)

in_category(
    "Mac",
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Mac64 ASAN - release builder",
        dimensions = {"os": "Mac", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "mac-release/d8-asan-mac-release-v8-component", "bucket": "v8-asan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Mac64 ASAN - debug builder",
        dimensions = {"os": "Mac", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "mac-debug/d8-asan-mac-debug-v8-component", "bucket": "v8-asan"}},
    ),
)

in_category(
    "Linux",
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 - release builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clusterfuzz_archive": {"archive_path": "linux-release/d8-linux-release-v8-component", "bucket": "v8-asan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 - debug builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clusterfuzz_archive": {"archive_path": "linux-debug/d8-linux-debug-v8-component", "bucket": "v8-asan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 - dumpling - release builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clusterfuzz_archive": {"archive_path": "linux-release/d8-dumpling-linux-release-v8-component", "bucket": "v8-asan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 ASAN - debug builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "linux-debug/d8-asan-linux-debug-v8-component", "bucket": "v8-asan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 ASAN - undefined double - debug builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "linux-debug/d8-asan-undefined-double-linux-debug-v8-component", "bucket": "v8-asan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 ASAN arm64 - debug builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "linux-debug/d8-arm64-asan-linux-debug-v8-component", "bucket": "v8-asan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 ASAN - release builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "linux-release/d8-asan-linux-release-v8-component", "bucket": "v8-asan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 ASAN no inline - release builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "linux-release/d8-asan-no-inline-linux-release-v8-component", "bucket": "v8-asan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 CFI - release builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "linux-release/d8-cfi-linux-release-v8-component", "bucket": "v8-cfi"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 TSAN - release builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clusterfuzz_archive": {"archive_path": "linux-release/d8-tsan-linux-release-v8-component", "bucket": "v8-tsan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 UBSan - release builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "linux-release/d8-ubsan-linux-release-v8-component", "bucket": "v8-ubsan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux - debug builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clusterfuzz_archive": {"archive_path": "linux32-debug/d8-linux32-debug-v8-component", "bucket": "v8-asan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux ASAN - debug builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "linux32-debug/d8-asan-linux32-debug-v8-component", "bucket": "v8-asan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux ASAN arm - debug builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "linux-debug/d8-arm-asan-linux-debug-v8-component", "bucket": "v8-asan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux ASAN - release builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "linux32-release/d8-asan-linux32-release-v8-component", "bucket": "v8-asan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux ASAN no inline - release builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "linux32-release/d8-asan-no-inline-linux32-release-v8-component", "bucket": "v8-asan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux MSAN chained origins",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "linux-release/d8-msan-chained-origins-linux-release-v8-component", "bucket": "v8-msan"}},
        gclient_vars = [GCLIENT_VARS.INSTRUMENTED_LIBRARIES],
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux MSAN no origins",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "linux-release/d8-msan-no-origins-linux-release-v8-component", "bucket": "v8-msan"}},
        gclient_vars = [GCLIENT_VARS.INSTRUMENTED_LIBRARIES],
    ),
)

in_category(
    "Sandbox",
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 sandbox testing - release builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "linux-release/d8-sandbox-testing-linux-release-v8-component", "bucket": "v8-asan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 fuzzilli sandbox testing - release builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "linux-release/d8-fuzzilli-sandbox-testing-linux-release-v8-component", "bucket": "v8-asan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 ASAN sandbox testing - release builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "linux-release/d8-asan-sandbox-testing-linux-release-v8-component", "bucket": "v8-asan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 ASAN fuzzilli sandbox testing - release builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "linux-release/d8-asan-fuzzilli-sandbox-testing-linux-release-v8-component", "bucket": "v8-asan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 ASAN arm64 fuzzilli sandbox testing - release builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "linux-release/d8-asan-arm64-fuzzilli-sandbox-testing-linux-release-v8-component", "bucket": "v8-asan"}},
    ),
    clusterfuzz_builder(
        name = "V8 Clusterfuzz Linux64 arm64 fuzzilli sandbox testing - release builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clobber": True, "clusterfuzz_archive": {"archive_path": "linux-release/d8-arm64-fuzzilli-sandbox-testing-linux-release-v8-component", "bucket": "v8-asan"}},
    ),
    v8_builder(
        name = "V8 Linux64 - sandbox testing",
        parent_builder = "V8 Clusterfuzz Linux64 sandbox testing - release builder",
        bucket = "ci",
        properties = {"builder_group": "client.v8.clusterfuzz"},
        barrier = BARRIER.TREE_CLOSER,
    ),
    v8_builder(
        name = "V8 Linux64 ASAN - sandbox testing",
        parent_builder = "V8 Clusterfuzz Linux64 ASAN sandbox testing - release builder",
        bucket = "ci",
        properties = {"builder_group": "client.v8.clusterfuzz"},
        barrier = BARRIER.NONE,
    ),
)

in_category(
    "Fuzzilli",
    builder_pair(
        name = "V8 Linux64 - Fuzzilli",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8"},
        disable_resultdb_exports = True,
        use_siso = SISO.CHROMIUM_TRUSTED,
        barrier = BARRIER.TREE_CLOSER,
    ),
)

in_category(
    "FuzzTest",
    clusterfuzz_builder(
        name = "V8 Centipede Linux64 ASAN  - release builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clusterfuzz_archive": {"archive_path": "linux-release/fuzztest-asan-linux-release-v8-component", "bucket": "v8-asan"}},
        gclient_vars = [GCLIENT_VARS.CENTIPEDE],
        default_target = "v8_fuzztests",
        barrier = BARRIER.NONE,
    ),
    clusterfuzz_builder(
        name = "V8 Centipede Linux64 ASAN  - debug builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"clusterfuzz_archive": {"archive_path": "linux-debug/fuzztest-asan-linux-debug-v8-component", "bucket": "v8-asan"}},
        gclient_vars = [GCLIENT_VARS.CENTIPEDE],
        default_target = "v8_fuzztests",
        barrier = BARRIER.NONE,
    ),
)

in_category(
    "BigSleep",
    v8_builder(
        name = "V8 Linux64 - big sleep",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:v8/bigsleep",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8"},
        use_siso = SISO.CHROMIUM_TRUSTED,
        gclient_vars = [GCLIENT_VARS.NO_BENCHMARKS],
        disable_resultdb_exports = True,
        barrier = BARRIER.NONE,
    ),
)

in_category(
    "NumFuzz",
    v8_builder(
        name = "V8 NumFuzz",
        parent_builder = "V8 Clusterfuzz Linux64 - release builder",
        bucket = "ci",
        execution_timeout = 19800,
        properties = {"builder_group": "client.v8.clusterfuzz", "disable_auto_bisect": True},
        barrier = BARRIER.NONE,
        experiments = {"v8.resultdb": 100},
        notifies = ["NumFuzz maintainer"],
        disable_resultdb_exports = True,
    ),
    v8_builder(
        name = "V8 NumFuzz - debug",
        parent_builder = "V8 Clusterfuzz Linux64 - debug builder",
        bucket = "ci",
        execution_timeout = 19800,
        properties = {"builder_group": "client.v8.clusterfuzz", "disable_auto_bisect": True},
        barrier = BARRIER.NONE,
        experiments = {"v8.resultdb": 100},
        notifies = ["NumFuzz maintainer"],
        disable_resultdb_exports = True,
    ),
    v8_builder(
        name = "V8 NumFuzz - TSAN",
        parent_builder = "V8 Clusterfuzz Linux64 TSAN - release builder",
        bucket = "ci",
        execution_timeout = 19800,
        properties = {"builder_group": "client.v8.clusterfuzz", "disable_auto_bisect": True},
        barrier = BARRIER.NONE,
        experiments = {"v8.resultdb": 100},
        notifies = ["NumFuzz maintainer"],
        disable_resultdb_exports = True,
    ),
)
