# This is a placeholder for python headers. Projects needing to use
# fast cpp protos in protobuf's python interface should build with
# --define=use_fast_cpp_protos=true, and in addition, provide
# //external:python_headers dependency that in turn provides Python.h.
#
# Projects that include protobuf using a Bazel external repository will need to
# add a workspace rule to their WORKSPACE files to add an external workspace
# that includes the Python headers. For example, the protobuf WORKSPACE file
# includes the following local_repository rule that points to this directory:
#
# new_local_repository(
#   name = "python",
#   path = __workspace_dir__ + "/util/python",
# )
cc_library(
    name = "python_headers",
    visibility = ["//visibility:public"],
)
