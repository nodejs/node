"""Creates config_setting that allows selecting based on 'compiler' value."""

def create_compiler_config_setting(name, value):
    # The "do_not_use_tools_cpp_compiler_present" attribute exists to
    # distinguish between older versions of Bazel that do not support
    # "@bazel_tools//tools/cpp:compiler" flag_value, and newer ones that do.
    # In the future, the only way to select on the compiler will be through
    # flag_values{"@bazel_tools//tools/cpp:compiler"} and the else branch can
    # be removed.
    if hasattr(cc_common, "do_not_use_tools_cpp_compiler_present"):
        native.config_setting(
            name = name,
            flag_values = {
                "@bazel_tools//tools/cpp:compiler": value,
            },
        )
    else:
        native.config_setting(
            name = name,
            values = {"compiler": value},
        )
