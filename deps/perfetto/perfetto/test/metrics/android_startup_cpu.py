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
trace.add_system_info(fingerprint="fingerprint/walleye/P")

trace.add_ftrace_packet(cpu=0)

# CPU counters for CPU 0.
trace.add_cpufreq(ts=9 * 1000000, freq=500000, cpu=0)
trace.add_cpufreq(ts=15 * 1000000, freq=1400000, cpu=0)
trace.add_cpufreq(ts=17 * 1000000, freq=2500000, cpu=0)

# CPU counters for CPU 6.
trace.add_cpufreq(ts=11 * 1000000, freq=2000000, cpu=6)
trace.add_cpufreq(ts=15 * 1000000, freq=8000000, cpu=6)

# Add 3 processes. This also adds one main thread per process.
trace.add_process_tree_packet()
trace.add_process(pid=1, ppid=0, cmdline="Process1")
trace.add_process(pid=2, ppid=0, cmdline="Process2")
trace.add_process(pid=3, ppid=0, cmdline="Process3")

# Add 3 additional threads.
trace.add_thread(tid=4, tgid=1, cmdline="p1-t2")
trace.add_thread(tid=5, tgid=2, cmdline="p2-t2")
trace.add_thread(tid=6, tgid=2, cmdline="p2-t3")

# Schedule threads in CPU 0.
trace.add_ftrace_packet(cpu=0)
trace.add_sched(ts=10 * 1000000, prev_pid=0, next_pid=1)
trace.add_sched(ts=12 * 1000000, prev_pid=1, next_pid=3)
trace.add_sched(ts=16 * 1000000, prev_pid=3, next_pid=4)
trace.add_sched(ts=17 * 1000000, prev_pid=4, next_pid=2)
trace.add_sched(ts=19 * 1000000, prev_pid=2, next_pid=0)

# Schedule threads in CPU 6.
trace.add_ftrace_packet(cpu=6)
trace.add_sched(ts=11 * 1000000, prev_pid=0, next_pid=5)
trace.add_sched(ts=13 * 1000000, prev_pid=5, next_pid=6)
trace.add_sched(ts=16 * 1000000, prev_pid=6, next_pid=3)
trace.add_sched(ts=18 * 1000000, prev_pid=3, next_pid=0)

print(trace.trace.SerializeToString())
