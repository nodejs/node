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

# This synthetic trace tests a process which forks and then execs.

from os import sys, path

sys.path.append(path.dirname(path.dirname(path.abspath(__file__))))
from synth_common import CLONE_THREAD
import synth_common

trace = synth_common.create_trace()

# Create a parent process which  will be forked below.
trace.add_process_tree_packet(ts=1)
trace.add_process(10, 0, "parent")

# Fork off the new process and then kill it 5ns later.
trace.add_ftrace_packet(0)
trace.add_newtask(ts=15, tid=10, new_tid=11, new_comm='child', flags=0)
trace.add_sched(ts=16, prev_pid=10, next_pid=11, next_comm='child')

# Create a parent process which  will be forked below.
trace.add_process_tree_packet(ts=20)
trace.add_process(11, 0, "child_process")

trace.add_ftrace_packet(0)
trace.add_rename(
    ts=25, tid=11, old_comm='child', new_comm='true_name', oom_score_adj=1000)

# Create a parent process which  will be forked below.
trace.add_process_tree_packet(ts=30)
trace.add_process(11, 10, "true_process_name")

print(trace.trace.SerializeToString())
