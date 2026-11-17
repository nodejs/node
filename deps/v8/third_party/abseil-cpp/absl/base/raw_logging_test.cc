// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This test serves primarily as a compilation test for base/raw_logging.h.
// Raw logging testing is covered by logging_unittest.cc, which is not as
// portable as this test.

#include "absl/base/internal/raw_logging.h"

#include <cstddef>
#include <string>

#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))) && \
    !defined(__EMSCRIPTEN__)
#include <unistd.h>

#endif

#include "gtest/gtest.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"

namespace {

TEST(RawLoggingCompilationTest, Log) {
  ABSL_RAW_LOG(INFO, "RAW INFO: %d", 1);
  ABSL_RAW_LOG(INFO, "RAW INFO: %d %d", 1, 2);
  ABSL_RAW_LOG(INFO, "RAW INFO: %d %d %d", 1, 2, 3);
  ABSL_RAW_LOG(INFO, "RAW INFO: %d %d %d %d", 1, 2, 3, 4);
  ABSL_RAW_LOG(INFO, "RAW INFO: %d %d %d %d %d", 1, 2, 3, 4, 5);
  ABSL_RAW_LOG(WARNING, "RAW WARNING: %d", 1);
  ABSL_RAW_LOG(ERROR, "RAW ERROR: %d", 1);
}

TEST(RawLoggingCompilationTest, LogWithNulls) {
  ABSL_RAW_LOG(INFO, "RAW INFO: %s%c%s", "Hello", 0, "World");
}

TEST(RawLoggingCompilationTest, PassingCheck) {
  ABSL_RAW_CHECK(true, "RAW CHECK");
}

TEST(RawLoggingCompilationTest, DebugLog) {
  ABSL_RAW_DLOG(INFO, "RAW DLOG: %d", 1);
}

TEST(RawLoggingCompilationTest, PassingDebugCheck) {
  ABSL_RAW_DCHECK(true, "failure message");
}

// Not all platforms support output from raw log, so we don't verify any
// particular output for RAW check failures (expecting the empty string
// accomplishes this).  This test is primarily a compilation test, but we
// are verifying process death when EXPECT_DEATH works for a platform.
const char kExpectedDeathOutput[] = "";

#if !defined(NDEBUG)  // if debug build
TEST(RawLoggingDeathTest, FailingDebugCheck) {
  EXPECT_DEATH_IF_SUPPORTED(ABSL_RAW_DCHECK(1 == 0, "explanation"),
                            kExpectedDeathOutput);
}
#endif  // if debug build

TEST(RawLoggingDeathTest, FailingCheck) {
  EXPECT_DEATH_IF_SUPPORTED(ABSL_RAW_CHECK(1 == 0, "explanation"),
                            kExpectedDeathOutput);
}

TEST(RawLoggingDeathTest, LogFatal) {
  EXPECT_DEATH_IF_SUPPORTED(ABSL_RAW_LOG(FATAL, "my dog has fleas"),
                            kExpectedDeathOutput);
}

// Raw logging writes to STDERR_FILENO via write()/syscall on POSIX platforms,
// so the truncation path can be exercised directly by capturing stderr, without
// relying on death tests.
#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))) && \
    !defined(__EMSCRIPTEN__)
TEST(RawLoggingTest, TruncationMarkerAtExactBufferBoundary) {
  if (!absl::raw_log_internal::RawLoggingFullySupported()) {
    GTEST_SKIP() << "Raw logging output is not supported on this platform.";
  }
  // kLogBufSize in raw_logging.cc.
  constexpr int kLogBufSize = 3000;

  // The space left for the message depends on the log prefix, which a prefix
  // hook can change, so try every length up to the buffer size.  Each message
  // must come out either whole or ending in the truncation marker.
  for (int len = 1; len <= kLogBufSize; ++len) {
    int fds[2];
    ASSERT_EQ(pipe(fds), 0);
    int saved_stderr = dup(STDERR_FILENO);
    ASSERT_NE(saved_stderr, -1);
    ASSERT_NE(dup2(fds[1], STDERR_FILENO), -1);

    const std::string msg(static_cast<size_t>(len), 'x');
    ABSL_RAW_LOG(ERROR, "%s", msg.c_str());

    ASSERT_NE(dup2(saved_stderr, STDERR_FILENO), -1);
    close(saved_stderr);
    close(fds[1]);
    char buf[kLogBufSize + 64];
    ssize_t n = read(fds[0], buf, sizeof(buf));
    close(fds[0]);
    ASSERT_GT(n, 0);
    const std::string output(buf, static_cast<size_t>(n));

    ASSERT_TRUE(absl::StrContains(output, msg) ||
                absl::EndsWith(output, " ... (message truncated)\n"))
        << "message length " << len;
  }
}
#endif

TEST(InternalLog, CompilationTest) {
  ABSL_INTERNAL_LOG(INFO, "Internal Log");
  std::string log_msg = "Internal Log";
  ABSL_INTERNAL_LOG(INFO, log_msg);

  ABSL_INTERNAL_LOG(INFO, log_msg + " 2");

  float d = 1.1f;
  ABSL_INTERNAL_LOG(INFO, absl::StrCat("Internal log ", 3, " + ", d));
}

TEST(InternalLogDeathTest, FailingCheck) {
  EXPECT_DEATH_IF_SUPPORTED(ABSL_INTERNAL_CHECK(1 == 0, "explanation"),
                            kExpectedDeathOutput);
}

TEST(InternalLogDeathTest, LogFatal) {
  EXPECT_DEATH_IF_SUPPORTED(ABSL_INTERNAL_LOG(FATAL, "my dog has fleas"),
                            kExpectedDeathOutput);
}

}  // namespace
