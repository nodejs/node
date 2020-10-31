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
trace.add_process(1, 0, 'init')
trace.add_process(2, 1, 'system_server')
trace.add_process(3, 1, 'com.google.android.calendar', 10001)
trace.add_process(4, 1, 'com.google.android.calendar')

trace.add_package_list(
    ts=1, name='com.google.android.calendar', uid=10001, version_code=123)

trace.add_ftrace_packet(cpu=0)
# Intent without any corresponding end state, will be ignored
trace.add_atrace_begin(
    ts=100, tid=2, pid=2, buf='MetricsLogger:launchObserverNotifyIntentStarted')
trace.add_atrace_end(ts=101, tid=2, pid=2)

# Start intent for a successful launch of calendar
trace.add_atrace_begin(
    ts=102, tid=2, pid=2, buf='MetricsLogger:launchObserverNotifyIntentStarted')
trace.add_atrace_end(ts=103, tid=2, pid=2)

trace.add_atrace_async_begin(
    ts=110, tid=2, pid=2, buf='launching: com.google.android.calendar')

trace.add_sched(ts=110, prev_pid=0, next_pid=3)
# P1: 10ns running
trace.add_sched(ts=120, prev_pid=3, next_pid=0, prev_state='S')
# P1: 10ns sleep
trace.add_sched(ts=130, prev_pid=0, next_pid=3)

trace.add_sched(ts=130, prev_pid=3, next_pid=4)

# Create an unrelated task
trace.add_newtask(ts=155, tid=1, new_tid=5, new_comm='', flags=0)

# P2: 30ns running
trace.add_sched(ts=160, prev_pid=4, next_pid=0, prev_state='R')
# P2: 49ns runnable
trace.add_sched(ts=209, prev_pid=0, next_pid=4)
# P2: 1ns running
trace.add_sched(ts=210, prev_pid=4, next_pid=0)

trace.add_atrace_async_end(
    ts=210, tid=2, pid=2, buf='launching: com.google.android.calendar')
trace.add_atrace_begin(
    ts=211,
    tid=2,
    pid=2,
    buf='MetricsLogger:launchObserverNotifyActivityLaunchFinished')
trace.add_atrace_end(ts=212, tid=2, pid=2)

# Start intent for calendar, we failed to launch the activity.
trace.add_atrace_begin(
    ts=402, tid=2, pid=2, buf='MetricsLogger:launchObserverNotifyIntentStarted')
trace.add_atrace_end(ts=403, tid=2, pid=2)

trace.add_atrace_async_begin(
    ts=410, tid=2, pid=2, buf='launching: com.google.android.calendar')

trace.add_atrace_async_end(
    ts=510,
    tid=2,
    pid=2,
    buf='launching: com.google.android.apps.nexuslauncher')

trace.add_ftrace_packet(cpu=1)
trace.add_sched(ts=160, prev_pid=0, next_pid=1)
trace.add_sched(ts=200, prev_pid=1, next_pid=0)

print(trace.trace.SerializeToString())
