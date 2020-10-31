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

# See gpu_counter_descriptor.proto
MILLISECOND = 21
SECOND = 22
VERTEX = 25
PIXEL = 26
TRIANGLE = 27

trace = synth_common.create_trace()

# Add 3 counter specs.
trace.add_gpu_counter_spec(
    ts=1,
    counter_id=31,
    name="Vertex / Second",
    description="Number of vertices per second",
    unit_numerators=[VERTEX],
    unit_denominators=[SECOND])
trace.add_gpu_counter_spec(
    ts=2,
    counter_id=32,
    name="Fragment / Second",
    description="Number of fragments per second",
    unit_numerators=[PIXEL],
    unit_denominators=[SECOND])
trace.add_gpu_counter_spec(
    ts=3,
    counter_id=34,
    name="Triangle Acceleration",
    description="Number of triangles per ms-ms",
    unit_numerators=[TRIANGLE],
    unit_denominators=[MILLISECOND, MILLISECOND])

# Add some counter value events.
trace.add_gpu_counter(11, 31, 5)
trace.add_gpu_counter(21, 31, 10)
trace.add_gpu_counter(31, 31, 15)

trace.add_gpu_counter(12, 32, 7)
trace.add_gpu_counter(22, 32, 14)
trace.add_gpu_counter(32, 32, 21)

# Counter without a spec.
trace.add_gpu_counter(13, 33, 8)
trace.add_gpu_counter(23, 33, 16)
trace.add_gpu_counter(33, 33, 25)

trace.add_gpu_counter(14, 34, 0)
trace.add_gpu_counter(24, 34, 9)
trace.add_gpu_counter(34, 34, 7)

print(trace.trace.SerializeToString())
