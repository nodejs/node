// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/output-stream-writer.h"

#include "include/v8-profiler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8::internal {

namespace {

class AbortingOutputStream : public v8::OutputStream {
 public:
  void EndOfStream() override { ++eos_count_; }
  int GetChunkSize() override { return 8; }
  WriteResult WriteAsciiChunk(char*, int) override { return kAbort; }

  int eos_count() const { return eos_count_; }

 private:
  int eos_count_ = 0;
};

}  // namespace

using OutputStreamWriterTest = ::testing::Test;

TEST_F(OutputStreamWriterTest, AbortOnFinalPartialChunkSuppressesEndOfStream) {
  AbortingOutputStream stream;
  OutputStreamWriter writer(&stream);
  writer.AddString("abc");
  EXPECT_FALSE(writer.aborted());

  writer.Finalize();

  EXPECT_TRUE(writer.aborted());
  EXPECT_EQ(0, stream.eos_count());
}

}  // namespace v8::internal
