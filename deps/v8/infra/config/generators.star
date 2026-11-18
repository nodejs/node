# Copyright 2021 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//definitions.star", "ACTIVE_BRANCHES")

# Buckets with builders building the main branch.
MAIN_BRANCH_BUCKETS = ["ci", "ci.experimental"]

def add_branch_headers(ctx):
    """
    This callback adds branch-related links and console groups to the headers
    of all consoles in `luci-milo.cfg`.
    """
    milo = ctx.output["luci-milo.cfg"]

    # Define common links for main headers.
    main_consoles_links = [
        dict(text = "main", url = "/p/v8/g/main", alt = "Main V8 Console"),
        dict(text = "memory", url = "/p/v8/g/memory", alt = "Memory V8 Console"),
        dict(text = "ports", url = "/p/v8/g/ports", alt = "Ports V8 Console"),
        dict(text = "integration", url = "/p/v8/g/integration", alt = "V8 Integration Console"),
        dict(text = "experiments", url = "/p/v8/g/experiments", alt = "Experiments V8 Console"),
        dict(text = "clusterfuzz", url = "/p/v8/g/clusterfuzz", alt = "Clusterfuzz V8 Console"),
        dict(text = "chromium", url = "/p/v8/g/chromium", alt = "Chromium V8 Console"),
    ]
    controls_links = [
        dict(text = "tree status", url = "https://v8-status.appspot.com/"),
        dict(text = "roll status", url = "https://autoroll.skia.org/r/v8-chromium-autoroll"),
    ]
    lkgr_links = [
        dict(text = "status", url = "https://storage.cloud.google.com/chromium-v8/lkgr-status/v8-lkgr-status.html"),
    ]
    readme_links = [
        dict(text = "readme", url = "https://chromium.googlesource.com/v8/v8/+/refs/heads/main/infra/config/docs/branch-coverage.md", alt = "readme"),
    ]

    # Define branch links for the "Branches" group.
    branch_links = []
    for v in ACTIVE_BRANCHES:
        branch_links.append(dict(
            text = v,
            url = "/p/v8/g/" + v,
            alt = v,
        ))

    # Define detailed branch links for branch console headers.
    detailed_branch_groups = []
    for v in ACTIVE_BRANCHES:
        detailed_branch_groups.append(dict(
            name = v,
            links = [
                dict(text = v, url = "/p/v8/g/" + v, alt = v + " Main"),
                dict(text = v + ".memory", url = "/p/v8/g/" + v + ".memory", alt = v + " Memory"),
                dict(text = v + ".ports", url = "/p/v8/g/" + v + ".ports", alt = v + " Ports"),
            ],
        ))

    # Define console groups.
    main_console_ids = ["v8/memory", "v8/ports", "v8/clusterfuzz", "v8/integration", "v8/experiments"]
    branch_console_ids = []
    for v in ACTIVE_BRANCHES:
        branch_console_ids.extend([
            "v8/" + v,
            "v8/" + v + ".memory",
            "v8/" + v + ".ports",
        ])

    main_console_groups = [dict(console_ids = main_console_ids)]
    branch_console_groups = [dict(console_ids = branch_console_ids)]

    EXCLUDED_CONSOLES = ["tryserver", "pgo", "crossbench", "tools"]

    for console in milo.consoles:
        if console.id in EXCLUDED_CONSOLES or "-headless" in console.id:
            # These consoles don't have the dynamic branch headers.
            continue

        # Determine if this is a branch console or a main console.
        is_branch_console = False
        for v in ACTIVE_BRANCHES:
            if console.id == v or console.id.startswith(v + "."):
                is_branch_console = True
                break

        if is_branch_console:
            console.header = dict(
                links = detailed_branch_groups + [dict(name = "README", links = readme_links)],
                console_groups = branch_console_groups,
            )
        else:
            # Main consoles.
            console.header = dict(
                tree_status_host = "v8-status.appspot.com",
                links = [
                    dict(name = "Consoles", links = main_consoles_links),
                    dict(name = "Controls", links = controls_links),
                    dict(name = "LKGR", links = lkgr_links),
                    dict(name = "Branches", links = branch_links),
                    dict(name = "README", links = readme_links),
                ],
                console_groups = main_console_groups,
            )

def consoles_map(ctx):
    """
    Returns a mapping of console ID to console config.
    """
    consoles = dict()
    milo = ctx.output["luci-milo.cfg"]
    for console in milo.consoles:
        consoles[console.id] = console
    return consoles

def aggregate_builder_tester_console(ctx):
    """
    This callback collects and groups all builders by parent name in separate
    categories containing both the parent and the children to later add this
    categories to the `builder/tester` console. The purpose of the console is
    to make the parent/child relationship evident.

    Warning! The callback needs to run before the `parent_builder` property
    gets removed, before `ensure_forward_triggering_properties` callback.
    """
    build_bucket = ctx.output["cr-buildbucket.cfg"]
    categories = {}
    for bucket in build_bucket.buckets:
        if bucket.name in MAIN_BRANCH_BUCKETS:
            for builder in bucket.swarming.builders:
                if builder.properties:
                    properties = json.decode(builder.properties)
                    parent = properties.pop("parent_builder", None)
                    parent_bucket = properties.pop("parent_bucket", bucket.name)
                    if parent:
                        parent_id = (parent_bucket, parent)
                        contents = categories.get(parent, [])
                        if contents:
                            contents.append((bucket.name, builder.name))
                        else:
                            categories[parent] = [parent_id, (bucket.name, builder.name)]
    consoles = consoles_map(ctx)
    for category, contents in categories.items():
        for bucket_name, builder in contents:
            cbuilder = dict(
                name = "buildbucket/luci.v8.%s/%s" % (bucket_name, builder),
                category = category,
            )
            consoles["builder-tester"].builders.append(cbuilder)

def separate_builder_tester_console(ctx):
    """
    Two console duplicates, one with all artifact builders and one with all
    testers separated.
    """
    consoles = consoles_map(ctx)
    for name in consoles:
        builders_name = name + "-builders"
        testers_name = name + "-testers"
        if builders_name in consoles and testers_name in consoles:
            consoles[builders_name].builders = [
                builder
                for builder in consoles[name].builders
                if is_artifact_builder(builder)
            ]
            consoles[testers_name].builders = [
                tester
                for tester in consoles[name].builders
                if not is_artifact_builder(tester)
            ]

def headless_consoles(ctx):
    """
    Console duplicates without headers for being included in an iFrame-based
    dashboard.
    """
    consoles = consoles_map(ctx)
    for name in consoles:
        headless_name = name + "-headless"
        if headless_name in consoles:
            consoles[headless_name].builders = list(consoles[name].builders)

def is_artifact_builder(builder):
    return builder.name.endswith("builder")

def is_release_builder(builder):
    return "debug" not in builder.name

BUILDER_TESTER_SPLICER = struct(
    decision_func = is_artifact_builder,
    left_category = "builder",
    right_category = "tester",
)

RELEASE_DEBUG_SPLICER = struct(
    decision_func = is_release_builder,
    left_category = "rel",
    right_category = "dbg",
)

def builder_with_category(builder, add_sub_cat, splicer):
    category = builder.category
    if splicer.decision_func(builder):
        subcat = splicer.left_category
    else:
        subcat = splicer.right_category
    if add_sub_cat:
        category += ("|" + subcat)
    return dict(
        name = builder.name,
        category = category,
    )

def splice_by_categories(builders, splicer):
    # Key builder lists by categories.
    categories = dict()
    for builder in builders:
        categories.setdefault(builder.category, []).append(builder)

    # Analyze each category separately for subcategories.
    result = []
    for category, contents in categories.items():
        left_builders = [b for b in contents if splicer.decision_func(b)]
        right_builders = [b for b in contents if not splicer.decision_func(b)]

        # Create subcategories only with enough builders of each kind.
        add_sub_cat = len(left_builders) > 1 and len(right_builders) > 1
        for builder in left_builders + right_builders:
            result.append(builder_with_category(builder, add_sub_cat, splicer))
    return result

def mirror_console(original_console, dev_console):
    dev_console.builders = splice_by_categories(
        original_console.builders,
        BUILDER_TESTER_SPLICER,
    )
    dev_console.builders = splice_by_categories(
        dev_console.builders,
        RELEASE_DEBUG_SPLICER,
    )

def mirror_dev_consoles(ctx):
    """
    Mirror the three main consoles as dev consoles with additional
    subcategories.
    """
    consoles = consoles_map(ctx)
    mirror_console(consoles["main"], consoles["main-dev"])
    mirror_console(consoles["memory"], consoles["memory-dev"])
    mirror_console(consoles["ports"], consoles["ports-dev"])

def ensure_forward_triggering_properties(ctx):
    """
    This callback collects (and removes) `parent_builder` and `parent_bucket`
    properties from the builders, then reverse the relationship and set
    `triggers` property on the corresponding builders. Parent and child
    builders may live in different buckets.
    """
    build_bucket = ctx.output["cr-buildbucket.cfg"]
    triggers = dict()
    for bucket in build_bucket.buckets:
        for builder in bucket.swarming.builders:
            if builder.properties:
                properties = json.decode(builder.properties)
                parent = properties.pop("parent_builder", None)
                parent_bucket = properties.pop("parent_bucket", bucket.name)
                if parent:
                    parent_id = parent_bucket + "/" + parent
                    triggers.setdefault(parent_id, []).append(builder.name)
                builder.properties = json.encode(properties)
    for bucket in build_bucket.buckets:
        for builder in bucket.swarming.builders:
            tlist = triggers.get(bucket.name + "/" + builder.name, [])
            if tlist:
                properties = json.decode(builder.properties)
                properties["triggers"] = sorted(tlist)
                builder.properties = json.encode(properties)

def extract_property(builder, key, default):
    value = default
    if builder.properties:
        properties = json.decode(builder.properties)
        value = properties.pop(key, default)
        builder.properties = json.encode(properties)
    return value

def builder_suffix(builder):
    return builder.name.split("/")[-1]

def console_bucket(builder):
    # Console builder names have the form buildbucket/luci.<project>.<bucket>/<name>.
    return builder.name.split("/")[1][len("luci.v8."):]

def builder_suffixes(console):
    return [builder_suffix(builder) for builder in console.builders]

def hide_wip_builders(ctx):
    """
    Move builders marked as WIP to a separate console.
    """
    wip_console = consoles_map(ctx)["wip"]
    wip_builders = []
    build_bucket = ctx.output["cr-buildbucket.cfg"]
    for bucket in build_bucket.buckets:
        for builder in bucket.swarming.builders:
            wip = extract_property(builder, "__wip__", False)
            if wip:
                wip_builders.append(builder.name)

    milo = ctx.output["luci-milo.cfg"]
    for console in milo.consoles:
        if console == wip_console:
            continue
        filtered_builders = []
        for console_builder in console.builders:
            if builder_suffix(console_builder) in builder_suffixes(wip_console):
                continue
            if (builder_suffix(console_builder) in wip_builders and
                console_bucket(console_builder) in MAIN_BRANCH_BUCKETS):
                wip_console.builders.append(console_builder)
            else:
                filtered_builders.append(console_builder)
        console.builders = filtered_builders

def scheduled_builder_cleanup(ctx):
    """
    Remove repository information from builders that are scheduled. This is
    necessary for luci-notify to ignore notifications for builders with no
    revision information and cron scheduled builders will always have no
    revision information.
    """
    schedule = ctx.output["luci-scheduler.cfg"]
    scheduled_builders = [job.id for job in schedule.job if job.schedule]

    notify = ctx.output["luci-notify.cfg"]
    for notifier in notify.notifiers:
        for builder in notifier.builders:
            if builder.name in scheduled_builders:
                builder.repository = None

def collect_sherriffed_non_tree_closer_builders(ctx):
    """
    This callback checks that no sheriff emails are sent on non-tree closers.
    """
    notify = ctx.output["luci-notify.cfg"]

    tree_closers = [
        builder
        for notifier in notify.notifiers
        for builder in notifier.builders
        if notifier.tree_closers
    ]

    def sheriff_in_recipients(notifier):
        return any([
            "sheriff" in recipient
            for notification in notifier.notifications
            for recipient in notification.email.recipients
        ])

    notifies_sheriff = [
        builder
        for notifier in notify.notifiers
        for builder in notifier.builders
        if (not notifier.tree_closers) and sheriff_in_recipients(notifier)
    ]

    sherriffed_builders = sorted(
        [
            struct(name = builder.name, bucket = builder.bucket)
            for builder in notifies_sheriff
            if builder not in tree_closers
        ],
        key = lambda b: (b.name, b.bucket),
    )

    ctx.output["sheriffed-builders.cfg"] = json.indent(
        json.encode(struct(builders = sherriffed_builders)),
        indent = "  ",
    )

def build_lkgr_list(ctx):
    """
    This callback configures the V8 lkgr finder to include all builders that
    have the `__lkgr_contributor` property set.
    """
    build_bucket = ctx.output["cr-buildbucket.cfg"]

    ci_builders = [
        builder
        for bucket in build_bucket.buckets
        for builder in bucket.swarming.builders
        if builder.properties
    ]

    lkgr_contributors = dict()
    for builder in ci_builders:
        properties = json.decode(builder.properties)
        if "__lkgr_contributor" in properties:
            properties.pop("__lkgr_contributor")
            builder.properties = json.encode(properties)
            lkgr_contributors[builder.name] = None
    lkgr_contributors = lkgr_contributors.keys()

    lkgr_finder = [
        builder
        for bucket in build_bucket.buckets
        for builder in bucket.swarming.builders
        if bucket.name == "ci-hp" and builder.name == "V8 lkgr finder"
    ][0]

    lkgr_properties = json.decode(lkgr_finder.properties)
    lkgr_properties["config"]["buckets"]["v8/ci"]["builders"] = lkgr_contributors
    lkgr_finder.properties = json.encode(lkgr_properties)

lucicfg.generator(aggregate_builder_tester_console)

lucicfg.generator(separate_builder_tester_console)

lucicfg.generator(headless_consoles)

lucicfg.generator(mirror_dev_consoles)

lucicfg.generator(ensure_forward_triggering_properties)

lucicfg.generator(hide_wip_builders)

lucicfg.generator(scheduled_builder_cleanup)

lucicfg.generator(collect_sherriffed_non_tree_closer_builders)

lucicfg.generator(build_lkgr_list)

lucicfg.generator(add_branch_headers)
