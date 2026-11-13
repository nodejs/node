# Copyright 2021 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/bucket-defaults.star", "bucket_defaults")
load("//lib/builders.star", "v8_builder")
load(
    "//lib/service-accounts.star",
    "V8_AUTOROLL_ACCOUNT",
    "V8_CI_ACCOUNT",
    "V8_TEST262_EXPORT_ACCOUNT",
    "V8_TEST262_IMPORT_ACCOUNT",
)

v8_builder(
    bucket_defaults["ci"],
    name = "Infra Expriments",
    bucket = "ci",
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    service_account = V8_CI_ACCOUNT,
    executable = "recipe:v8/spike",
    schedule = "* * * * * 1970",
    in_list = "tools",
)

v8_builder(
    bucket_defaults["ci"],
    name = "Branch Monitor",
    bucket = "ci",
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    service_account = V8_CI_ACCOUNT,
    executable = "recipe:v8/branch_monitor",
    properties = {"max_gap_seconds": 43200},
    schedule = "49 * * * *",
    in_list = "tools",
    notifies = ["branch monitor", "infra"],
)

v8_builder(
    bucket_defaults["ci"],
    name = "Canary Detector",
    bucket = "ci-hp",
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    service_account = V8_AUTOROLL_ACCOUNT,
    executable = "recipe:v8/canary_detector",
    priority = 30,
    execution_timeout = 10800,
    expiration_timeout = 158400 * time.second,
    schedule = "10,25,40,55 * * * *",
    in_list = "tools",
    notifies = ["infra"],
)

v8_builder(
    name = "Test262 exporter",
    bucket = "ci-hp",
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    service_account = V8_TEST262_EXPORT_ACCOUNT,
    executable = "recipe:v8/test262_export",
    schedule = "18 * * * *",
    in_list = "tools",
    execution_timeout = 5400,
    notifies = ["test262 impex", "infra"],
)

v8_builder(
    name = "Test262 PR approver",
    bucket = "ci-hp",
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    service_account = V8_TEST262_EXPORT_ACCOUNT,
    executable = "recipe:v8/test262_export",
    schedule = "17 * * * *",
    in_list = "tools",
    execution_timeout = 3600,
    notifies = ["test262 impex", "infra"],
    properties = {"approver": True},
)

v8_builder(
    name = "Test262 importer",
    bucket = "ci-hp",
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    service_account = V8_TEST262_IMPORT_ACCOUNT,
    executable = "recipe:v8/test262_import",
    schedule = "0 2 * * 0",
    in_list = "tools",
    execution_timeout = 3600,
    notifies = ["test262 impex", "infra"],
)

v8_builder(
    name = "Test262 import watcher",
    bucket = "ci-hp",
    dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    service_account = V8_TEST262_IMPORT_ACCOUNT,
    executable = "recipe:v8/test262_watch",
    schedule = "*/30 * * * *",
    in_list = "tools",
    execution_timeout = 3600,
    notifies = ["test262 impex", "infra"],
)

v8_builder(
    name = "V8 lkgr finder",
    bucket = "ci-hp",
    executable = "recipe:lkgr_finder",
    build_numbers = True,
    service_account = V8_AUTOROLL_ACCOUNT,
    properties = {
        "config": {
            "allowed_lag": 7,
            "allowed_gap": 150,
            "source_url": "https://chromium.googlesource.com/v8/v8",
            "status_url": "https://v8-status.appspot.com",
            "monkeypatch_rev_map": {
                "7b9ea585802485b842a8bbbc941e53543963dde2": (24865, "refs/heads/master"),
                "4c0decf17e75806f956a253e786af6606659231e": (24866, "refs/heads/master"),
                "29e361f9dc64ff542502780fcd58b5f38fa711a6": (24867, "refs/heads/master"),
                "602eb9bdb529cc12f95b1bff0b2d690aba732d06": (24868, "refs/heads/master"),
                "f546c6e449ab6147cda1c3cf060e47ec03f7bcd1": (24869, "refs/heads/master"),
                "c5a9917aeaa17cef6051cac057cb237dbbab1b7e": (24870, "refs/heads/master"),
                "20992764bf73f3bbf70beee0dfb7fe4eafb5d055": (24871, "refs/heads/master"),
                "4897f7d89cc506ade9b2f99ea97d88d0f5108da1": (24872, "refs/heads/master"),
                "ec878cfe59572fdaf26051b40a2c4157197aa738": (24873, "refs/heads/master"),
                "1dd8281162f30d6dda51f702b5ecfe70e366c945": (24874, "refs/heads/master"),
                "ad7a50bd7b511064915bc2a09f53157b6bbc7c5b": (24875, "refs/heads/master"),
                "0c5049ea2fe5002a0032a83de35bd7c1a033b62b": (24876, "refs/heads/master"),
                "be4aa1490dfc912800a02bcf48102ace7bde2f65": (24877, "refs/heads/master"),
            },
            "buckets": {
                "v8/ci": {
                    # bucket alias: luci.v8.ci
                    "builders": [
                        # Maintained by build_lkgr_list generator
                    ],
                },
            },
        },
        "project": "v8",
        "repo": "https://chromium.googlesource.com/v8/v8",
        "ref": "refs/heads/lkgr",
        "lkgr_status_gs_path": "chromium-v8/lkgr-status",
        "src_ref": "refs/heads/main",
    },
    schedule = "2/6 * * * *",
    execution_timeout = 3600,
    disable_resultdb_exports = True,
    in_list = "tools",
    notifies = ["infra"],
)

v8_builder(
    name = "Release Branch Updater",
    bucket = "ci-hp",
    executable = "recipe:v8/release_branch_updater",
    build_numbers = True,
    service_account = V8_AUTOROLL_ACCOUNT,
    properties = {
        "channels": [
            {"refname": "beta", "source": "MILESTONES", "channel": "beta", "max_age_weeks": 2},
            {"refname": "stable", "source": "MILESTONES", "channel": "stable", "max_age_weeks": 5},
            {"refname": "extended", "source": "RELEASES", "channel": "Extended", "platform": "Mac", "max_age_weeks": 9},
        ],
    },
    schedule = "0 * * * *",
    execution_timeout = 1800,
    in_list = "tools",
    notifies = ["infra"],
)
