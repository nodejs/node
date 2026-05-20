// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <fuzzer/FuzzedDataProvider.h>

#include "zlib.h"

// Fuzzer builds often have NDEBUG set, so roll our own assert macro.
#define ASSERT(cond)                                                           \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "%s:%d Assert failed: %s\n", __FILE__, __LINE__, #cond); \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

static void chunked_inflate(gz_header* header,
                            uint8_t* data,
                            size_t size,
                            size_t in_chunk_size,
                            size_t out_chunk_size) {
  z_stream stream;
  stream.next_in = data;
  stream.avail_in = 0;
  stream.zalloc = Z_NULL;
  stream.zfree = Z_NULL;

  static const int kDefaultWindowBits = MAX_WBITS;
  static const int kGzipOrZlibHeader = 32;
  ASSERT(inflateInit2(&stream, kDefaultWindowBits + kGzipOrZlibHeader) == Z_OK);
  ASSERT(inflateGetHeader(&stream, header) == Z_OK);

  auto out_buffer = std::make_unique<uint8_t[]>(out_chunk_size);
  while (true) {
    stream.next_in = &data[stream.total_in];
    stream.avail_in =
        std::min(in_chunk_size, size - static_cast<size_t>(stream.total_in));
    stream.next_out = out_buffer.get();
    stream.avail_out = out_chunk_size;

    if (inflate(&stream, stream.avail_in == 0 ? Z_SYNC_FLUSH : Z_NO_FLUSH) !=
        Z_OK) {
      break;
    }
  }

  inflateEnd(&stream);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size > 250 * 1024) {
    // Cap the input size so the fuzzer doesn't time out. For example,
    // crbug.com/1362206 saw timeouts with 450 KB input, so use a limit that's
    // well below that but still large enough to hit most code.
    return 0;
  }

  FuzzedDataProvider fdp(data, size);

  // Fuzz zlib's inflate() with inflateGetHeader() enabled, various sizes for
  // the gz_header field sizes, and various-sized chunks for input/output. This
  // would have found CVE-2022-37434 which was a heap buffer read overflow when
  // filling in gz_header's extra field.

  gz_header header;
  header.extra_max = fdp.ConsumeIntegralInRange(0, 100000);
  header.name_max = fdp.ConsumeIntegralInRange(0, 100000);
  header.comm_max = fdp.ConsumeIntegralInRange(0, 100000);

  auto extra_buf = std::make_unique<uint8_t[]>(header.extra_max);
  auto name_buf = std::make_unique<uint8_t[]>(header.name_max);
  auto comment_buf = std::make_unique<uint8_t[]>(header.comm_max);

  header.extra = extra_buf.get();
  header.name = name_buf.get();
  header.comment = comment_buf.get();

  size_t in_chunk_size = fdp.ConsumeIntegralInRange(1, 4097);
  size_t out_chunk_size = fdp.ConsumeIntegralInRange(1, 4097);
  std::vector<uint8_t> remaining_data = fdp.ConsumeRemainingBytes<uint8_t>();

  chunked_inflate(&header, remaining_data.data(), remaining_data.size(),
                  in_chunk_size, out_chunk_size);

  return 0;
}
