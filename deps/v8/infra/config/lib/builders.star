# Copyright 2024 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

load("//lib/bucket-defaults.star", "bucket_defaults")
load("//lib/description.star", "to_html")
load("//lib/gclient.star", "gclient_vars_properties")
load("//lib/lib.star", "BARRIER", "CQ", "add_barrier_properties", "bq_exports", "branch_descriptors", "fix_args", "resolve_parent_triggering", "skip_builder")
load("//lib/siso.star", "SISO")

# Paths that no CQ builder compiles or tests. The presubmit builders still
# check them.
NO_BUILD_PATHS = [
    "agents/.+",
    "docs/.+",
    "experimental/[^/]+",
    "experimental/(d8|dataflow|merge_concurrency_model_checker|regmole)/.+",
    "tools/(bigsleep|dev|jj|profiling|release|turbolizer)/.+",
    "tools/debug_helper/plugins/.+",
    # Sources of the gcmole plugin. Bots use the prebuilt plugin pinned in
    # tools/gcmole/gcmole-tools.tar.gz.sha1.
    "tools/gcmole/(Makefile|README|bootstrap\\.sh|gcmole\\.cc|package\\.sh)",
]

# Redline is compiled as part of gn_all on Linux, but not tested. Only a single
# compile-only builder verifies changes touching only Redline.
REDLINE_PATHS = ["experimental/redline/.+"]

def _exclude(path_regexps):
    return [
        cq.location_filter(path_regexp = p, exclude = True)
        for p in path_regexps
    ]

def _infra_paths():
    return ["{}/.+".format(lucicfg.main_package().repo_path)]

def default_excluded_paths():
    """Paths skipped by default CQ builders and by the filtered CI poller."""
    return _infra_paths() + NO_BUILD_PATHS + REDLINE_PATHS

def infra_location_filters():
    return _exclude(_infra_paths())

def default_location_filters():
    return _exclude(default_excluded_paths())

def with_cancel(cq_properties):
    if not cq_properties:
        return cq_properties
    result = dict(cq_properties)
    result["cancel_stale"] = True
    return result

def v8_basic_builder(defaults, **kwargs):
    add_default_filters = kwargs.pop("add_default_filters", True)
    for cq_group, prop_name in [
        ("v8-cq", "cq_properties"),
        ("v8-branch-cq", "cq_branch_properties"),
        ("v8-chromium-branch-cq", "cq_chromium_branch_properties"),
    ]:
        cq_props = kwargs.pop(prop_name, None)
        if cq_props != None:
            if add_default_filters and not cq_props.get("includable_only"):
                cq_props = dict(cq_props)
                cq_props["location_filters"] = (
                    cq_props.get("location_filters", []) + default_location_filters()
                )
            luci.cq_tryjob_verifier(
                kwargs["name"],
                cq_group = cq_group,
                **cq_props
            )
    properties = dict(kwargs.pop("properties", {}))

    # TODO(https://crbug.com/1372352): Temporary name property for investigation.
    # Should be replaced by the description below at some point.
    properties["__builder_name__"] = kwargs["name"]

    properties.update(kwargs.pop("use_siso", SISO.NONE))
    properties.update(gclient_vars_properties(kwargs.pop("gclient_vars", [])))

    always_isolate_targets = kwargs.pop("always_isolate_targets", [])
    if always_isolate_targets:
        v8_properties = dict(properties.pop("$build/v8", {}))
        v8_properties["always_isolate_targets"] = always_isolate_targets
        properties["$build/v8"] = v8_properties

    # Fake property to move WIP builders to a special console by a generator
    # in the end.
    properties["__wip__"] = kwargs.pop("work_in_progress", False)

    kwargs["properties"] = properties
    kwargs = fix_args(defaults, **kwargs)

    description = kwargs.pop("description", None)
    if description:
        kwargs["description_html"] = to_html(
            kwargs["name"],
            kwargs["bucket"],
            description,
        )

    rdb_export_disabled = kwargs.pop("disable_resultdb_exports", False)
    resultdb_bq_table_prefix = defaults.get("resultdb_bq_table_prefix")
    kwargs["resultdb_settings"] = resultdb.settings(
        enable = True,
        bq_exports = bq_exports(rdb_export_disabled, resultdb_bq_table_prefix),
    )

    luci.builder(**kwargs)

def v8_builder(defaults = None, **kwargs):
    bucket_name = kwargs["bucket"]
    in_console = kwargs.pop("in_console", None)
    in_list = kwargs.pop("in_list", None)
    defaults = defaults or bucket_defaults[bucket_name]
    barrier = kwargs.pop("barrier", BARRIER.NONE)
    parent_builder = kwargs.pop("parent_builder", None)

    # The parent builder defaults to the same bucket, but can be overridden
    # (e.g. for ci.experimental testers triggered by ci builders).
    parent_bucket = kwargs.pop("parent_bucket", bucket_name)
    if barrier.closes_tree:
        notifies = kwargs.pop("notifies", [])
        if parent_builder:
            notifies.append("v8 tree closer")
        elif kwargs.get("executable") != "recipe:v8":
            notifies.append("generic tree closer")
        notifies.append("infra-failure")
        kwargs["notifies"] = notifies
    if parent_builder:
        resolve_parent_triggering(kwargs, parent_bucket, parent_builder)
    kwargs["repo"] = "https://chromium.googlesource.com/v8/v8"
    add_barrier_properties(barrier, kwargs)
    v8_basic_builder(defaults, **kwargs)
    if in_console:
        splited = in_console.split("/")
        console_name = splited[0]
        category_name = None
        if len(splited) > 1:
            category_name = splited[1]
        luci.console_view_entry(
            console_view = console_name,
            builder = "%s/%s" % (bucket_name, kwargs["name"]),
            category = category_name,
        )
    if in_list:
        luci.list_view_entry(
            list_view = in_list,
            builder = kwargs["name"],
        )
    return kwargs["name"]

def multibranch_builder(release_branches = False, unfiltered_trigger = False, **kwargs):
    """Defines a builder on main and optionally twins on release branches.

    Args:
      release_branches: Opt-in switch for also creating twin builders in the
        release-branch buckets (and consoles). Off by default.
      unfiltered_trigger: On main, trigger on every commit instead of skipping
        commits that only touch paths exempted from default CQ builders.
        Release branches only have a single poller, so this has no effect
        there.
      **kwargs: Arguments passed through to the builder definition. The
        optional first_branch_version restricts the release-branch twins to
        branches of at least that version and requires release_branches.

    Returns:
      A list of "<bucket>/<name>" strings for all created builders.
    """
    added_builders = []
    barrier = kwargs.pop("barrier", BARRIER.TREE_CLOSER)
    first_branch_version = kwargs.pop("first_branch_version", None)
    if first_branch_version and not release_branches:
        fail("Builder '%s' sets first_branch_version without release_branches = True" % kwargs["name"])
    for branch in branch_descriptors:
        args = dict(kwargs)
        parent_builder = args.pop("parent_builder", None)
        if parent_builder:
            args["triggered_by_gitiles"] = False
            resolve_parent_triggering(args, branch.bucket, parent_builder)
        triggered_by_gitiles = args.pop("triggered_by_gitiles", True)
        if triggered_by_gitiles:
            poller = branch.poller_name if unfiltered_trigger else branch.filtered_poller_name
            args["triggered_by"] = args.get("triggered_by", []) + [poller]
            args["use_siso"] = args.get("use_siso", SISO.CHROMIUM_TRUSTED)
        args["priority"] = branch.priority

        if branch.bucket == "ci":
            if barrier.closes_tree:
                # Note: This intentionally alters a passed-in notifies list
                # in place. As args is only a shallow copy of kwargs, the
                # tree closer added here for main is also inherited by the
                # release-branch twins created in later iterations. This
                # way failures on release branches close the tree as well,
                # which is the only signal we get from those builders.
                # Builders that don't pass notifies get a fresh list here,
                # hence their release-branch twins don't close the tree.
                notifies = args.pop("notifies", [])
                if parent_builder:
                    notifies.append("v8 tree closer")
                else:
                    notifies.append("generic tree closer")
                args["notifies"] = notifies
        else:
            args["disable_resultdb_exports"] = True
            if not release_branches:
                continue
            if skip_builder(branch.bucket, first_branch_version):
                continue

            # Greedy triggering policies (e.g. building every revision with
            # several concurrent invocations) are only meant for the main
            # branch, which has a much higher commit load. Branch twins use
            # the scheduler's default batching policy to save resources.
            args.pop("triggering_policy", None)
        add_barrier_properties(barrier, args)
        properties = args.get("properties", {})
        properties.update(barrier.properties)
        args["properties"] = properties
        v8_basic_builder(bucket_defaults["ci"], bucket = branch.bucket, **args)
        added_builders.append(branch.bucket + "/" + kwargs["name"])
    return added_builders

def main_multibranch_builder(**kwargs):
    props = kwargs.pop("properties", {})
    props["builder_group"] = "client.v8"
    kwargs["properties"] = props
    if "notifies" in kwargs["properties"]:
        kwargs["properties"]["notifies"].append("V8 Flake Sheriff")
    else:
        kwargs["properties"]["notifies"] = ["V8 Flake Sheriff"]
    return multibranch_builder(**kwargs)

def try_builder(
        name,
        bucket = "try",
        cq_properties = CQ.NONE,
        cq_branch_properties = CQ.NONE,
        add_default_filters = True,
        disable_resultdb_exports = True,
        console = None,
        **kwargs):
    # All unspecified branch trybots are per default optional.
    if (cq_properties != CQ.NONE and cq_branch_properties == CQ.NONE):
        cq_branch_properties = CQ.OPTIONAL
    use_siso = kwargs.pop("use_siso", None)
    if not use_siso:
        use_siso = SISO.CHROMIUM_UNTRUSTED
    v8_builder(
        name = name,
        bucket = bucket,
        cq_properties = with_cancel(cq_properties),
        cq_branch_properties = with_cancel(cq_branch_properties),
        add_default_filters = add_default_filters,
        in_list = console or "tryserver",
        disable_resultdb_exports = disable_resultdb_exports,
        use_siso = use_siso,
        **kwargs
    )

def presubmit_builder(
        name,
        bucket,
        cq_properties = CQ.NONE,
        cq_branch_properties = CQ.NONE,
        cq_chromium_branch_properties = CQ.NONE,
        timeout = 8 * 60,
        console = None):
    try_builder(
        name = name,
        bucket = bucket,
        cq_properties = cq_properties,
        cq_branch_properties = cq_branch_properties,
        cq_chromium_branch_properties = cq_chromium_branch_properties,
        add_default_filters = False,
        executable = "recipe:run_presubmit",
        dimensions = {
            "os": "Ubuntu-22.04",
            "cpu": "x86-64",
            "host_class": "default",
        },
        execution_timeout = timeout + 2 * 60,
        properties = {"runhooks": True, "timeout": timeout},
        priority = 25,
        console = console,
    )
