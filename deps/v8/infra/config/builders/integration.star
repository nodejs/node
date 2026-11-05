# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "v8_builder")
load("//lib/lib.star", "BARRIER", "in_console", "parallel_5_greedy_batching_of_1")
load("//lib/siso.star", "SISO")

def integration_builder(**kwargs):
    return v8_builder(
        disable_resultdb_exports = True,
        use_siso = SISO.CHROMIUM_TRUSTED,
        **kwargs
    )

in_category = in_console("integration")

in_category(
    "Layout",
    integration_builder(
        name = "V8 Blink Win",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        execution_timeout = 3600 * 4,
        properties = {"builder_group": "client.v8.fyi"},
        notifies = ["sheriffs"],
    ),
    integration_builder(
        name = "V8 Blink Mac",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Mac", "cpu": "x86-64"},
        execution_timeout = 3600 * 3,
        properties = {"builder_group": "client.v8.fyi"},
        notifies = ["sheriffs"],
    ),
    integration_builder(
        name = "V8 Blink Linux",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        triggering_policy = parallel_5_greedy_batching_of_1,
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.fyi"},
        notifies = ["sheriffs", "blink tree closer", "infra-failure"],
        barrier = BARRIER.LKGR_ONLY,
    ),
    integration_builder(
        name = "V8 Blink Linux Debug",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        triggering_policy = scheduler.policy(
            kind = scheduler.LOGARITHMIC_BATCHING_KIND,
            log_base = 2.0,
        ),
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.fyi"},
        notifies = ["sheriffs"],
    ),
    integration_builder(
        name = "V8 Blink Linux Future",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.fyi"},
        notifies = ["sheriffs"],
    ),
)

in_category(
    "Nonlayout",
    integration_builder(
        name = "Linux Debug Builder",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium",
        dimensions = {"host_class": "large_disk", "os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.fyi"},
        notifies = ["v8-infra-cc"],
    ),
    integration_builder(
        name = "Linux ASAN Builder",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium",
        dimensions = {"host_class": "large_disk", "os": "Ubuntu-22.04", "cpu": "x86-64"},
        execution_timeout = 3600 * 5,
        properties = {"builder_group": "client.v8.fyi"},
        notifies = ["v8-infra-cc"],
    ),
)

in_category(
    "GPU",
    integration_builder(
        name = "Win V8 FYI Release (NVIDIA)",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        execution_timeout = 3600 * 3,
        properties = {"builder_group": "client.v8.fyi"},
        notifies = ["v8-infra-cc"],
    ),
    integration_builder(
        name = "Mac V8 FYI Release (Apple M2)",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Mac", "cpu": "x86-64"},
        execution_timeout = 3600 * 3,
        properties = {"builder_group": "client.v8.fyi"},
        notifies = ["v8-infra-cc"],
    ),
    integration_builder(
        name = "Mac V8 FYI Release (Intel)",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Mac", "cpu": "x86-64"},
        execution_timeout = 3600 * 3,
        properties = {"builder_group": "client.v8.fyi"},
        notifies = ["v8-infra-cc"],
    ),
    integration_builder(
        name = "Linux V8 FYI Release (NVIDIA)",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        execution_timeout = 3600 * 3,
        properties = {"builder_group": "client.v8.fyi"},
        notifies = ["v8-infra-cc"],
        barrier = BARRIER.LKGR_ONLY,
    ),
    integration_builder(
        name = "Linux V8 FYI Release - pointer compression (NVIDIA)",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        execution_timeout = 3600 * 3,
        properties = {"builder_group": "client.v8.fyi"},
        notifies = ["v8-infra-cc"],
    ),
    integration_builder(
        name = "Android V8 FYI Release",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:chromium_integration",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        execution_timeout = 3600 * 3,
        properties = {"builder_group": "client.v8.fyi"},
        notifies = ["v8-infra-cc"],
    ),
)

in_category(
    "Node.js",
    integration_builder(
        name = "V8 Linux64 - node.js integration ng",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:v8/node_integration_ng",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"v8_tot": True, "builder_group": "client.v8.fyi"},
        notifies = ["sheriffs"],
    ),
)

in_category(
    "PGO",
    integration_builder(
        name = "V8 Builtins PGO",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        executable = "recipe:v8/pgo_builder",
        dimensions = {"host_class": "multibot", "os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"compilators": ["x64"]},
        barrier = BARRIER.LKGR_TREE_CLOSER,
    ),
)
