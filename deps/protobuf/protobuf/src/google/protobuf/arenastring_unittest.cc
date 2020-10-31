// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Based on mvels@'s frankenstring.

#include <google/protobuf/arenastring.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <gtest/gtest.h>


namespace google {
namespace protobuf {

using internal::ArenaStringPtr;


static std::string WrapString(const char* value) { return value; }

// Test ArenaStringPtr with arena == NULL.
TEST(ArenaStringPtrTest, ArenaStringPtrOnHeap) {
  ArenaStringPtr field;
  std::string default_value = "default";
  field.UnsafeSetDefault(&default_value);
  EXPECT_EQ(std::string("default"), field.Get());
  field.Set(&default_value, WrapString("Test short"), NULL);
  EXPECT_EQ(std::string("Test short"), field.Get());
  field.Set(&default_value, WrapString("Test long long long long value"), NULL);
  EXPECT_EQ(std::string("Test long long long long value"), field.Get());
  field.Set(&default_value, std::string(""), NULL);
  field.Destroy(&default_value, NULL);

  ArenaStringPtr field2;
  field2.UnsafeSetDefault(&default_value);
  std::string* mut = field2.Mutable(&default_value, NULL);
  EXPECT_EQ(mut, field2.Mutable(&default_value, NULL));
  EXPECT_EQ(mut, &field2.Get());
  EXPECT_NE(&default_value, mut);
  EXPECT_EQ(std::string("default"), *mut);
  *mut = "Test long long long long value";  // ensure string allocates storage
  EXPECT_EQ(std::string("Test long long long long value"), field2.Get());
  field2.Destroy(&default_value, NULL);
}

TEST(ArenaStringPtrTest, ArenaStringPtrOnArena) {
  Arena arena;
  ArenaStringPtr field;
  std::string default_value = "default";
  field.UnsafeSetDefault(&default_value);
  EXPECT_EQ(std::string("default"), field.Get());
  field.Set(&default_value, WrapString("Test short"), &arena);
  EXPECT_EQ(std::string("Test short"), field.Get());
  field.Set(&default_value, WrapString("Test long long long long value"),
            &arena);
  EXPECT_EQ(std::string("Test long long long long value"), field.Get());
  field.Set(&default_value, std::string(""), &arena);
  field.Destroy(&default_value, &arena);

  ArenaStringPtr field2;
  field2.UnsafeSetDefault(&default_value);
  std::string* mut = field2.Mutable(&default_value, &arena);
  EXPECT_EQ(mut, field2.Mutable(&default_value, &arena));
  EXPECT_EQ(mut, &field2.Get());
  EXPECT_NE(&default_value, mut);
  EXPECT_EQ(std::string("default"), *mut);
  *mut = "Test long long long long value";  // ensure string allocates storage
  EXPECT_EQ(std::string("Test long long long long value"), field2.Get());
  field2.Destroy(&default_value, &arena);
}

TEST(ArenaStringPtrTest, ArenaStringPtrOnArenaNoSSO) {
  Arena arena;
  ArenaStringPtr field;
  std::string default_value = "default";
  field.UnsafeSetDefault(&default_value);
  EXPECT_EQ(std::string("default"), field.Get());

  // Avoid triggering the SSO optimization by setting the string to something
  // larger than the internal buffer.
  field.Set(&default_value, WrapString("Test long long long long value"),
            &arena);
  EXPECT_EQ(std::string("Test long long long long value"), field.Get());
  field.Set(&default_value, std::string(""), &arena);
  field.Destroy(&default_value, &arena);

  ArenaStringPtr field2;
  field2.UnsafeSetDefault(&default_value);
  std::string* mut = field2.Mutable(&default_value, &arena);
  EXPECT_EQ(mut, field2.Mutable(&default_value, &arena));
  EXPECT_EQ(mut, &field2.Get());
  EXPECT_NE(&default_value, mut);
  EXPECT_EQ(std::string("default"), *mut);
  *mut = "Test long long long long value";  // ensure string allocates storage
  EXPECT_EQ(std::string("Test long long long long value"), field2.Get());
  field2.Destroy(&default_value, &arena);
}


}  // namespace protobuf
}  // namespace google
