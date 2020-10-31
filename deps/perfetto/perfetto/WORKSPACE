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

# This file is used only in standalone builds. This file is ignored both in
# embedder builds (i.e. when other projects pull perfetto under /third_party/
# or similar) and in google internal builds.

workspace(name = "perfetto")

new_local_repository(
    name = "perfetto_cfg",
    path = "bazel/standalone",
    build_file_content = ""
)

load("@perfetto//bazel:deps.bzl", "perfetto_deps")
perfetto_deps()

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
protobuf_deps()
