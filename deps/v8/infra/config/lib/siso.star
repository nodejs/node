# Copyright 2024 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def siso_properties(project, jobs):
    return {
        "$build/siso": {
            "configs": [
                "builder",
            ],
            "enable_cloud_profiler": True,
            "enable_cloud_trace": True,
            "experiments": [],
            "project": project,
            "remote_jobs": jobs,
        },
    }

SISO = struct(
    CHROMIUM_TRUSTED = siso_properties("rbe-chromium-trusted", 250),
    # For better performance on CQ, let siso calculate the jobs based
    # on CPUs.
    CHROMIUM_UNTRUSTED = siso_properties("rbe-chromium-untrusted", -1),
    CHROMIUM_UNTRUSTED_J500 = siso_properties("rbe-chromium-untrusted", 500),
    NONE = {},
)
