# Copyright 2024 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load(
    "//lib/service-accounts.star",
    "V8_CI_ACCOUNT",
    "V8_HP_SERVICE_ACCOUNTS",
)

waterfall_acls = [
    acl.entry(
        roles = acl.BUILDBUCKET_TRIGGERER,
        users = V8_CI_ACCOUNT,
        groups = ["service-account-v8-bot"],
    ),
]

waterfall_hp_acls = [
    acl.entry(
        roles = acl.BUILDBUCKET_TRIGGERER,
        users = V8_HP_SERVICE_ACCOUNTS,
    ),
]

tryserver_acls = [
    acl.entry(
        roles = acl.BUILDBUCKET_TRIGGERER,
        groups = [
            "service-account-cq",
            "project-v8-tryjob-access",
            "service-account-v8-bot",
        ],
    ),
]
