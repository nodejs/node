// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/libplatform/libplatform.h"
#include "include/v8.h"
#include "src/base/compiler-specific.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

class DefaultPlatformEnvironment final : public ::testing::Environment {
 public:
  DefaultPlatformEnvironment() = default;

  void SetUp() override {
    platform_ = v8::platform::NewDefaultPlatform(
        0, v8::platform::IdleTaskSupport::kEnabled);
    ASSERT_TRUE(platform_.get() != nullptr);
    v8::V8::InitializePlatform(platform_.get());
    ASSERT_TRUE(v8::V8::Initialize());
  }

  void TearDown() override {
    ASSERT_TRUE(platform_.get() != nullptr);
    v8::V8::Dispose();
    v8::V8::ShutdownPlatform();
  }

 private:
  std::unique_ptr<v8::Platform> platform_;
};

}  // namespace


int main(int argc, char** argv) {
  // Don't catch SEH exceptions and continue as the following tests might hang
  // in an broken environment on windows.
  testing::GTEST_FLAG(catch_exceptions) = false;
  testing::InitGoogleMock(&argc, argv);
  testing::AddGlobalTestEnvironment(new DefaultPlatformEnvironment);
  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
  v8::V8::InitializeExternalStartupData(argv[0]);
  return RUN_ALL_TESTS();
}
