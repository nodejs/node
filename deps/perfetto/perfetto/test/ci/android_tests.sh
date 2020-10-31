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

INSTALL_BUILD_DEPS_ARGS="--android"
source $(dirname ${BASH_SOURCE[0]})/common.sh

# Run the emulator earlier so by the time we build it's booted.
# tools/run_android_test will perform a wait-for-device. This is just an
# optimization to remove bubbles in the pipeline.
tools/run_android_emulator --pid /tmp/emulator.pid -v &

tools/gn gen ${OUT_PATH} --args="${PERFETTO_TEST_GN_ARGS}" --check
tools/ninja -C ${OUT_PATH} ${PERFETTO_TEST_NINJA_ARGS}

# run_android_test takes long time to push the test_data files. Do that only
# once and avoid re-pushing for each type of test.
tools/run_android_test -n ${OUT_PATH} perfetto_unittests
tools/run_android_test -n -x ${OUT_PATH} perfetto_integrationtests
tools/run_android_test -n -x --env BENCHMARK_FUNCTIONAL_TEST_ONLY=true \
  ${OUT_PATH} perfetto_benchmarks

test -f /tmp/emulator.pid && kill -9 $(cat /tmp/emulator.pid); true
