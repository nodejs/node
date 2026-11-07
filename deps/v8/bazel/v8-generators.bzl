# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Private helpers for V8 generators."""

visibility("private")

# Use a single generator target for torque definitions and initializers. We can
# split the set of outputs by using OutputGroupInfo, that way we do not need to
# run the torque generator twice.
def _torque_files_impl(ctx):
    # Allow building V8 as a dependency: workspace_root points to external/v8
    # when building V8 from a different repository and empty otherwise.
    v8root = ctx.label.workspace_root
    if v8root == "":
        v8root = "."

    # Arguments
    args = []
    args += ctx.attr.args
    args.append("-o")
    args.append(ctx.bin_dir.path + "/" + v8root + "/" + ctx.attr.prefix + "/torque-generated")
    args.append("-strip-v8-root")
    args.append("-v8-root")
    args.append(v8root)
    inputs = ctx.files.srcs
    if ctx.file.layout_json:
        args.append("-use-cpp-layouts")
        args.append("-layout-json")
        args.append(ctx.file.layout_json.path)
        inputs = inputs + [ctx.file.layout_json]

    # Sources
    args += [f.path for f in ctx.files.srcs]

    # Generate/declare output files
    defs = []
    inits = []
    for src in ctx.files.srcs:
        root, _period, _ext = src.path.rpartition(".")

        # Strip v8root
        if root[:len(v8root)] == v8root:
            root = root[len(v8root):]
        file = ctx.attr.prefix + "/torque-generated/" + root
        defs.append(ctx.actions.declare_file(file + "-tq.cc"))
        inits.append(ctx.actions.declare_file(file + "-tq-csa.cc"))
        inits.append(ctx.actions.declare_file(file + "-tq-csa.h"))

    defs += [ctx.actions.declare_file(ctx.attr.prefix + "/torque-generated/" + f) for f in ctx.attr.definition_extras]
    inits += [ctx.actions.declare_file(ctx.attr.prefix + "/torque-generated/" + f) for f in ctx.attr.initializer_extras]
    outs = defs + inits
    ctx.actions.run(
        outputs = outs,
        inputs = inputs,
        arguments = args,
        executable = ctx.attr.tool[V8ToolInfo].files_to_run,
        mnemonic = "GenTorqueFiles",
        progress_message = "Generating Torque files",
        toolchain = None,
    )
    return [
        DefaultInfo(files = depset(outs)),
        OutputGroupInfo(
            initializers = depset(inits),
            definitions = depset(defs),
        ),
    ]

# Generator tools must be built for the machine that runs them while using
# the V8 settings that determine their output. These helpers apply those
# settings after Bazel chooses the execution platform, then pass the original
# executable and runfiles to the action without copying the binary.
# The tool and action use the same default execution group so the executable
# is built for the platform that runs the action.
# TODO: Remove these helpers once V8 requires a Bazel version that preserves
# scoped build settings when composing exec and V8 transitions.
V8ToolInfo = provider(
    doc = "The configured executable used by a V8 generator.",
    fields = {
        "files_to_run": "The original binary's FilesToRunProvider, including its runfiles.",
    },
)

def v8_tool_impl(ctx):
    [binary] = ctx.attr.binary
    return [V8ToolInfo(files_to_run = binary[DefaultInfo].files_to_run)]

def _v8_torque_pointer_compression_transition_impl(_settings, attr):
    return {
        "@v8//:v8_enable_pointer_compression": str(attr.pointer_compression),
    }

_v8_torque_pointer_compression_transition = transition(
    implementation = _v8_torque_pointer_compression_transition_impl,
    inputs = [],
    outputs = ["@v8//:v8_enable_pointer_compression"],
)

_v8_torque_tool = rule(
    implementation = v8_tool_impl,
    attrs = {
        "binary": attr.label(
            mandatory = True,
            executable = True,
            cfg = _v8_torque_pointer_compression_transition,
        ),
        "pointer_compression": attr.bool(mandatory = True),
    },
)

_v8_torque_files = rule(
    implementation = _torque_files_impl,
    attrs = {
        "prefix": attr.string(mandatory = True),
        "srcs": attr.label_list(allow_files = True, mandatory = True),
        "definition_extras": attr.string_list(),
        "initializer_extras": attr.string_list(),
        "tool": attr.label(
            mandatory = True,
            providers = [V8ToolInfo],
            cfg = "exec",
        ),
        "args": attr.string_list(),
        "layout_json": attr.label(
            allow_single_file = True,
        ),
    },
)

def v8_torque_files(
        name,
        noicu_srcs,
        icu_srcs,
        args,
        definition_extras,
        initializer_extras,
        noicu_layout_json,
        icu_layout_json):
    # Torque's pointer sizes must match the V8 build that consumes its output.
    # Select pointer compression before Bazel configures Torque for execution.
    for prefix, srcs, layout_json in [
        ("noicu", noicu_srcs, noicu_layout_json),
        ("icu", icu_srcs, icu_layout_json),
    ]:
        tool_name = prefix + "/" + name + "_tool_"
        tools = {}
        for layout, pointer_compression in [("compressed", True), ("uncompressed", False)]:
            _v8_torque_tool(
                name = tool_name + layout,
                binary = ":" + prefix + "/torque",
                pointer_compression = pointer_compression,
                tags = ["manual"],
                visibility = ["//visibility:private"],
            )
            tools[layout] = native.package_relative_label(":" + tool_name + layout)
        _v8_torque_files(
            name = prefix + "/" + name,
            prefix = prefix,
            srcs = srcs,
            args = args,
            definition_extras = definition_extras,
            initializer_extras = initializer_extras,
            layout_json = layout_json,
            tool = select({
                Label("//:is_v8_enable_pointer_compression"): tools["compressed"],
                "//conditions:default": tools["uncompressed"],
            }),
        )
