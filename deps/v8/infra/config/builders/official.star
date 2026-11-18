# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "v8_builder")
load("//lib/lib.star", "greedy_batching_of_1", "in_console")
load("//lib/siso.star", "SISO")

def official_builder(properties = None, **kwargs):
    properties = dict(properties or {})
    properties["builder_group"] = "client.v8.official"
    return v8_builder(
        bucket = "ci",
        triggered_by = ["v8-trigger-official"],
        triggering_policy = greedy_batching_of_1,
        executable = "recipe:v8/archive",
        properties = properties,
        use_siso = SISO.CHROMIUM_TRUSTED,
        # Official builds are not time-critical. Run them below all other
        # CI builders, even below the FYI priority of 35.
        priority = 50,
        **kwargs
    )

in_category = in_console("official")

in_category(
    "Linux",
    official_builder(
        name = "V8 Official Arm32",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"build_config": "Release", "target_bits": 32, "target_arch": "arm"},
    ),
    official_builder(
        name = "V8 Official Arm64",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"build_config": "Release", "target_bits": 64, "target_arch": "arm"},
    ),
    official_builder(
        name = "V8 Official Android Arm32",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"target_bits": 32, "build_config": "Release", "target_platform": "android", "target_arch": "arm"},
    ),
    official_builder(
        name = "V8 Official Android Arm64",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"target_bits": 64, "build_config": "Release", "target_platform": "android", "target_arch": "arm"},
    ),
    official_builder(
        name = "V8 Official Linux32",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"build_config": "Release", "target_bits": 32},
    ),
    official_builder(
        name = "V8 Official Linux32 Debug",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"build_config": "Debug", "target_bits": 32},
    ),
    official_builder(
        name = "V8 Official Linux64",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"build_config": "Release", "target_bits": 64},
    ),
    official_builder(
        name = "V8 Official Linux64 Debug",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"build_config": "Debug", "target_bits": 64},
    ),
)

in_category(
    "Windows",
    official_builder(
        name = "V8 Official Win32",
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        properties = {"build_config": "Release", "target_bits": 32},
    ),
    official_builder(
        name = "V8 Official Win32 Debug",
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        properties = {"build_config": "Debug", "target_bits": 32},
    ),
    official_builder(
        name = "V8 Official Win64",
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        properties = {"build_config": "Release", "target_bits": 64},
    ),
    official_builder(
        name = "V8 Official Win64 Debug",
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        properties = {"build_config": "Debug", "target_bits": 64},
    ),
)

in_category(
    "Mac",
    official_builder(
        name = "V8 Official Mac64",
        dimensions = {"os": "Mac", "cpu": "x86-64"},
        properties = {"build_config": "Release", "target_bits": 64},
    ),
    official_builder(
        name = "V8 Official Mac64 Debug",
        dimensions = {"os": "Mac", "cpu": "x86-64"},
        properties = {"build_config": "Debug", "target_bits": 64},
    ),
    official_builder(
        name = "V8 Official Mac ARM64",
        dimensions = {"os": "Mac", "cpu": "x86-64"},
        properties = {"build_config": "Release", "target_bits": 64, "target_arch": "arm"},
    ),
    official_builder(
        name = "V8 Official Mac ARM64 Debug",
        dimensions = {"os": "Mac", "cpu": "x86-64"},
        properties = {"build_config": "Debug", "target_bits": 64, "target_arch": "arm"},
    ),
)
