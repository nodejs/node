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
trace.add_packet()

# Add Power Rails description for 3 rails.
trace.add_power_rails_desc(1, 'PR_1')
trace.add_power_rails_desc(2, 'PR_2')
trace.add_power_rails_desc(3, 'PR_3')

# Add data at ts = 5 ms.
trace.add_power_rails_data(5, 1, 12)
trace.add_power_rails_data(5, 2, 10)
trace.add_power_rails_data(5, 3, 8)

# Add data at ts = 6 ms.
trace.add_power_rails_data(6, 1, 50)
trace.add_power_rails_data(6, 2, 70)
trace.add_power_rails_data(6, 3, 15)

print(trace.trace.SerializeToString())
