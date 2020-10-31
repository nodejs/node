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

#ifndef SRC_TRACE_PROCESSOR_TRACE_PARSER_H_
#define SRC_TRACE_PROCESSOR_TRACE_PARSER_H_

#include <stdint.h>

#include "src/trace_processor/timestamped_trace_piece.h"

namespace perfetto {
namespace trace_processor {

class TraceParser {
 public:
  virtual ~TraceParser();

  virtual void ParseTracePacket(int64_t timestamp, TimestampedTracePiece) = 0;
  virtual void ParseFtracePacket(uint32_t cpu,
                                 int64_t timestamp,
                                 TimestampedTracePiece) = 0;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TRACE_PARSER_H_
