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
# event during clone events which have various flag combinations set.

from os import sys, path

sys.path.append(path.dirname(path.dirname(path.abspath(__file__))))
import synth_common

trace = synth_common.create_trace()

trace.add_process_tree_packet(ts=1)
trace.add_process(10, 1, "parent_process")
trace.add_process(3, 2, "kernel_thread")

# In this packet, check what happens to userspace processes with different
# clone flags.
trace.add_ftrace_packet(1)

# Emit an rss stat event for the main thread of the process to associate it
# with an mm_id.
trace.add_rss_stat(100, tid=10, member=0, size=100, mm_id=0x1234, curr=1)

# Create a newtask event emulating vfork/posix_spawn (i.e. CLONE_VM and
# CLONE_VFORK set).
trace.add_newtask(
    101,
    tid=10,
    new_tid=11,
    new_comm="child_process",
    flags=synth_common.CLONE_VFORK | synth_common.CLONE_VM)

# The child process will now change its own (and parent's) VM space with
# |curr| set to 1 (emulating cleaning up some memory in parent).
trace.add_rss_stat(102, tid=11, member=0, size=90, mm_id=0x1234, curr=1)

# At this point, the child process will obtain a new mm struct. From this
# point on, all mm_ids from the child should be different from the parent.

# The child process will now change its parents VM space with curr set to
# 0 (emulating e.g. cleaning up its stack).
trace.add_rss_stat(103, tid=11, member=0, size=85, mm_id=0x1234, curr=0)

# Now the child process should exec another process.

# The child can now change its own memory.
trace.add_rss_stat(104, tid=11, member=0, size=10, mm_id=0x5678, curr=1)

# The parent can now resume execution and may emit another rss event.
trace.add_rss_stat(105, tid=10, member=0, size=95, mm_id=0x1234, curr=1)

# The parent can now go ahead and start a new thread.
trace.add_newtask(
    106,
    tid=10,
    new_tid=12,
    new_comm="parent_thread",
    flags=synth_common.CLONE_VM | synth_common.CLONE_THREAD)

# Since this thread shares mm space with the parent, it should have the
# same mm id and have curr set to 1.
trace.add_rss_stat(107, tid=12, member=0, size=105, mm_id=0x1234, curr=1)

# The parent can also emit events with the same mm struct at the same time.
trace.add_rss_stat(108, tid=10, member=0, size=110, mm_id=0x1234, curr=1)

# In this packet, we check what happens to kernel threads in RSS stat.
trace.add_ftrace_packet(1)

# Emit an rss stat event for the the existing kernel thread.
trace.add_rss_stat(100, tid=3, member=0, size=10, mm_id=0x2345, curr=1)

# Start a new kernel thread.
trace.add_newtask(
    101,
    tid=2,
    new_tid=4,
    new_comm="kernel_thread2",
    flags=synth_common.CLONE_VM)

# Emit a rss stat for the new kernel thread.
trace.add_rss_stat(102, tid=4, member=0, size=20, mm_id=0x2345, curr=1)

print(trace.trace.SerializeToString())
