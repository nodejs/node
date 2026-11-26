# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "main_multibranch_builder")
load("//lib/gclient.star", "GCLIENT_VARS")
load("//lib/lib.star", "BARRIER", "ci_pair_factory", "in_branch_console", "parallel_3_greedy_batching_of_1")

in_category = in_branch_console("main")
main_multibranch_builder_pair = ci_pair_factory(main_multibranch_builder)

in_category(
    "Linux",
    main_multibranch_builder_pair(
        name = "V8 Linux",
        release_branches = True,
        triggering_policy = parallel_3_greedy_batching_of_1,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"binary_size_tracking": {"category": "linux32", "binary": "d8"}},
        gclient_vars = [GCLIENT_VARS.GCMOLE],
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder_pair(
        name = "V8 Linux - debug",
        release_branches = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder_pair(
        name = "V8 Linux - shared",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"binary_size_tracking": {"category": "linux32", "binary": "libv8.so"}},
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder_pair(
        name = "V8 Linux - noi18n - debug",
        release_branches = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    ),
    main_multibranch_builder_pair(
        name = "V8 Linux - verify csa",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    # this is only a builder
    main_multibranch_builder(
        name = "V8 Linux - vtunejit",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        gclient_vars = [GCLIENT_VARS.ITTAPI],
    ),
)

in_category(
    "Linux64",
    main_multibranch_builder(
        name = "V8 Linux64 - builder",
        release_branches = True,
        unfiltered_trigger = True,
        triggering_policy = parallel_3_greedy_batching_of_1,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"track_build_dependencies": True, "binary_size_tracking": {"category": "linux64", "binary": "d8"}},
        gclient_vars = [GCLIENT_VARS.GCMOLE],
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder(
        name = "V8 Linux64",
        release_branches = True,
        parent_builder = "V8 Linux64 - builder",
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder(
        name = "V8 Linux64 - debug builder",
        release_branches = True,
        unfiltered_trigger = True,
        triggering_policy = parallel_3_greedy_batching_of_1,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder(
        name = "V8 Linux64 - debug",
        release_branches = True,
        parent_builder = "V8 Linux64 - debug builder",
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder_pair(
        name = "V8 Linux64 - full debug",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        barrier = BARRIER.NONE,
        disable_resultdb_exports = True,
    ),
    main_multibranch_builder_pair(
        name = "V8 Linux64 css - debug",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        disable_resultdb_exports = True,
    ),
    main_multibranch_builder(
        name = "V8 Linux64 - custom snapshot - debug builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    ),
    main_multibranch_builder(
        name = "V8 Linux64 - custom snapshot - debug",
        parent_builder = "V8 Linux64 - custom snapshot - debug builder",
    ),
    main_multibranch_builder_pair(
        name = "V8 Linux64 - internal snapshot",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    ),
    main_multibranch_builder_pair(
        name = "V8 Linux64 - debug - header includes",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        gclient_vars = [GCLIENT_VARS.V8_HEADER_INCLUDES],
    ),
    main_multibranch_builder_pair(
        name = "V8 Linux64 - shared",
        release_branches = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    ),
    main_multibranch_builder_pair(
        name = "V8 Linux64 - verify csa",
        release_branches = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder_pair(
        name = "V8 Linux64 - no pointer compression",
        release_branches = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        disable_resultdb_exports = True,
    ),
    main_multibranch_builder(
        name = "V8 Linux64 - no wasm - builder",
        release_branches = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"track_build_dependencies": True, "binary_size_tracking": {"category": "linux64_no_wasm", "binary": "d8"}},
    ),
    main_multibranch_builder(
        name = "V8 Linux64 - bazel - builder",
        release_branches = True,
        dimensions = {"host_class": "strong", "os": "Ubuntu-24.04", "cpu": "x86-64"},
        executable = "recipe:v8/bazel",
    ),
    main_multibranch_builder_pair(
        name = "V8 Linux64 - debug - single generation",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    ),
    main_multibranch_builder(
        name = "V8 Linux64 - sticky mark bits - debug builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        barrier = BARRIER.TREE_CLOSER,
    ),
    main_multibranch_builder(
        name = "V8 Linux64 - verify builtins",
        release_branches = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"default_targets": ["verify_all_builtins_hashes"]},
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder(
        name = "V8 Linux64 - verify deterministic",
        release_branches = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"default_targets": ["verify_deterministic_mksnapshot"]},
        always_isolate_targets = ["snapshot_set"],
        first_branch_version = "13.0",
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder_pair(
        name = "V8 Linux64 - PKU",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        barrier = BARRIER.NONE,
    ),
    main_multibranch_builder_pair(
        name = "V8 Linux64 - PKU - debug",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        barrier = BARRIER.NONE,
    ),
    main_multibranch_builder(
        name = "V8 Linux64 - dumpling - builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        barrier = BARRIER.TREE_CLOSER,
    ),
)

in_category(
    "Fuchsia",
    main_multibranch_builder(
        name = "V8 Fuchsia - builder",
        release_branches = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"target_platform": "fuchsia"},
        barrier = BARRIER.LKGR_ONLY,
    ),
    main_multibranch_builder(
        name = "V8 Fuchsia - debug builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"target_platform": "fuchsia"},
        barrier = BARRIER.LKGR_ONLY,
    ),
)

in_category(
    "Windows",
    main_multibranch_builder(
        name = "V8 Win32 - builder",
        release_branches = True,
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        properties = {"binary_size_tracking": {"category": "win32", "binary": "d8.exe"}},
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder(
        name = "V8 Win32 - debug builder",
        release_branches = True,
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder(
        name = "V8 Win32",
        release_branches = True,
        parent_builder = "V8 Win32 - builder",
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder(
        name = "V8 Win32 - debug",
        release_branches = True,
        parent_builder = "V8 Win32 - debug builder",
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder_pair(
        name = "V8 Win64",
        release_branches = True,
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        properties = {"binary_size_tracking": {"category": "win64", "binary": "d8.exe"}},
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder_pair(
        name = "V8 Win64 - debug",
        release_branches = True,
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder(
        name = "V8 Win - arm64 - debug builder",
        release_branches = True,
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        barrier = BARRIER.LKGR_TREE_CLOSER,
        disable_resultdb_exports = True,
    ),
)

in_category(
    "Mac",
    main_multibranch_builder(
        name = "V8 Mac64 - builder",
        release_branches = True,
        triggered_by_gitiles = True,
        dimensions = {"os": "Mac", "cpu": "x86-64"},
        properties = {"binary_size_tracking": {"category": "mac64", "binary": "d8"}},
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder(
        name = "V8 Mac64",
        release_branches = True,
        parent_builder = "V8 Mac64 - builder",
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder(
        name = "V8 Mac64 - debug builder",
        release_branches = True,
        triggered_by_gitiles = True,
        dimensions = {"os": "Mac", "cpu": "x86-64"},
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder(
        name = "V8 Mac64 - debug",
        release_branches = True,
        parent_builder = "V8 Mac64 - debug builder",
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder_pair(
        name = "V8 Mac - arm64",
        release_branches = True,
        dimensions = {"os": "Mac", "cpu": "arm64"},
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder_pair(
        name = "V8 Mac - arm64 - debug",
        release_branches = True,
        dimensions = {"os": "Mac", "cpu": "arm64"},
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder_pair(
        name = "V8 Mac - arm64 - no pointer compression debug",
        release_branches = True,
        dimensions = {"os": "Mac", "cpu": "arm64"},
        disable_resultdb_exports = True,
    ),
)

in_category(
    "GCStress",
    main_multibranch_builder(
        name = "V8 Linux - gc stress",
        parent_builder = "V8 Linux - debug builder",
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder(
        name = "V8 Linux64 - gc stress",
        parent_builder = "V8 Linux64 - debug builder",
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder(
        name = "V8 Linux64 GC Stress - custom snapshot",
        parent_builder = "V8 Linux64 - custom snapshot - debug builder",
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    main_multibranch_builder(
        name = "V8 Mac - arm64 - gc stress",
        parent_builder = "V8 Mac - arm64 - debug builder",
        barrier = BARRIER.NONE,
    ),
)

in_category(
    "Misc",
    main_multibranch_builder(
        name = "V8 Presubmit",
        release_branches = True,
        unfiltered_trigger = True,
        triggering_policy = parallel_3_greedy_batching_of_1,
        executable = "recipe:v8/presubmit",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        disable_resultdb_exports = True,
    ),
    main_multibranch_builder(
        name = "V8 Test Tools",
        executable = "recipe:v8/test_tools",
        dimensions = {"host_class": "docker", "os": "Ubuntu-22.04", "cpu": "x86-64"},
        disable_resultdb_exports = True,
    ),
    main_multibranch_builder_pair(
        name = "V8 Linux64 - official",
        release_branches = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    ),
)
