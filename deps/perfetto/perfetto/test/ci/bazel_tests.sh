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

bazel build //:all --verbose_failures

# Smoke test that processes run without crashing.
./bazel-bin/traced &
./bazel-bin/traced_probes &
sleep 5
TRACE=/ci/artifacts/bazel.trace
./bazel-bin/perfetto -c :test -o $TRACE
kill $(jobs -p)
./bazel-bin/trace_processor_shell -q <(echo 'select count(1) from sched') $TRACE

# Check the amalgamated build here to avoid slowing down all the Linux bots.
echo -e "\n\n***** Testing amalgamated build *****\n"
tools/test_gen_amalgamated.py
