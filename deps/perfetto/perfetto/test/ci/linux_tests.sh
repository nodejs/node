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
tools/ninja -C ${OUT_PATH} ${PERFETTO_TEST_NINJA_ARGS}

# Run the tests

${OUT_PATH}/perfetto_unittests
${OUT_PATH}/perfetto_integrationtests
${OUT_PATH}/trace_processor_minimal_smoke_tests

# If this is a split host+target build, use the trace_processoer_shell binary
# from the host directory. In some cases (e.g. lsan x86 builds) the host binary
# that is copied into the target directory (OUT_PATH) cannot run because depends
# on libc++.so within the same folder (which is built using target bitness,
# not host bitness).
TP_SHELL=${OUT_PATH}/gcc_like_host/trace_processor_shell
if [ ! -f ${TP_SHELL} ]; then
  TP_SHELL=${OUT_PATH}/trace_processor_shell
fi

mkdir -p /ci/artifacts/perf

tools/diff_test_trace_processor.py \
  --test-type=all \
  --perf-file=/ci/artifacts/perf/tp-perf-all.json \
  ${TP_SHELL}

# Don't run benchmarks under sanitizers or debug, too slow and pointless.
USING_SANITIZER="$(tools/gn args --short --list=using_sanitizer ${OUT_PATH} | awk '{print $3}')"
IS_DEBUG="$(tools/gn args --short --list=is_debug ${OUT_PATH} | awk '{print $3}')"
if [ "$USING_SANITIZER" == "false" ] && [ "$IS_DEBUG" == "false" ]; then
  BENCHMARK_FUNCTIONAL_TEST_ONLY=true ${OUT_PATH}/perfetto_benchmarks
fi
