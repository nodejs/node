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

# This synthetic trace tests handling of the mm_id field in the rss_stat
# event when mm_structs are reused on process death.

from os import sys, path

sys.path.append(path.dirname(path.dirname(path.abspath(__file__))))
import synth_common

trace = synth_common.create_trace()

trace.add_process_tree_packet(ts=1)
trace.add_process(10, 1, "parent_process")

trace.add_ftrace_packet(1)

# Emit an event for the process.
trace.add_rss_stat(100, tid=10, member=0, size=100, mm_id=0x1234, curr=1)

# Now kill the process.
trace.add_process_free(ts=101, tid=10, comm="parent_process", prio=0)

# Create a new thread which reuses the pid and mm struct.
trace.add_newtask(102, tid=1, new_tid=10, new_comm="new_process", flags=0)

# Emit an event for the new thread.
trace.add_rss_stat(103, tid=10, member=0, size=10, mm_id=0x1234, curr=1)

print(trace.trace.SerializeToString())
