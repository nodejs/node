// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TEST_BENCHMARK_CPP_CPPGC_UTILS_H_
#define TEST_BENCHMARK_CPP_CPPGC_UTILS_H_

#include "include/cppgc/heap.h"
#include "include/cppgc/platform.h"
#include "test/unittests/heap/cppgc/test-platform.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace cppgc {
namespace internal {
namespace testing {

class BenchmarkWithHeap : public benchmark::Fixture {
 protected:
  void SetUp(const ::benchmark::State& state) override {
    platform_ = std::make_shared<testing::TestPlatform>();
    cppgc::InitializeProcess(platform_->GetPageAllocator());
    heap_ = cppgc::Heap::Create(platform_);
  }

  void TearDown(const ::benchmark::State& state) override {
    heap_.reset();
    cppgc::ShutdownProcess();
  }

  cppgc::Heap& heap() const { return *heap_.get(); }

 private:
  std::shared_ptr<testing::TestPlatform> platform_;
  std::unique_ptr<cppgc::Heap> heap_;
};

}  // namespace testing
}  // namespace internal
}  // namespace cppgc

#endif  // TEST_BENCHMARK_CPP_CPPGC_UTILS_H_
