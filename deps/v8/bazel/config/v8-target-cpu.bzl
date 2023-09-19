# Copyright 2021 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build rules to choose the v8 target architecture."""

load("@bazel_skylib//lib:selects.bzl", "selects")

V8CpuTypeInfo = provider(
    doc = "A singleton provider that specifies the V8 target CPU type",
    fields = {
        "value": "The V8 Target CPU selected.",
    },
)

def _host_target_cpu_impl(ctx):
    allowed_values = ["arm", "arm64", "ia32", "ppc64le", "riscv64", "s390x", "x64", "none"]
    cpu_type = ctx.build_setting_value
    if cpu_type in allowed_values:
        return V8CpuTypeInfo(value = cpu_type)
    else:
        fail("Error setting " + str(ctx.label) + ": invalid v8 target cpu '" +
             cpu_type + "'. Allowed values are " + str(allowed_values))

v8_target_cpu = rule(
    implementation = _host_target_cpu_impl,
    build_setting = config.string(flag = True),
    doc = "CPU that V8 will generate code for.",
)

def v8_configure_target_cpu(name, matching_configs):
    selects.config_setting_group(
        name = "is_" + name,
        match_any = matching_configs,
    )

    # If v8_target_cpu flag is set to 'name'
    native.config_setting(
        name = "v8_host_target_is_" + name,
        flag_values = {
            ":v8_target_cpu": name,
        },
    )

    # Default target if no v8_host_target flag is set.
    selects.config_setting_group(
        name = "v8_target_is_" + name,
        match_all = [
            ":v8_host_target_is_none",
            ":is_" + name,
        ],
    )

    # Select either the default target or the flag.
    selects.config_setting_group(
        name = "v8_target_" + name,
        match_any = [
            ":v8_host_target_is_" + name,
            ":v8_target_is_" + name,
        ],
    )
