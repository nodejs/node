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

from os import sys, path
sys.path.append(path.dirname(path.dirname(path.abspath(__file__))))
import synth_common

trace = synth_common.create_trace()

# kfree:
# field:unsigned long call_site;        offset:8;       size:8; signed:0;
# field:const void * ptr;               offset:16;      size:8; signed:0;

# kmalloc:
# field:unsigned long call_site;        offset:8;       size:8; signed:0;
# field:const void * ptr;               offset:16;      size:8; signed:0;
# field:size_t bytes_req;               offset:24;      size:8; signed:0;
# field:size_t bytes_alloc;             offset:32;      size:8; signed:0;
# field:gfp_t gfp_flags;                offset:40;      size:4; signed:0;

# Without special-casing, we print everything as unsigned decimal.

trace.add_process_tree_packet()
trace.add_process(pid=10, ppid=1, cmdline="perfetto")

trace.add_ftrace_packet(cpu=0)
trace.add_kfree(ts=100, tid=10, call_site=16, ptr=32)
trace.add_kfree(ts=101, tid=10, call_site=(1 << 64) - 16, ptr=(1 << 64) - 32)
trace.add_kmalloc(
    ts=102,
    tid=10,
    bytes_alloc=32,
    bytes_req=16,
    call_site=(1 << 64) - 16,
    gfp_flags=0,
    ptr=(1 << 64) - 32)

print(trace.trace.SerializeToString())
