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
trace.add_process(3, 1, 'com.google.android.calendar')

trace.add_ftrace_packet(cpu=0)
trace.add_ion_event(ts=100, tid=3, heap_name='system', len=1000)
trace.add_ion_event(ts=150, tid=3, heap_name='adsp', len=1000)
trace.add_ion_event(ts=200, tid=3, heap_name='system', size=1000, len=1000)
trace.add_ion_event(ts=299, tid=3, heap_name='adsp', size=1000, len=100)

print(trace.trace.SerializeToString())
