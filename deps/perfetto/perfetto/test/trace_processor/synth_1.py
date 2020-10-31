#!/usr/bin/python
# Copyright (C) 2018 The Android Open Source Project
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
trace.add_process_tree_packet()
trace.add_process(1, 0, "init")
trace.add_process(2, 1, "two_thread_process")
trace.add_process(4, 1, "single_thread_process")
trace.add_thread(3, 2, "two_thread_process")

trace.add_ftrace_packet(cpu=0)
trace.add_sched(ts=1, prev_pid=1, next_pid=3)
trace.add_cpufreq(ts=50, freq=50, cpu=0)
trace.add_sched(ts=100, prev_pid=3, next_pid=2)
trace.add_sched(ts=115, prev_pid=2, next_pid=3)

trace.add_ftrace_packet(cpu=1)
trace.add_sched(ts=50, prev_pid=4, next_pid=1)
trace.add_sched(ts=120, prev_pid=1, next_pid=2)
trace.add_sched(ts=170, prev_pid=2, next_pid=0)
trace.add_sched(ts=250, prev_pid=0, next_pid=2)
trace.add_sched(ts=390, prev_pid=2, next_pid=4)
trace.add_cpufreq(ts=400, freq=100, cpu=0)

print(trace.trace.SerializeToString())
