# This BUILD file shows how to use protobuf with bazel. Before you can use
# proto_library/<lang>_proto_library rules in a BUILD file, you need to
# include protobuf repo as remote repositories in your WORKSPACE file. See
# the WORKSPACE file in the same directory with this BUILD file for an
# example.

# For each .proto file, a proto_library target should be defined. This target
# is not bound to any particular language. Instead, it defines the dependency
# graph of the .proto files (i.e., proto imports) and serves as the provider
# of .proto source files to the protocol compiler.
#
# Remote repository "com_google_protobuf" must be defined to use this rule.
proto_library(
    name = "addressbook_proto",
    srcs = ["addressbook.proto"],
    deps = ["@com_google_protobuf//:timestamp_proto"],
)

# The cc_proto_library rule generates C++ code for a proto_library rule. It
# must have exactly one proto_library dependency. If you want to use multiple
# proto_library targets, create a separate cc_proto_library target for each
# of them.
#
# Remote repository "com_google_protobuf_cc" must be defined to use this rule.
cc_proto_library(
    name = "addressbook_cc_proto",
    deps = [":addressbook_proto"],
)

# cc_library/cc_binary targets can depend on cc_proto_library targets.
cc_binary(
    name = "add_person_cpp",
    srcs = ["add_person.cc"],
    deps = [":addressbook_cc_proto"],
)

cc_binary(
    name = "list_people_cpp",
    srcs = ["list_people.cc"],
    deps = [":addressbook_cc_proto"],
)

# Similar to cc_proto_library but for Java.
#
# Remote repository "com_google_protobuf_java" must be defined to use this rule.
java_proto_library(
    name = "addressbook_java_proto",
    deps = [":addressbook_proto"],
)

java_binary(
    name = "add_person_java",
    srcs = ["AddPerson.java"],
    main_class = "AddPerson",
    deps = [":addressbook_java_proto"],
)

java_binary(
    name = "list_people_java",
    srcs = ["ListPeople.java"],
    main_class = "ListPeople",
    deps = [":addressbook_java_proto"],
)

# Java lite.
#
# Remote repository "com_google_protobuf_javalite" must be defined to use this
# rule.
java_lite_proto_library(
    name = "addressbook_java_lite_proto",
    deps = [":addressbook_proto"],
)

# Java lite API is a subset of the regular Java API so if you only uses this
# subset in your code, you can actually compile your code against both (i.e.,
# share code between server build and Android build).
#
# The lite version has a smaller code size, and you can see that by comparing
# the resulted .jar file:
#
#   $ bazel build :add_person_java_deploy.jar :add_person_java_lite_deploy.jar
#   $ ls -l bazel-bin/*_deploy.jar
#   -r-xr-xr-x 1 xiaofeng eng 1230797 Sep  8 12:24 bazel-bin/add_person_java_deploy.jar
#   -r-xr-xr-x 1 xiaofeng eng  236166 Sep  8 12:24 bazel-bin/add_person_java_lite_deploy.jar
#
# In the above example, the lite .jar file is 6 times smaller. With proper
# proguard inlining/stripping, the difference can be much more larger than
# that.
java_binary(
    name = "add_person_java_lite",
    srcs = ["AddPerson.java"],
    main_class = "AddPerson",
    deps = [":addressbook_java_lite_proto"],
)

java_binary(
    name = "list_people_java_lite",
    srcs = ["ListPeople.java"],
    main_class = "ListPeople",
    deps = [":addressbook_java_lite_proto"],
)
