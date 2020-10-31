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
trace.add_gpu_log(ts=1, severity=1, tag="tag0", message="message0")
trace.add_gpu_log(ts=2, severity=2, tag="tag0", message="message1")
trace.add_gpu_log(ts=3, severity=3, tag="tag0", message="message2")
trace.add_gpu_log(ts=4, severity=4, tag="tag0", message="message3")
trace.add_gpu_log(ts=4, severity=5, tag="tag0", message="message4")
trace.add_gpu_log(ts=5, severity=1, tag="tag1", message="message5")

print(trace.trace.SerializeToString())
