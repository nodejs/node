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
# event.

from os import sys, path

sys.path.append(path.dirname(path.dirname(path.abspath(__file__))))
import synth_common

trace = synth_common.create_trace()

trace.add_process_tree_packet(ts=1)
trace.add_process(10, 0, "process")

trace.add_ftrace_packet(0)

# Create a new child process (treated internally as a thread) of kthreadd.
trace.add_newtask(ts=50, tid=2, new_tid=3, new_comm="kthread_child", flags=0)

# Add an event on tid 3 which affects its own rss.
trace.add_rss_stat(ts=90, tid=3, member=0, size=9, mm_id=4321, curr=True)

# Try to add an event for tid 10. However, as we've not seen an event
# with curr == True for tid == 10, this event will be dropped.
trace.add_rss_stat(ts=91, tid=3, member=0, size=900, mm_id=1234, curr=False)

# Add an event for tid 3 from tid 10. This emulates e.g. direct reclaim
# where a process reaches into another process' mm struct.
trace.add_rss_stat(ts=99, tid=10, member=0, size=10, mm_id=4321, curr=False)

# Add an event on tid 10 which affects its own rss.
trace.add_rss_stat(ts=100, tid=10, member=0, size=1000, mm_id=1234, curr=True)

# Add an event on tid 10 from tid 3. This emlates e.g. background reclaim
# where kthreadd is cleaning up the mm struct of another process.
trace.add_rss_stat(ts=101, tid=3, member=0, size=900, mm_id=1234, curr=False)

print(trace.trace.SerializeToString())
