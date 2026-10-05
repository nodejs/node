// Copyright 2026 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   https://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

#include "absl/time/internal/cctz/src/time_zone_posix.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace time_internal {
namespace cctz {

using ::testing::Eq;
using ::testing::IsEmpty;

// We only support the second POSIX format (that is, neither the
// "first character is a <colon>" format, nor the "geographical
// or a special timezone" format).  We also require DST start/end
// rules whenever a DST abbreviation is given (zic always provides
// them).  So, ...
//
//   spec = abbr offset [ abbr [ offset ] datetime datetime ]
//   abbr = <.+?> | [^-+,\d]{3,}
//   offset = [+|-]hh[:mm[:ss]]
//   datetime = , ( Jn | n | Mm.w.d ) [ / offset ]

TEST(ParsePosixSpec, UnsupportedFormats) {
  PosixTimeZone zone;
  EXPECT_FALSE(ParsePosixSpec(":characters", &zone));
  EXPECT_FALSE(ParsePosixSpec("Area/Location", &zone));
}

TEST(ParsePosixSpec, StdOnly) {
  PosixTimeZone zone;

  // America/Cancun
  EXPECT_TRUE(ParsePosixSpec("EST5", &zone));
  EXPECT_THAT(zone.std_abbr, Eq("EST"));
  EXPECT_THAT(zone.std_offset, Eq(-5 * 60 * 60));
  EXPECT_THAT(zone.dst_abbr, IsEmpty());

  // Australia/Darwin
  EXPECT_TRUE(ParsePosixSpec("ACST-9:30", &zone));
  EXPECT_THAT(zone.std_abbr, Eq("ACST"));
  EXPECT_THAT(zone.std_offset, Eq((9 * 60 + 30) * 60));
  EXPECT_THAT(zone.dst_abbr, IsEmpty());

  // Australia/Eucla
  EXPECT_TRUE(ParsePosixSpec("<+0845>-8:45", &zone));
  EXPECT_THAT(zone.std_abbr, Eq("+0845"));
  EXPECT_THAT(zone.std_offset, Eq((8 * 60 + 45) * 60));
  EXPECT_THAT(zone.dst_abbr, IsEmpty());
}

TEST(ParsePosixSpec, WithDst) {
  PosixTimeZone zone;

  // America/New_York
  EXPECT_TRUE(ParsePosixSpec("EST5EDT,M3.2.0,M11.1.0", &zone));
  EXPECT_THAT(zone.std_abbr, Eq("EST"));
  EXPECT_THAT(zone.std_offset, Eq(-5 * 60 * 60));
  EXPECT_THAT(zone.dst_abbr, Eq("EDT"));
  EXPECT_THAT(zone.dst_offset, Eq(-4 * 60 * 60));
  EXPECT_THAT(zone.dst_start.date.fmt, Eq(PosixTransition::M));
  EXPECT_THAT(zone.dst_start.date.m.month, Eq(3));
  EXPECT_THAT(zone.dst_start.date.m.week, Eq(2));
  EXPECT_THAT(zone.dst_start.date.m.weekday, Eq(0));
  EXPECT_THAT(zone.dst_start.time.offset, Eq(2 * 60 * 60));
  EXPECT_THAT(zone.dst_end.date.fmt, Eq(PosixTransition::M));
  EXPECT_THAT(zone.dst_end.date.m.month, Eq(11));
  EXPECT_THAT(zone.dst_end.date.m.week, Eq(1));
  EXPECT_THAT(zone.dst_end.date.m.weekday, Eq(0));
  EXPECT_THAT(zone.dst_end.time.offset, Eq(2 * 60 * 60));

  // Australia/Adelaide
  EXPECT_TRUE(ParsePosixSpec("ACST-9:30ACDT,M10.1.0,M4.1.0/3", &zone));
  EXPECT_THAT(zone.std_abbr, Eq("ACST"));
  EXPECT_THAT(zone.std_offset, Eq((9 * 60 + 30) * 60));
  EXPECT_THAT(zone.dst_abbr, Eq("ACDT"));
  EXPECT_THAT(zone.dst_offset, Eq((10 * 60 + 30) * 60));
  EXPECT_THAT(zone.dst_start.date.fmt, Eq(PosixTransition::M));
  EXPECT_THAT(zone.dst_start.date.m.month, Eq(10));
  EXPECT_THAT(zone.dst_start.date.m.week, Eq(1));
  EXPECT_THAT(zone.dst_start.date.m.weekday, Eq(0));
  EXPECT_THAT(zone.dst_start.time.offset, Eq(2 * 60 * 60));
  EXPECT_THAT(zone.dst_end.date.fmt, Eq(PosixTransition::M));
  EXPECT_THAT(zone.dst_end.date.m.month, Eq(4));
  EXPECT_THAT(zone.dst_end.date.m.week, Eq(1));
  EXPECT_THAT(zone.dst_end.date.m.weekday, Eq(0));
  EXPECT_THAT(zone.dst_end.time.offset, Eq(3 * 60 * 60));

  // Australia/Lord_Howe
  EXPECT_TRUE(ParsePosixSpec("<+1030>-10:30<+11>-11,M10.1.0,M4.1.0", &zone));
  EXPECT_THAT(zone.std_abbr, Eq("+1030"));
  EXPECT_THAT(zone.std_offset, Eq((10 * 60 + 30) * 60));
  EXPECT_THAT(zone.dst_abbr, Eq("+11"));
  EXPECT_THAT(zone.dst_offset, Eq(11 * 60 * 60));
  EXPECT_THAT(zone.dst_start.date.fmt, Eq(PosixTransition::M));
  EXPECT_THAT(zone.dst_start.date.m.month, Eq(10));
  EXPECT_THAT(zone.dst_start.date.m.week, Eq(1));
  EXPECT_THAT(zone.dst_start.date.m.weekday, Eq(0));
  EXPECT_THAT(zone.dst_start.time.offset, Eq(2 * 60 * 60));
  EXPECT_THAT(zone.dst_end.date.fmt, Eq(PosixTransition::M));
  EXPECT_THAT(zone.dst_end.date.m.month, Eq(4));
  EXPECT_THAT(zone.dst_end.date.m.week, Eq(1));
  EXPECT_THAT(zone.dst_end.date.m.weekday, Eq(0));
  EXPECT_THAT(zone.dst_end.time.offset, Eq(2 * 60 * 60));

  // Africa/Casablanca (year-round DST)
  EXPECT_TRUE(ParsePosixSpec("<+00>0<+01>,0/0,J365/25", &zone));
  EXPECT_THAT(zone.std_abbr, Eq("+00"));
  EXPECT_THAT(zone.std_offset, Eq(0));
  EXPECT_THAT(zone.dst_abbr, Eq("+01"));
  EXPECT_THAT(zone.dst_offset, Eq(1 * 60 * 60));
  EXPECT_THAT(zone.dst_start.date.fmt, Eq(PosixTransition::N));
  EXPECT_THAT(zone.dst_start.date.n.day, Eq(0));
  EXPECT_THAT(zone.dst_start.time.offset, Eq(0));
  EXPECT_THAT(zone.dst_end.date.fmt, Eq(PosixTransition::J));
  EXPECT_THAT(zone.dst_end.date.n.day, Eq(365));
  EXPECT_THAT(zone.dst_end.time.offset, Eq(25 * 60 * 60));
}

TEST(TimeZonePosix, ParseErrors) {
  PosixTimeZone zone;

  // STD abbreviation errors.
  EXPECT_FALSE(ParsePosixSpec("ET5", &zone));
  EXPECT_FALSE(ParsePosixSpec("ET+", &zone));
  EXPECT_FALSE(ParsePosixSpec("ET-", &zone));
  EXPECT_FALSE(ParsePosixSpec("ET,", &zone));
  EXPECT_FALSE(ParsePosixSpec("<", &zone));
  EXPECT_FALSE(ParsePosixSpec("<00", &zone));
  EXPECT_FALSE(ParsePosixSpec("<>0", &zone));

  // STD offset errors.
  EXPECT_FALSE(ParsePosixSpec("<00>", &zone));
  EXPECT_FALSE(ParsePosixSpec("<00>+", &zone));
  EXPECT_FALSE(ParsePosixSpec("<00>-", &zone));
  EXPECT_FALSE(ParsePosixSpec("<00>?", &zone));

  // DST abbreviation errors.
  EXPECT_FALSE(ParsePosixSpec("EST5DT,M3.2.0,M11.1.0", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT+,M3.2.0,M11.1.0", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT-,M3.2.0,M11.1.0", &zone));
  EXPECT_FALSE(ParsePosixSpec("<00>0<-01", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5<>,M3.2.0,M11.1.0", &zone));

  // DST offset errors.
  EXPECT_FALSE(ParsePosixSpec("<01>1<00>?,0,0", &zone));
  EXPECT_FALSE(ParsePosixSpec("<01>1<00>+?,0,0", &zone));
  EXPECT_FALSE(ParsePosixSpec("<01>1<00>-?,0,0", &zone));

  // Malformed DST start date/time.
  EXPECT_FALSE(ParsePosixSpec("EST5EDT", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,M13.2.0,M11.1.0", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,M3.6.0,M11.1.0", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,M3.2.7,M11.1.0", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,J0,M11.1.0", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,366,M11.1.0", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,M3.2.0/,M11.1.0", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,M3.2.0/?,M11.1.0", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,M3.2.0/1:?,M11.1.0", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,M3.2.0/1:2:?,M11.1.0", &zone));

  // Malformed DST end date/time.
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,M3.2.0", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,M3.2.0,M?.1.0", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,M3.2.0,M11.?.0", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,M3.2.0,M11.1.?", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,M3.2.0,J?", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,M3.2.0,?", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,M3.2.0,M11.1.0/", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,M3.2.0,M11.1.0/168", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,M3.2.0,M11.1.0/167:60", &zone));
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,M3.2.0,M11.1.0/167:59:60", &zone));

  // Trailing data.
  EXPECT_FALSE(ParsePosixSpec("EST5EDT,M3.2.0,M11.1.0junk", &zone));
}

}  // namespace cctz
}  // namespace time_internal
ABSL_NAMESPACE_END
}  // namespace absl
