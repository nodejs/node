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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_JSON_JSON_TRACE_TOKENIZER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_JSON_JSON_TRACE_TOKENIZER_H_

#include <stdint.h>

#include "src/trace_processor/chunked_trace_reader.h"
#include "src/trace_processor/importers/systrace/systrace_line_tokenizer.h"
#include "src/trace_processor/storage/trace_storage.h"

namespace Json {
class Value;
}

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

#if PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
// Visible for testing.
enum class ReadDictRes {
  kFoundDict,
  kNeedsMoreData,
  kEndOfTrace,
  kEndOfArray,
  kFatalError,
};

// Parses at most one JSON dictionary and returns a pointer to the end of it,
// or nullptr if no dict could be detected.
// This is to avoid decoding the full trace in memory and reduce heap traffic.
// E.g.  input:  { a:1 b:{ c:2, d:{ e:3 } } } , { a:4, ... },
//       output: [   only this is parsed    ] ^return value points here.
// Visible for testing.
ReadDictRes ReadOneJsonDict(const char* start,
                            const char* end,
                            Json::Value* value,
                            const char** next);

enum class ReadKeyRes {
  kFoundKey,
  kNeedsMoreData,
  kEndOfDictionary,
  kFatalError,
};

// Parses at most one JSON key and returns a pointer to the start of the value
// associated with that key.
// This is to avoid decoding the full trace in memory and reduce heap traffic.
// E.g. input:  a:1 b:{ c:2}}
//     output:    ^ return value points here, key is set to "a".
// Note: even if the whole key may be available, this method will return
// kNeedsMoreData until the first character of the value is available.
// Visible for testing.
ReadKeyRes ReadOneJsonKey(const char* start,
                          const char* end,
                          std::string* key,
                          const char** next);

enum class ReadSystemLineRes {
  kFoundLine,
  kNeedsMoreData,
  kEndOfSystemTrace,
  kFatalError,
};

ReadSystemLineRes ReadOneSystemTraceLine(const char* start,
                                         const char* end,
                                         std::string* line,
                                         const char** next);
#endif

// Reads a JSON trace in chunks and extracts top level json objects.
class JsonTraceTokenizer : public ChunkedTraceReader {
 public:
  explicit JsonTraceTokenizer(TraceProcessorContext*);
  ~JsonTraceTokenizer() override;

  // ChunkedTraceReader implementation.
  util::Status Parse(std::unique_ptr<uint8_t[]>, size_t) override;
  void NotifyEndOfFile() override;

 private:
  // Enum which tracks which type of JSON trace we are dealing with.
  enum class TraceFormat {
    // Enum value when ther outer-most layer is a dictionary with multiple
    // key value pairs
    kOuterDictionary,

    // Enum value when we only have trace events (i.e. the outermost
    // layer is just a array of trace events).
    kOnlyTraceEvents,
  };

  // Enum which tracks our current position within the trace.
  enum class TracePosition {
    // This indicates that we are inside the outermost dictionary of the
    // trace and need to read the next key of the dictionary.
    // This position is only valid when the |format_| == |kOuterDictionary|.
    kDictionaryKey,

    // This indicates we are inside the systemTraceEvents string.
    // This position is only valid when the |format_| == |kOuterDictionary|.
    kSystemTraceEventsString,

    // This indicates we are waiting for the entire metadata dictionary to be
    // available.
    kWaitingForMetadataDictionary,

    // This indicates where are inside the traceEvents array.
    kTraceEventsArray,

    // This indicates we cannot parse any more data in the trace.
    kEof,
  };

#if PERFETTO_BUILDFLAG(PERFETTO_TP_JSON)
  util::Status ParseInternal(const char* start,
                             const char* end,
                             const char** next);
#endif

  TraceProcessorContext* const context_;

  TraceFormat format_ = TraceFormat::kOuterDictionary;
  TracePosition position_ = TracePosition::kDictionaryKey;

  SystraceLineTokenizer systrace_line_tokenizer_;

  uint64_t offset_ = 0;
  // Used to glue together JSON objects that span across two (or more)
  // Parse boundaries.
  std::vector<char> buffer_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_JSON_JSON_TRACE_TOKENIZER_H_
