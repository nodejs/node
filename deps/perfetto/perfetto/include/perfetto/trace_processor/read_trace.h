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

#ifndef INCLUDE_PERFETTO_TRACE_PROCESSOR_READ_TRACE_H_
#define INCLUDE_PERFETTO_TRACE_PROCESSOR_READ_TRACE_H_

#include <functional>
#include <vector>

#include "perfetto/base/export.h"
#include "perfetto/trace_processor/status.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessor;

util::Status PERFETTO_EXPORT ReadTrace(
    TraceProcessor* tp,
    const char* filename,
    const std::function<void(uint64_t parsed_size)>& progress_callback = {});

util::Status PERFETTO_EXPORT DecompressTrace(const uint8_t* data,
                                             size_t size,
                                             std::vector<uint8_t>* output);

}  // namespace trace_processor
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACE_PROCESSOR_READ_TRACE_H_
