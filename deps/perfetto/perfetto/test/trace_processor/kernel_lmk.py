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
trace.add_process(pid=1, ppid=0, cmdline="init")
trace.add_process(pid=2, ppid=1, cmdline="two_thread_process")
trace.add_process(pid=4, ppid=1, cmdline="single_thread_process")

trace.add_process(pid=1000, ppid=1, cmdline="padding_process")
trace.add_thread(tid=1301, tgid=1000, cmdline="padding_process_thread_1")
trace.add_thread(tid=1302, tgid=1000, cmdline="padding_process_thread_2")

trace.add_thread(tid=300, tgid=2, cmdline="two_thread_process")

trace.add_ftrace_packet(0)
trace.add_kernel_lmk(ts=100, tid=300)
trace.add_kernel_lmk(ts=101, tid=4)

print(trace.trace.SerializeToString())
