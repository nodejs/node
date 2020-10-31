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

#include "perfetto/ext/base/utils.h"

#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <unistd.h>

#include "perfetto/ext/base/file_utils.h"
#include "perfetto/ext/base/pipe.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace base {
namespace {

TEST(UtilsTest, ArraySize) {
  char char_arr_1[1];
  char char_arr_4[4];
  EXPECT_EQ(1u, ArraySize(char_arr_1));
  EXPECT_EQ(4u, ArraySize(char_arr_4));

  int32_t int32_arr_1[1];
  int32_t int32_arr_4[4];
  EXPECT_EQ(1u, ArraySize(int32_arr_1));
  EXPECT_EQ(4u, ArraySize(int32_arr_4));

  uint64_t int64_arr_1[1];
  uint64_t int64_arr_4[4];
  EXPECT_EQ(1u, ArraySize(int64_arr_1));
  EXPECT_EQ(4u, ArraySize(int64_arr_4));

  char kString[] = "foo";
  EXPECT_EQ(4u, ArraySize(kString));

  struct Bar {
    int32_t a;
    int32_t b;
  };
  Bar bar_1[1];
  Bar bar_4[4];
  EXPECT_EQ(1u, ArraySize(bar_1));
  EXPECT_EQ(4u, ArraySize(bar_4));
}

// Fuchsia doesn't currently support sigaction(), see
// fuchsia.atlassian.net/browse/ZX-560.
#if !PERFETTO_BUILDFLAG(PERFETTO_OS_FUCHSIA)
TEST(UtilsTest, EintrWrapper) {
  Pipe pipe = Pipe::Create();

  struct sigaction sa = {};
  struct sigaction old_sa = {};

// Glibc headers for sa_sigaction trigger this.
#pragma GCC diagnostic push
#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wdisabled-macro-expansion"
#endif
  sa.sa_sigaction = [](int, siginfo_t*, void*) {};
#pragma GCC diagnostic pop

  ASSERT_EQ(0, sigaction(SIGUSR2, &sa, &old_sa));
  int parent_pid = getpid();
  pid_t pid = fork();
  ASSERT_NE(-1, pid);
  if (pid == 0 /* child */) {
    usleep(5000);
    kill(parent_pid, SIGUSR2);
    ignore_result(WriteAll(*pipe.wr, "foo\0", 4));
    _exit(0);
  }

  char buf[6] = {};
  EXPECT_EQ(4, PERFETTO_EINTR(read(*pipe.rd, buf, sizeof(buf))));
  EXPECT_TRUE(close(*pipe.rd) == 0 || errno == EINTR);
  pipe.wr.reset();

  // A 2nd close should fail with the proper errno.
  int res = close(*pipe.rd);
  auto err = errno;
  EXPECT_EQ(-1, res);
  EXPECT_EQ(EBADF, err);
  pipe.rd.release();

  // Restore the old handler.
  sigaction(SIGUSR2, &old_sa, nullptr);
}
#endif  // !PERFETTO_BUILDFLAG(PERFETTO_OS_FUCHSIA)

TEST(UtilsTest, Align) {
  EXPECT_EQ(0u, AlignUp<4>(0));
  EXPECT_EQ(4u, AlignUp<4>(1));
  EXPECT_EQ(4u, AlignUp<4>(3));
  EXPECT_EQ(4u, AlignUp<4>(4));
  EXPECT_EQ(8u, AlignUp<4>(5));
  EXPECT_EQ(0u, AlignUp<16>(0));
  EXPECT_EQ(16u, AlignUp<16>(1));
  EXPECT_EQ(16u, AlignUp<16>(15));
  EXPECT_EQ(16u, AlignUp<16>(16));
  EXPECT_EQ(32u, AlignUp<16>(17));
  EXPECT_EQ(0xffffff00u, AlignUp<16>(0xffffff00 - 1));
}

}  // namespace
}  // namespace base
}  // namespace perfetto
