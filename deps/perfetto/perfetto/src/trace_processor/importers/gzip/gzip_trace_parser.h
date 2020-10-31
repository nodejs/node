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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_GZIP_GZIP_TRACE_PARSER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_GZIP_GZIP_TRACE_PARSER_H_

#include "src/trace_processor/chunked_trace_reader.h"
#include "src/trace_processor/importers/gzip/gzip_utils.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

class GzipTraceParser : public ChunkedTraceReader {
 public:
  explicit GzipTraceParser(TraceProcessorContext*);
  ~GzipTraceParser() override;

  // ChunkedTraceReader implementation
  util::Status Parse(std::unique_ptr<uint8_t[]>, size_t) override;
  void NotifyEndOfFile() override;

 private:
  TraceProcessorContext* const context_;
  GzipDecompressor decompressor_;
  std::unique_ptr<ChunkedTraceReader> inner_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_GZIP_GZIP_TRACE_PARSER_H_
