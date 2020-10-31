/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "src/traced/probes/ftrace/cpu_stats_parser.h"

#include "src/traced/probes/ftrace/ftrace_controller.h"
#include "src/traced/probes/ftrace/ftrace_stats.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace {

TEST(CpuStatsParserTest, DumpCpu) {
  std::string text = R"(entries: 1
overrun: 2
commit overrun: 3
bytes: 4
oldest event ts:     5123.000
now ts:  6123.123
dropped events	 	:7
read events: 8
)";

  FtraceCpuStats stats{};
  EXPECT_TRUE(DumpCpuStats(text, &stats));

  EXPECT_EQ(stats.entries, 1u);
  EXPECT_EQ(stats.overrun, 2u);
  EXPECT_EQ(stats.commit_overrun, 3u);
  EXPECT_EQ(stats.bytes_read, 4u);
  EXPECT_DOUBLE_EQ(stats.oldest_event_ts, 5123.0);
  EXPECT_DOUBLE_EQ(stats.now_ts, 6123.123);
  EXPECT_EQ(stats.dropped_events, 7u);
  EXPECT_EQ(stats.read_events, 8u);
}

}  // namespace
}  // namespace perfetto
