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

# This synthetic trace tests the clock-sync logic. It synthesizes a trace with
# (i) builtin clocks, (ii) custom global clocks, (iii) sequence-scoped clocks.
# It uses gpu counters because that is a quite simple packet and doesn't have
# special treatement. We can't use ftrace because ftrace events use nested
# per-event timestamps and they are assumed to be in the CLOCK_MONOTONIC
# domains regardless of the TracePacket's timestamp_clock_id.

from os import sys, path

sys.path.append(path.dirname(path.dirname(path.abspath(__file__))))
import synth_common
from synth_common import CLONE_THREAD

# Clock IDs in the range [64, 128) are sequence-scoped. See comments in
# clock_snapshot.proto.
CLOCK_MONOTONIC = 3  # Builtin clock, see clock_snapshot.proto.
CLOCK_BOOTTIME = 6  # Builtin clock, see clock_snapshot.proto.
GLOBAL_CLK1 = 128
GLOBAL_CLK2 = 129
SEQ_CLOCK1 = 64

trace = synth_common.create_trace()

# The default trace clock domain is CLOCK_BOOTTIME.
trace.add_gpu_counter(ts=1, counter_id=42, value=3)

# Emit a ClockSnapshot that sets BOOTTIME = MONOTONIC + 100.
trace.add_clock_snapshot(clocks={CLOCK_MONOTONIC: 1, CLOCK_BOOTTIME: 101})

# Emit a counter synced against the global built-in clock CLOCK_MONOTONIC.
# This should be translated, at import time, to BOOTTIME = 2 + 100 = 102.
trace.add_gpu_counter(ts=2, clock_id=CLOCK_MONOTONIC, counter_id=42, value=5)

# Use two global custom clocks. We sync them as follows:
# BOOTTIME = GLOBAL_CLK1 + 1000
# GLOBAL_CLK1 = GLOBAL_CLK2 + 1
# Hence, recursively:
# BOOTTIME = GLOBAL_CLK2 + 1000 + 1
trace.add_clock_snapshot(clocks={GLOBAL_CLK1: 1, CLOCK_BOOTTIME: 1001})
trace.add_clock_snapshot(clocks={GLOBAL_CLK1: 2, GLOBAL_CLK2: 1})

# This counter should be translated, at import time, to BOOTTIME = 3 + 1000
trace.add_gpu_counter(ts=3, clock_id=GLOBAL_CLK1, counter_id=42, value=7)

# This one instead to BOOTTIME = 4 + 1000 + 1 = 1005
trace.add_gpu_counter(ts=4, clock_id=GLOBAL_CLK2, counter_id=42, value=9)

# Use a sequence-scoped clock on two differents sequences.
# On seq 2, BOOTTIME = SEQ_CLOCK1 + 2000
# On seq 3, BOOTTIME = SEQ_CLOCK1 + 3000
trace.add_clock_snapshot(seq_id=2, clocks={SEQ_CLOCK1: 1, CLOCK_BOOTTIME: 2001})
trace.add_clock_snapshot(seq_id=3, clocks={SEQ_CLOCK1: 1, CLOCK_BOOTTIME: 3001})

# This counter should be translated @ BOOTTIME : 3000 + 7
trace.add_gpu_counter(
    ts=7, clock_id=SEQ_CLOCK1, counter_id=42, value=14, seq_id=3)

# This counter should be translated @ BOOTTIME : 2000 + 6
trace.add_gpu_counter(
    ts=6, clock_id=SEQ_CLOCK1, seq_id=2, counter_id=42, value=11)

# Set default clock for sequence 2.
defaults_packet = trace.add_packet()
defaults_packet.trusted_packet_sequence_id = 2
defaults_packet.trace_packet_defaults.timestamp_clock_id = SEQ_CLOCK1

# This counter should be translated @ BOOTTIME : 2000 + 10
trace.add_gpu_counter(ts=10, seq_id=2, counter_id=42, value=12)

# Manually specified clock_id overrides the default clock.
trace.add_gpu_counter(
    ts=2013, clock_id=CLOCK_BOOTTIME, seq_id=2, counter_id=42, value=13)

# Other sequence's default clock isn't changed, so this should be in BOOTTIME.
trace.add_gpu_counter(ts=3010, counter_id=42, value=15, seq_id=3)

print(trace.trace.SerializeToString())
