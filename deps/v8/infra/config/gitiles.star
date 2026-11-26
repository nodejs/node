# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "default_excluded_paths")
load("//lib/lib.star", "TRIGGER", "branch_descriptors")

def _path_filters(path_regexps_exclude):
    if not path_regexps_exclude:
        return {}

    # The scheduler doesn't allow an exclusion list on its own.
    return {"path_regexps": [".*"], "path_regexps_exclude": path_regexps_exclude}

def branch_pollers():
    for branch in branch_descriptors:
        luci.gitiles_poller(
            name = branch.poller_name,
            bucket = branch.bucket,
            repo = "https://chromium.googlesource.com/v8/v8",
            refs = branch.refs,
            **_path_filters(branch.path_regexps_exclude)
        )

branch_pollers()

luci.gitiles_poller(
    name = TRIGGER.FILTERED,
    bucket = "ci",
    repo = "https://chromium.googlesource.com/v8/v8",
    refs = ["refs/heads/main"],
    **_path_filters(default_excluded_paths())
)

luci.gitiles_poller(
    name = "v8-trigger-official",
    bucket = "ci",
    repo = "https://chromium.googlesource.com/v8/v8",
    refs = [
        "refs/branch-heads/\\d+\\.\\d+",
        "refs/heads/\\d+\\.\\d+\\.\\d+",
    ],
)
