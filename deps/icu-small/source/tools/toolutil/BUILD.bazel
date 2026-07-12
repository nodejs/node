# © 2021 and later: Unicode, Inc. and others.
# License & terms of use: http://www.unicode.org/copyright.html

# This Bazel build file defines targets that are dependencies for building
# the gennorm2 and genprops binaries.

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "toolutil",
    includes = ["."],
    hdrs = ["toolutil.h"],
    srcs = ["toolutil.cpp"],
    local_defines = [
        "U_TOOLUTIL_IMPLEMENTATION",
    ],
    deps = ["//icu4c/source/common:platform"],
)

cc_library(
    name = "unewdata",
    includes = ["."],
    hdrs = ["unewdata.h"],
    srcs = ["unewdata.cpp"],
    local_defines = [
        "U_TOOLUTIL_IMPLEMENTATION",
    ],
    deps = [
        ":filestrm",
        "//icu4c/source/common:platform",
    ],
)

cc_library(
    name = "uoptions",
    includes = ["."],
    hdrs = ["uoptions.h"],
    srcs = ["uoptions.cpp"],
    local_defines = [
        "U_TOOLUTIL_IMPLEMENTATION",
    ],
    deps = ["//icu4c/source/common:platform"],
)

cc_library(
    name = "writesrc",
    includes = ["."],
    hdrs = ["writesrc.h"],
    srcs = ["writesrc.cpp"],
    local_defines = [
        "U_TOOLUTIL_IMPLEMENTATION",
    ],
    deps = [
        "//icu4c/source/common:bytestream",
        "//icu4c/source/common:platform",
        "//icu4c/source/common:uniset_core",
    ],
)

cc_library(
    name = "uparse",
    includes = ["."],
    hdrs = ["uparse.h"],
    srcs = ["uparse.cpp"],
    local_defines = [
        "U_TOOLUTIL_IMPLEMENTATION",
    ],
    deps = [
        ":filestrm",
        "//icu4c/source/common:platform",
        ],
)

cc_library(
    name = "filestrm",
    includes = ["."],
    hdrs = ["filestrm.h"],
    srcs = ["filestrm.cpp"],
    local_defines = [
        "U_TOOLUTIL_IMPLEMENTATION",
    ],
    deps = ["//icu4c/source/common:platform"],
)

cc_library(
    name = "ppucd",
    includes = ["."],
    hdrs = ["ppucd.h"],
    srcs = ["ppucd.cpp"],
    local_defines = [
        "U_TOOLUTIL_IMPLEMENTATION",
    ],
    deps = [
        ":uparse",
        "//icu4c/source/common:platform",
    ],
)

cc_library(
    name = "denseranges",
    includes = ["."],
    hdrs = ["denseranges.h"],
    srcs = ["denseranges.cpp"],
    local_defines = [
        "U_TOOLUTIL_IMPLEMENTATION",
    ],
    deps = ["//icu4c/source/common:platform"],
)

cc_library(
    name = "collationinfo",
    includes = ["."],
    hdrs = ["collationinfo.h"],
    srcs = ["collationinfo.cpp"],
    local_defines = [
        "U_TOOLUTIL_IMPLEMENTATION",
    ],
    deps = [
        "//icu4c/source/common:platform",
        "//icu4c/source/i18n:headers",
    ],
)
