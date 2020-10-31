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

#include "perfetto/base/time.h"

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace base {
namespace {

TEST(TimeTest, Conversions) {
  TimeMillis ms = GetWallTimeMs();
  TimeNanos ns = GetWallTimeNs();
  EXPECT_NEAR(ms.count(), ns.count() / 1000000, 1000);

  {
    struct timespec ts = ToPosixTimespec(TimeMillis(0));
    EXPECT_EQ(0, ts.tv_sec);
    EXPECT_EQ(0, ts.tv_nsec);
  }
  {
    struct timespec ts = ToPosixTimespec(TimeMillis(1));
    EXPECT_EQ(0, ts.tv_sec);
    EXPECT_EQ(1000000, ts.tv_nsec);
  }
  {
    struct timespec ts = ToPosixTimespec(TimeMillis(12345));
    EXPECT_EQ(12, ts.tv_sec);
    EXPECT_EQ(345000000, ts.tv_nsec);
  }
  {
    struct timespec ts = ToPosixTimespec(TimeMillis(1000000000001LL));
    EXPECT_EQ(1000000000, ts.tv_sec);
    EXPECT_EQ(1000000, ts.tv_nsec);
  }
}

TEST(TimeTest, GetTime) {
  const auto start_time = GetWallTimeNs();
  const auto start_cputime = GetThreadCPUTimeNs();

  const unsigned ns_in_ms = 1000000;

  for (;;) {
    auto cur_time = GetWallTimeNs();
    auto elapsed = cur_time - start_time;
    // Spin for a little while.
    if (elapsed > TimeNanos(20 * ns_in_ms))
      break;
  }

  auto end_cputime = GetThreadCPUTimeNs();
  auto elapsed_cputime = end_cputime - start_cputime;
  // Check that we're not burning much more CPU time than the length of time
  // that we spun in the loop. We may burn much less, depending on what else is
  // happening on the test machine.
  EXPECT_LE(elapsed_cputime.count(), 50 * ns_in_ms);
}

}  // namespace
}  // namespace base
}  // namespace perfetto
