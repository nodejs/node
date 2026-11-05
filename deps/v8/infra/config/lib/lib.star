# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//definitions.star", "ACTIVE_BRANCHES")

V8_ICON = "https://storage.googleapis.com/chrome-infra-public/logo/v8.ico"

def _to_int_tuple(version_str):
    return [int(n) for n in version_str.split(".")]

def branch_descriptor(
        bucket,
        poller_name,
        refs,
        version_tag = None,
        priority = None,
        has_console_name_prefix = True,
        version = None):
    if version_tag:
        version = _to_int_tuple(version_tag)
        version_str = "%s\\.%s" % (version[0], version[1])
        refs = [ref % version_str for ref in refs]
    return struct(
        bucket = bucket,
        version = version,
        poller_name = poller_name,
        create_consoles = branch_console_factory(bucket, version_tag, refs) if has_console_name_prefix else main_console_factory(),
        refs = refs,
        priority = priority,
        console_id_by_name = console_id_resolver(bucket, has_console_name_prefix),
    )

def main_console_factory():
    def factory():
        console_view("main", add_headless = True, add_builder_tester = True)
        console_view("memory", add_headless = True, add_builder_tester = True)
        console_view("ports", add_headless = True, add_builder_tester = True)
        console_view("main-dev")
        console_view("memory-dev")
        console_view("ports-dev")

    return factory

def branch_console_factory(bucket, version_tag, refs):
    def factory():
        base_name = bucket[3:]
        base_display_name = version_tag + " "
        header = "//consoles/header_branch.textpb"
        console_view(base_name, title = base_display_name + "main", refs = refs, header = header)
        console_view(base_name + ".memory", title = base_display_name + "memory", refs = refs, header = header)
        console_view(base_name + ".ports", title = base_display_name + "ports", refs = refs, header = header)

    return factory

def console_id_resolver(bucket, has_console_name_prefix):
    def resolver(console_kind):
        if has_console_name_prefix:
            suffix = ""
            if console_kind == "ports":
                suffix = ".ports"
            if console_kind == "memory":
                suffix = ".memory"
            return bucket[3:] + suffix
        else:
            return console_kind

    return resolver

branch_descriptors = [
    branch_descriptor(
        "ci",
        "v8-trigger",
        ["refs/heads/main"],
        priority = 30,  # default
        has_console_name_prefix = False,
    ),  # main
] + [
    branch_descriptor(
        "ci." + version,
        "v8-trigger-br-" + version,
        ["refs/branch-heads/%s"],
        version_tag = version,
        priority = 50,
    )
    for version in ACTIVE_BRANCHES
]

NAMING_CONVENTION_EXCLUDED_BUILDERS = [
    "V8 Linux64 - arm64 - sim - heap sandbox - debug",
    "V8 Linux64 - cppgc-non-default - debug",
    "V8 Linux64 - external code space - debug",
    "V8 Linux64 - heap sandbox - debug",
]

def cq_on_files(*regexp_list):
    filters = [cq.location_filter(path_regexp = p) for p in regexp_list]
    return {"location_filters": filters, "cancel_stale": False}

CQ = struct(
    BLOCK = {"cancel_stale": False},
    BLOCK_NO_REUSE = {"disable_reuse": "true", "cancel_stale": False},
    EXP_5_PERCENT = {"experiment_percentage": 5, "cancel_stale": False},
    EXP_20_PERCENT = {"experiment_percentage": 20, "cancel_stale": False},
    EXP_50_PERCENT = {"experiment_percentage": 50, "cancel_stale": False},
    EXP_100_PERCENT = {"experiment_percentage": 100, "cancel_stale": False},
    NONE = None,
    OPTIONAL = {"includable_only": "true", "cancel_stale": False},
    on_files = cq_on_files,
)

CQ_MODE = struct(
    MEGA_CQ_DRY_RUN_NAME = "CQ_MODE_MEGA_DRY_RUN",
    MEGA_CQ_FULL_RUN_NAME = "CQ_MODE_MEGA_FULL_RUN",
)

BARRIER = struct(
    LKGR_TREE_CLOSER = struct(
        closes_tree = True,
        properties = {"__lkgr_contributor": True},
    ),
    LKGR_ONLY = struct(
        closes_tree = False,
        properties = {"__lkgr_contributor": True},
    ),
    TREE_CLOSER = struct(
        closes_tree = True,
        properties = {},
    ),
    NONE = struct(
        closes_tree = False,
        properties = {},
    ),
)

multibot_caches = [
    swarming.cache(
        path = "builder",
        name = "v8_builder_cache_nowait",
    ),
]

def bq_exports(rdb_export_disabled, resultdb_bq_table_prefix):
    if rdb_export_disabled:
        return None

    return [
        resultdb.export_test_results(
            bq_table = "v8-resultdb.resultdb." + resultdb_bq_table_prefix + "_test_results",
        ),
        resultdb.export_text_artifacts(
            bq_table = "v8-resultdb.resultdb." + resultdb_bq_table_prefix + "_text_artifacts",
        ),
    ]

def add_barrier_properties(barrier, properties_holder):
    properties = properties_holder.get("properties", {})
    properties.update(barrier.properties)
    properties_holder["properties"] = properties

def resolve_parent_triggering(args, bucket_name, parent_builder):
    # By the time the generators are executed the parent_builder property
    # is no longer present on the builder struct, so we add it here in
    # the properties and it will get removed by a generator
    args.setdefault("properties", {})["parent_builder"] = parent_builder

    # Disambiguate the scheduler job names, because they are not
    # nested by bucket, while builders are.
    args.setdefault("triggered_by", []).append(
        bucket_name + "/" + parent_builder,
    )

def skip_builder(bucket_name, first_branch_version):
    # do we need to skip the builder in this bucket?
    if first_branch_version:
        branch_version = branch_by_name(bucket_name).version
        builder_first_version = _to_int_tuple(first_branch_version)
        return branch_version < builder_first_version
    return False

def fix_args(defaults, **kwargs):
    args = dict(kwargs)
    overridable_keys = [
        "executable",
        "swarming_tags",
        "service_account",
        "execution_timeout",
        "build_numbers",
        "priority",
        "caches",
    ]
    for key in overridable_keys:
        override_defaults(defaults, args, key)
    mergeable_keys = ["dimensions", "properties"]
    for key in mergeable_keys:
        merge_defaults(defaults, args, key)
    args["execution_timeout"] = args["execution_timeout"] * time.second
    if args.get("properties", {}).get("parent_builder", None):
        args["dimensions"]["host_class"] = "multibot"
        args["caches"] = multibot_caches
    if args.get("properties", {}).get("triggers", None):
        bucket_name = args["bucket"]
        args["triggers"] = [bucket_name + "/" + t for t in args["properties"]["triggers"]]
    return args

def override_defaults(defaults, args, key):
    args[key] = args.pop(key, None) or defaults.get(key, None)

def merge_defaults(defaults, args, key):
    args[key] = dict(clean_dict_items(defaults, key) + clean_dict_items(args, key))

def clean_dict_items(dictionary, key):
    return {k: v for k, v in dictionary.get(key, {}).items() if v != None}.items()

def in_branch_console(console_name, *builder_sets):
    def in_category(category_name, *builder_sets):
        for builder_set in builder_sets:
            for builder in builder_set:
                if not type(builder) == "list":
                    builder = [builder]
                for sub_builder in builder:
                    branch_name = sub_builder.split("/")[0]
                    luci.console_view_entry(
                        console_view = branch_by_name(branch_name).console_id_by_name(console_name),
                        builder = sub_builder,
                        category = category_name,
                    )

    return in_category

def branch_by_name(name):
    for branch in branch_descriptors:
        if branch.bucket == name:
            return branch

def in_console(console_id, *builders):
    def in_category(category_name, *builders):
        for builder in builders:
            if not type(builder) == "list":
                builder = [builder]
            for sub_builder in builder:
                luci.console_view_entry(
                    console_view = console_id,
                    builder = sub_builder,
                    category = category_name,
                )

    return in_category

def is_ci_debug(builder_name):
    return (
        builder_name.endswith(" debug") and
        builder_name not in NAMING_CONVENTION_EXCLUDED_BUILDERS
    )

def dictLevel1Copy(from_dict, to_dict):
    for k, v in from_dict.items():
        if type(v) == "dict":
            to_dict[k] = dict(v)
        elif type(v) == "list":
            to_dict[k] = list(v)
        else:
            to_dict[k] = v
    return to_dict

def ci_pair_factory(func):
    """
    Creates a CI pair function out of the function passed to it. The resulting function
    will create a builder-tester pair with similar properties. If properties specific to
    the tester are wanted we can add 'tester_' prefix to the property name.
    """

    def pair_func(**kwargs):
        tester_name = kwargs["name"]
        builder_name = tester_name + ("" if is_ci_debug(tester_name) else " -") + " builder"

        tester_kwargs = {}

        for k, v in kwargs.items():
            if k.startswith("tester_"):
                tester_kwargs[k[7:]] = kwargs.pop(k)

        builder_kwargs = dict(kwargs)
        builder_kwargs["name"] = builder_name

        tester_included_args = [
            "name",
            "bucket",
            "properties",
            "experiments",
            "barrier",
            "first_branch_version",
            "notifies",
            "notify_owners",
        ]

        tester_excluded_properties = [
            "binary_size_tracking",
        ]

        for k, v in kwargs.items():
            if k in tester_included_args and k not in tester_kwargs:
                tester_kwargs[k] = dict(v) if type(v) == "dict" else v

        tester_kwargs["parent_builder"] = builder_name
        if "properties" in tester_kwargs:
            for prop in tester_excluded_properties:
                tester_kwargs["properties"].pop(prop, None)

        description = kwargs.pop("description", None)
        if description:
            builder_kwargs["description"] = dict(description)
            builder_kwargs["description"]["triggers"] = tester_kwargs["name"]
            tester_kwargs["description"] = dict(description)
            tester_kwargs["description"]["triggered by"] = builder_kwargs["name"]

        # this is done in order to avoid the two functions referencing the same kwargs dict
        builder_kwargs_copy = dictLevel1Copy(builder_kwargs, {})
        tester_kwargs_copy = dictLevel1Copy(tester_kwargs, {})
        return [
            func(**builder_kwargs_copy),
            func(**tester_kwargs_copy),
        ]

    return pair_func

FAILED_STEPS_EXCLUDE = [
    "bot_update",
    "isolate tests",
    "package build",
    "extract build",
    "cleanup_temp",
    "gsutil upload",
    "taskkill",
    "Failure reason",
    "steps",
    ".* \\(flakes\\)",
    ".* \\(retry shards with patch\\)",
    ".* \\(with patch\\)",
    ".* \\(without patch\\)",
]

def v8_notifier(notify_emails = [], **kwargs):
    emails = list(notify_emails)
    infra_cc = "v8-infra-alerts-cc@google.com"
    if not infra_cc in emails:
        emails.append(infra_cc)
    luci.notifier(
        notify_emails = emails,
        **kwargs
    )

def v8_failure_notifier(**kwargs):
    v8_notifier(
        on_new_status = ["FAILURE", "INFRA_FAILURE"],
        **kwargs
    )

greedy_batching_of_1 = scheduler.policy(
    kind = scheduler.GREEDY_BATCHING_KIND,
    max_batch_size = 1,
)

parallel_3_greedy_batching_of_1 = scheduler.policy(
    kind = scheduler.GREEDY_BATCHING_KIND,
    max_concurrent_invocations = 3,
    max_batch_size = 1,
)

parallel_5_greedy_batching_of_1 = scheduler.policy(
    kind = scheduler.GREEDY_BATCHING_KIND,
    max_concurrent_invocations = 5,
    max_batch_size = 1,
)

def console_view(
        name,
        title = None,
        repo = None,
        refs = None,
        exclude_ref = None,
        header = None,
        add_headless = False,
        add_builder_tester = False):
    def add_console(name, **kwargs):
        luci.console_view(
            name = name,
            title = title or name,
            repo = repo or "https://chromium.googlesource.com/v8/v8",
            refs = refs or ["refs/heads/main"],
            exclude_ref = exclude_ref,
            **kwargs
        )

    add_console(
        name = name,
        favicon = V8_ICON,
    )

    if add_headless:
        add_console(name = name + "-headless")

    if add_builder_tester:
        add_console(
            name = name + "-builders",
            favicon = V8_ICON,
        )
        add_console(
            name = name + "-testers",
            favicon = V8_ICON,
        )
