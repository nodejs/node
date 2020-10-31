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

#include <fstream>
#include <set>
#include <sstream>
#include <string>

#include "perfetto/ext/base/file_utils.h"
#include "src/traced/probes/ftrace/ftrace_controller.h"
#include "src/traced/probes/ftrace/ftrace_procfs.h"
#include "test/gtest_and_gmock.h"

using testing::Contains;
using testing::HasSubstr;
using testing::IsEmpty;
using testing::Not;
using testing::UnorderedElementsAre;

// These tests run only on Android because on linux they require access to
// ftrace, which would be problematic in the CI when multiple tests run
// concurrently on the same machine. Android instead uses one emulator instance
// for each worker.
#if PERFETTO_BUILDFLAG(PERFETTO_OS_ANDROID)
// On Android these tests conflict with traced_probes which expects to be the
// only one modifying tracing. This led to the Setup code which attempts to
// to skip these tests when traced_probes is using tracing. Unfortunately this
// is racey and we still see spurious failures in practice. For now disable
// these tests on Android also.
// TODO(b/150675975) Re-enable these tests.
#define ANDROID_ONLY_TEST(x) DISABLED_##x
#else
#define ANDROID_ONLY_TEST(x) DISABLED_##x
#endif

namespace perfetto {
namespace {

std::string GetFtracePath() {
  size_t i = 0;
  while (!FtraceProcfs::Create(FtraceController::kTracingPaths[i])) {
    i++;
  }
  return std::string(FtraceController::kTracingPaths[i]);
}

std::string ReadFile(const std::string& name) {
  std::string result;
  PERFETTO_CHECK(base::ReadFile(GetFtracePath() + name, &result));
  return result;
}

std::string GetTraceOutput() {
  std::string output = ReadFile("trace");
  if (output.empty()) {
    ADD_FAILURE() << "Could not read trace output";
  }
  return output;
}

class FtraceProcfsIntegrationTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<FtraceProcfs> ftrace_;
};

void FtraceProcfsIntegrationTest::SetUp() {
  ftrace_ = FtraceProcfs::Create(GetFtracePath());
  ASSERT_TRUE(ftrace_);
  if (ftrace_->IsTracingEnabled()) {
    GTEST_SKIP() << "Something else is using ftrace, skipping";
  }

  ftrace_->DisableAllEvents();
  ftrace_->ClearTrace();
  ftrace_->EnableTracing();
}

void FtraceProcfsIntegrationTest::TearDown() {
  ftrace_->DisableAllEvents();
  ftrace_->ClearTrace();
  ftrace_->DisableTracing();
}

TEST_F(FtraceProcfsIntegrationTest, ANDROID_ONLY_TEST(CreateWithBadPath)) {
  EXPECT_FALSE(FtraceProcfs::Create(GetFtracePath() + std::string("bad_path")));
}

TEST_F(FtraceProcfsIntegrationTest, ANDROID_ONLY_TEST(ClearTrace)) {
  ftrace_->WriteTraceMarker("Hello, World!");
  ftrace_->ClearTrace();
  EXPECT_THAT(GetTraceOutput(), Not(HasSubstr("Hello, World!")));
}

TEST_F(FtraceProcfsIntegrationTest, ANDROID_ONLY_TEST(TraceMarker)) {
  ftrace_->WriteTraceMarker("Hello, World!");
  EXPECT_THAT(GetTraceOutput(), HasSubstr("Hello, World!"));
}

TEST_F(FtraceProcfsIntegrationTest, ANDROID_ONLY_TEST(EnableDisableEvent)) {
  ASSERT_TRUE(ftrace_->EnableEvent("sched", "sched_switch"));
  sleep(1);
  ASSERT_TRUE(ftrace_->DisableEvent("sched", "sched_switch"));

  EXPECT_THAT(GetTraceOutput(), HasSubstr("sched_switch"));

  ftrace_->ClearTrace();
  sleep(1);
  EXPECT_THAT(GetTraceOutput(), Not(HasSubstr("sched_switch")));
}

TEST_F(FtraceProcfsIntegrationTest, ANDROID_ONLY_TEST(EnableDisableTracing)) {
  EXPECT_TRUE(ftrace_->IsTracingEnabled());
  ftrace_->WriteTraceMarker("Before");
  ftrace_->DisableTracing();
  EXPECT_FALSE(ftrace_->IsTracingEnabled());
  ftrace_->WriteTraceMarker("During");
  ftrace_->EnableTracing();
  EXPECT_TRUE(ftrace_->IsTracingEnabled());
  ftrace_->WriteTraceMarker("After");
  EXPECT_THAT(GetTraceOutput(), HasSubstr("Before"));
  EXPECT_THAT(GetTraceOutput(), Not(HasSubstr("During")));
  EXPECT_THAT(GetTraceOutput(), HasSubstr("After"));
}

TEST_F(FtraceProcfsIntegrationTest, ANDROID_ONLY_TEST(ReadFormatFile)) {
  std::string format = ftrace_->ReadEventFormat("ftrace", "print");
  EXPECT_THAT(format, HasSubstr("name: print"));
  EXPECT_THAT(format, HasSubstr("field:char buf"));
}

TEST_F(FtraceProcfsIntegrationTest, ANDROID_ONLY_TEST(CanOpenTracePipeRaw)) {
  EXPECT_TRUE(ftrace_->OpenPipeForCpu(0));
}

TEST_F(FtraceProcfsIntegrationTest, ANDROID_ONLY_TEST(Clock)) {
  std::set<std::string> clocks = ftrace_->AvailableClocks();
  EXPECT_THAT(clocks, Contains("local"));
  EXPECT_THAT(clocks, Contains("global"));

  EXPECT_TRUE(ftrace_->SetClock("global"));
  EXPECT_EQ(ftrace_->GetClock(), "global");
  EXPECT_TRUE(ftrace_->SetClock("local"));
  EXPECT_EQ(ftrace_->GetClock(), "local");
}

TEST_F(FtraceProcfsIntegrationTest, ANDROID_ONLY_TEST(CanSetBufferSize)) {
  EXPECT_TRUE(ftrace_->SetCpuBufferSizeInPages(4ul));
  EXPECT_EQ(ReadFile("buffer_size_kb"), "16\n");  // (4096 * 4) / 1024
  EXPECT_TRUE(ftrace_->SetCpuBufferSizeInPages(5ul));
  EXPECT_EQ(ReadFile("buffer_size_kb"), "20\n");  // (4096 * 5) / 1024
}

TEST_F(FtraceProcfsIntegrationTest,
       ANDROID_ONLY_TEST(FtraceControllerHardReset)) {
  ftrace_->SetCpuBufferSizeInPages(4ul);
  ftrace_->EnableTracing();
  ftrace_->EnableEvent("sched", "sched_switch");
  ftrace_->WriteTraceMarker("Hello, World!");

  EXPECT_EQ(ReadFile("buffer_size_kb"), "16\n");
  EXPECT_EQ(ReadFile("tracing_on"), "1\n");
  EXPECT_EQ(ReadFile("events/enable"), "X\n");

  HardResetFtraceState();

  EXPECT_EQ(ReadFile("buffer_size_kb"), "4\n");
  EXPECT_EQ(ReadFile("tracing_on"), "0\n");
  EXPECT_EQ(ReadFile("events/enable"), "0\n");
  EXPECT_THAT(GetTraceOutput(), Not(HasSubstr("Hello")));
}

TEST_F(FtraceProcfsIntegrationTest, ANDROID_ONLY_TEST(ReadEnabledEvents)) {
  EXPECT_THAT(ftrace_->ReadEnabledEvents(), IsEmpty());

  ftrace_->EnableEvent("sched", "sched_switch");
  ftrace_->EnableEvent("kmem", "kmalloc");

  EXPECT_THAT(ftrace_->ReadEnabledEvents(),
              UnorderedElementsAre("sched/sched_switch", "kmem/kmalloc"));

  ftrace_->DisableEvent("sched", "sched_switch");
  ftrace_->DisableEvent("kmem", "kmalloc");

  EXPECT_THAT(ftrace_->ReadEnabledEvents(), IsEmpty());
}

}  // namespace
}  // namespace perfetto
