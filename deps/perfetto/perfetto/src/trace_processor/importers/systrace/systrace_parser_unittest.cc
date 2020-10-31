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

#include "src/trace_processor/importers/systrace/systrace_parser.h"

#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace systrace_utils {
namespace {

using Result = SystraceParseResult;

TEST(SystraceParserTest, SystraceEvent) {
  SystraceTracePoint result{};
  ASSERT_EQ(ParseSystraceTracePoint("", &result), Result::kFailure);
  ASSERT_EQ(ParseSystraceTracePoint("abcdef", &result), Result::kFailure);
  ASSERT_EQ(ParseSystraceTracePoint("  ", &result), Result::kFailure);
  ASSERT_EQ(ParseSystraceTracePoint("|", &result), Result::kFailure);
  ASSERT_EQ(ParseSystraceTracePoint("||", &result), Result::kFailure);
  ASSERT_EQ(ParseSystraceTracePoint("|||", &result), Result::kFailure);
  ASSERT_EQ(ParseSystraceTracePoint("\n", &result), Result::kFailure);
  ASSERT_EQ(ParseSystraceTracePoint("|\n", &result), Result::kFailure);
  ASSERT_EQ(ParseSystraceTracePoint("||\n", &result), Result::kFailure);
  ASSERT_EQ(ParseSystraceTracePoint("||\n", &result), Result::kFailure);
  ASSERT_EQ(ParseSystraceTracePoint("B", &result), Result::kFailure);
  ASSERT_EQ(ParseSystraceTracePoint("C", &result), Result::kFailure);
  ASSERT_EQ(ParseSystraceTracePoint("S", &result), Result::kFailure);
  ASSERT_EQ(ParseSystraceTracePoint("F", &result), Result::kFailure);
  ASSERT_EQ(ParseSystraceTracePoint("B|42|\n", &result), Result::kFailure);

  ASSERT_EQ(ParseSystraceTracePoint("B|1|foo", &result), Result::kSuccess);
  EXPECT_EQ(result, SystraceTracePoint::B(1, "foo"));

  ASSERT_EQ(ParseSystraceTracePoint("B|42|Bar\n", &result), Result::kSuccess);
  EXPECT_EQ(result, SystraceTracePoint::B(42, "Bar"));

  ASSERT_EQ(ParseSystraceTracePoint("E\n", &result), Result::kSuccess);
  EXPECT_EQ(result, SystraceTracePoint::E(0));

  ASSERT_EQ(ParseSystraceTracePoint("E", &result), Result::kSuccess);
  EXPECT_EQ(result, SystraceTracePoint::E(0));

  ASSERT_EQ(ParseSystraceTracePoint("E|42\n", &result), Result::kSuccess);
  EXPECT_EQ(result, SystraceTracePoint::E(42));

  ASSERT_EQ(ParseSystraceTracePoint("E|42", &result), Result::kSuccess);
  EXPECT_EQ(result, SystraceTracePoint::E(42));

  ASSERT_EQ(ParseSystraceTracePoint("C|543|foo|", &result), Result::kFailure);
  ASSERT_EQ(ParseSystraceTracePoint("C|543|foo|8", &result), Result::kSuccess);
  EXPECT_EQ(result, SystraceTracePoint::C(543, "foo", 8));

  ASSERT_EQ(ParseSystraceTracePoint("C|543|foo|8|", &result), Result::kFailure);
  ASSERT_EQ(ParseSystraceTracePoint("C|543|foo|8|group", &result),
            Result::kSuccess);
  EXPECT_EQ(result, SystraceTracePoint::C(543, "foo", 8, "group"));

  ASSERT_EQ(ParseSystraceTracePoint("S|", &result), Result::kFailure);

  ASSERT_EQ(ParseSystraceTracePoint("S|123|foo|456", &result),
            Result::kSuccess);
  EXPECT_EQ(result, SystraceTracePoint::S(123, "foo", 456));

  ASSERT_EQ(ParseSystraceTracePoint("F|123|foo|456", &result),
            Result::kSuccess);
  EXPECT_EQ(result, SystraceTracePoint::F(123, "foo", 456));

  ASSERT_EQ(ParseSystraceTracePoint("trace_event_clock_sync: parent_ts=0.123\n",
                                    &result),
            Result::kUnsupported);
  ASSERT_EQ(ParseSystraceTracePoint("trace_event_clock_sync: realtime_ts=123\n",
                                    &result),
            Result::kUnsupported);
}

}  // namespace
}  // namespace systrace_utils
}  // namespace trace_processor
}  // namespace perfetto
