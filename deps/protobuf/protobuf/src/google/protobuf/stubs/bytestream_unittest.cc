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

#include <google/protobuf/stubs/bytestream.h>

#include <stdio.h>
#include <string.h>
#include <algorithm>

#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace strings {
namespace {

// We use this class instead of ArrayByteSource to simulate a ByteSource that
// contains multiple fragments.  ArrayByteSource returns the entire array in
// one fragment.
class MockByteSource : public ByteSource {
 public:
  MockByteSource(StringPiece data, int block_size)
    : data_(data), block_size_(block_size) {}

  size_t Available() const { return data_.size(); }
  StringPiece Peek() {
    return data_.substr(0, block_size_);
  }
  void Skip(size_t n) { data_.remove_prefix(n); }

 private:
  StringPiece data_;
  int block_size_;
};

TEST(ByteSourceTest, CopyTo) {
  StringPiece data("Hello world!");
  MockByteSource source(data, 3);
  string str;
  StringByteSink sink(&str);

  source.CopyTo(&sink, data.size());
  EXPECT_EQ(data, str);
}

TEST(ByteSourceTest, CopySubstringTo) {
  StringPiece data("Hello world!");
  MockByteSource source(data, 3);
  source.Skip(1);
  string str;
  StringByteSink sink(&str);

  source.CopyTo(&sink, data.size() - 2);
  EXPECT_EQ(data.substr(1, data.size() - 2), str);
  EXPECT_EQ("!", source.Peek());
}

TEST(ByteSourceTest, LimitByteSource) {
  StringPiece data("Hello world!");
  MockByteSource source(data, 3);
  LimitByteSource limit_source(&source, 6);
  EXPECT_EQ(6, limit_source.Available());
  limit_source.Skip(1);
  EXPECT_EQ(5, limit_source.Available());

  {
    string str;
    StringByteSink sink(&str);
    limit_source.CopyTo(&sink, limit_source.Available());
    EXPECT_EQ("ello ", str);
    EXPECT_EQ(0, limit_source.Available());
    EXPECT_EQ(6, source.Available());
  }

  {
    string str;
    StringByteSink sink(&str);
    source.CopyTo(&sink, source.Available());
    EXPECT_EQ("world!", str);
    EXPECT_EQ(0, source.Available());
  }
}

TEST(ByteSourceTest, CopyToStringByteSink) {
  StringPiece data("Hello world!");
  MockByteSource source(data, 3);
  string str;
  StringByteSink sink(&str);
  source.CopyTo(&sink, data.size());
  EXPECT_EQ(data, str);
}

// Verify that ByteSink is subclassable and Flush() overridable.
class FlushingByteSink : public StringByteSink {
 public:
  explicit FlushingByteSink(string* dest) : StringByteSink(dest) {}
  virtual void Flush() { Append("z", 1); }
 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(FlushingByteSink);
};

// Write and Flush via the ByteSink superclass interface.
void WriteAndFlush(ByteSink* s) {
  s->Append("abc", 3);
  s->Flush();
}

TEST(ByteSinkTest, Flush) {
  string str;
  FlushingByteSink f_sink(&str);
  WriteAndFlush(&f_sink);
  EXPECT_STREQ("abcz", str.c_str());
}

}  // namespace
}  // namespace strings
}  // namespace protobuf
}  // namespace google
