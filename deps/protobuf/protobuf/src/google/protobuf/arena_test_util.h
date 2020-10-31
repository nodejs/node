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

#ifndef GOOGLE_PROTOBUF_ARENA_TEST_UTIL_H__
#define GOOGLE_PROTOBUF_ARENA_TEST_UTIL_H__

#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/arena.h>

namespace google {
namespace protobuf {

template <typename T, bool use_arena>
void TestParseCorruptedString(const T& message) {
  int success_count = 0;
  std::string s;
  {
    // Map order is not deterministic. To make the test deterministic we want
    // to serialize the proto deterministically.
    io::StringOutputStream output(&s);
    io::CodedOutputStream out(&output);
    out.SetSerializationDeterministic(true);
    message.SerializePartialToCodedStream(&out);
  }
  const int kMaxIters = 900;
  const int stride = s.size() <= kMaxIters ? 1 : s.size() / kMaxIters;
  const int start = stride == 1 || use_arena ? 0 : (stride + 1) / 2;
  for (int i = start; i < s.size(); i += stride) {
    for (int c = 1 + (i % 17); c < 256; c += 2 * c + (i & 3)) {
      s[i] ^= c;
      Arena arena;
      T* message = Arena::CreateMessage<T>(use_arena ? &arena : nullptr);
      if (message->ParseFromString(s)) {
        ++success_count;
      }
      if (!use_arena) {
        delete message;
      }
      s[i] ^= c;  // Restore s to its original state.
    }
  }
  // This next line is a low bar.  But getting through the test without crashing
  // due to use-after-free or other bugs is a big part of what we're checking.
  GOOGLE_CHECK_GT(success_count, 0);
}

namespace internal {

class NoHeapChecker {
 public:
  NoHeapChecker() { capture_alloc.Hook(); }
  ~NoHeapChecker();

 private:
  class NewDeleteCapture {
   public:
    // TOOD(xiaofeng): Implement this for opensource protobuf.
    void Hook() {}
    void Unhook() {}
    int alloc_count() { return 0; }
    int free_count() { return 0; }
  } capture_alloc;
};

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_ARENA_TEST_UTIL_H__
