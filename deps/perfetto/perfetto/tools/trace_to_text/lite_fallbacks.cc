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

// This file is used when targeting the protobuf-lite only target. It provides
// fallback implementations for TraceToText which simply return an error
// message.

#include "perfetto/base/logging.h"
#include "tools/trace_to_text/trace_to_text.h"

namespace perfetto {
namespace trace_to_text {

int TraceToText(std::istream*, std::ostream*) {
  PERFETTO_FATAL(
      "The 'text' command is not available in lite builds of trace_to_text");
}

}  // namespace trace_to_text
}  // namespace perfetto
