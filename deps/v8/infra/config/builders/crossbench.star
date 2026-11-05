# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/service-accounts.star", "V8_TRY_ACCOUNT")
load("//lib/builders.star", "presubmit_builder")

def crossbench_cbb_builder(builder_name, recipe_path, os, cpu, caches = None, properties = None):
    """
    Adds a new crossbench-cbb builder

    Args:
      builder_name: Name of the builder
      recipe_path: Path to the LUCI recipe the builder should run.
      os: The os for the builder.

    """
    dims = {
        "pool": "luci.v8.try",
        "os": os,
        "cpu": cpu,
        "host_class": "default",
    }

    luci.builder(
        name = builder_name,
        bucket = "crossbench.try",
        dimensions = dims,
        executable = luci.recipe(
            name = recipe_path,
            cipd_package = "infra/recipe_bundles/chromium.googlesource.com/chromium/tools/build",
        ),
        service_account = V8_TRY_ACCOUNT,
        caches = caches or [],
        properties = properties,
        resultdb_settings = resultdb.settings(enable = True),
    )

    # Add the builder to the "crossbench" list in milo
    luci.list_view_entry(
        list_view = "crossbench",
        builder = builder_name,
    )

presubmit_builder("Presubmit", "crossbench.try", timeout = 900, console = "crossbench")
crossbench_cbb_builder("e2e Mac arm64 Try", "perf/crossbench", "Mac", "arm64")
crossbench_cbb_builder("e2e Linux x64 Try", "perf/crossbench", "Ubuntu-22.04", "x86-64")
crossbench_cbb_builder("e2e Windows x64 Try", "perf/crossbench", "Windows-10", "x86-64")
crossbench_cbb_builder(
    "e2e-Loadline Android x64 Try",
    "perf/crossbench_android",
    "Ubuntu-22.04",
    "x86-64",
    properties = {
        "test_driver": "crossbench/tests/end2end/android/loadline/runner.py",
        "test_run_config": [
            {
                "sdk_version": 37,
                "avd_suffix": "",
            },
        ],
    },
)
crossbench_cbb_builder(
    "e2e-Speedometer Android x64 Try",
    "perf/crossbench_android",
    "Ubuntu-22.04",
    "x86-64",
    properties = {
        "test_driver": "crossbench/tests/end2end/android/speedometer/runner.py",
        "test_run_config": [
            {
                "sdk_version": 37,
                "avd_suffix": "",
            },
        ],
    },
)
crossbench_cbb_builder(
    "e2e-WebPower Android x64 Try",
    "perf/crossbench_android",
    "Ubuntu-22.04",
    "x86-64",
    properties = {
        "test_driver": "crossbench/tests/end2end/android/web_power/runner.py",
        "test_run_config": [
            {
                "sdk_version": 37,
                "avd_suffix": "",
            },
        ],
    },
)
crossbench_cbb_builder(
    "e2e-Others Android x64 Try",
    "perf/crossbench_android",
    "Ubuntu-22.04",
    "x86-64",
    properties = {
        "test_driver": "crossbench/tests/end2end/android/others/runner.py",
        "test_run_config": [
            {
                "sdk_version": 37,
                "avd_suffix": "",
                "extra_flags": [
                    "-m",
                    "not legacy_android_sdk",
                ],
            },
            {
                "sdk_version": 32,
                "avd_suffix": "_foldable",
                "extra_flags": [
                    "-m",
                    "legacy_android_sdk",
                ],
            },
        ],
    },
)

luci.cq_group(
    name = "crossbench-main-cq",
    watch = cq.refset(
        repo = "https://chromium.googlesource.com/crossbench",
        refs = ["refs/heads/main"],
    ),
    acls = [
        acl.entry(
            [acl.CQ_COMMITTER],
            groups = [
                "project-chromium-submit-access",
            ],
        ),
    ],
    verifiers = [
        luci.cq_tryjob_verifier(
            builder = "Presubmit",
            disable_reuse = True,
        ),
        luci.cq_tryjob_verifier(
            builder = "e2e Mac arm64 Try",
        ),
        luci.cq_tryjob_verifier(
            builder = "e2e Linux x64 Try",
        ),
        luci.cq_tryjob_verifier(
            builder = "e2e Windows x64 Try",
        ),
        luci.cq_tryjob_verifier(
            builder = "e2e-Loadline Android x64 Try",
        ),
        luci.cq_tryjob_verifier(
            builder = "e2e-Speedometer Android x64 Try",
        ),
        luci.cq_tryjob_verifier(
            builder = "e2e-WebPower Android x64 Try",
        ),
        luci.cq_tryjob_verifier(
            builder = "e2e-Others Android x64 Try",
        ),
    ],
)
