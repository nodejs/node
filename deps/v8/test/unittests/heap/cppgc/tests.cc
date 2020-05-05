// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/heap/cppgc/tests.h"

namespace cppgc {
namespace testing {

// static
std::unique_ptr<cppgc::PageAllocator> TestWithPlatform::page_allocator_;

// static
void TestWithPlatform::SetUpTestSuite() {
  page_allocator_.reset(new v8::base::PageAllocator());
  cppgc::InitializePlatform(page_allocator_.get());
}

// static
void TestWithPlatform::TearDownTestSuite() {
  cppgc::ShutdownPlatform();
  page_allocator_.reset();
}

void TestWithHeap::SetUp() {
  heap_ = Heap::Create();
  TestWithPlatform::SetUp();
}

void TestWithHeap::TearDown() {
  heap_.reset();
  TestWithPlatform::TearDown();
}

}  // namespace testing
}  // namespace cppgc
