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

INSTALL_BUILD_DEPS_ARGS=""
source $(dirname ${BASH_SOURCE[0]})/common.sh

tools/gn gen ${OUT_PATH} --args="${PERFETTO_TEST_GN_ARGS}" --check
tools/ninja -C ${OUT_PATH} ${PERFETTO_TEST_NINJA_ARGS} fuzzers

# Run a single iteration each to make sure they are not crashing.
for fuzzer in $(find ${OUT_PATH} -name '*_fuzzer' -executable); do
  $fuzzer -runs=1
done
