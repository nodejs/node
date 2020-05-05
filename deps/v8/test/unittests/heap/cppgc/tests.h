// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_HEAP_CPPGC_TESTS_H_
#define V8_UNITTESTS_HEAP_CPPGC_TESTS_H_

#include "include/cppgc/heap.h"
#include "include/cppgc/platform.h"
#include "src/base/page-allocator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace testing {

class TestWithPlatform : public ::testing::Test {
 protected:
  static void SetUpTestSuite();
  static void TearDownTestSuite();

 private:
  static std::unique_ptr<cppgc::PageAllocator> page_allocator_;
};

class TestWithHeap : public TestWithPlatform {
 protected:
  void SetUp() override;
  void TearDown() override;

  Heap* GetHeap() const { return heap_.get(); }

 private:
  std::unique_ptr<cppgc::Heap> heap_;
};

}  // namespace testing
}  // namespace cppgc

#endif  // V8_UNITTESTS_HEAP_CPPGC_TESTS_H_
