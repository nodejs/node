# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "multibranch_builder")
load("//lib/gclient.star", "GCLIENT_VARS")
load("//lib/lib.star", "BARRIER", "ci_pair_factory", "greedy_batching_of_1", "in_branch_console")

def port_builder(*args, **kwargs):
    experiments = kwargs.pop("experiments", {})
    experiments["v8.resultdb"] = 100
    kwargs["experiments"] = experiments
    return multibranch_builder(*args, **kwargs)

in_category = in_branch_console("ports")
multibranch_builder_pair = ci_pair_factory(port_builder)

in_category(
    "Arm",
    multibranch_builder(
        name = "V8 Arm - builder",
        triggering_policy = greedy_batching_of_1,
        triggered_by_gitiles = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.ports", "target_arch": "arm", "binary_size_tracking": {"category": "linux_arm32", "binary": "d8"}},
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    multibranch_builder(
        name = "V8 Arm - debug builder",
        triggered_by_gitiles = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"target_arch": "arm", "builder_group": "client.v8.ports"},
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    multibranch_builder(
        name = "V8 Android Arm - builder",
        triggering_policy = greedy_batching_of_1,
        triggered_by_gitiles = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.ports", "target_arch": "arm", "target_platform": "android", "binary_size_tracking": {"category": "android_arm32", "binary": "d8"}},
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    multibranch_builder(
        name = "V8 Android Arm - verify deterministic - debug",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.ports", "target_arch": "arm", "target_platform": "android", "default_targets": ["verify_deterministic_mksnapshot"]},
        always_isolate_targets = ["snapshot_set"],
        first_branch_version = "13.0",
        barrier = BARRIER.NONE,
    ),
    multibranch_builder_pair(
        name = "V8 Linux - arm - sim",
        triggered_by_gitiles = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.ports"},
        gclient_vars = [GCLIENT_VARS.GCMOLE],
        tester_notifies = ["V8 Flake Sheriff"],
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    multibranch_builder_pair(
        name = "V8 Linux - arm - sim - debug",
        triggered_by_gitiles = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.ports"},
        tester_notifies = ["V8 Flake Sheriff"],
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    multibranch_builder_pair(
        name = "V8 Linux - arm - sim - lite",
        triggered_by_gitiles = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.ports"},
        tester_notifies = ["V8 Flake Sheriff"],
        barrier = BARRIER.TREE_CLOSER,
        tester_barrier = BARRIER.NONE,
    ),
    multibranch_builder_pair(
        name = "V8 Linux - arm - sim - lite - debug",
        triggered_by_gitiles = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.ports"},
        tester_notifies = ["V8 Flake Sheriff"],
        barrier = BARRIER.TREE_CLOSER,
        tester_barrier = BARRIER.NONE,
    ),
)

in_category(
    "Arm64",
    multibranch_builder(
        name = "V8 Arm64 - builder",
        triggering_policy = greedy_batching_of_1,
        triggered_by_gitiles = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"target_arch": "arm", "target_bits": 64, "builder_group": "client.v8.ports"},
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    multibranch_builder(
        name = "V8 Android Arm64 - builder",
        triggering_policy = greedy_batching_of_1,
        triggered_by_gitiles = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.ports", "target_arch": "arm", "target_platform": "android", "binary_size_tracking": {"category": "android_arm64", "binary": "d8"}},
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    multibranch_builder(
        name = "V8 Android Arm64 - debug builder",
        triggered_by_gitiles = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"target_platform": "android", "target_arch": "arm", "builder_group": "client.v8.ports"},
    ),
    multibranch_builder(
        name = "V8 Android Arm64 - P7",
        parent_builder = "V8 Android Arm64 - builder",
        properties = {"builder_group": "client.v8.ports"},
        barrier = BARRIER.NONE,
        notifies = ["V8 Flake Sheriff"],
        disable_resultdb_exports = True,
    ),
    multibranch_builder_pair(
        name = "V8 Linux64 - arm64",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8", "target_arch": "arm", "target_bits": 64},
        barrier = BARRIER.TREE_CLOSER,
        first_branch_version = "14.9",
    ),
    multibranch_builder_pair(
        name = "V8 Linux64 - arm64 - debug",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8", "target_arch": "arm", "target_bits": 64},
        barrier = BARRIER.TREE_CLOSER,
        first_branch_version = "14.9",
    ),
    multibranch_builder(
        name = "V8 Linux64 - arm64 - no wasm - debug builder",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {
            "builder_group": "client.v8.ports",
            "target_arch": "arm",
            "target_bits": 64,
        },
    ),
    multibranch_builder(
        name = "V8 Linux64 - arm64 - no pointer compression - builder",
        triggered_by_gitiles = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {
            "builder_group": "client.v8.ports",
            "target_arch": "arm",
            "target_bits": 64,
        },
        first_branch_version = "13.3",
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    multibranch_builder_pair(
        name = "V8 Linux - arm64 - sim",
        triggered_by_gitiles = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.ports"},
        gclient_vars = [GCLIENT_VARS.GCMOLE],
        tester_notifies = ["V8 Flake Sheriff"],
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    multibranch_builder_pair(
        name = "V8 Linux - arm64 - sim - debug",
        triggered_by_gitiles = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.ports"},
        tester_notifies = ["V8 Flake Sheriff"],
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
    multibranch_builder_pair(
        name = "V8 Linux - arm64 - sim - gc stress",
        triggered_by_gitiles = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        execution_timeout = 23400,
        properties = {"builder_group": "client.v8.ports"},
        tester_notifies = ["V8 Flake Sheriff"],
    ),
    multibranch_builder_pair(
        name = "V8 Linux64 - arm64 - sim - no pointer compression",
        triggered_by_gitiles = True,
        tester_execution_timeout = 19800,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8"},
        tester_notifies = ["V8 Flake Sheriff"],
        disable_resultdb_exports = True,
    ),
)

in_category(
    "Mips",
    multibranch_builder_pair(
        name = "V8 Linux - mips64el - sim",
        triggered_by_gitiles = True,
        tester_execution_timeout = 19800,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.ports"},
        barrier = BARRIER.NONE,
        disable_resultdb_exports = True,
    ),
)

in_category(
    "IBM",
    multibranch_builder_pair(
        name = "V8 Linux - ppc64 - sim",
        triggered_by_gitiles = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        execution_timeout = 19800,
        properties = {"builder_group": "client.v8.ports"},
        barrier = BARRIER.NONE,
        disable_resultdb_exports = True,
    ),
    multibranch_builder_pair(
        name = "V8 Linux - s390x - sim",
        triggered_by_gitiles = True,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        execution_timeout = 19800,
        properties = {"builder_group": "client.v8.ports"},
        barrier = BARRIER.NONE,
        disable_resultdb_exports = True,
    ),
)

in_category(
    "RISC-V",
    multibranch_builder_pair(
        name = "V8 Linux - riscv64 - sim",
        tester_execution_timeout = 19800,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.ports"},
        barrier = BARRIER.NONE,
        disable_resultdb_exports = True,
    ),
    multibranch_builder_pair(
        name = "V8 Linux - riscv64 - sim - debug",
        tester_execution_timeout = 19800,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.ports"},
        barrier = BARRIER.NONE,
        disable_resultdb_exports = True,
        first_branch_version = "14.2",
    ),
    multibranch_builder_pair(
        name = "V8 Linux - riscv64 - sim - pointer compression",
        tester_execution_timeout = 19800,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.ports"},
        barrier = BARRIER.NONE,
        disable_resultdb_exports = True,
    ),
)

in_category(
    "Loongson",
    multibranch_builder_pair(
        name = "V8 Linux - loong64 - sim",
        triggered_by_gitiles = True,
        tester_execution_timeout = 19800,
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.ports"},
        barrier = BARRIER.NONE,
        disable_resultdb_exports = True,
    ),
)
