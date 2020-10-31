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

# This build file is used for both @perfetto_dep_sqlite and
# @perfetto_dep_sqlite_src.

load("@perfetto_cfg//:perfetto_cfg.bzl", "PERFETTO_CONFIG")

# #############################
# @perfetto_dep_sqlite section
# #############################

filegroup(
    name = "headers",
    srcs = [
        "sqlite3.h",
        "sqlite3ext.h",
    ],
    visibility = ["//visibility:public"],
)

include_sqlite = [
    ".",
]

sqlite_copts = [
    "-DSQLITE_THREADSAFE=0",
    "-DQLITE_DEFAULT_MEMSTATUS=0",
    "-DSQLITE_LIKE_DOESNT_MATCH_BLOBS",
    "-DSQLITE_OMIT_DEPRECATED",
    "-DSQLITE_OMIT_SHARED_CACHE",
    "-DHAVE_USLEEP",
    "-DHAVE_UTIME",
    "-DSQLITE_BYTEORDER=1234",
    "-DSQLITE_DEFAULT_AUTOVACUUM=0",
    "-DSQLITE_DEFAULT_MMAP_SIZE=0",
    "-DSQLITE_CORE",
    "-DSQLITE_TEMP_STORE=3",
    "-DSQLITE_OMIT_LOAD_EXTENSION",
    "-DSQLITE_OMIT_RANDOMNESS",
] + PERFETTO_CONFIG.deps_copts.sqlite

cc_library(
    name = "sqlite",
    srcs = [
        "sqlite3.c",
        "sqlite3.h",
    ],
    hdrs = [":headers"],
    copts = sqlite_copts,
    includes = include_sqlite,
    visibility = ["//visibility:public"],
)

# ################################
# @perfetto_dep_sqlite_src section
# ################################

cc_library(
    name = "percentile_ext",
    srcs = [
        "ext/misc/percentile.c",
    ],
    copts = sqlite_copts,
    deps = PERFETTO_CONFIG.deps.sqlite,
    visibility = ["//visibility:public"],
)
