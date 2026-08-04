// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/benchmarks/cpp/cppgc/benchmark_utils.h"

#include "include/cppgc/platform.h"
#include "test/unittests/heap/cppgc/test-platform.h"

namespace cppgc {
namespace internal {
namespace testing {

// static
std::shared_ptr<testing::TestPlatform> BenchmarkWithHeap::platform_;

// static
void BenchmarkWithHeap::InitializeProcess() {
  platform_ = std::make_shared<testing::TestPlatform>();
  cppgc::InitializeProcess(platform_->GetPageAllocator());
}

// static
void BenchmarkWithHeap::ShutdownProcess() {
  cppgc::ShutdownProcess();
  platform_.reset();
}

}  // namespace testing
}  // namespace internal
}  // namespace cppgc
