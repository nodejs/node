// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/string-builder.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace v8::internal::wasm {
namespace string_builder_unittest {

TEST(StringBuilder, Simple) {
  StringBuilder sb;
  sb << "foo"
     << "bar" << -42 << "\n";
  EXPECT_STREQ(std::string(sb.start(), sb.length()).c_str(), "foobar-42\n");
}

TEST(StringBuilder, DontLeak) {
  // Should be bigger than StringBuilder::kStackSize = 256.
  constexpr size_t kMoreThanStackBufferSize = 300;
  StringBuilder sb;
  const char* on_stack = sb.start();
  sb.allocate(kMoreThanStackBufferSize);
  const char* on_heap = sb.start();
  // If this fails, then kMoreThanStackBufferSize was too small.
  ASSERT_NE(on_stack, on_heap);
  // Still don't leak on further growth.
  sb.allocate(kMoreThanStackBufferSize * 4);
}

TEST(StringBuilder, SuperLongStrings) {
  // Should be bigger than StringBuilder::kChunkSize = 1024 * 1024.
  constexpr size_t kMoreThanChunkSize = 2 * 1024 * 1024;
  StringBuilder sb;
  char* s = sb.allocate(kMoreThanChunkSize);
  for (size_t i = 0; i < kMoreThanChunkSize; i++) {
    s[i] = 'a';
  }
}

}  // namespace string_builder_unittest
}  // namespace v8::internal::wasm
