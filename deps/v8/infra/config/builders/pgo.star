# Copyright 2023 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "v8_builder")
load("//lib/service-accounts.star", "V8_PGO_ACCOUNT")
load("//lib/siso.star", "SISO")

v8_builder(
    name = "PGO Builder",
    bucket = "ci-hp",
    service_account = V8_PGO_ACCOUNT,
    executable = "recipe:v8/pgo_builder",
    schedule = "*/10 * * * *",
    in_list = "pgo",
    execution_timeout = 3600,
    notifies = ["infra-failure", "infra"],
)

def pgo_compilator(name, os):
    v8_builder(
        name = name,
        bucket = "ci",
        dimensions = {"os": os, "cpu": "x86-64"},
        executable = "recipe:v8/compilator",
        properties = {
            "default_targets": ["d8_pgo"],
            "builder_group": "client.v8",
        },
        use_siso = SISO.CHROMIUM_TRUSTED,
        in_list = "pgo",
    )

# GN variables depend on the builder name and are defined in
# https://chromium.googlesource.com/v8/v8/+/refs/heads/main/infra/mb/mb_config.pyl
pgo_compilator("V8 Linux PGO instrumentation - builder", os = "Ubuntu-22.04")
pgo_compilator("V8 Linux64 PGO instrumentation - builder", os = "Ubuntu-22.04")
pgo_compilator("V8 Win32 PGO instrumentation - builder", os = "Windows-10")
pgo_compilator("V8 Win64 PGO instrumentation - builder", os = "Windows-10")
