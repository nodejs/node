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

# Noop function used to override rules we don't want to support in standalone.
def _noop_override(**kwargs):
    pass

PERFETTO_CONFIG = struct(
    # This is used to refer to deps within perfetto's BUILD files.
    # In standalone and bazel-based embedders use '//', because perfetto has its
    # own repository, and //xxx would be relative to @perfetto//xxx.
    # In Google internal builds, instead, this is set to //third_party/perfetto,
    # because perfetto doesn't have its own repository there.
    root = "//",

    # These variables map dependencies to perfetto third-party projects. This is
    # to allow perfetto embedders (e.g. gapid) and google internal builds to
    # override paths and target names to their own third_party.
    deps = struct(
        # Target exposing the build config header. It should be a valid
        # cc_library dependency as it will become a dependency of every
        # perfetto_cc_library target. It needs to expose a
        # "perfetto_build_flags.h" file that can be included via:
        # #include "perfetto_build_flags.h".
        build_config = ["//:build_config_hdr"],
        zlib = ["@perfetto_dep_zlib//:zlib"],
        jsoncpp = ["@perfetto_dep_jsoncpp//:jsoncpp"],
        linenoise = ["@perfetto_dep_linenoise//:linenoise"],
        sqlite = ["@perfetto_dep_sqlite//:sqlite"],
        sqlite_ext_percentile = ["@perfetto_dep_sqlite_src//:percentile_ext"],
        protoc = ["@com_google_protobuf//:protoc"],
        protoc_lib = ["@com_google_protobuf//:protoc_lib"],
        protobuf_lite = ["@com_google_protobuf//:protobuf_lite"],
        protobuf_full = ["@com_google_protobuf//:protobuf"],
    ),

    # This struct allows embedders to customize the cc_opts for Perfetto
    # 3rd party dependencies. They only have an effect if the dependencies are
    # initialized with the Perfetto build files (i.e. via perfetto_deps()).
    deps_copts = struct(
        zlib = [],
        jsoncpp = [],
        linenoise = [],
        sqlite = [],
    ),

    # Allow Bazel embedders to change the visibility of "public" targets.
    # This variable has been introduced to limit the change to Bazel and avoid
    # making the targets fully public in the google internal tree.
    public_visibility = [
        "//visibility:public",
    ],

    # Allow Bazel embedders to change the visibility of the proto targets.
    # This variable has been introduced to limit the change to Bazel and avoid
    # making the targets public in the google internal tree.
    proto_library_visibility = "//visibility:private",

    # This struct allows the embedder to customize copts and other args passed
    # to rules like cc_binary. Prefixed rules (e.g. perfetto_cc_binary) will
    # look into this struct before falling back on native.cc_binary().
    # This field is completely optional, the embedder can omit the whole
    # |rule_overrides| or invidivual keys. They are assigned to None or noop
    # actions here just for documentation purposes.
    rule_overrides = struct(
        cc_binary = None,
        cc_library = None,
        cc_proto_library = None,
        # Supporting java rules pulls in the JDK and generally is not something
        # we need for most embedders.
        java_proto_library = _noop_override,
        proto_library = None,
        py_binary = None,

        # We only need this for internal binaries. No other embeedder should
        # care about this.
        gensignature_internal_only = None,
    ),
)
