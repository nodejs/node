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

# This synthetic trace is designed to test the process tracking logic. It
# synthesizes a combination of sched_switch, task_newtask, and process tree
# packets (i.e. the result of the userspace /proc/pid scraper).

from os import sys, path
sys.path.append(path.dirname(path.dirname(path.abspath(__file__))))
from synth_common import CLONE_THREAD
import synth_common

trace = synth_common.create_trace()

# Create the first process (pid=10) with three threads(tids=10,11,12).
# In this synthetic trace we will pretend we never saw the start of this threads
# (i.e. the task_newtask event). Process association for these threads will
# leverage only the /proc/pid scraper.
trace.add_ftrace_packet(0)
trace.add_sched(ts=1, prev_pid=0, next_pid=10, next_comm='p1-t0')
trace.add_sched(
    ts=2, prev_pid=10, next_pid=11, prev_comm='p1-t0', next_comm='p1-t1')
trace.add_sched(
    ts=3, prev_pid=11, next_pid=12, prev_comm='p1-t1', next_comm='p1-t2')
trace.add_sched(ts=4, prev_pid=12, next_pid=0, prev_comm='p1-t2')

# In the synthetic /proc/pic scraper packet, we pretend we missed p1-1. At the
# SQL level we should be able to tell that p1-t0 and p1-t2 belong to 'process1'
# but p1-t1 should be left unjoinable.
trace.add_process_tree_packet(ts=5)
trace.add_process(10, 0, "process1", 1001)
trace.add_thread(12, 10, "p1-t2")

# Now create another process (pid=20) with three threads(tids=20,21,22).
# This will be slightly more complicated. We'll pretend that tids=20,21 were
# created before the start of the trace, and 22 is created only later.
trace.add_ftrace_packet(0)
trace.add_sched(ts=10, prev_pid=0, next_pid=20, next_comm='p2-t0')
trace.add_sched(
    ts=11, prev_pid=20, next_pid=21, prev_comm='p2-t0', next_comm='p2-t1')
trace.add_newtask(
    ts=12, tid=21, new_tid=22, new_comm='p2-t2', flags=CLONE_THREAD)
trace.add_sched(
    ts=13, prev_pid=21, next_pid=22, prev_comm='p2-t1', next_comm='p2-t2')
trace.add_sched(ts=14, prev_pid=22, next_pid=0, prev_comm='p2-t2')

# From the process tracker viewpoint we pretend we only scraped tids=20,21.
trace.add_process_tree_packet(ts=15)
trace.add_process(20, 0, "process_2", 1002)
trace.add_thread(21, 20, "p2-t1")

# Finally the very complex case: a third process (pid=30) which spawns threads
# in the following order (notation: A->B means thread A spawns thread B).
# 31->32, 31->33, 32->34.
# Last but not least we spawn a further process (pid=40) which spawns a thread
# which, unluckily, recycles TID=34. We expect a new UTID for TID=34 then.
trace.add_ftrace_packet(0)
trace.add_sched(ts=20, prev_pid=0, next_pid=30, next_comm='p3-t0')
trace.add_sched(
    ts=21, prev_pid=30, next_pid=31, prev_comm='p3-t0', next_comm='p3-t1')
trace.add_newtask(
    ts=22, tid=31, new_tid=32, new_comm='p3-t2', flags=CLONE_THREAD)
trace.add_newtask(
    ts=23, tid=31, new_tid=33, new_comm='p3-t3', flags=CLONE_THREAD)
trace.add_sched(
    ts=24, prev_pid=31, next_pid=32, prev_comm='p3-t1', next_comm='p3-t2')
trace.add_newtask(
    ts=25, tid=32, new_tid=34, new_comm='p3-t4', flags=CLONE_THREAD)
trace.add_process_tree_packet(ts=26)
trace.add_process(30, 0, "process_3")
trace.add_thread(31, 30, "p3-t1")

# This event pretends that TID=32 forks() a new process 40 (note the absence of
# CLONE_THREAD in the add_newtask flags).
trace.add_ftrace_packet(0)
trace.add_newtask(ts=27, tid=32, new_tid=40, new_comm='p4-t0', flags=0)
trace.add_sched(
    ts=28, prev_pid=32, next_pid=40, prev_comm='p3-t2', next_comm='p4-t0')

trace.add_process_tree_packet(ts=29)
trace.add_process(40, 0, "process_4")

# And now, this new process starts a new thread that recycles TID=31 (previously
# used as p3-t1, now becomes p4-t1).
trace.add_ftrace_packet(0)
trace.add_newtask(
    ts=30, tid=40, new_tid=31, new_comm='p4-t1', flags=CLONE_THREAD)
trace.add_sched(
    ts=31, prev_pid=40, next_pid=31, prev_comm='p4-t0', next_comm='p4-t1')

print(trace.trace.SerializeToString())
