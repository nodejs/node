#!/bin/bash
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

set -eux -o pipefail

cd $(dirname ${BASH_SOURCE[0]})/../..
OUT_PATH="out/dist"

if [[ -e buildtools/clang/bin/llvm-symbolizer ]]; then
  export ASAN_SYMBOLIZER_PATH="buildtools/clang/bin/llvm-symbolizer"
  export MSAN_SYMBOLIZER_PATH="buildtools/clang/bin/llvm-symbolizer"
fi

tools/install-build-deps $INSTALL_BUILD_DEPS_ARGS

# Performs checks on generated protos and build files.
tools/gn gen out/tmp.protoc --args="is_debug=false cc_wrapper=\"ccache\""
tools/gen_all --check-only out/tmp.protoc
rm -rf out/tmp.protoc
