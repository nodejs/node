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

trace = synth_common.create_trace()
trace.add_process_tree_packet()
trace.add_process(1, 0, "init")
trace.add_process(2, 1, "process_a")
trace.add_process(3, 1, "process_b")

trace.add_ftrace_packet(0)
trace.add_rss_stat(100, 2, file_member, 100)
trace.add_rss_stat(100, 2, anon_member, 200)
trace.add_oom_score_update(100, 1000, 2)
trace.add_rss_stat(105, 2, file_member, 50)
trace.add_oom_score_update(107, -300, 2)
trace.add_rss_stat(110, 2, anon_member, 100)
trace.add_rss_stat(120, 2, anon_member, 75)
trace.add_rss_stat(121, 2, file_member, 95)
trace.add_oom_score_update(122, 300, 2)
trace.add_rss_stat(123, 2, anon_member, 200)
trace.add_rss_stat(124, 2, file_member, 75)
trace.add_rss_stat(125, 2, file_member, 35)
trace.add_oom_score_update(126, 0, 2)
trace.add_rss_stat(126, 2, file_member, 0)
trace.add_rss_stat(126, 2, anon_member, 0)

trace.add_ftrace_packet(0)
trace.add_rss_stat(104, 3, file_member, 400)
trace.add_rss_stat(107, 3, anon_member, 300)
trace.add_oom_score_update(110, 0, 3)
trace.add_rss_stat(113, 3, file_member, 75)
trace.add_rss_stat(114, 3, anon_member, 800)
trace.add_oom_score_update(115, 600, 3)
trace.add_rss_stat(117, 3, file_member, 25)
trace.add_rss_stat(118, 3, file_member, 80)
trace.add_oom_score_update(118, -500, 3)
trace.add_rss_stat(123, 3, file_member, 85)
trace.add_rss_stat(126, 3, anon_member, 600)
trace.add_rss_stat(129, 3, file_member, 90)
trace.add_oom_score_update(130, 200, 3)
trace.add_rss_stat(130, 2, file_member, 0)
trace.add_rss_stat(130, 2, anon_member, 0)

print(trace.trace.SerializeToString())
