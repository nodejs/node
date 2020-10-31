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

#ifndef SRC_PROFILING_SYMBOLIZER_SYMBOLIZE_DATABASE_H_
#define SRC_PROFILING_SYMBOLIZER_SYMBOLIZE_DATABASE_H_

#include "src/profiling/symbolizer/symbolizer.h"

#include <functional>
#include <string>
#include <vector>

namespace perfetto {
namespace trace_processor {
class TraceProcessor;
}
namespace profiling {
std::vector<std::string> GetPerfettoBinaryPath();
// Generate ModuleSymbol protos for all unsymbolized frames in the database.
// Wrap them in proto-encoded TracePackets messages and call callback.
void SymbolizeDatabase(trace_processor::TraceProcessor* tp,
                       Symbolizer* symbolizer,
                       std::function<void(const std::string&)> callback);
}  // namespace profiling
}  // namespace perfetto

#endif  // SRC_PROFILING_SYMBOLIZER_SYMBOLIZE_DATABASE_H_
