// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/platform.h"
#include "test/unittests/heap/cppgc/test-platform.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

class DefaultPlatformEnvironment final : public ::testing::Environment {
 public:
  DefaultPlatformEnvironment() = default;

  void SetUp() override {
    platform_ =
        std::make_unique<cppgc::internal::testing::TestPlatform>(nullptr);
    cppgc::InitializeProcess(platform_->GetPageAllocator());
  }

  void TearDown() override { cppgc::ShutdownProcess(); }

 private:
  std::shared_ptr<cppgc::internal::testing::TestPlatform> platform_;
};

}  // namespace

int main(int argc, char** argv) {
  // Don't catch SEH exceptions and continue as the following tests might hang
  // in an broken environment on windows.
  testing::GTEST_FLAG(catch_exceptions) = false;

  // Most unit-tests are multi-threaded, so enable thread-safe death-tests.
  testing::FLAGS_gtest_death_test_style = "threadsafe";

  testing::InitGoogleMock(&argc, argv);
  testing::AddGlobalTestEnvironment(new DefaultPlatformEnvironment);
  return RUN_ALL_TESTS();
}
