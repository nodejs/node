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

cc_library(
    name = "jsoncpp",
    srcs = [
        "src/lib_json/json_batchallocator.h",
        "src/lib_json/json_internalarray.inl",
        "src/lib_json/json_internalmap.inl",
        "src/lib_json/json_reader.cpp",
        "src/lib_json/json_tool.h",
        "src/lib_json/json_value.cpp",
        "src/lib_json/json_valueiterator.inl",
        "src/lib_json/json_writer.cpp",
    ],
    hdrs = [
        "include/json/assertions.h",
        "include/json/autolink.h",
        "include/json/config.h",
        "include/json/features.h",
        "include/json/forwards.h",
        "include/json/json.h",
        "include/json/reader.h",
        "include/json/value.h",
        "include/json/version.h",
        "include/json/writer.h",
    ],
    copts = [
        "-Isrc/lib_json",
    ] + PERFETTO_CONFIG.deps_copts.jsoncpp,
    defines = [
        "JSON_USE_EXCEPTION=0",
    ],
    includes = [
        "include",
    ],
    visibility = ["//visibility:public"],
)
