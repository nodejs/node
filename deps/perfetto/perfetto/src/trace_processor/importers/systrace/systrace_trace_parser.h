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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_SYSTRACE_SYSTRACE_TRACE_PARSER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_SYSTRACE_SYSTRACE_TRACE_PARSER_H_

#include <deque>
#include <regex>

#include "src/trace_processor/chunked_trace_reader.h"
#include "src/trace_processor/importers/systrace/systrace_line_parser.h"
#include "src/trace_processor/importers/systrace/systrace_line_tokenizer.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/types/trace_processor_context.h"

namespace perfetto {
namespace trace_processor {

class SystraceTraceParser : public ChunkedTraceReader {
 public:
  explicit SystraceTraceParser(TraceProcessorContext*);
  ~SystraceTraceParser() override;

  // ChunkedTraceReader implementation.
  util::Status Parse(std::unique_ptr<uint8_t[]>, size_t size) override;
  void NotifyEndOfFile() override;

 private:
  enum ParseState {
    kBeforeParse,
    kHtmlBeforeSystrace,
    kTraceDataSection,
    kSystrace,
    kProcessDumpLong,
    kProcessDumpShort,
    kEndOfSystrace,
  };

  ParseState state_ = ParseState::kBeforeParse;

  // Used to glue together trace packets that span across two (or more)
  // Parse() boundaries.
  std::deque<uint8_t> partial_buf_;

  SystraceLineTokenizer line_tokenizer_;
  SystraceLineParser line_parser_;
  TraceProcessorContext* ctx_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_SYSTRACE_SYSTRACE_TRACE_PARSER_H_
