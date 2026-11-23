// Copyright 2023 Datadog, Inc
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Modified for inclusion in Node.js core: the sample attribution logic of
// WallProfiler::GetContextsByNode() from
// https://github.com/DataDog/pprof-nodejs/blob/3905f382723fc2554284e1abbba2df0f2dc6f82b/bindings/profilers/wall.cc
// rewritten as a function that does not depend on V8.
//
// SPDX-License-Identifier: Apache-2.0

#include "pprof/time_profile.h"

namespace node {
namespace pprof {

AttributionByNode AttributeSamples(const std::vector<ProfileSample>& samples,
                                   const std::vector<SampleRecord>& records,
                                   int64_t start_cpu_time) {
  AttributionByNode result;
  const size_t sample_count = samples.size();
  if (records.empty() || sample_count == 0) return result;

  auto record = records.begin();
  int64_t last_cpu_time = start_cpu_time;
  // Offset of the sample to process relative to the iteration index, to
  // handle out-of-order samples.
  int delta = 0;

  // Skip the first sample, which V8 takes when profiling starts, outside of
  // the signal handler.
  for (size_t i = 1; i < sample_count; i++) {
    if (delta == 1) {
      // The previous iteration processed the next sample, so process the
      // previous one now.
      delta = -1;
    } else if (delta == -1) {
      // Back to the normal order.
      delta = 0;
    } else if (i < sample_count - 1 &&
               samples[i + 1].timestamp < samples[i].timestamp) {
      // Out-of-order sample: process the next one first. The hypothesis is
      // that at most two consecutive samples are out of order.
      delta = 1;
    }

    const ProfileSample& sample = samples[i + delta];

    // Drop the records that are too old to match this sample. A sample
    // matches the record whose [time_from, time_to] interval contains its
    // timestamp.
    while (record != records.end()) {
      if (record->time_to < sample.timestamp) {
        ++record;
      } else if (record->time_from > sample.timestamp) {
        // Too recent: try to match it with the next sample.
        break;
      } else {
        NodeAttribution& attribution = result[sample.node];
        attribution.hit_count++;
        // The CPU time of a `(program)` sample goes to the next sample.
        if (!sample.is_program) {
          attribution.cpu_time += record->cpu_time - last_cpu_time;
          last_cpu_time = record->cpu_time;
        }
        // Match at most one record with each sample.
        ++record;
        break;
      }
    }
  }

  return result;
}

int64_t Midpoint(int64_t x, int64_t y) {
  static_assert((-1L >> 1) == -1L, "Right shift is not arithmetic");
  return ((x ^ y) >> 1) + (x & y);
}

}  // namespace pprof
}  // namespace node
