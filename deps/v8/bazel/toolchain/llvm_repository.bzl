def _llvm_toolchain_impl(ctx):
    # Find the workspace root by resolving a label in the main repository
    # This assumes the main repo is "@" or the default.
    workspace_root = ctx.path(Label("@//:BUILD.bazel")).dirname

    # Construct the path to the LLVM build directory within the workspace
    # ctx.attr.path should be relative to the workspace root,
    # e.g. "third_party/llvm-build/Release+Asserts"
    # We manually split and traverse because get_child handles one component.
    llvm_path = workspace_root
    for component in ctx.attr.path.split("/"):
        llvm_path = llvm_path.get_child(component)

    # Symlink top-level directories and files
    items = ["bin", "lib", "include", "share"]
    for item in items:
        # Check if the item exists before symlinking to avoid errors
        src_path = llvm_path.get_child(item)
        if src_path.exists:
             ctx.symlink(src_path, item)

    # Create the config file
    config_content = ctx.attr.config_file_content.replace("{WORKSPACE_ROOT}", str(workspace_root))
    ctx.file("cc_toolchain_config.bzl", config_content)

    # Create the BUILD file
    ctx.file("BUILD.bazel", ctx.attr.build_file_content)

llvm_toolchain_repository = repository_rule(
    implementation = _llvm_toolchain_impl,
    attrs = {
        "path": attr.string(mandatory = True,
                            doc = "Path to the LLVM install directory relative to workspace root"),
        "build_file_content": attr.string(mandatory = True, doc = "Content of the BUILD file"),
        "config_file_content": attr.string(mandatory = True,
                                           doc = "Content of the cc_toolchain_config.bzl file"),
    },
    local = True,
    configure = True, # Indicates this rule depends on system configuration/files
)
