/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_GZIP_GZIP_UTILS_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_GZIP_GZIP_UTILS_H_

#include <memory>

struct z_stream_s;

namespace perfetto {
namespace trace_processor {

namespace gzip {

// Returns whether gzip related functioanlity is supported with the current
// build flags.
bool IsGzipSupported();

}  // namespace gzip

class GzipDecompressor {
 public:
  enum class ResultCode {
    kOk,
    kEof,
    kError,
    kNoProgress,
    kNeedsMoreInput,
  };
  struct Result {
    // The return code of the decompression.
    ResultCode ret;

    // The amount of bytes written to output.
    // Only valid if |ResultCode::kOk|.
    size_t bytes_written;
  };

  GzipDecompressor();
  ~GzipDecompressor();

  // Sets the input pointer and size of the gzip stream to inflate.
  void SetInput(const uint8_t* data, size_t size);

  // Decompresses the input previously provided in |SetInput|.
  Result Decompress(uint8_t* out, size_t out_size);

  // Sets the state of the decompressor to reuse with other gzip streams.
  void Reset();

 private:
  std::unique_ptr<z_stream_s> z_stream_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_GZIP_GZIP_UTILS_H_
