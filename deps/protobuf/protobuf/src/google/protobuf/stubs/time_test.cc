// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#include <google/protobuf/stubs/time.h>

#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace internal {
namespace {
static const int64 kSecondsPerDay = 3600 * 24;

// For DateTime, tests will mostly focuse on the date part because that's
// the tricky one.
int64 CreateTimestamp(int year, int month, int day) {
  DateTime time;
  time.year = year;
  time.month = month;
  time.day = day;
  time.hour = time.minute = time.second = 0;
  int64 result;
  GOOGLE_CHECK(DateTimeToSeconds(time, &result));
  // Check that a roundtrip produces the same result.
  GOOGLE_CHECK(SecondsToDateTime(result, &time));
  GOOGLE_CHECK(time.year == year);
  GOOGLE_CHECK(time.month == month);
  GOOGLE_CHECK(time.day == day);
  return result;
}

TEST(DateTimeTest, SimpleTime) {
  DateTime time;
  ASSERT_TRUE(SecondsToDateTime(1, &time));
  EXPECT_EQ(1970, time.year);
  EXPECT_EQ(1, time.month);
  EXPECT_EQ(1, time.day);
  EXPECT_EQ(0, time.hour);
  EXPECT_EQ(0, time.minute);
  EXPECT_EQ(1, time.second);
  int64 seconds;
  ASSERT_TRUE(DateTimeToSeconds(time, &seconds));
  EXPECT_EQ(1, seconds);

  ASSERT_TRUE(SecondsToDateTime(-1, &time));
  EXPECT_EQ(1969, time.year);
  EXPECT_EQ(12, time.month);
  EXPECT_EQ(31, time.day);
  EXPECT_EQ(23, time.hour);
  EXPECT_EQ(59, time.minute);
  EXPECT_EQ(59, time.second);
  ASSERT_TRUE(DateTimeToSeconds(time, &seconds));
  EXPECT_EQ(-1, seconds);

  DateTime start, end;
  start.year = 1;
  start.month = 1;
  start.day = 1;
  start.hour = 0;
  start.minute = 0;
  start.second = 0;
  end.year = 9999;
  end.month = 12;
  end.day = 31;
  end.hour = 23;
  end.minute = 59;
  end.second = 59;
  int64 start_time, end_time;
  ASSERT_TRUE(DateTimeToSeconds(start, &start_time));
  ASSERT_TRUE(DateTimeToSeconds(end, &end_time));
  EXPECT_EQ(315537897599LL, end_time - start_time);
  ASSERT_TRUE(SecondsToDateTime(start_time, &time));
  ASSERT_TRUE(DateTimeToSeconds(time, &seconds));
  EXPECT_EQ(start_time, seconds);
  ASSERT_TRUE(SecondsToDateTime(end_time, &time));
  ASSERT_TRUE(DateTimeToSeconds(time, &seconds));
  EXPECT_EQ(end_time, seconds);
}

TEST(DateTimeTest, DayInMonths) {
  // Check that month boundaries are handled correctly.
  EXPECT_EQ(kSecondsPerDay,
            CreateTimestamp(2015, 1, 1) - CreateTimestamp(2014, 12, 31));
  EXPECT_EQ(kSecondsPerDay,
            CreateTimestamp(2015, 2, 1) - CreateTimestamp(2015, 1, 31));
  EXPECT_EQ(kSecondsPerDay,
            CreateTimestamp(2015, 3, 1) - CreateTimestamp(2015, 2, 28));
  EXPECT_EQ(kSecondsPerDay,
            CreateTimestamp(2015, 4, 1) - CreateTimestamp(2015, 3, 31));
  EXPECT_EQ(kSecondsPerDay,
            CreateTimestamp(2015, 5, 1) - CreateTimestamp(2015, 4, 30));
  EXPECT_EQ(kSecondsPerDay,
            CreateTimestamp(2015, 6, 1) - CreateTimestamp(2015, 5, 31));
  EXPECT_EQ(kSecondsPerDay,
            CreateTimestamp(2015, 7, 1) - CreateTimestamp(2015, 6, 30));
  EXPECT_EQ(kSecondsPerDay,
            CreateTimestamp(2015, 8, 1) - CreateTimestamp(2015, 7, 31));
  EXPECT_EQ(kSecondsPerDay,
            CreateTimestamp(2015, 9, 1) - CreateTimestamp(2015, 8, 31));
  EXPECT_EQ(kSecondsPerDay,
            CreateTimestamp(2015, 10, 1) - CreateTimestamp(2015, 9, 30));
  EXPECT_EQ(kSecondsPerDay,
            CreateTimestamp(2015, 11, 1) - CreateTimestamp(2015, 10, 31));
  EXPECT_EQ(kSecondsPerDay,
            CreateTimestamp(2015, 12, 1) - CreateTimestamp(2015, 11, 30));
  EXPECT_EQ(kSecondsPerDay,
            CreateTimestamp(2016, 1, 1) - CreateTimestamp(2015, 12, 31));
}

TEST(DateTimeTest, LeapYear) {
  // Non-leap year.
  EXPECT_EQ(kSecondsPerDay,
            CreateTimestamp(2015, 3, 1) - CreateTimestamp(2015, 2, 28));
  // Leap year.
  EXPECT_EQ(kSecondsPerDay,
            CreateTimestamp(2016, 3, 1) - CreateTimestamp(2016, 2, 29));
  // Non-leap year.
  EXPECT_EQ(kSecondsPerDay,
            CreateTimestamp(2100, 3, 1) - CreateTimestamp(2100, 2, 28));
  // Leap year.
  EXPECT_EQ(kSecondsPerDay,
            CreateTimestamp(2400, 3, 1) - CreateTimestamp(2400, 2, 29));
}

TEST(DateTimeTest, WrongDays) {
  int64 seconds;
  DateTime time;
  time.hour = 0;
  time.minute = 0;
  time.second = 0;
  time.month = 2;

  // Non-leap year.
  time.year = 2015;
  time.day = 29;
  ASSERT_FALSE(DateTimeToSeconds(time, &seconds));

  // Leap year.
  time.year = 2016;
  time.day = 29;
  ASSERT_TRUE(DateTimeToSeconds(time, &seconds));
  time.day = 30;
  ASSERT_FALSE(DateTimeToSeconds(time, &seconds));

  // Non-leap year.
  time.year = 2100;
  time.day = 29;
  ASSERT_FALSE(DateTimeToSeconds(time, &seconds));

  // Leap year.
  time.year = 2400;
  time.day = 29;
  ASSERT_TRUE(DateTimeToSeconds(time, &seconds));
  time.day = 30;
  ASSERT_FALSE(DateTimeToSeconds(time, &seconds));

  // Non-february
  time.year = 2015;
  time.month = 1;
  time.day = 0;
  ASSERT_FALSE(DateTimeToSeconds(time, &seconds));
  time.day = 1;
  ASSERT_TRUE(DateTimeToSeconds(time, &seconds));
  time.day = 31;
  ASSERT_TRUE(DateTimeToSeconds(time, &seconds));
  time.day = 32;
  ASSERT_FALSE(DateTimeToSeconds(time, &seconds));

  // Bad month
  time.year = 2015;
  time.month = 0;
  time.day = 1;
  ASSERT_FALSE(DateTimeToSeconds(time, &seconds));
  time.month = 13;
  ASSERT_FALSE(DateTimeToSeconds(time, &seconds));
}

TEST(DateTimeTest, StringFormat) {
  DateTime start, end;
  start.year = 1;
  start.month = 1;
  start.day = 1;
  start.hour = 0;
  start.minute = 0;
  start.second = 0;
  end.year = 9999;
  end.month = 12;
  end.day = 31;
  end.hour = 23;
  end.minute = 59;
  end.second = 59;
  int64 start_time, end_time;
  ASSERT_TRUE(DateTimeToSeconds(start, &start_time));
  ASSERT_TRUE(DateTimeToSeconds(end, &end_time));

  EXPECT_EQ("0001-01-01T00:00:00Z", FormatTime(start_time, 0));
  EXPECT_EQ("9999-12-31T23:59:59Z", FormatTime(end_time, 0));

  // Make sure the nanoseconds part is formated correctly.
  EXPECT_EQ("1970-01-01T00:00:00.010Z", FormatTime(0, 10000000));
  EXPECT_EQ("1970-01-01T00:00:00.000010Z", FormatTime(0, 10000));
  EXPECT_EQ("1970-01-01T00:00:00.000000010Z", FormatTime(0, 10));
}

TEST(DateTimeTest, ParseString) {
  int64 seconds;
  int32 nanos;
  ASSERT_TRUE(ParseTime("0001-01-01T00:00:00Z", &seconds, &nanos));
  EXPECT_EQ("0001-01-01T00:00:00Z", FormatTime(seconds, nanos));
  ASSERT_TRUE(ParseTime("9999-12-31T23:59:59.999999999Z", &seconds, &nanos));
  EXPECT_EQ("9999-12-31T23:59:59.999999999Z", FormatTime(seconds, nanos));

  // Test time zone offsets.
  ASSERT_TRUE(ParseTime("1970-01-01T00:00:00-08:00", &seconds, &nanos));
  EXPECT_EQ("1970-01-01T08:00:00Z", FormatTime(seconds, nanos));
  ASSERT_TRUE(ParseTime("1970-01-01T00:00:00+08:00", &seconds, &nanos));
  EXPECT_EQ("1969-12-31T16:00:00Z", FormatTime(seconds, nanos));

  // Test nanoseconds.
  ASSERT_TRUE(ParseTime("1970-01-01T00:00:00.01Z", &seconds, &nanos));
  EXPECT_EQ("1970-01-01T00:00:00.010Z", FormatTime(seconds, nanos));
  ASSERT_TRUE(ParseTime("1970-01-01T00:00:00.00001-08:00", &seconds, &nanos));
  EXPECT_EQ("1970-01-01T08:00:00.000010Z", FormatTime(seconds, nanos));
  ASSERT_TRUE(ParseTime("1970-01-01T00:00:00.00000001+08:00", &seconds, &nanos));
  EXPECT_EQ("1969-12-31T16:00:00.000000010Z", FormatTime(seconds, nanos));
  // Fractional parts less than 1 nanosecond will be ignored.
  ASSERT_TRUE(ParseTime("1970-01-01T00:00:00.0123456789Z", &seconds, &nanos));
  EXPECT_EQ("1970-01-01T00:00:00.012345678Z", FormatTime(seconds, nanos));
}

}  // namespace
}  // namespace internal
}  // namespace protobuf
}  // namespace google
