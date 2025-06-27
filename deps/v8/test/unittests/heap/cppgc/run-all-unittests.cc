// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/platform.h"
#include "src/base/page-allocator.h"
#include "test/unittests/heap/cppgc/test-platform.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

class CppGCEnvironment final : public ::testing::Environment {
 public:
  void SetUp() override {
    // Initialize the process for cppgc with an arbitrary page allocator. This
    // has to survive as long as the process, so it's ok to leak the allocator
    // here.
    cppgc::InitializeProcess(new v8::base::PageAllocator());
  }

  void TearDown() override { cppgc::ShutdownProcess(); }
};

}  // namespace

int main(int argc, char** argv) {
  // Don't catch SEH exceptions and continue as the following tests might hang
  // in an broken environment on windows.
  testing::GTEST_FLAG(catch_exceptions) = false;

  // Most unit-tests are multi-threaded, so enable thread-safe death-tests.
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  testing::InitGoogleMock(&argc, argv);
  testing::AddGlobalTestEnvironment(new CppGCEnvironment);
  return RUN_ALL_TESTS();
}
