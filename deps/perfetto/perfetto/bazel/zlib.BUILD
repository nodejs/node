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
    name = "zlib",
    srcs = [
        "src/adler32.c",
        "src/compress.c",
        "src/crc32.c",
        "src/crc32.h",
        "src/deflate.c",
        "src/deflate.h",
        "src/gzclose.c",
        "src/gzguts.h",
        "src/gzlib.c",
        "src/gzread.c",
        "src/gzwrite.c",
        "src/infback.c",
        "src/inffast.c",
        "src/inffast.h",
        "src/inffixed.h",
        "src/inflate.c",
        "src/inflate.h",
        "src/inftrees.c",
        "src/inftrees.h",
        "src/trees.c",
        "src/trees.h",
        "src/uncompr.c",
        "src/zconf.h",
        "src/zlib.h",
        "src/zutil.c",
        "src/zutil.h",
    ],
    hdrs = [
        "zlib.h",
    ],
    copts = [
        "-DHAVE_HIDDEN",
        "-Isrc",
    ] + PERFETTO_CONFIG.deps_copts.zlib,
    includes = ["zlib"],
    visibility = ["//visibility:public"],
)
