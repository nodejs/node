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

#include "src/trace_processor/importers/gzip/gzip_utils.h"

// For bazel build.
#include "perfetto/base/build_config.h"

#if PERFETTO_BUILDFLAG(PERFETTO_ZLIB)
#include <zlib.h>
#else
struct z_stream_s {};
#endif

namespace perfetto {
namespace trace_processor {
namespace gzip {

bool IsGzipSupported() {
#if PERFETTO_BUILDFLAG(PERFETTO_ZLIB)
  return true;
#else
  return false;
#endif
}

}  // namespace gzip

#if PERFETTO_BUILDFLAG(PERFETTO_ZLIB)
GzipDecompressor::GzipDecompressor() : z_stream_(new z_stream()) {
  z_stream_->zalloc = Z_NULL;
  z_stream_->zfree = Z_NULL;
  z_stream_->opaque = Z_NULL;
  inflateInit2(z_stream_.get(), 32 + 15);
}
#else
GzipDecompressor::GzipDecompressor() = default;
#endif

GzipDecompressor::~GzipDecompressor() {
#if PERFETTO_BUILDFLAG(PERFETTO_ZLIB)
  // Ensure the call to inflateEnd to prevent leaks of internal state.
  inflateEnd(z_stream_.get());
#endif
}

void GzipDecompressor::Reset() {
#if PERFETTO_BUILDFLAG(PERFETTO_ZLIB)
  inflateReset(z_stream_.get());
#endif
}

void GzipDecompressor::SetInput(const uint8_t* data, size_t size) {
#if PERFETTO_BUILDFLAG(PERFETTO_ZLIB)
  // This const_cast is not harmfull as zlib will not modify the data in this
  // pointer. This is only necessary because of the build flags we use to be
  // compatible with other embedders.
  z_stream_->next_in = const_cast<uint8_t*>(data);
  z_stream_->avail_in = static_cast<uInt>(size);
#endif
}

GzipDecompressor::Result GzipDecompressor::Decompress(uint8_t* out,
                                                      size_t out_size) {
#if PERFETTO_BUILDFLAG(PERFETTO_ZLIB)
  if (z_stream_->avail_in == 0)
    return Result{ResultCode::kNeedsMoreInput, 0};

  z_stream_->next_out = out;
  z_stream_->avail_out = static_cast<uInt>(out_size);

  int ret = inflate(z_stream_.get(), Z_NO_FLUSH);
  switch (ret) {
    case Z_NEED_DICT:
    case Z_DATA_ERROR:
    case Z_MEM_ERROR:
      // Ignore inflateEnd error as we will error out anyway.
      inflateEnd(z_stream_.get());
      return Result{ResultCode::kError, 0};
    case Z_STREAM_END:
      return Result{ResultCode::kEof, out_size - z_stream_->avail_out};
    case Z_BUF_ERROR:
      return Result{ResultCode::kNoProgress, 0};
    default:
      return Result{ResultCode::kOk, out_size - z_stream_->avail_out};
  }
#else
  return Result{ResultCode::kError, 0};
#endif
}

}  // namespace trace_processor
}  // namespace perfetto
