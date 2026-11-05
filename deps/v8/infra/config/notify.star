# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/lib.star", "FAILED_STEPS_EXCLUDE", "v8_notifier")

v8_notifier(
    name = "sheriffs",
    on_occurrence = ["FAILURE"],
    failed_step_regexp_exclude = FAILED_STEPS_EXCLUDE,
    notify_emails = [
        "v8-waterfall-sheriff@grotations.appspotmail.com",
        "mtv-sf-v8-sheriff@grotations.appspotmail.com",
        "v8-infra-alerts-cc@google.com",
    ],
)

v8_notifier(
    name = "sheriffs on new failure",
    on_new_status = ["FAILURE", "INFRA_FAILURE"],
    notify_emails = [
        "v8-waterfall-sheriff@grotations.appspotmail.com",
        "mtv-sf-v8-sheriff@grotations.appspotmail.com",
        "v8-infra-alerts-cc@google.com",
    ],
)

v8_notifier(
    name = "blamelist",
    on_new_status = ["FAILURE"],
    notify_blamelist = True,
)

v8_notifier(
    name = "v8-infra-cc",
    on_new_status = ["FAILURE"],
    notify_emails = ["v8-infra-alerts-cc@google.com"],
)

v8_notifier(
    name = "infra-failure",
    on_occurrence = ["INFRA_FAILURE"],
    notify_emails = [
        "v8-infra@google.com",
    ],
)

v8_notifier(
    name = "memory sheriffs",
    on_occurrence = ["FAILURE"],
    failed_step_regexp_exclude = FAILED_STEPS_EXCLUDE,
    notify_emails = [
        "v8-memory-sheriffs@google.com",
        "v8-infra-alerts-cc@google.com",
    ],
)

luci.notifier(
    name = "V8 Flake Sheriff",
    on_occurrence = ["FAILURE"],
    failed_step_regexp = [
        ".* \\(flakes\\)",
    ],
    notify_emails = [
        "almuthanna@chromium.org",
    ],
)

luci.notifier(
    name = "TSAN debug failures",
    on_occurrence = ["FAILURE"],
    notify_emails = [
        "omerkatz@chromium.org",
        "almuthanna@chromium.org",
    ],
)

luci.notifier(
    name = "NumFuzz maintainer",
    on_occurrence = ["FAILURE"],
    failed_step_regexp_exclude = [
        "bot_update",
        "isolate tests",
        "package build",
        "extract build",
        "cleanup_temp",
        "gsutil upload",
        "taskkill",
        "Failure reason",
        "steps",
        ".* \\(retry shards with patch\\)",
        ".* \\(with patch\\)",
        ".* \\(without patch\\)",
    ],
    notify_emails = [
        "almuthanna@chromium.org",
        "v8-infra-alerts-cc@google.com",
    ],
)

v8_notifier(
    name = "jsvu/esvu maintainer",
    on_occurrence = ["FAILURE"],
    failed_step_regexp_exclude = FAILED_STEPS_EXCLUDE,
    notify_emails = [
        "mathiasb@google.com",
        "v8-infra-alerts-cc@google.com",
    ],
)

luci.notify(tree_closing_enabled = True)

# V8 tree closer filtering on flakiness steps.
luci.tree_closer(
    name = "v8 tree closer",
    tree_status_host = "v8-status.appspot.com",
    failed_step_regexp_exclude = [
        ".* \\(flakes\\)",
    ],
)

# Generic tree closer only checking the overall build result.
luci.tree_closer(
    name = "generic tree closer",
    tree_status_host = "v8-status.appspot.com",
)

# Blink tree closer ignoring unreliable wpt steps.
luci.tree_closer(
    name = "blink tree closer",
    tree_status_host = "v8-status.appspot.com",
    failed_step_regexp_exclude = [
        ".*blink_wpt_tests.*",
    ],
)

v8_notifier(
    name = "api stability notifier",
    on_new_status = ["FAILURE"],
    notify_emails = [
        "machenbach@chromium.org",
        "hablich@chromium.org",
        "v8-infra-alerts-cc@google.com",
    ],
    notified_by = [
        "Linux V8 API Stability",
    ],
)

v8_notifier(
    name = "vtunejit notifier",
    on_occurrence = ["FAILURE"],
    failed_step_regexp = [
        "gclient runhooks",
        "compile",
    ],
    notify_emails = [
        "chunyang.dai@intel.com",
        "v8-infra-alerts-cc@google.com",
    ],
    notified_by = ["ci/V8 Linux - vtunejit"],
)

v8_notifier(
    name = "branch monitor",
    on_new_status = ["FAILURE"],
    notify_emails = [
        "v8-infra-alerts@google.com",
        "v8-waterfall-sheriff@grotations.appspotmail.com",
        "mtv-sf-v8-sheriff@grotations.appspotmail.com",
        "vahl@google.com",
    ],
    template = luci.notifier_template(
        name = "branch_monitor_email",
        body = """Branch monitor found revision lagging behind

The builder {{.Build.Builder.Builder}} has detected an active V8 branch where a
revision has remained unrolled into the corresponding Chromium branch for an
extended period.

Please <a href=\"https://bugs.chromium.org/p/chromium/issues/entry?summary=Branch%20monitor%20found%20revision%20lagging%20behind&description=See%20build%20https://ci.chromium.org/b/{{.Build.Id}}&components=Infra%3EClient%3EV8&priority=0\">
open a bug</a>.

<a href=\"https://ci.chromium.org/b/{{.Build.Id}}\">Build {{.Build.Number}}</a>
on {{.Build.EndTime | time}}
""",
    ),
)

v8_notifier(
    name = "test262 impex",
    on_new_status = ["FAILURE"],
    notify_emails = [
        "liviurau@google.com",
    ],
)

v8_notifier(
    name = "infra",
    on_new_status = ["SUCCESS", "FAILURE", "INFRA_FAILURE"],
    notify_emails = [
        "v8-infra-alerts-cc@google.com",
        "v8-infra-alerts@google.com",
        "liviurau@google.com",
    ],
)
