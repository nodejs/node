#!/usr/bin/python
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

from os import sys, path
sys.path.append(path.dirname(path.dirname(path.abspath(__file__))))
import synth_common

trace = synth_common.create_trace()

trace.add_ftrace_packet(cpu=0)
trace.add_cpufreq(ts=1000, freq=3000, cpu=1)
trace.add_cpufreq(ts=1001, freq=4000, cpu=1)
trace.add_cpufreq(ts=1003, freq=3000, cpu=1)

print(trace.trace.SerializeToString())
