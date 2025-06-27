// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <cassert>
#include <vector>

#include "zlib.h"

static Bytef buffer[256 * 1024] = {0};

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // We need to strip the 'const' for zlib
  std::vector<unsigned char> input_buffer{data, data+size};

  uLongf buffer_length = static_cast<uLongf>(sizeof(buffer));

  z_stream stream;
  stream.next_in = input_buffer.data();
  stream.avail_in = size;
  stream.total_in = size;
  stream.next_out = buffer;
  stream.avail_out = buffer_length;
  stream.total_out = buffer_length;
  stream.zalloc = Z_NULL;
  stream.zfree = Z_NULL;

  if (Z_OK != inflateInit(&stream)) {
    inflateEnd(&stream);
    assert(false);
  }

  inflate(&stream, Z_NO_FLUSH);
  inflateEnd(&stream);

  return 0;
}
