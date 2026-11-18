# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "v8_builder")
load("//lib/siso.star", "SISO")

def chromium_builder(name):
    v8_builder(
        name = name,
        bucket = "ci",
        triggered_by = ["chromium-trigger"],
        executable = "recipe:chromium",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8.chromium"},
        disable_resultdb_exports = True,
        use_siso = SISO.CHROMIUM_TRUSTED,
        execution_timeout = 3 * 60 * 60,
    )

chromium_builder("Linux V8 API Stability")
