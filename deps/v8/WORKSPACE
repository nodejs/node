# Copyright 2021 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

workspace(name = "v8")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
http_archive(
    name = "bazel_skylib",
    urls = [
        "https://github.com/bazelbuild/bazel-skylib/releases/download/1.0.3/bazel-skylib-1.0.3.tar.gz",
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.0.3/bazel-skylib-1.0.3.tar.gz",
    ],
    sha256 = "1c531376ac7e5a180e0237938a2536de0c54d93f5c278634818e0efc952dd56c",
)
load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")
bazel_skylib_workspace()

new_local_repository(
    name = "config",
    path = "bazel/config",
    build_file = "bazel/config/BUILD.bazel",
)

new_local_repository(
    name = "zlib",
    path = "third_party/zlib",
    build_file = "bazel/BUILD.zlib",
)

new_local_repository(
    name = "icu",
    path = "third_party/icu",
    build_file = "bazel/BUILD.icu",
)
