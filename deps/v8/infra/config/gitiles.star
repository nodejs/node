# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/lib.star", "branch_descriptors")

luci.gitiles_poller(
    name = "chromium-trigger",
    bucket = "ci",
    repo = "https://chromium.googlesource.com/chromium/src",
    refs = ["refs/heads/main"],
)

def branch_pollers():
    for branch in branch_descriptors:
        luci.gitiles_poller(
            name = branch.poller_name,
            bucket = branch.bucket,
            repo = "https://chromium.googlesource.com/v8/v8",
            refs = branch.refs,
        )

branch_pollers()

luci.gitiles_poller(
    name = "v8-trigger-official",
    bucket = "ci",
    repo = "https://chromium.googlesource.com/v8/v8",
    refs = [
        "refs/branch-heads/\\d+\\.\\d+",
        "refs/heads/\\d+\\.\\d+\\.\\d+",
    ],
)
