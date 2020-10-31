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

#include "src/trace_processor/importers/gzip/gzip_trace_parser.h"

#include <string>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/string_utils.h"
#include "perfetto/ext/base/string_view.h"
#include "src/trace_processor/forwarding_trace_parser.h"

namespace perfetto {
namespace trace_processor {

GzipTraceParser::GzipTraceParser(TraceProcessorContext* context)
    : context_(context) {}

GzipTraceParser::~GzipTraceParser() = default;

util::Status GzipTraceParser::Parse(std::unique_ptr<uint8_t[]> data,
                                    size_t size) {
  uint8_t* start = data.get();
  size_t len = size;

  if (!inner_) {
    inner_.reset(new ForwardingTraceParser(context_));

    // .ctrace files begin with: "TRACE:\n" or "done. TRACE:\n" strip this if
    // present.
    base::StringView beginning(reinterpret_cast<char*>(start), size);

    static const char* kSystraceFileHeader = "TRACE:\n";
    size_t offset = Find(kSystraceFileHeader, beginning);
    if (offset != std::string::npos) {
      start += strlen(kSystraceFileHeader) + offset;
      len -= strlen(kSystraceFileHeader) + offset;
    }
  }
  decompressor_.SetInput(start, len);

  // Our default uncompressed buffer size is 32MB as it allows for good
  // throughput.
  using ResultCode = GzipDecompressor::ResultCode;
  constexpr size_t kUncompressedBufferSize = 32 * 1024 * 1024;

  for (auto ret = ResultCode::kOk; ret != ResultCode::kEof;) {
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[kUncompressedBufferSize]);
    auto result =
        decompressor_.Decompress(buffer.get(), kUncompressedBufferSize);
    ret = result.ret;
    if (ret == ResultCode::kError || ret == ResultCode::kNoProgress)
      return util::ErrStatus("Unable to decompress gzip/ctrace trace");
    if (ret == ResultCode::kNeedsMoreInput)
      break;

    util::Status status =
        inner_->Parse(std::move(buffer), result.bytes_written);
    if (!status.ok())
      return status;
  }
  return util::OkStatus();
}

void GzipTraceParser::NotifyEndOfFile() {}

}  // namespace trace_processor
}  // namespace perfetto
