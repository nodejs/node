# Copyright 2021 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This module contains helper functions to compile V8.
"""

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:find_cc_toolchain.bzl", "find_cc_toolchain", "use_cc_toolchain")
load("@rules_cc//cc/common:cc_common.bzl", "cc_common")
load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")

FlagInfo = provider("The value of an option.",
fields = ["value"])

def _options_impl(ctx):
    return FlagInfo(value = ctx.build_setting_value)

_create_option_flag = rule(
    implementation = _options_impl,
    build_setting = config.bool(flag = True),
    attrs = {
        "scope": attr.string(),
    },
)

_create_option_string = rule(
    implementation = _options_impl,
    build_setting = config.string(flag = True),
    attrs = {
        "scope": attr.string(),
    },
)

_create_option_int = rule(
    implementation = _options_impl,
    build_setting = config.int(flag = True),
    attrs = {
        "scope": attr.string(),
    },
)

def v8_flag(name, default = False):
    _create_option_flag(name = name, build_setting_default = default, scope = "universal")
    native.config_setting(name = "is_" + name, flag_values = {name: "True"})
    native.config_setting(name = "is_not_" + name, flag_values = {name: "False"})

def v8_string(name, default = ""):
    _create_option_string(name = name, build_setting_default = default, scope = "universal")

def v8_int(name, default = 0):
    _create_option_int(name = name, build_setting_default = default, scope = "universal")

def _custom_config_impl(ctx):
    defs = []
    defs.append("V8_TYPED_ARRAY_MAX_SIZE_IN_HEAP=" +
                str(ctx.attr._v8_typed_array_max_size_in_heap[FlagInfo].value))
    context = cc_common.create_compilation_context(defines = depset(defs))
    return [CcInfo(compilation_context = context)]

v8_custom_config = rule(
    implementation = _custom_config_impl,
    attrs = {
        "_v8_typed_array_max_size_in_heap": attr.label(default = ":v8_typed_array_max_size_in_heap"),
    },
)

def _config_impl(ctx):
    hdrs = []

    # Add headers
    for h in ctx.attr.hdrs:
        hdrs += h[DefaultInfo].files.to_list()
    defs = []

    # Add conditional_defines
    for f, d in ctx.attr.conditional_defines.items():
        if f[FlagInfo].value:
            defs.append(d)

    # Add defines
    for d in ctx.attr.defines:
        defs.append(d)
    context = cc_common.create_compilation_context(
        defines = depset(
            defs,
            transitive = [dep[CcInfo].compilation_context.defines for dep in ctx.attr.deps],
        ),
        headers = depset(
            hdrs,
            transitive = [dep[CcInfo].compilation_context.headers for dep in ctx.attr.deps],
        ),
    )
    return [CcInfo(compilation_context = context)]

v8_config = rule(
    implementation = _config_impl,
    attrs = {
        "conditional_defines": attr.label_keyed_string_dict(),
        "defines": attr.string_list(),
        "deps": attr.label_list(),
        "hdrs": attr.label_list(allow_files = True),
    },
)

def _default_args():
    return struct(
        deps = [":define_flags", "@libcxx//:libc++"],
        defines = select({
            "@v8//bazel/config:is_windows": [
                "UNICODE",
                "_UNICODE",
                "_CRT_RAND_S",
                "_WIN32_WINNT=0x0A00",  # Override bazel default to Windows 10
            ],
            "//conditions:default": [],
        }),
        copts = select({
            "@v8//bazel/config:is_posix": [
                "-fPIC",
                "-fno-strict-aliasing",
                "-fconstexpr-steps=2000000",
                "-Werror",
                "-Wextra",
                "-Wno-unneeded-internal-declaration",
                "-Wno-unknown-warning-option", # b/330781959
                "-Wno-cast-function-type-mismatch",  # b/330781959
                "-Wno-bitwise-instead-of-logical",
                "-Wno-builtin-assume-aligned-alignment",
                "-Wno-unused-parameter",
                "-Wno-implicit-int-float-conversion",
                "-Wno-deprecated-copy",
                "-Wno-non-virtual-dtor",
                "-isystem .",
            ],
            "//conditions:default": [],
        }) + select({
            "@v8//bazel/config:is_clang": [
                "-Wno-invalid-offsetof",
                "-Wno-deprecated-this-capture",
                "-Wno-deprecated-declarations",
                "-Wno-deprecated-attributes",
                "-std=c++20",
            ],
            "@v8//bazel/config:is_gcc": [
                "-Wno-extra",
                "-Wno-array-bounds",
                "-Wno-class-memaccess",
                "-Wno-comments",
                "-Wno-deprecated-declarations",
                "-Wno-implicit-fallthrough",
                "-Wno-int-in-bool-context",
                "-Wno-maybe-uninitialized",
                "-Wno-mismatched-new-delete",
                "-Wno-redundant-move",
                "-Wno-return-type",
                "-Wno-stringop-overflow",
                "-Wno-deprecated-this-capture",
                # Use GNU dialect, because GCC doesn't allow using
                # ##__VA_ARGS__ when in standards-conforming mode.
                "-std=gnu++2a",
            ],
            "@v8//bazel/config:is_windows": [
                "/std:c++20",
            ],
            "//conditions:default": [],
        }) + select({
            "@v8//bazel/config:is_gcc_fastbuild": [
                # Non-debug builds without optimizations fail because
                # of recursive inlining of "always_inline" functions.
                "-O1",
            ],
            "//conditions:default": [],
        }) + select({
            "@v8//bazel/config:is_clang_s390x": [
                "-fno-integrated-as",
            ],
            "//conditions:default": [],
        }) +  select({
            "@v8//bazel/config:is_opt_android": [
                "-fvisibility=hidden",
                "-fvisibility-inlines-hidden",
            ],
            "//conditions:default": [
            ],
        }),
        includes = ["include"],
        linkopts = select({
            "@v8//bazel/config:is_windows": [
                "Winmm.lib",
                "DbgHelp.lib",
                "Advapi32.lib",
            ],
            "@v8//bazel/config:is_macos": ["-pthread"],
            "//conditions:default": ["-Wl,--no-as-needed -ldl -latomic -pthread"],
        }) + select({
            ":should_add_rdynamic": ["-rdynamic"],
            "//conditions:default": [],
        }),
    )

ENABLE_I18N_SUPPORT_DEFINES = [
    "-DV8_INTL_SUPPORT",
    "-DICU_UTIL_DATA_IMPL=ICU_UTIL_DATA_STATIC",
    # src/regexp/regexp-compiler-tonode.cc uses an unsafe ICU method and
    # access a character implicitly.
    "-DUNISTR_FROM_CHAR_EXPLICIT=",
]

def _should_emit_noicu_and_icu(noicu_srcs, noicu_deps, noicu_defines, icu_srcs, icu_deps, icu_defines):
     return noicu_srcs != [] or noicu_deps != [] or noicu_defines != [] or icu_srcs != [] or icu_deps != [] or icu_defines != []

# buildifier: disable=function-docstring
def v8_binary(
        name,
        srcs,
        deps = [],
        defines = [],
        includes = [],
        copts = [],
        linkopts = [],
        noicu_srcs = [],
        noicu_deps = [],
        noicu_defines = [],
        icu_srcs = [],
        icu_deps = [],
        icu_defines = [],
        **kwargs):
    default = _default_args()
    if _should_emit_noicu_and_icu(noicu_srcs, noicu_deps, noicu_defines, icu_srcs, icu_deps, icu_defines):
        cc_binary(
            name = "noicu/" + name,
            srcs = srcs + noicu_srcs,
            deps = deps + noicu_deps + default.deps,
            defines = defines + noicu_defines + default.defines,
            includes = includes + ["noicu/"] + default.includes,
            copts = copts + default.copts,
            linkopts = linkopts + default.linkopts,
            **kwargs
        )
        cc_binary(
            name = "icu/" + name,
            srcs = srcs + icu_srcs,
            deps = deps + icu_deps + default.deps,
            includes = includes + ["icu/"] + default.includes,
            defines = defines + icu_defines + default.defines,
            copts = copts + default.copts + ENABLE_I18N_SUPPORT_DEFINES,
            linkopts = linkopts + default.linkopts,
            **kwargs
        )
    else:
        cc_binary(
            name = name,
            srcs = srcs,
            deps = deps + default.deps,
            defines = defines + default.defines,
            includes = includes + default.includes,
            copts = copts + default.copts,
            linkopts = linkopts + default.linkopts,
            **kwargs
        )

# buildifier: disable=function-docstring
def v8_library(
        name,
        srcs,
        deps = [],
        includes = [],
        copts = [],
        linkopts = [],
        noicu_srcs = [],
        noicu_deps = [],
        noicu_defines = [],
        icu_srcs = [],
        icu_deps = [],
        icu_defines = [],
        **kwargs):
    default = _default_args()
    if _should_emit_noicu_and_icu(noicu_srcs, noicu_deps, noicu_defines, icu_srcs, icu_deps, icu_defines):
        cc_library(
            name = name + "_noicu",
            srcs = srcs + noicu_srcs,
            deps = deps + noicu_deps + default.deps,
            includes = includes + ["noicu/"] + default.includes,
            copts = copts + default.copts,
            linkopts = linkopts + default.linkopts,
            alwayslink = 1,
            linkstatic = 1,
            **kwargs
        )

        # Alias target used because of cc_library bug in bazel on windows
        # https://github.com/bazelbuild/bazel/issues/14237
        # TODO(victorgomes): Remove alias once bug is fixed
        native.alias(
            name = "noicu/" + name,
            actual = name + "_noicu",
        )
        cc_library(
            name = name + "_icu",
            srcs = srcs + icu_srcs,
            deps = deps + icu_deps + default.deps,
            includes = includes + ["icu/"] + default.includes,
            copts = copts + default.copts + ENABLE_I18N_SUPPORT_DEFINES,
            linkopts = linkopts + default.linkopts,
            alwayslink = 1,
            linkstatic = 1,
            **kwargs
        )

        # Alias target used because of cc_library bug in bazel on windows
        # https://github.com/bazelbuild/bazel/issues/14237
        # TODO(victorgomes): Remove alias once bug is fixed
        native.alias(
            name = "icu/" + name,
            actual = name + "_icu",
        )
    else:
        cc_library(
            name = name,
            srcs = srcs,
            deps = deps + default.deps,
            includes = includes + default.includes,
            copts = copts + default.copts,
            linkopts = linkopts + default.linkopts,
            alwayslink = 1,
            linkstatic = 1,
            **kwargs
        )

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
        inputs = ctx.files.srcs,
        arguments = args,
        executable = ctx.executable.tool,
        mnemonic = "GenTorqueFiles",
        progress_message = "Generating Torque files",
    )
    return [
        DefaultInfo(files = depset(outs)),
        OutputGroupInfo(
            initializers = depset(inits),
            definitions = depset(defs),
        ),
    ]

_v8_torque_files = rule(
    implementation = _torque_files_impl,
    # cfg = v8_target_cpu_transition,
    attrs = {
        "prefix": attr.string(mandatory = True),
        "srcs": attr.label_list(allow_files = True, mandatory = True),
        "definition_extras": attr.string_list(),
        "initializer_extras": attr.string_list(),
        "tool": attr.label(
            allow_files = True,
            executable = True,
            cfg = "exec",
        ),
        "args": attr.string_list(),
    },
)

def v8_torque_files(name, noicu_srcs, icu_srcs, args, definition_extras, initializer_extras):
    _v8_torque_files(
        name = "noicu/" + name,
        prefix = "noicu",
        srcs = noicu_srcs,
        args = args,
        definition_extras = definition_extras,
        initializer_extras = initializer_extras,
        tool = select({
            "@v8//bazel/config:v8_target_is_32_bits": ":noicu/torque_non_pointer_compression",
            "//conditions:default": ":noicu/torque",
        }),
    )
    _v8_torque_files(
        name = "icu/" + name,
        prefix = "icu",
        srcs = icu_srcs,
        args = args,
        definition_extras = definition_extras,
        initializer_extras = initializer_extras,
        tool = select({
            "@v8//bazel/config:v8_target_is_32_bits": ":icu/torque_non_pointer_compression",
            "//conditions:default": ":icu/torque",
        }),
    )

def _v8_target_cpu_transition_impl(settings,
                                   attr, # @unused
                                  ):
    # Check for an existing v8_target_cpu flag.
    if "@v8//bazel/config:v8_target_cpu" in settings:
        if settings["@v8//bazel/config:v8_target_cpu"] != "none":
            return {}

    # Auto-detect target architecture based on the --cpu flag.
    mapping = {
        "haswell": "x64",
        "k8": "x64",
        "x86_64": "x64",
        "darwin": "x64",
        "darwin_x86_64": "x64",
        "x64_windows": "x64",
        "x86": "ia32",
        "aarch64": "arm64",
        "arm64-v8a": "arm64",
        "arm": "arm64",
        "darwin_arm64": "arm64",
        "armeabi-v7a": "arm32",
        "s390x": "s390x",
        "riscv64": "riscv64",
        "ppc": "ppc64le",
    }
    v8_target_cpu = mapping[settings["//command_line_option:cpu"]]
    return {"@v8//bazel/config:v8_target_cpu": v8_target_cpu}

# Set the v8_target_cpu to be the correct architecture given the cpu specified
# on the command line.
v8_target_cpu_transition = transition(
    implementation = _v8_target_cpu_transition_impl,
    inputs = ["@v8//bazel/config:v8_target_cpu", "//command_line_option:cpu"],
    outputs = ["@v8//bazel/config:v8_target_cpu"],
)

def _mksnapshot(ctx):
    prefix = ctx.attr.prefix
    suffix = ctx.attr.suffix
    outs = [
        ctx.actions.declare_file(prefix + "/snapshot" + suffix + ".cc"),
        ctx.actions.declare_file(prefix + "/embedded" + suffix + ".S"),
    ]
    ctx.actions.run(
        outputs = outs,
        inputs = [],
        mnemonic = "V8Mksnapshot",
        arguments = [
            "--embedded_variant=Default",
            "--target_os",
            ctx.attr.target_os,
            "--startup_src",
            outs[0].path,
            "--embedded_src",
            outs[1].path,
        ] + ctx.attr.args,
        executable = ctx.executable.tool,
        progress_message = "Running mksnapshot",
    )
    return [DefaultInfo(files = depset(outs))]

_v8_mksnapshot = rule(
    implementation = _mksnapshot,
    attrs = {
        "args": attr.string_list(),
        "tool": attr.label(
            mandatory = True,
            allow_files = True,
            executable = True,
            cfg = "exec",
        ),
        "target_os": attr.string(mandatory = True),
        "prefix": attr.string(mandatory = True),
        "suffix": attr.string(mandatory = True),
    },
    cfg = v8_target_cpu_transition,
)

def v8_mksnapshot(name, args, suffix = ""):
    _v8_mksnapshot(
        name = "noicu/" + name,
        args = args,
        prefix = "noicu",
        tool = ":noicu/mksnapshot" + suffix,
        suffix = suffix,
        target_os = select({
            "@v8//bazel/config:is_macos": "mac",
            "@v8//bazel/config:is_windows": "win",
            "//conditions:default": "",
        }),
    )
    _v8_mksnapshot(
        name = "icu/" + name,
        args = args,
        prefix = "icu",
        tool = ":icu/mksnapshot" + suffix,
        suffix = suffix,
        target_os = select({
            "@v8//bazel/config:is_macos": "mac",
            "@v8//bazel/config:is_windows": "win",
            "//conditions:default": "",
        }),
    )

def _quote(val):
    if val[0] == '"' and val[-1] == '"':
        fail("String", val, "already quoted")
    return '"' + val + '"'

def _kv_bool_pair(k, v):
    return _quote(k) + ": " + v

def _json(kv_pairs):
    content = "{"
    for (k, v) in kv_pairs[:-1]:
        content += _kv_bool_pair(k, v) + ", "
    (k, v) = kv_pairs[-1]
    content += _kv_bool_pair(k, v)
    content += "}\n"
    return content

def build_config_content(cpu, icu):
    arch = cpu
    if cpu == 'x86':
        arch = 'ia32'
    return _json([
        ("arch", arch),
        ("asan", "false"),
        ("atomic_object_field_writes", "false"),
        ("cet_shadow_stack", "false"),
        ("cfi", "false"),
        ("clang_coverage", "false"),
        ("clang", "true"),
        ("code_comments", "false"),
        ("component_build", "false"),
        ("concurrent_marking", "false"),
        ("current_cpu", cpu),
        ("dcheck_always_on", "false"),
        ("debug_code", "false"),
        ("DEBUG_defined", "false"),
        ("debugging_features", "false"),
        ("dict_property_const_tracking", "false"),
        ("direct_handle", "false"),
        ("disassembler", "false"),
        ("dumpling", "false"),
        ("full_debug", "false"),
        ("gdbjit", "false"),
        ("has_jitless", "false"),
        ("sparkplug_plus", "true" if cpu in ['"x64"', '"arm64"'] else "false"),
        ("has_maglev", "true"),
        ("has_turbofan", "true"),
        ("has_webassembly", "false"),
        ("has_wasm_interpreter", "false"),
        ("i18n", icu),
        ("is_android", "false"),
        ("is_ios", "false"),
        ("is_linux", "true"),
        ("js_shared_memory", "false"),
        ("leaptiering", "true"),
        ("lite_mode", "false"),
        ("local_off_stack_check", "false"),
        ("lower_limits_mode", "false"),
        ("memory_corruption_api", "false"),
        ("mips_arch_variant", '""'),
        ("mips_use_msa", "false"),
        ("msan", "false"),
        ("official_build", "false"),
        ("pointer_compression_shared_cage", "false"),
        ("pointer_compression", "true"),
        ("runtime_call_stats", "false"),
        ("sandbox", "false"),
        ("sandbox_hardware_support", "false"),
        ("shared_ro_heap", "false"),
        ("simd_mips", "false"),
        ("simulator_run", "false"),
        ("single_generation", "false"),
        ("slow_dchecks", "false"),
        ("target_cpu", cpu),
        ("temporal", "false"),
        ("tsan", "false"),
        ("ubsan", "false"),
        ("use_sanitizer", "false"),
        ("v8_cfi", "false"),
        ("v8_current_cpu", cpu),
        ("v8_target_cpu", cpu),
        ("verify_csa", "false"),
        ("verify_heap", "false"),
        ("verify_predictable", "false"),
        ("wasm_random_fuzzers", "false"),
        ("test_only_sync_points", "false"),
        ("write_barriers", "false"),
    ])

# =============================================================================
# Metagen: libclang-driven instance-type generator.
#
# Produces gen/metagen/instance-types.h, the file `src/objects/instance-types-
# gen.h` includes when V8_USE_METAGEN_INSTANCE_TYPES=1, i.e. under
# --//:v8_use_metagen_instance_types.
#
# The generator runs tools/metagen/metagen.py, which uses libclang to parse
# V8_OBJECT-annotated C++ headers and emits the IT enum macros. We feed it
# a compile_commands.json synthesized from cc_common: the defines / includes
# come from the cc_libraries passed via `cc_compilation_context_from`, the
# toolchain bits (libc++ -isystem, sysroot, target triple, ...) come from
# the active cc toolchain, and metagen-specific cflags (-std=c++20,
# -fno-rtti, -fno-exceptions, -fsyntax-only) come from a small list below.
# =============================================================================

# Metagen-specific cflags that don't propagate via CcInfo (they're copts on
# v8_library, not on the `:define_flags` we depend on). Kept tiny on
# purpose -- everything else comes from the toolchain or CcInfo.
_METAGEN_USER_COPTS = [
    "-std=c++20",
    "-fno-rtti",
    "-fno-exceptions",
]

def _metagen_instance_types_impl(ctx):
    v8root = ctx.label.workspace_root
    if v8root == "":
        v8root = "."

    out_h = ctx.actions.declare_file(
        ctx.attr.prefix + "/metagen/instance-types.h",
    )
    out_dir = out_h.dirname

    # Merge CcInfo from all cc_compilation_context_from targets -- this is
    # where V8's defines (`:define_flags`), the libc++ system include path
    # (`@libcxx//:libc++`), and the V8 source-root include come from.
    cc_infos = [d[CcInfo] for d in ctx.attr.cc_compilation_context_from]
    merged_cc_info = cc_common.merge_cc_infos(cc_infos = cc_infos)
    cc_context = merged_cc_info.compilation_context

    cc_toolchain = find_cc_toolchain(ctx)
    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
        requested_features = ctx.features,
        unsupported_features = ctx.disabled_features + ["module_maps"],
    )

    # -I dirs for the generated headers (torque, bytecode_builtins).
    # None of these live in a cc_library CcInfo so they're added directly.
    extra_quote_includes = [
        ctx.bin_dir.path + "/" + v8root,
        ctx.bin_dir.path + "/" + v8root + "/" + ctx.attr.prefix,
        # V8 source root: needed because `is_posix` copts include
        # `-isystem .` and that's a copt, not propagated via CcInfo.
        ".",
    ]

    compile_variables = cc_common.create_compile_variables(
        feature_configuration = feature_configuration,
        cc_toolchain = cc_toolchain,
        source_file = "tools/metagen/probe.cc",
        user_compile_flags = _METAGEN_USER_COPTS,
        include_directories = cc_context.includes,
        quote_include_directories = depset(
            extra_quote_includes,
            transitive = [cc_context.quote_includes],
        ),
        system_include_directories = cc_context.system_includes,
        framework_include_directories = cc_context.framework_includes,
        preprocessor_defines = depset(
            ctx.attr.extra_defines,
            transitive = [
                cc_context.defines,
                cc_context.local_defines,
            ],
        ),
    )

    cc_cmd_line = cc_common.get_memory_inefficient_command_line(
        feature_configuration = feature_configuration,
        action_name = "c++-compile",
        variables = compile_variables,
    )
    cc_binary = cc_common.get_tool_for_action(
        feature_configuration = feature_configuration,
        action_name = "c++-compile",
    )

    # Synthesize a one-entry compile_commands.json for metagen. (The GN
    # side instead harvests flags via `gn desc`; this is the Bazel path.)
    # The `directory` is the action's cwd (execroot); the bogus
    # `probe.cc` file + `-c`/`-o` args are filtered out by metagen.
    compile_db = ctx.actions.declare_file(
        ctx.attr.prefix + "/metagen-compile-commands.json",
    )
    ctx.actions.write(
        output = compile_db,
        content = json.encode([{
            "directory": ".",
            "file": "tools/metagen/probe.cc",
            "arguments": [cc_binary] + cc_cmd_line,
        }]),
    )

    args = ctx.actions.args()
    args.add("--v8-root", v8root)
    args.add("--compile-commands", compile_db.path)
    args.add("--driver", ctx.file.driver.path)
    args.add("--out", out_dir)

    # Where libclang comes from. Both modes are explicit, so a mismatch
    # is an analysis-time error rather than a harvest that quietly parses
    # with the wrong library.
    if ctx.attr.libclang_from_python_env:
        if ctx.files.libclang_files:
            fail("libclang_from_python_env = True, but libclang_files is " +
                 "non-empty. In this mode the bindings come from the tool's " +
                 "Python deps; drop libclang_files or unset the flag.")
        args.add("--libclang-from-python-env")
    else:
        if not ctx.files.libclang_files:
            fail("libclang_files is empty. Pass the llvm-libclang package " +
                 "files, or set libclang_from_python_env = True to take the " +
                 "bindings from the tool's Python deps instead.")

        # The llvm-libclang package root, as an execroot-relative path:
        # metagen resolves it against the action's cwd. The files get
        # there as action inputs (:metagen_libclang_files below), which
        # is why the tool needs no runfiles copy of them. This mirrors
        # what the GN action passes for //third_party/llvm-libclang.
        args.add("--libclang-dir", v8root + "/third_party/llvm-libclang")

    # Clang's builtin headers. Find the directory via stddef.h rather
    # than files[0].dirname: the staged set contains subdirectories
    # (sanitizer/, cuda_wrappers/, ...), so element 0 sits at the include
    # root only by luck of ordering, and a dir one level too deep fails
    # as a wall of parse errors rather than a build error.
    builtin_headers_dir = None
    for f in ctx.files.clang_builtin_headers:
        if f.basename == "stddef.h":
            builtin_headers_dir = f.dirname
            break
    if builtin_headers_dir == None:
        fail("clang_builtin_headers contains no stddef.h, so the clang " +
             "builtin-header directory cannot be located. Pass the target " +
             "that stages clang's builtin headers (lib/Headers).")
    args.add("--clang-builtin-headers-dir", builtin_headers_dir)

    all_inputs = depset(
        direct = [compile_db, ctx.file.driver],
        transitive = [
            depset(ctx.files.libclang_files),
            depset(ctx.files.clang_builtin_headers),
            depset(ctx.files.python_srcs),
            depset(ctx.files.extra_sandbox_files),
            cc_context.headers,
            cc_toolchain.all_files,
        ],
    )

    ctx.actions.run(
        outputs = [out_h],
        inputs = all_inputs,
        executable = ctx.executable.tool,
        arguments = [args],
        mnemonic = "MetagenInstanceTypes",
        progress_message = "Generating metagen/instance-types.h",
    )

    return [DefaultInfo(files = depset([out_h]))]

# The harvest is a host tool that must reason about the target build, so
# this rule's inputs fall into two groups -- the same split GN gets from
# v8_generator_toolchain (see gni/snapshot_toolchain.gni), where the
# generator is a host binary compiled with the target's V8 configuration:
#
#   exec-configured   the parsing machinery: the tool, libclang, and
#                     clang's builtin headers. These are host-only
#                     artifacts; a build that constrains targets by
#                     platform will reject them for a non-host target
#                     unless they are exec-configured.
#
#   target-configured what we parse and how: `headers`,
#                     `cc_compilation_context_from`, `extra_defines`, and
#                     `extra_sandbox_files` (V8's own source headers).
#                     These must reflect the TARGET build -- exec-
#                     configuring them would harvest the host's defines,
#                     which today changes nothing but would silently emit
#                     host object layout once metagen generates layout.
_metagen_instance_types = rule(
    implementation = _metagen_instance_types_impl,
    attrs = {
        "prefix": attr.string(mandatory = True),
        # The checked-in C++ driver, passed straight through as --driver. Its
        # direct includes are staged by extra_sandbox_files, not by this attr.
        "driver": attr.label(allow_single_file = True, mandatory = True),
        "libclang_files": attr.label_list(allow_files = True, cfg = "exec"),
        # Clang's builtin headers (stddef.h etc.), staged into the action
        # sandbox and passed to metagen as -isystem. Separate from
        # extra_sandbox_files because this is host toolchain material:
        # it needs cfg = "exec", whereas V8's own headers must not have it.
        "clang_builtin_headers": attr.label_list(
            allow_files = True,
            cfg = "exec",
            mandatory = True,
        ),
        # Selects where libclang comes from; see the impl. Explicit rather
        # than inferred from libclang_files being empty, so no call site
        # can end up in environment mode by forgetting an argument.
        "libclang_from_python_env": attr.bool(default = False),
        "python_srcs": attr.label_list(allow_files = True, cfg = "exec"),
        # Additional files that need to live in the action sandbox so
        # libclang's transitive #include resolution succeeds, but that
        # should NOT be directly included by the driver.
        # Bazel hermetic sandboxing makes this necessary: header files
        # only reach the sandbox via declared inputs, and the CcInfo of
        # :v8_libbase / :define_flags doesn't reach into src/objects,
        # src/handles, src/wasm etc. v8_libshared (which would) depends
        # on this rule's output, so a CcInfo-based propagation cycles.
        # V8's own headers, so target-configured; host-toolchain material
        # goes in clang_builtin_headers instead.
        "extra_sandbox_files": attr.label_list(allow_files = True),
        # cc_library / v8_library / v8_config labels whose CcInfo merged
        # compilation_context supplies the defines, includes, and libc++
        # paths that V8 normally compiles with.
        "cc_compilation_context_from": attr.label_list(
            providers = [CcInfo],
            mandatory = True,
        ),
        # Additional -D defines for the harvest that do not propagate
        # via CcInfo (v8_library applies them as per-variant copts,
        # e.g. V8_INTL_SUPPORT on the icu prefix).
        "extra_defines": attr.string_list(),
        "tool": attr.label(
            executable = True,
            cfg = "exec",
            mandatory = True,
        ),
    },
    fragments = ["cpp"],
    # use_cc_toolchain() rather than a literal @bazel_tools label: the
    # helper resolves to whichever cc toolchain type the surrounding
    # build defines, and find_cc_toolchain() reads it back out of
    # ctx.toolchains. Spelling the label directly makes the rule
    # unusable in builds that do not have a @bazel_tools repository.
    toolchains = use_cc_toolchain(),
)

def metagen_instance_types(name, driver,
                           python_srcs, tool,
                           cc_compilation_context_from,
                           clang_builtin_headers,
                           libclang_files = [],
                           libclang_from_python_env = False,
                           extra_sandbox_files = [],
                           icu_extra_defines = [],
                           icu_cc_compilation_context_from = []):
    """Emit gen/metagen/instance-types.h for both icu/ and noicu/ prefixes.

    Mirrors the v8_torque_files double-emission pattern so each consumer
    library can include the correctly-prefixed copy. The two harvests
    differ: v8_library compiles the icu variant with V8_INTL_SUPPORT
    (via copts, invisible to CcInfo) and the intl object sources, so
    the icu emission additionally enables the driver's i18n includes with the
    intl defines and ICU's compilation context.

    Pass either `libclang_files` (the bundled llvm-libclang package) or
    `libclang_from_python_env = True` (bindings supplied through `tool`'s
    Python deps) -- exactly one, enforced by the rule.
    """
    for prefix in ("noicu", "icu"):
        is_icu = prefix == "icu"
        _metagen_instance_types(
            name = prefix + "/" + name,
            prefix = prefix,
            driver = driver,
            libclang_files = libclang_files,
            libclang_from_python_env = libclang_from_python_env,
            clang_builtin_headers = clang_builtin_headers,
            python_srcs = python_srcs,
            tool = tool,
            cc_compilation_context_from = cc_compilation_context_from +
                                          (icu_cc_compilation_context_from if is_icu else []),
            extra_defines = icu_extra_defines if is_icu else [],
            extra_sandbox_files = extra_sandbox_files,
        )

# TODO(victorgomes): Create a rule (instead of a macro), that can
# dynamically populate the build config.
def v8_build_config(name, arch):
    cpu = '"' + arch + '"'
    native.genrule(
        name = "noicu/" + name,
        outs = ["noicu/" + name + ".json"],
        cmd = "echo '" + build_config_content(cpu, "false") + "' > \"$@\"",
    )
    native.genrule(
        name = "icu/" + name,
        outs = ["icu/" + name + ".json"],
        cmd = "echo '" + build_config_content(cpu, "true") + "' > \"$@\"",
    )
