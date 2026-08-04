"""Repository rule for building libc++ from third_party sources."""

def _libcxx_repository_impl(ctx):
    # Find the workspace root
    workspace_root = ctx.path(Label("@//:BUILD.bazel")).dirname

    # Symlink the source directories
    ctx.symlink(workspace_root.get_child("third_party").get_child("libc++"), "libc++")
    ctx.symlink(workspace_root.get_child("third_party").get_child("libc++abi"), "libc++abi")
    ctx.symlink(workspace_root.get_child("third_party").get_child("llvm-libc"), "llvm-libc")
    ctx.symlink(workspace_root.get_child("buildtools").get_child("third_party").get_child("libc++"), "buildtools_libc++")

    # Get the external repository path for include flags
    # In bzlmod, repo names may have prefixes, so we need to determine the actual path
    repo_path = "external/" + ctx.name

    # Create the BUILD file
    # NOTE: We don't use 'includes' attribute here because it creates relative paths
    # that conflict with the toolchain's absolute paths, breaking #include_next.
    # The toolchain provides the libc++ include paths via -isystem flags.
    build_content = '''
package(default_visibility = ["//visibility:public"])

LIBCXX_COPTS = [
    "-std=c++23",
    "-fPIC",
    "-fstrict-aliasing",
    "-fexceptions",
    "-frtti",
    "-D_LIBCPP_BUILDING_LIBRARY",
    "-D_LIBCPP_HARDENING_MODE_DEFAULT=_LIBCPP_HARDENING_MODE_NONE",
    "-DLIBC_NAMESPACE=__llvm_libc_cr",
]

cc_library(
    name = "libc++abi",
    srcs = glob([
        "libc++abi/src/src/*.cpp",
        "libc++abi/src/src/*.h",
        "libc++abi/src/src/demangle/*.h",
    ], exclude = [
        # Exclude files not needed for Linux build
        "libc++abi/src/src/cxa_noexception.cpp",
        "libc++abi/src/src/stdlib_new_delete.cpp",
    ]),
    hdrs = glob([
        "libc++abi/src/include/**/*.h",
        "libc++/src/include/**/*",
        "libc++/src/src/include/*.h",
        "libc++abi/src/src/demangle/*.def",
        "buildtools_libc++/__config_site",
        "buildtools_libc++/__assertion_handler",
        "llvm-libc/src/**/*.h",
    ]),
    copts = LIBCXX_COPTS + [
        "-DLIBCXXABI_SILENT_TERMINATE",
        "-iquote", "{REPO_PATH}/libc++abi/src/src",
        "-iquote", "{REPO_PATH}/libc++abi/src/src/demangle",
    ],
    linkstatic = True,
)

cc_library(
    name = "libc++",
    srcs = glob([
        "libc++/src/src/*.cpp",
        "libc++/src/src/*.h",
        "libc++/src/src/filesystem/*.cpp",
        "libc++/src/src/filesystem/*.h",
        "libc++/src/src/ryu/*.cpp",
        "libc++/src/src/include/*.h",
        "libc++/src/src/include/ryu/*.h",
    ]) + glob(["libc++/src/src/support/**/*.ipp"], allow_empty = True),
    hdrs = glob([
        "libc++/src/include/**/*",
        "buildtools_libc++/__config_site",
        "buildtools_libc++/__assertion_handler",
        "llvm-libc/src/**/*.h",
    ]),
    copts = LIBCXX_COPTS + [
        "-DLIBCXX_BUILDING_LIBCXXABI",
        "-iquote", "{REPO_PATH}/libc++/src/src",
        "-iquote", "{REPO_PATH}/libc++/src/src/filesystem",
    ],
    linkopts = [
        "-lpthread",
        "-lm",
    ],
    deps = [":libc++abi"],
    linkstatic = True,
)
'''.format(REPO_PATH=repo_path)
    ctx.file("BUILD.bazel", build_content)

libcxx_repository = repository_rule(
    implementation = _libcxx_repository_impl,
    local = True,
    configure = True,
)
