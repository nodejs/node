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

load("@perfetto_cfg//:perfetto_cfg.bzl", "PERFETTO_CONFIG")
load("@perfetto//bazel:proto_gen.bzl", "proto_gen")

# +----------------------------------------------------------------------------+
# | Base C++ rules.                                                            |
# +----------------------------------------------------------------------------+

def default_cc_args():
    return {
        "deps": PERFETTO_CONFIG.deps.build_config,
        "copts": [
            "-Wno-pragma-system-header-outside-header",
        ],
        "includes": ["include"],
        "linkopts": select({
            "@perfetto//bazel:os_linux": ["-ldl", "-lrt", "-lpthread"],
            "@perfetto//bazel:os_osx": [],
            "@perfetto//bazel:os_windows": [],
            "//conditions:default": ["-ldl"],
        }),
    }

def perfetto_cc_library(**kwargs):
    args = _merge_dicts(default_cc_args(), kwargs)
    if not _rule_override("cc_library", **args):
        native.cc_library(**args)

def perfetto_cc_binary(**kwargs):
    args = _merge_dicts(default_cc_args(), kwargs)
    if not _rule_override("cc_binary", **args):
        native.cc_binary(**args)

def perfetto_py_binary(**kwargs):
    if not _rule_override("py_binary", **kwargs):
        native.py_binary(**kwargs)

# +----------------------------------------------------------------------------+
# | Proto-related rules                                                        |
# +----------------------------------------------------------------------------+

def perfetto_proto_library(**kwargs):
    if not _rule_override("proto_library", **kwargs):
        native.proto_library(**kwargs)

def perfetto_cc_proto_library(**kwargs):
    if not _rule_override("cc_proto_library", **kwargs):
        native.cc_proto_library(**kwargs)

def perfetto_java_proto_library(**kwargs):
    if not _rule_override("java_proto_library", **kwargs):
        native.java_proto_library(**kwargs)

# +----------------------------------------------------------------------------+
# | Misc rules.                                                                |
# +----------------------------------------------------------------------------+

# Unlike all the other rules, this is an noop by default because Bazel does not
# support gensignature.
def perfetto_gensignature_internal_only(**kwargs):
    _rule_override("gensignature_internal_only", **kwargs)

# Generates .pbzero.{cc,h} from .proto(s). We deliberately do NOT generate
# conventional .pb.{cc,h} from here as protozero gen sources do not have any
# dependency on libprotobuf.
def perfetto_cc_protozero_library(name, deps, **kwargs):
    if _rule_override(
        "cc_protozero_library",
        name = name,
        deps = deps,
        **kwargs
    ):
        return

    proto_gen(
        name = name + "_src",
        deps = deps,
        suffix = "pbzero",
        plugin = PERFETTO_CONFIG.root + ":protozero_plugin",
        wrapper_namespace = "pbzero",
        protoc = PERFETTO_CONFIG.deps.protoc[0],
        root = PERFETTO_CONFIG.root,
    )

    native.filegroup(
        name = name + "_h",
        srcs = [":" + name + "_src"],
        output_group = "h",
    )

    perfetto_cc_library(
        name = name,
        srcs = [":" + name + "_src"],
        hdrs = [":" + name + "_h"],
        deps = [PERFETTO_CONFIG.root + ":libprotozero"],
        **kwargs
    )

# Generates .ipc.{cc,h} and .pb.{cc.h} from .proto(s). The IPC sources depend
# on .pb.h so we need to generate also the standard protobuf sources here.
def perfetto_cc_ipc_library(name, deps, **kwargs):
    if _rule_override("cc_ipc_library", name = name, deps = deps, **kwargs):
        return

    # A perfetto_cc_ipc_library has two types of dependencies:
    # 1. Exactly one dependency on a proto_library target. This defines the
    #    .proto sources for the target
    # 2. Zero or more deps on other perfetto_cc_protocpp_library targets. This
    #    to deal with the case of foo.proto including common.proto from another
    #    target.
    _proto_deps = [d for d in deps if d.endswith("_protos")]
    _cc_deps = [d for d in deps if d not in _proto_deps]
    if len(_proto_deps) != 1:
        fail("Too many proto deps for target %s" % name)

    # Generates .ipc.{cc,h}.
    proto_gen(
        name = name + "_src",
        deps = _proto_deps,
        suffix = "ipc",
        plugin = PERFETTO_CONFIG.root + ":ipc_plugin",
        wrapper_namespace = "gen",
        protoc = PERFETTO_CONFIG.deps.protoc[0],
        root = PERFETTO_CONFIG.root,
    )

    native.filegroup(
        name = name + "_h",
        srcs = [":" + name + "_src"],
        output_group = "h",
    )

    perfetto_cc_library(
        name = name,
        srcs = [":" + name + "_src"],
        hdrs = [":" + name + "_h"],
        deps = [
            # Generated .ipc.{cc,h} depend on this and protozero.
            PERFETTO_CONFIG.root + ":perfetto_ipc",
            PERFETTO_CONFIG.root + ":libprotozero",
        ] + _cc_deps,
        **kwargs
    )


# Generates .gen.{cc,h} from .proto(s).
def perfetto_cc_protocpp_library(name, deps, **kwargs):
    if _rule_override(
        "cc_protocpp_library",
        name = name,
        deps = deps,
        **kwargs
    ):
        return

    # A perfetto_cc_protocpp_library has two types of dependencies:
    # 1. Exactly one dependency on a proto_library target. This defines the
    #    .proto sources for the target
    # 2. Zero or more deps on other perfetto_cc_protocpp_library targets. This
    #    to deal with the case of foo.proto including common.proto from another
    #    target.
    _proto_deps = [d for d in deps if d.endswith("_protos")]
    _cc_deps = [d for d in deps if d not in _proto_deps]
    if len(_proto_deps) != 1:
        fail("Too many proto deps for target %s" % name)

    proto_gen(
        name = name + "_gen",
        deps = _proto_deps,
        suffix = "gen",
        plugin = PERFETTO_CONFIG.root + ":cppgen_plugin",
        wrapper_namespace = "gen",
        protoc = PERFETTO_CONFIG.deps.protoc[0],
        root = PERFETTO_CONFIG.root,
    )

    native.filegroup(
        name = name + "_gen_h",
        srcs = [":" + name + "_gen"],
        output_group = "h",
    )

    perfetto_cc_library(
        name = name,
        srcs = [":" + name + "_gen"],
        hdrs = [":" + name + "_gen_h"],
        deps = [
            PERFETTO_CONFIG.root + ":libprotozero"
        ] + _cc_deps,
        **kwargs
    )

# +----------------------------------------------------------------------------+
# | Misc utility functions                                                     |
# +----------------------------------------------------------------------------+

def _rule_override(rule_name, **kwargs):
    overrides = getattr(PERFETTO_CONFIG, "rule_overrides", struct())
    overridden_rule = getattr(overrides, rule_name, None)
    if overridden_rule:
        overridden_rule(**kwargs)
        return True
    return False

def _merge_dicts(*args):
    res = {}
    for arg in args:
        for k, v in arg.items():
            if type(v) == "string" or type(v) == "bool":
                res[k] = v
            elif type(v) == "list" or type(v) == "select":
                res[k] = res.get(k, []) + v
            else:
                fail("key type not supported: " + type(v))
    return res
