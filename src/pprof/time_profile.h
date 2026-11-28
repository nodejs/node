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

#ifndef SRC_PPROF_TIME_PROFILE_H_
#define SRC_PPROF_TIME_PROFILE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace node {
namespace pprof {

// What the SIGPROF handler records around each sample that V8 takes: the
// interval, in V8 timer microseconds, during which V8's handler ran, and the
// CPU time of the thread, in nanoseconds, when the signal arrived.
struct SampleRecord {
  int64_t time_from;
  int64_t time_to;
  int64_t cpu_time;
};

// A sample of a CpuProfile, as far as attribution is concerned.
struct ProfileSample {
  int64_t timestamp;
  // Identifies the profile node the sample belongs to.
  const void* node;
  // The sample is a `(program)` sample, i.e. no JS was running.
  bool is_program;
};

// The samples of a profile node that matched a SampleRecord, and the thread
// CPU time attributed to them.
struct NodeAttribution {
  uint32_t hit_count = 0;
  int64_t cpu_time = 0;
};

using AttributionByNode = std::unordered_map<const void*, NodeAttribution>;

// Matches each sample of a profile with the record whose interval contains its
// timestamp, and attributes the thread CPU time consumed since the previous
// matched sample to its node.
//
// The first sample is skipped, since V8 takes it when profiling starts rather
// than from the signal handler. Up to two consecutive samples may be out of
// order. Samples without a matching record (e.g. deoptimization samples) are
// not attributed. The CPU time of `(program)` samples is carried over to the
// next sample.
AttributionByNode AttributeSamples(const std::vector<ProfileSample>& samples,
                                   const std::vector<SampleRecord>& records,
                                   int64_t start_cpu_time);

// Returns ((x + y) / 2) rounded towards negative infinity, without overflow.
int64_t Midpoint(int64_t x, int64_t y);

}  // namespace pprof
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_PPROF_TIME_PROFILE_H_
