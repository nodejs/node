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

file_member = 0
anon_member = 1
swap_member = 2

trace = synth_common.create_trace()
trace.add_process_tree_packet()
trace.add_process(1, 0, 'init')
trace.add_process(2, 1, 'system_server')
trace.add_process(3, 1, 'com.google.android.calendar')
trace.add_process(4, 1, 'com.google.android.deskclock')

trace.add_ftrace_packet(cpu=0)
# We are not yet aware of the OOM score. Will be ignored in the breakdowns.
trace.add_rss_stat(100, 3, file_member, 10000)
trace.add_rss_stat(100, 4, file_member, 10000)
trace.add_rss_stat(100, 3, anon_member, 3000)
trace.add_rss_stat(100, 4, anon_member, 6000)
trace.add_rss_stat(100, 3, swap_member, 0)
trace.add_rss_stat(100, 4, swap_member, 0)

# Update the OOM scores.
trace.add_oom_score_update(200, 0, 3)
trace.add_oom_score_update(200, 900, 4)

trace.add_rss_stat(200, 3, anon_member, 2000)
trace.add_rss_stat(200, 4, anon_member, 4000)

trace.add_rss_stat(250, 3, anon_member, 4000)
trace.add_rss_stat(250, 4, anon_member, 8000)

trace.add_oom_score_update(300, 910, 3)
trace.add_oom_score_update(300, 0, 4)

trace.add_rss_stat(300, 3, anon_member, 3000)
trace.add_rss_stat(300, 4, anon_member, 6000)

print(trace.trace.SerializeToString())
