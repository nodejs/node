/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_PROFILING_PPROF_BUILDER_H_
#define INCLUDE_PERFETTO_PROFILING_PPROF_BUILDER_H_

#include <iostream>
#include <string>
#include <vector>

namespace perfetto {

namespace trace_processor {
class TraceProcessor;
}

namespace profiling {
class Symbolizer;
}

namespace trace_to_text {

struct SerializedProfile {
  uint64_t pid;
  std::string serialized;
};

bool TraceToPprof(trace_processor::TraceProcessor*,
                  std::vector<SerializedProfile>* output,
                  profiling::Symbolizer* symbolizer,
                  uint64_t pid = 0,
                  const std::vector<uint64_t>& timestamps = {});

bool TraceToPprof(std::istream* input,
                  std::vector<SerializedProfile>* output,
                  profiling::Symbolizer* symbolizer,
                  uint64_t pid = 0,
                  const std::vector<uint64_t>& timestamps = {});

bool TraceToPprof(std::istream* input,
                  std::vector<SerializedProfile>* output,
                  uint64_t pid = 0,
                  const std::vector<uint64_t>& timestamps = {});

}  // namespace trace_to_text
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_PROFILING_PPROF_BUILDER_H_
