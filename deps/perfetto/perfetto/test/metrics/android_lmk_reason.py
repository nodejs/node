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

anon_member = 1
swap_member = 2

trace = synth_common.create_trace()

trace.add_process_tree_packet()
trace.add_process(1, 0, 'init')
trace.add_process(2, 1, 'system_server')
trace.add_process(3, 1, 'lmk_victim:no_data:ignored')
trace.add_process(4, 1, 'lmk_victim:no_ion')
trace.add_process(5, 1, 'lmk_victim:with_ion')
trace.add_process(6, 1, 'process')
trace.add_process(7, 1, 'lmk_victim:with_process')
trace.add_process(8, 1, 'app:ui', 10001)
trace.add_process(9, 1, 'lmk_victim:with_app')

trace.add_package_list(ts=1, name="app", uid=10001, version_code=123)
trace.add_package_list(ts=1, name="shared_uid_app", uid=10001, version_code=345)

trace.add_ftrace_packet(cpu=0)
trace.add_kernel_lmk(ts=101, tid=3)

trace.add_ftrace_packet(cpu=0)
trace.add_oom_score_update(ts=201, oom_score_adj=0, pid=4)
trace.add_kernel_lmk(ts=202, tid=4)

trace.add_ftrace_packet(cpu=0)
trace.add_ion_event(ts=301, tid=5, heap_name='system', len=1000)
trace.add_oom_score_update(ts=302, oom_score_adj=100, pid=5)
trace.add_kernel_lmk(ts=303, tid=5)

trace.add_ftrace_packet(cpu=0)
trace.add_oom_score_update(ts=401, oom_score_adj=0, pid=6)
trace.add_oom_score_update(ts=402, oom_score_adj=200, pid=7)
trace.add_rss_stat(ts=403, tid=6, member=anon_member, size=2000)
trace.add_kernel_lmk(ts=404, tid=7)
trace.add_process_free(ts=405, tid=6, comm='', prio=0)

trace.add_ftrace_packet(cpu=0)
trace.add_oom_score_update(ts=501, oom_score_adj=0, pid=8)
trace.add_oom_score_update(ts=502, oom_score_adj=100, pid=9)
trace.add_rss_stat(ts=503, tid=8, member=anon_member, size=2500)
trace.add_rss_stat(ts=503, tid=8, member=swap_member, size=2500)
trace.add_kernel_lmk(ts=504, tid=9)

# Dummy trace event to ensure the trace does not end on an LMK.
trace.add_ftrace_packet(cpu=0)
trace.add_oom_score_update(ts=1001, oom_score_adj=-800, pid=2)

print(trace.trace.SerializeToString())
