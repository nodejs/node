# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")

# This file must be kept in sync with tools/install-build-deps.

def perfetto_deps():
    # Note: this is more recent than the version of protobuf we use in the
    # GN and Android builds. This is because older versions of protobuf don't
    # support Bazel.
    _add_repo_if_not_existing(
        http_archive,
        name = "com_google_protobuf",
        strip_prefix = "protobuf-3.9.0",
        url = "https://github.com/google/protobuf/archive/v3.9.0.tar.gz",
        sha256 = "2ee9dcec820352671eb83e081295ba43f7a4157181dad549024d7070d079cf65",
    )

    _add_repo_if_not_existing(
        http_archive,
        name = "perfetto_dep_sqlite",
        url = "https://storage.googleapis.com/perfetto/sqlite-amalgamation-3250300.zip",
        sha256 = "2ad5379f3b665b60599492cc8a13ac480ea6d819f91b1ef32ed0e1ad152fafef",
        strip_prefix = "sqlite-amalgamation-3250300",
        build_file = "//bazel:sqlite.BUILD",
    )

    _add_repo_if_not_existing(
        http_archive,
        name = "perfetto_dep_sqlite_src",
        url = "https://storage.googleapis.com/perfetto/sqlite-src-3250300.zip",
        sha256 = "c7922bc840a799481050ee9a76e679462da131adba1814687f05aa5c93766421",
        strip_prefix = "sqlite-src-3250300",
        build_file = "//bazel:sqlite.BUILD",
    )

    _add_repo_if_not_existing(
        new_git_repository,
        name = "perfetto_dep_linenoise",
        remote = "https://fuchsia.googlesource.com/third_party/linenoise.git",
        commit = "c894b9e59f02203dbe4e2be657572cf88c4230c3",
        build_file = "//bazel:linenoise.BUILD",
        shallow_since = "1469784335 +0200",
    )

    _add_repo_if_not_existing(
        new_git_repository,
        name = "perfetto_dep_jsoncpp",
        remote = "https://github.com/open-source-parsers/jsoncpp",
        commit = "7165f6ac4c482e68475c9e1dac086f9e12fff0d0",  # v1.0.0
        build_file = "//bazel:jsoncpp.BUILD",
        shallow_since = "1416494758 -0600",
    )

    _add_repo_if_not_existing(
        new_git_repository,
        name = "perfetto_dep_zlib",
        remote = "https://android.googlesource.com/platform/external/zlib.git",
        commit = "dfa0646a03b4e1707469e04dc931b09774968fe6",
        build_file = "//bazel:zlib.BUILD",
        shallow_since = "1557160162 -0700",
    )

    # Without this protobuf.bzl fails. This seems a bug in protobuf_deps().
    _add_repo_if_not_existing(
        http_archive,
        name = "bazel_skylib",
        sha256 = "bbccf674aa441c266df9894182d80de104cabd19be98be002f6d478aaa31574d",
        strip_prefix = "bazel-skylib-2169ae1c374aab4a09aa90e65efe1a3aad4e279b",
        url = "https://github.com/bazelbuild/bazel-skylib/archive/2169ae1c374aab4a09aa90e65efe1a3aad4e279b.tar.gz",
    )

def _add_repo_if_not_existing(repo_rule, name, **kwargs):
    if name not in native.existing_rules():
        repo_rule(name = name, **kwargs)
