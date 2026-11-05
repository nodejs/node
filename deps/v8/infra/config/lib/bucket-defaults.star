# Copyright 2024 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load(
    "//lib/service-accounts.star",
    "V8_TRY_ACCOUNT",
)

defaults_ci = {
    "executable": "recipe:v8",
    "swarming_tags": ["vpython:native-python-wrapper"],
    "dimensions": {
        "host_class": "default",
        "pool": "luci.v8.ci",
    },
    "service_account": "v8-ci-builder@chops-service-accounts.iam.gserviceaccount.com",
    "execution_timeout": 7200,
    "build_numbers": True,
    "resultdb_bq_table_prefix": "ci",
}

defaults_ci_hp = {
    "dimensions": {"os": "Linux", "pool": "luci.v8.highly-privileged"},
    "resultdb_bq_table_prefix": "ci_hp",
}

defaults_try = {
    "executable": "recipe:v8",
    "swarming_tags": ["vpython:native-python-wrapper"],
    "dimensions": {"host_class": "default", "pool": "luci.v8.try"},
    "service_account": V8_TRY_ACCOUNT,
    "execution_timeout": 1800,
    "properties": {"builder_group": "tryserver.v8"},
    "resultdb_bq_table_prefix": "try",
}

defaults_crossbench_try = {
    "executable": "recipe:v8",
    "swarming_tags": ["vpython:native-python-wrapper"],
    "dimensions": {"pool": "luci.v8.try"},
    "service_account": V8_TRY_ACCOUNT,
    "execution_timeout": 1800,
    "resultdb_bq_table_prefix": "try",
}

defaults_triggered = {
    "executable": "recipe:v8",
    "swarming_tags": ["vpython:native-python-wrapper"],
    "dimensions": {"host_class": "multibot", "pool": "luci.v8.try"},
    "service_account": V8_TRY_ACCOUNT,
    "execution_timeout": 7200,
    "properties": {"builder_group": "tryserver.v8"},
    "resultdb_bq_table_prefix": "try",
    "caches": [
        swarming.cache(
            path = "builder",
            name = "v8_builder_cache_nowait",
        ),
    ],
}

bucket_defaults = {
    "ci": defaults_ci,
    "ci-hp": defaults_ci_hp,
    "crossbench.try": defaults_crossbench_try,
    "try": defaults_try,
    "try.triggered": defaults_triggered,
}
