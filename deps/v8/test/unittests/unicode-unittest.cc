// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "src/unicode-decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

namespace {

using Utf8Decoder = unibrow::Utf8Decoder<512>;

void Decode(Utf8Decoder* decoder, const std::string& str) {
  // Put the string in its own buffer on the heap to make sure that
  // AddressSanitizer's heap-buffer-overflow logic can see what's going on.
  std::unique_ptr<char[]> buffer(new char[str.length()]);
  memcpy(buffer.get(), str.data(), str.length());
  decoder->Reset(buffer.get(), str.length());
}

}  // namespace

TEST(UnicodeTest, ReadOffEndOfUtf8String) {
  Utf8Decoder decoder;

  // Not enough continuation bytes before string ends.
  Decode(&decoder, "\xE0");
  Decode(&decoder, "\xED");
  Decode(&decoder, "\xF0");
  Decode(&decoder, "\xF4");
}

}  // namespace internal
}  // namespace v8
