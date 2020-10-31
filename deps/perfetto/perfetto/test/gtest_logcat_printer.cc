/*
 * Copyright (C) 2020 The Android Open Source Project
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

// This file registers a GTest listener that logs test begin/end in logcat.
// This is to avoid problems like b/149852934 where the test output cannot be
// correlated with the prouction code's logcat logs because they use different
// clock sources.

#include <android/log.h>

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace test {

namespace {

#define PERFETTO_TEST_LOG(...) \
  __android_log_print(ANDROID_LOG_DEBUG, "perfetto", ##__VA_ARGS__)

class LogcatPrinter : public testing::EmptyTestEventListener {
 public:
  LogcatPrinter();
  ~LogcatPrinter() override;

  // testing::EmptyTestEventListener:
  void OnTestCaseStart(const testing::TestCase& test_case) override;
  void OnTestStart(const testing::TestInfo& test_info) override;
  void OnTestEnd(const testing::TestInfo& test_info) override;
  void OnTestCaseEnd(const testing::TestCase& test_case) override;
};

LogcatPrinter::LogcatPrinter() = default;
LogcatPrinter::~LogcatPrinter() = default;

void LogcatPrinter::OnTestCaseStart(const testing::TestCase& test_case) {
  PERFETTO_TEST_LOG("Test case start: %s", test_case.name());
}

void LogcatPrinter::OnTestStart(const testing::TestInfo& test_info) {
  PERFETTO_TEST_LOG("Test start: %s.%s", test_info.test_case_name(),
                    test_info.name());
}

void LogcatPrinter::OnTestEnd(const testing::TestInfo& test_info) {
  const auto* result = test_info.result();
  const char* state = "N/A";
  if (result)
    state = result->Passed() ? "PASS" : "FAIL";
  PERFETTO_TEST_LOG("Test end: %s.%s [%s]", test_info.test_case_name(),
                    test_info.name(), state);
}

void LogcatPrinter::OnTestCaseEnd(const testing::TestCase& test_case) {
  PERFETTO_TEST_LOG("Test case end: %s. succeeded=%d, failed=%d",
                    test_case.name(), test_case.successful_test_count(),
                    test_case.failed_test_count());
}

// This static initializer
void __attribute__((constructor)) __attribute__((visibility("default")))
SetupGtestLogcatPrinter() {
  static LogcatPrinter* instance = new LogcatPrinter();
  auto& listeners = testing::UnitTest::GetInstance()->listeners();
  listeners.Append(instance);
}

}  // namespace
}  // namespace test
}  // namespace perfetto
