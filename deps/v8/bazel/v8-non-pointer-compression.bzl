def _v8_disable_pointer_compression(settings, attr):
    return {
        "//:v8_enable_pointer_compression": "False",
    }

v8_disable_pointer_compression = transition(
    implementation = _v8_disable_pointer_compression,
    inputs = [],
    outputs = ["//:v8_enable_pointer_compression"],
)

# The implementation of transition_rule: all this does is copy the
# cc_binary's output to its own output and propagate its runfiles
# and executable to use for "$ bazel run".
#
# This makes transition_rule as close to a pure wrapper of cc_binary
# as possible.
def _v8_binary_non_pointer_compression(ctx):
    binary = ctx.attr.binary[0]
    outfile = ctx.actions.declare_file(ctx.label.name)
    cc_binary_outfile = binary[DefaultInfo].files.to_list()[0]

    ctx.actions.run_shell(
        inputs = [cc_binary_outfile],
        outputs = [outfile],
        command = "cp %s %s" % (cc_binary_outfile.path, outfile.path),
    )
    return [
        DefaultInfo(
            executable = outfile,
            data_runfiles = binary[DefaultInfo].data_runfiles,
        ),
    ]

# The purpose of this rule is to transition to a config where v8_target_cpu is
# set to the appropriate architecture, which will remain in place through exec
# transitions, so mksnapshot can for instance build on x64 but for arm64.
v8_binary_non_pointer_compression = rule(
    implementation = _v8_binary_non_pointer_compression,
    attrs = {
        # This is the cc_binary whose deps will select() on that feature.
        # Note specificaly how it's configured with v8_target_cpu_transition, which
        # ensures that setting propagates down the graph.
        "binary": attr.label(cfg = v8_disable_pointer_compression),
        # This is a stock Bazel requirement for any rule that uses Starlark
        # transitions. It's okay to copy the below verbatim for all such rules.
        #
        # The purpose of this requirement is to give the ability to restrict
        # which packages can invoke these rules, since Starlark transitions
        # make much larger graphs possible that can have memory and performance
        # consequences for your build. The whitelist defaults to "everything".
        # But you can redefine it more strictly if you feel that's prudent.
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
    # Making this executable means it works with "$ bazel run".
    executable = True,
)
