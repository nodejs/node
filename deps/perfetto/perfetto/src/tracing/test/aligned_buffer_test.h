/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef SRC_TRACING_TEST_ALIGNED_BUFFER_TEST_H_
#define SRC_TRACING_TEST_ALIGNED_BUFFER_TEST_H_

#include <stdlib.h>

#include <memory>

#include "perfetto/ext/base/utils.h"
#include "src/tracing/test/test_shared_memory.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {

// Base parametrized test for unittests that require an aligned buffer.
class AlignedBufferTest : public ::testing::TestWithParam<size_t> {
 public:
  static constexpr size_t kNumPages = 14;
  void SetUp() override;
  void TearDown() override;

  uint8_t* buf() const { return reinterpret_cast<uint8_t*>(buf_->start()); }
  size_t buf_size() const { return buf_->size(); }
  size_t page_size() const { return page_size_; }

 private:
  size_t page_size_ = 0;

  // This doesn't need any sharing. TestSharedMemory just happens to have the
  // right harness to create a zeroed page-aligned buffer.
  std::unique_ptr<TestSharedMemory> buf_;
};

}  // namespace perfetto

#endif  // SRC_TRACING_TEST_ALIGNED_BUFFER_TEST_H_
