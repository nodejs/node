# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/builders.star", "v8_builder")
load("//lib/lib.star", "ci_pair_factory", "in_console", "v8_failure_notifier")
load("//lib/gclient.star", "GCLIENT_VARS")
load("//lib/siso.star", "SISO")

def experiment_builder(**kwargs):
    notify_owners = kwargs.pop("notify_owners", None)
    if notify_owners:
        builder_name = kwargs["name"]
        v8_failure_notifier(
            name = "notification for %s" % builder_name,
            notify_emails = notify_owners,
            notified_by = [builder_name],
        )
    bucket = kwargs.pop("bucket", "ci")
    properties = kwargs.pop("properties", {})
    if "builder_group" not in properties:
        properties["builder_group"] = "client.v8"
    experiments = kwargs.pop("experiments", {})
    experiments["v8.resultdb"] = 100
    return v8_builder(
        bucket = bucket,
        properties = properties,
        experiments = experiments,
        disable_resultdb_exports = True,
        use_siso = SISO.CHROMIUM_TRUSTED,
        **kwargs
    )

in_category = in_console("experiments")
experiment_builder_pair = ci_pair_factory(experiment_builder)

in_category(
    "Features",
    experiment_builder_pair(
        name = "V8 Linux64 - cppgc-non-default - debug",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        notify_owners = ["mlippautz@chromium.org"],
        notifies = ["blamelist"],
    ),
    experiment_builder_pair(
        name = "V8 Linux64 - debug - perfetto",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        notify_owners = ["skyostil@google.com"],
        notifies = ["blamelist"],
    ),
    experiment_builder_pair(
        name = "V8 Linux64 - no metagen - debug",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        notify_owners = ["jgruber@chromium.org"],
        notifies = ["blamelist"],
    ),
    experiment_builder_pair(
        name = "V8 Linux64 - runtime call stats",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        notify_owners = ["cbruni@chromium.org"],
        notifies = ["blamelist"],
    ),
    experiment_builder(
        name = "V8 Linux64 - sticky mark bits - debug",
        parent_builder = "V8 Linux64 - sticky mark bits - debug builder",
        notify_owners = ["bikineev@chromium.org", "omerkatz@chromium.org"],
    ),
    experiment_builder_pair(
        name = "V8 Linux64 - undefined double - debug",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        notify_owners = ["nicohartmann@chromium.org"],
        notifies = ["blamelist"],
    ),
    experiment_builder_pair(
        name = "V8 Win64 - CET shadow stack",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        notify_owners = ["choongwoo.han@microsoft.com"],
    ),
)

in_category(
    "Coverage",
    experiment_builder(
        name = "V8 Linux64 - coverage",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        gclient_vars = [GCLIENT_VARS.COVERAGE],
        properties = {
            "enable_swarming": False,
            "coverage": "llvm",
        },
        notify_owners = ["machenbach@chromium.org"],
    ),
    experiment_builder(
        name = "V8 Linux64 - coverage - debug",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        gclient_vars = [GCLIENT_VARS.COVERAGE],
        properties = {
            "enable_swarming": False,
            "coverage": "llvm",
        },
        execution_timeout = 9000,
        notify_owners = ["machenbach@chromium.org"],
    ),
)

in_category(
    "FYI",
    experiment_builder_pair(
        name = "Linux64 - icu4x - debug",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    ),
    experiment_builder(
        name = "V8 Linux64 - debug - fyi",
        parent_builder = "V8 Linux64 - debug builder",
        execution_timeout = 19800,
        notify_owners = ["jgruber@chromium.org"],
        notifies = ["blamelist"],
    ),
    experiment_builder_pair(
        name = "V8 Linux64 - no shared cage - debug",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        execution_timeout = 19800,
        notify_owners = ["jgruber@chromium.org"],
        notifies = ["blamelist"],
    ),
    experiment_builder(
        name = "V8 Linux64 - fyi",
        parent_builder = "V8 Linux64 - builder",
        execution_timeout = 19800,
        notify_owners = ["jgruber@chromium.org"],
        notifies = ["blamelist"],
    ),
    experiment_builder_pair(
        name = "V8 Linux64 - predictable",
        bucket = "ci",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        properties = {"builder_group": "client.v8"},
        notifies = ["sheriffs on new failure", "blamelist"],
    ),
    experiment_builder_pair(
        name = "V8 Linux64 - lower limits",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        notifies = ["blamelist"],
    ),
    experiment_builder(
        name = "V8 Linux64 - dumpling",
        parent_builder = "V8 Linux64 - dumpling - builder",
    ),
    experiment_builder_pair(
        name = "V8 Linux - full debug",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
    ),
)

in_category(
    "Drumbrake",
    experiment_builder_pair(
        name = "V8 Win64 - drumbrake - debug",
        dimensions = {"os": "Windows-10", "cpu": "x86-64"},
        priority = 35,
        triggered_by = ["v8-trigger"],
        notify_owners = ["choongwoo.han@microsoft.com", "emromero@microsoft.com", "paolosev@microsoft.com"],
    ),
)

in_category(
    "Linux64 no sandbox",
    experiment_builder_pair(
        name = "V8 Linux64 - no sandbox",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        triggered_by = ["v8-trigger"],
    ),
    experiment_builder_pair(
        name = "V8 Linux64 - no sandbox - debug",
        dimensions = {"os": "Ubuntu-22.04", "cpu": "x86-64"},
        triggered_by = ["v8-trigger"],
    ),
)

in_category(
    "Mac",
    experiment_builder(
        name = "V8 iOS - sim - builder",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Mac", "cpu": "x86-64"},
        properties = {"$depot_tools/osx_sdk": {"sdk_version": "16c5032a"}, "target_platform": "ios"},
        caches = [
            swarming.cache(
                path = "osx_sdk",
                name = "osx_sdk",
            ),
        ],
        notifies = ["v8-infra-cc"],
    ),
    experiment_builder(
        name = "V8 Mac64 - full debug builder",
        triggered_by = ["v8-trigger"],
        dimensions = {"os": "Mac", "cpu": "x86-64"},
        notifies = ["sheriffs on new failure", "blamelist"],
    ),
)
