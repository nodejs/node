/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef TOOLS_TRACE_TO_TEXT_TRACE_TO_SYSTRACE_H_
#define TOOLS_TRACE_TO_TEXT_TRACE_TO_SYSTRACE_H_

#include <iostream>

namespace perfetto {

namespace trace_processor {
class TraceProcessor;
}  // namespace trace_processor

namespace trace_to_text {

class TraceWriter;

enum class Keep { kStart = 0, kEnd, kAll };

int TraceToSystrace(std::istream* input,
                    std::ostream* output,
                    bool ctrace,
                    Keep truncate_keep,
                    bool full_sort);

int ExtractSystrace(trace_processor::TraceProcessor*,
                    TraceWriter*,
                    bool wrapped_in_json,
                    Keep truncate_keep);

}  // namespace trace_to_text
}  // namespace perfetto

#endif  // TOOLS_TRACE_TO_TEXT_TRACE_TO_SYSTRACE_H_
