#!/usr/bin/env -S bash -xc 'lucicfg format && lucicfg "$0"'
# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

lucicfg.check_version("1.33.7", "Please update depot_tools")

load("//definitions.star", "ACTIVE_BRANCHES")
load(
    "//lib/acls.star",
    "tryserver_acls",
    "waterfall_acls",
    "waterfall_hp_acls",
)
load(
    "//lib/service-accounts.star",
    "V8_CI_ACCOUNT",
    "V8_HP_SERVICE_ACCOUNTS",
    "V8_PGO_ACCOUNT",
    "V8_SERVICE_ACCOUNTS",
    "V8_TRY_ACCOUNT",
)

# Use LUCI Scheduler BBv2 names and add Scheduler realms configs.
lucicfg.enable_experiment("crbug.com/1182002")

lucicfg.config(
    config_dir = "generated",
    tracked_files = [
        "commit-queue.cfg",
        "cr-buildbucket.cfg",
        "luci-milo.cfg",
        "luci-logdog.cfg",
        "luci-scheduler.cfg",
        "luci-notify.cfg",
        "luci-notify/email-templates/*.template",
        "project.cfg",
        "realms.cfg",
        "sheriffed-builders.cfg",
    ],
    fail_on_warnings = True,
    lint_checks = ["none", "+formatting"],
)

# Keeps track of the non-tree-closer builders that still need to be sheriffed.
lucicfg.emit(
    dest = "sheriffed-builders.cfg",
    data = "",
)

def analysis_bindings():
    return [
        luci.binding(
            roles = "role/analysis.reader",
            groups = "all",
        ),
        luci.binding(
            roles = "role/analysis.queryUser",
            groups = "mdb/v8-infra",
        ),
        luci.binding(
            roles = "role/analysis.editor",
            groups = "mdb/v8-infra",
        ),
    ]

luci.project(
    name = "v8",
    buildbucket = "cr-buildbucket.appspot.com",
    logdog = "luci-logdog",
    milo = "luci-milo",
    notify = "luci-notify.appspot.com",
    scheduler = "luci-scheduler",
    swarming = "chromium-swarm.appspot.com",
    acls = [
        acl.entry(
            [
                acl.BUILDBUCKET_READER,
                acl.LOGDOG_READER,
                acl.PROJECT_CONFIGS_READER,
                acl.SCHEDULER_READER,
            ],
            groups = ["all"],
        ),
        acl.entry(
            [acl.SCHEDULER_OWNER],
            groups = [
                "project-v8-sheriffs",
            ],
        ),
        acl.entry(
            [acl.LOGDOG_WRITER],
            groups = ["luci-logdog-chromium-writers"],
        ),
        acl.entry(
            [acl.CQ_COMMITTER],
            groups = [
                "project-v8-submit-access",
            ],
        ),
        acl.entry(
            [acl.CQ_DRY_RUNNER],
            groups = [
                "project-v8-tryjob-access",
            ],
        ),
    ],
    bindings = [
        luci.binding(
            roles = "role/configs.validator",
            users = [V8_TRY_ACCOUNT],
        ),
        luci.binding(
            roles = "role/swarming.poolOwner",
            groups = "mdb/v8-infra",
        ),
        luci.binding(
            roles = "role/swarming.poolViewer",
            groups = "all",
        ),
        # Allow any V8 build to trigger a test ran under chromium's task
        # service accounts.
        luci.binding(
            roles = "role/swarming.taskServiceAccount",
            users = [
                "chromium-tester@chops-service-accounts.iam.gserviceaccount.com",
                "chrome-gpu-gold@chops-service-accounts.iam.gserviceaccount.com",
                "chrome-gold@chops-service-accounts.iam.gserviceaccount.com",
            ],
        ),
    ] + analysis_bindings(),
)

luci.realm(
    name = "@project",
    bindings = [
        # Allow everyone (including non-logged-in users) to see the tree status.
        luci.binding(
            roles = "role/treestatus.limitedReader",
            groups = [
                "all",
            ],
        ),
        # Only allow Googlers to see PII.
        luci.binding(
            roles = "role/treestatus.reader",
            groups = [
                "googlers",
            ],
            users = [
                "chromium-status-hr@appspot.gserviceaccount.com",
                "luci-notify@appspot.gserviceaccount.com",
            ],
        ),
        # Only allow Googlers and service accounts.
        luci.binding(
            roles = "role/treestatus.writer",
            groups = [
                "googlers",
            ],
            users = [
                "luci-notify@appspot.gserviceaccount.com",
            ],
        ),
    ],
)

## Swarming permissions

LED_GROUPS = ["project-v8-led-users", "mdb/v8-infra"]

# Configure pool and bucket access for individuals and other projects.
def pool(*, name, users = None, groups = None, projects = None, bucket_realms = None):
    luci.realm(
        name = name,
        bindings = [luci.binding(
            roles = "role/swarming.poolUser",
            groups = groups,
            users = users,
            projects = projects,
        )],
    )
    for br in (bucket_realms or []):
        luci.binding(
            realm = br,
            roles = [
                "role/buildbucket.triggerer",
                "role/swarming.taskTriggerer",
            ],
            groups = groups,
            users = users,
        )

# Allow this AoD group to use all pools and trigger all builders
pool(
    name = "@root",
    bucket_realms = ["@root"],
    users = [V8_CI_ACCOUNT, V8_PGO_ACCOUNT],
    groups = "google/v8-infra-users-highly-privileged@twosync.google.com",
)

pool(
    name = "pools/ci",
    bucket_realms = ["ci"] + ["ci." + v for v in ACTIVE_BRANCHES],
    users = [V8_CI_ACCOUNT, V8_PGO_ACCOUNT],
    groups = LED_GROUPS,
    projects = "emscripten-releases",
)

pool(
    name = "pools/try",
    bucket_realms = ["try", "try.triggered", "crossbench.try"],
    users = V8_SERVICE_ACCOUNTS,
    groups = LED_GROUPS + ["project-v8-tryjob-access"],
    projects = "emscripten-releases",
)

pool(
    name = "pools/highly-privileged",
    # Allow the devtools-frontend project to use V8's highly-privileged pool.
    projects = "devtools-frontend",
    users = V8_HP_SERVICE_ACCOUNTS,
)

pool(
    name = "pools/tests",
    users = V8_SERVICE_ACCOUNTS,
    # Allow ci/try runs and infra team to spawn test tasks in the testing pool.
    groups = "mdb/v8-infra",
)

def grantInvocationCreator(realms, users):
    for realm in realms:
        luci.realm(name = realm, bindings = [
            # Allow try builders to create invocations in their own builds.
            luci.binding(
                roles = "role/resultdb.invocationCreator",
                users = users,
            ),
        ])

grantInvocationCreator(["try", "try.triggered", "crossbench.try"], [V8_TRY_ACCOUNT])
grantInvocationCreator(["ci-hp"], [V8_PGO_ACCOUNT])
grantInvocationCreator(["ci"], [V8_CI_ACCOUNT])

luci.logdog(gs_bucket = "chromium-luci-logdog")

def led_config(service_accounts, pools = None, groups = None):
    return struct(
        service_accounts = service_accounts,
        pools = pools,
        groups = groups or LED_GROUPS,
    )

def bucket(name, acls, led_config = None):
    bindings, constraints = None, None
    if led_config:
        constraints = luci.bucket_constraints(
            service_accounts = led_config.service_accounts,
            pools = led_config.pools or ["luci.v8.%s" % name],
        )
        bindings = [
            luci.binding(
                roles = "role/buildbucket.creator",
                groups = led_config.groups,
            ),
        ]
    return luci.bucket(
        name = name,
        acls = acls,
        shadows = name,
        bindings = bindings,
        constraints = constraints,
    )

bucket(
    name = "ci",
    acls = waterfall_acls,
    led_config = led_config([V8_CI_ACCOUNT]),
)
bucket(
    name = "ci-hp",
    acls = waterfall_hp_acls,
    led_config = led_config(
        V8_HP_SERVICE_ACCOUNTS,
        ["luci.v8.highly-privileged"],
        ["google/v8-infra-users-highly-privileged@twosync.google.com"],
    ),
)
bucket(
    name = "try",
    acls = tryserver_acls,
    led_config = led_config([V8_TRY_ACCOUNT]),
)
bucket(name = "try.triggered", acls = tryserver_acls)
[bucket(name = "ci." + v, acls = waterfall_acls) for v in ACTIVE_BRANCHES]

bucket(name = "crossbench.try", acls = tryserver_acls, led_config = led_config([V8_TRY_ACCOUNT], groups = ["project-v8-tryjob-access"] + LED_GROUPS))

exec("//lib/recipes.star")

exec("//builders/all.star")

exec("//cq.star")
exec("//gitiles.star")
exec("//milo.star")
exec("//notify.star")
exec("//generators.star")
