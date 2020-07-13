// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/heap/cppgc/tests.h"

#include <memory>

namespace cppgc {
namespace internal {
namespace testing {

// static
std::unique_ptr<cppgc::PageAllocator> TestWithPlatform::page_allocator_;

// static
void TestWithPlatform::SetUpTestSuite() {
  page_allocator_ = std::make_unique<v8::base::PageAllocator>();
  cppgc::InitializePlatform(page_allocator_.get());
}

// static
void TestWithPlatform::TearDownTestSuite() {
  cppgc::ShutdownPlatform();
  page_allocator_.reset();
}

TestWithHeap::TestWithHeap() : heap_(Heap::Create()) {}

TestSupportingAllocationOnly::TestSupportingAllocationOnly()
    : no_gc_scope_(internal::Heap::From(GetHeap())) {}

}  // namespace testing
}  // namespace internal
}  // namespace cppgc
