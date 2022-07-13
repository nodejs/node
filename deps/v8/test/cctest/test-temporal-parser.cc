// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate.h"
#include "src/handles/global-handles.h"
#include "src/heap/factory-inl.h"
#include "src/init/v8.h"
#include "src/temporal/temporal-parser.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

// This file test the TemporalParser to parse ISO 8601 according to
// #sec-temporal-iso8601grammar

// For int32_t fields in ParsedISO8601Result, kMinInt31 denote 'undefined'
// value.
const int32_t kUndefined = kMinInt31;

void CheckCalendar(Isolate* isolate, Handle<String> iso_string,
                   int32_t calendar_start, int32_t calendar_length,
                   const std::string& expected_calendar) {
  Handle<String> actual_calendar = isolate->factory()->NewSubString(
      iso_string, calendar_start, calendar_start + calendar_length);
  CHECK(expected_calendar == actual_calendar->ToCString().get());
}
void CheckDate(const ParsedISO8601Result& actual, int32_t date_year,
               int32_t date_month, int32_t date_day) {
  CHECK_EQ(date_year, actual.date_year);
  CHECK_EQ(date_month, actual.date_month);
  CHECK_EQ(date_day, actual.date_day);
}

void CheckTime(const ParsedISO8601Result& actual, int32_t time_hour,
               int32_t time_minute, int32_t time_second,
               int32_t time_nanosecond) {
  CHECK_EQ(time_hour, actual.time_hour);
  CHECK_EQ(time_minute, actual.time_minute);
  CHECK_EQ(time_second, actual.time_second);
  CHECK_EQ(time_nanosecond, actual.time_nanosecond);
}

void CheckTimeZoneNumericUTCOffset(const ParsedISO8601Result& actual,
                                   int32_t tzuo_sign, int32_t tzuo_hour,
                                   int32_t tzuo_minute, int32_t tzuo_second,
                                   int32_t tzuo_nanosecond) {
  CHECK_EQ(tzuo_sign, actual.tzuo_sign);
  CHECK_EQ(tzuo_hour, actual.tzuo_hour);
  CHECK_EQ(tzuo_minute, actual.tzuo_minute);
  CHECK_EQ(tzuo_second, actual.tzuo_second);
  CHECK_EQ(tzuo_nanosecond, actual.tzuo_nanosecond);
}

#define IMPL_VERIFY_PARSE_TEMPORAL_DATE_STRING_SUCCESS(R)                     \
  void VerifyParseTemporal##R##StringSuccess(                                 \
      Isolate* isolate, const char* str, int32_t date_year,                   \
      int32_t date_month, int32_t date_day, const char* calendar_name) {      \
    Handle<String> input = CcTest::MakeString(str);                           \
    ParsedISO8601Result actual =                                              \
        TemporalParser::ParseTemporal##R##String(isolate, input).ToChecked(); \
    CheckDate(actual, date_year, date_month, date_day);                       \
    CheckCalendar(isolate, input, actual.calendar_name_start,                 \
                  actual.calendar_name_length, calendar_name);                \
  }

IMPL_VERIFY_PARSE_TEMPORAL_DATE_STRING_SUCCESS(Date)
IMPL_VERIFY_PARSE_TEMPORAL_DATE_STRING_SUCCESS(YearMonth)
IMPL_VERIFY_PARSE_TEMPORAL_DATE_STRING_SUCCESS(MonthDay)
IMPL_VERIFY_PARSE_TEMPORAL_DATE_STRING_SUCCESS(RelativeTo)

#define IMPL_VERIFY_PARSE_TEMPORAL_DATE_TIME_STRING_SUCCESS(R)                \
  void VerifyParseTemporal##R##StringSuccess(                                 \
      Isolate* isolate, const char* str, int32_t date_year,                   \
      int32_t date_month, int32_t date_day, int32_t time_hour,                \
      int32_t time_minute, int32_t time_second, int32_t time_nanosecond,      \
      const char* calendar_name) {                                            \
    Handle<String> input = CcTest::MakeString(str);                           \
    ParsedISO8601Result actual =                                              \
        TemporalParser::ParseTemporal##R##String(isolate, input).ToChecked(); \
    CheckDate(actual, date_year, date_month, date_day);                       \
    CheckCalendar(isolate, input, actual.calendar_name_start,                 \
                  actual.calendar_name_length, calendar_name);                \
    CheckTime(actual, time_hour, time_minute, time_second, time_nanosecond);  \
  }

IMPL_VERIFY_PARSE_TEMPORAL_DATE_TIME_STRING_SUCCESS(DateTime)
IMPL_VERIFY_PARSE_TEMPORAL_DATE_TIME_STRING_SUCCESS(RelativeTo)

#define IMPL_VERIFY_PARSE_TEMPORAL_ZONED_DATE_TIME_STRING_SUCCESS(R)           \
  void VerifyParseTemporal##R##StringSuccess(                                  \
      Isolate* isolate, const char* str, int32_t date_year,                    \
      int32_t date_month, int32_t date_day, int32_t time_hour,                 \
      int32_t time_minute, int32_t time_second, int32_t time_nanosecond,       \
      const char* calendar_name, int32_t tzuo_sign, int32_t tzuo_hour,         \
      int32_t tzuo_minute, int32_t tzuo_second, int32_t tzuo_nanosecond,       \
      bool utc_designator, const char* tzi_name) {                             \
    Handle<String> input = CcTest::MakeString(str);                            \
    ParsedISO8601Result actual =                                               \
        TemporalParser::ParseTemporal##R##String(isolate, input).ToChecked();  \
    CheckDate(actual, date_year, date_month, date_day);                        \
    CheckCalendar(isolate, input, actual.calendar_name_start,                  \
                  actual.calendar_name_length, calendar_name);                 \
    CheckTime(actual, time_hour, time_minute, time_second, time_nanosecond);   \
    CHECK_EQ(utc_designator, actual.utc_designator);                           \
    std::string actual_tzi_name(str + actual.tzi_name_start,                   \
                                actual.tzi_name_length);                       \
    CHECK(actual_tzi_name == tzi_name);                                        \
    if (!utc_designator) {                                                     \
      CheckTimeZoneNumericUTCOffset(actual, tzuo_sign, tzuo_hour, tzuo_minute, \
                                    tzuo_second, tzuo_nanosecond);             \
    }                                                                          \
  }

IMPL_VERIFY_PARSE_TEMPORAL_ZONED_DATE_TIME_STRING_SUCCESS(ZonedDateTime)
IMPL_VERIFY_PARSE_TEMPORAL_ZONED_DATE_TIME_STRING_SUCCESS(RelativeTo)

void VerifyParseTemporalInstantStringSuccess(
    Isolate* isolate, const char* str, bool utc_designator, int32_t tzuo_sign,
    int32_t tzuo_hour, int32_t tzuo_minute, int32_t tzuo_second,
    int32_t tzuo_nanosecond) {
  Handle<String> input = CcTest::MakeString(str);
  ParsedISO8601Result actual =
      TemporalParser::ParseTemporalInstantString(isolate, input).ToChecked();
  CHECK_EQ(utc_designator, actual.utc_designator);
  if (!utc_designator) {
    CheckTimeZoneNumericUTCOffset(actual, tzuo_sign, tzuo_hour, tzuo_minute,
                                  tzuo_second, tzuo_nanosecond);
  }
}

void VerifyParseTemporalCalendarStringSuccess(
    Isolate* isolate, const char* str, const std::string& calendar_name) {
  Handle<String> input = CcTest::MakeString(str);
  ParsedISO8601Result actual =
      TemporalParser::ParseTemporalCalendarString(isolate, input).ToChecked();
  CheckCalendar(isolate, input, actual.calendar_name_start,
                actual.calendar_name_length, calendar_name);
}

#define VERIFY_PARSE_FAIL(R, str)                                \
  do {                                                           \
    Handle<String> input = CcTest::MakeString(str);              \
    CHECK(TemporalParser::Parse##R(isolate, input).IsNothing()); \
  } while (false)

void VerifyParseTemporalTimeStringSuccess(
    Isolate* isolate, const char* str, int32_t time_hour, int32_t time_minute,
    int32_t time_second, int32_t time_nanosecond, const char* calendar_name) {
  Handle<String> input = CcTest::MakeString(str);
  ParsedISO8601Result actual =
      TemporalParser::ParseTemporalTimeString(isolate, input).ToChecked();
  CheckTime(actual, time_hour, time_minute, time_second, time_nanosecond);
  CheckCalendar(isolate, input, actual.calendar_name_start,
                actual.calendar_name_length, calendar_name);
}

TEST(TemporalDateStringSuccess) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  // CalendarDateTime : DateTime Calendaropt
  // DateTime
  // DateYear - DateMonth - DateDay
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-03", 2021, 11, 03, "");
  // DateYear DateMonth DateDay
  VerifyParseTemporalDateStringSuccess(isolate, "20211103", 2021, 11, 03, "");
  // DateExtendedYear
  VerifyParseTemporalDateStringSuccess(isolate, "+002021-11-03", 2021, 11, 03,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "+000001-11-03", 1, 11, 03, "");
  VerifyParseTemporalDateStringSuccess(isolate, "+0020211103", 2021, 11, 03,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "+0000011231", 1, 12, 31, "");
  VerifyParseTemporalDateStringSuccess(isolate, "+0000000101", 0, 1, 1, "");
  VerifyParseTemporalDateStringSuccess(isolate, "+0000000101", 0, 1, 1, "");
  VerifyParseTemporalDateStringSuccess(isolate, "-0000000101", 0, 1, 1, "");
  VerifyParseTemporalDateStringSuccess(isolate, u8"\u22120000000101", 0, 1, 1,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "-0000000101", 0, 1, 1, "");
  VerifyParseTemporalDateStringSuccess(isolate, "+654321-11-03", 654321, 11, 3,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "+999999-12-31", 999999, 12, 31,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "-654321-11-03", -654321, 11, 3,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "-999999-12-31", -999999, 12,
                                       31, "");
  VerifyParseTemporalDateStringSuccess(isolate, u8"\u2212999999-12-31", -999999,
                                       12, 31, "");
  VerifyParseTemporalDateStringSuccess(isolate, "+6543211103", 654321, 11, 3,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "+9999991231", 999999, 12, 31,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "-6543211103", -654321, 11, 3,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "-9999991231", -999999, 12, 31,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, u8"\u22129999991231", -999999,
                                       12, 31, "");

  // DateTime: Date TimeSpecSeparator_opt TimeZone_opt
  // Date TimeSpecSeparator
  // Differeent DateTimeSeparator: <S> T or t
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09T01", 2021, 11, 9,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-12-07t01", 2021, 12, 7,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-09-31 01", 2021, 9, 31,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09T0102", 2021, 11, 9,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-12-07t01:02", 2021, 12, 7,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-09-31 01:03:04", 2021, 9,
                                       31, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-09-31 01:03:60", 2021, 9,
                                       31, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-09-31 010304", 2021, 9,
                                       31, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-09-31 01:03:04.987654321",
                                       2021, 9, 31, "");
  VerifyParseTemporalDateStringSuccess(isolate, "1964-07-10 01:03:04,1", 1964,
                                       7, 10, "");
  VerifyParseTemporalDateStringSuccess(isolate, "1964-07-10 01:03:60,1", 1964,
                                       7, 10, "");
  VerifyParseTemporalDateStringSuccess(isolate, "1964-07-10 01:03:04,123456789",
                                       1964, 7, 10, "");
  VerifyParseTemporalDateStringSuccess(isolate, "19640710 01:03:04,123456789",
                                       1964, 7, 10, "");
  // Date TimeZone
  // Date TimeZoneOffsetRequired
  // Date TimeZoneUTCOffset TimeZoneBracketedAnnotation_opt
  // Date TimeZoneNumericUTCOffset
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09+11", 2021, 11, 9,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09-12:03", 2021, 11, 9,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09-1203", 2021, 11, 9,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09-12:03:04", 2021, 11,
                                       9, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09-120304", 2021, 11,
                                       9, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09-12:03:04,987654321",
                                       2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09-120304.123456789",
                                       2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09T03+11", 2021, 11, 9,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09t04:55-12:03", 2021,
                                       11, 9, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09t06:22:01.987654321",
                                       2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09t062202,987654321",
                                       2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(
      isolate, "2021-11-09t06:22:03.987654321-1203", 2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(
      isolate, "2021-11-09 062204.987654321-1203", 2021, 11, 9, "");

  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09T12-12:03:04", 2021,
                                       11, 9, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09t1122-120304", 2021,
                                       11, 9, "");
  VerifyParseTemporalDateStringSuccess(
      isolate, "2021-11-09 223344-12:03:04,987654321", 2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(
      isolate, "2021-11-09T223344.987654321-120304.123456789", 2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(
      isolate, "19670316T223344.987654321-120304.123456789", 1967, 3, 16, "");
  // Date UTCDesignator
  // Date UTCDesignator
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09z", 2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09Z", 2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09T11z", 2021, 11, 9,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09t12Z", 2021, 11, 9,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09 01:23Z", 2021, 11,
                                       9, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09 01:23:45Z", 2021,
                                       11, 9, "");
  VerifyParseTemporalDateStringSuccess(
      isolate, "2021-11-09 01:23:45.678912345Z", 2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(
      isolate, "2021-11-09 01:23:45,567891234Z", 2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09 0123Z", 2021, 11, 9,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09 012345Z", 2021, 11,
                                       9, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09t012345.678912345Z",
                                       2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09 012345,891234567Z",
                                       2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(isolate, "20211109T012345,891234567Z",
                                       2021, 11, 9, "");
  // Date TimeZoneNameRequired
  // Date TimeZoneBracketedAnnotation
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09[Etc/GMT+01]", 2021,
                                       11, 9, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09[Etc/GMT-23]", 2021,
                                       11, 9, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09[Etc/GMT+23]", 2021,
                                       11, 9, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09[Etc/GMT-00]", 2021,
                                       11, 9, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09Z[Etc/GMT+01]", 2021,
                                       11, 9, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09z[Etc/GMT-23]", 2021,
                                       11, 9, "");
  VerifyParseTemporalDateStringSuccess(
      isolate, "2021-11-09 23:45:56.891234567Z[Etc/GMT+23]", 2021, 11, 9, "");
  // TimeZoneIANAName
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09[ABCDEFGHIJKLMN]",
                                       2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(
      isolate, "2021-11-09[ABCDEFGHIJKLMN/abcdefghijklmn/opeqrstuv]", 2021, 11,
      9, "");
  VerifyParseTemporalDateStringSuccess(
      isolate, "2021-11-09[aBcDEfGHiJ.L_N/ABC...G_..KLMN]", 2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(
      isolate, "2021-11-09[aBcDE-GHiJ.L_N/ABCbcdG-IJKLMN]", 2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(
      isolate, "2021-11-09T12z[.BCDEFGHIJKLMN]", 2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(
      isolate, "2021-11-09T23:45Z[ABCDEFGHIJKLMN/_bcde-ghij_lmn/.peqrstuv]",
      2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(
      isolate, "2021-11-09t234534.234+1234[aBcDEfGHiJ.L_N/ABC...G_..KLMN]",
      2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(
      isolate,
      "2021-11-09 "
      "123456.789123456-012345.789123456[aBcDEfGHiJ.L_N/ABCbcdGfIJKLMN]",
      2021, 11, 9, "");
  // TimeZoneUTCOffsetName
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09[+12]", 2021, 11, 9,
                                       "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09[+12:34]", 2021, 11,
                                       9, "");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-09[-12:34:56]", 2021,
                                       11, 9, "");
  VerifyParseTemporalDateStringSuccess(
      isolate, "2021-11-09[+12:34:56,789123456]", 2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(
      isolate, "2021-11-09[+12:34:56.789123456]", 2021, 11, 9, "");
  VerifyParseTemporalDateStringSuccess(
      isolate, u8"2021-11-09[\u221200:34:56.789123456]", 2021, 11, 9, "");

  // Date TimeSpecSeparator TimeZone
  // DateTime Calendaropt
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-03[u-ca=abc]", 2021,
                                       11, 03, "abc");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-03[u-ca=iso-8601]",
                                       2021, 11, 03, "iso-8601");
  VerifyParseTemporalDateStringSuccess(isolate, "2021-11-03[u-ca=123456-789]",
                                       2021, 11, 03, "123456-789");
  VerifyParseTemporalDateStringSuccess(isolate,
                                       "2021-03-11[u-ca=abcdefgh-wxyzefg]",
                                       2021, 3, 11, "abcdefgh-wxyzefg");
  VerifyParseTemporalDateStringSuccess(
      isolate, "2021-03-11[u-ca=abcdefgh-wxyzefg-ijklmnop]", 2021, 3, 11,
      "abcdefgh-wxyzefg-ijklmnop");

  VerifyParseTemporalDateStringSuccess(
      isolate,
      "2021-11-03 "
      "123456.789123456-012345.789123456[aBcDEfGHiJ.L_N/"
      "ABCbcdGfIJKLMN][u-ca=abc]",
      2021, 11, 03, "abc");
  VerifyParseTemporalDateStringSuccess(
      isolate, "2021-03-11[+12:34:56,789123456][u-ca=abcdefgh-wxyzefg]", 2021,
      3, 11, "abcdefgh-wxyzefg");
  VerifyParseTemporalDateStringSuccess(
      isolate,
      u8"20210311[\u221200:34:56.789123456][u-ca=abcdefgh-wxyzefg-ijklmnop]",
      2021, 3, 11, "abcdefgh-wxyzefg-ijklmnop");
}

#define VERIFY_PARSE_FAIL_ON_DATE(R)                            \
  do {                                                          \
    VERIFY_PARSE_FAIL(R, "");                                   \
    /* sign only go with DateExtendedYear */                    \
    VERIFY_PARSE_FAIL(R, "+2021-03-04");                        \
    VERIFY_PARSE_FAIL(R, "-2021-03-04");                        \
    /* 1, 2, 3, 5 digits are not year */                        \
    VERIFY_PARSE_FAIL(R, "921-03-04");                          \
    VERIFY_PARSE_FAIL(R, "-821-03-04");                         \
    VERIFY_PARSE_FAIL(R, "9210304");                            \
    VERIFY_PARSE_FAIL(R, "-8210304");                           \
    VERIFY_PARSE_FAIL(R, "21-03-04");                           \
    VERIFY_PARSE_FAIL(R, "-31-03-04");                          \
    VERIFY_PARSE_FAIL(R, u8"\u221231-03-04");                   \
    VERIFY_PARSE_FAIL(R, "-310304");                            \
    VERIFY_PARSE_FAIL(R, "1-03-04");                            \
    VERIFY_PARSE_FAIL(R, "-3-03-04");                           \
    VERIFY_PARSE_FAIL(R, "10304");                              \
    VERIFY_PARSE_FAIL(R, "-30304");                             \
    VERIFY_PARSE_FAIL(R, "12921-03-04");                        \
    VERIFY_PARSE_FAIL(R, "-32821-03-04");                       \
    VERIFY_PARSE_FAIL(R, "129210304");                          \
    VERIFY_PARSE_FAIL(R, "-328210304");                         \
    VERIFY_PARSE_FAIL(R, "123456-03-04");                       \
    VERIFY_PARSE_FAIL(R, "1234560304");                         \
                                                                \
    /* 7 digits year */                                         \
    VERIFY_PARSE_FAIL(R, "0002021-09-03");                      \
    VERIFY_PARSE_FAIL(R, "-0002021-09-03");                     \
                                                                \
    /* single digit month */                                    \
    VERIFY_PARSE_FAIL(R, "1900-9-03");                          \
    VERIFY_PARSE_FAIL(R, "1900903");                            \
    /* out of range month */                                    \
    VERIFY_PARSE_FAIL(R, "1900-13-03");                         \
    VERIFY_PARSE_FAIL(R, "19001401");                           \
    /* single digit day */                                      \
    VERIFY_PARSE_FAIL(R, "1900-12-3");                          \
    VERIFY_PARSE_FAIL(R, "1900121");                            \
    /* Out of range day */                                      \
    VERIFY_PARSE_FAIL(R, "1900-12-32");                         \
    VERIFY_PARSE_FAIL(R, "19001232");                           \
    VERIFY_PARSE_FAIL(R, "1900-12-00");                         \
    VERIFY_PARSE_FAIL(R, "19001200");                           \
                                                                \
    /* Legal Date with other illegal stuff */                   \
    /* only with DateTimeSeparator */                           \
    VERIFY_PARSE_FAIL(R, "1900-12-31 ");                        \
    VERIFY_PARSE_FAIL(R, "19001231T");                          \
    VERIFY_PARSE_FAIL(R, "1900-12-31t");                        \
    VERIFY_PARSE_FAIL(R, "19001231 ");                          \
                                                                \
    /* Single digit hour */                                     \
    VERIFY_PARSE_FAIL(R, "1900-12-31 1");                       \
    VERIFY_PARSE_FAIL(R, "19001231T2");                         \
                                                                \
    /* Out of range hour */                                     \
    VERIFY_PARSE_FAIL(R, "1900-12-31t24");                      \
    VERIFY_PARSE_FAIL(R, "19001231 -1");                        \
                                                                \
    /* Single digit minute */                                   \
    VERIFY_PARSE_FAIL(R, "1900-12-31 03:1");                    \
    VERIFY_PARSE_FAIL(R, "19001231T024");                       \
                                                                \
    /* Out of range minute */                                   \
    VERIFY_PARSE_FAIL(R, "1900-12-31t04:61");                   \
    VERIFY_PARSE_FAIL(R, "19001231 23:70");                     \
                                                                \
    /* Single digit second */                                   \
    VERIFY_PARSE_FAIL(R, "1900-12-31 03:22:9");                 \
    VERIFY_PARSE_FAIL(R, "19001231T02494");                     \
                                                                \
    /* Out of range second */                                   \
    VERIFY_PARSE_FAIL(R, "1900-12-31t04:23:61");                \
    VERIFY_PARSE_FAIL(R, "19001231 23:12:80");                  \
                                                                \
    /* DecimalSeparator without TimeFractionalPart */           \
    VERIFY_PARSE_FAIL(R, "1900-12-31 03:22:09,");               \
    VERIFY_PARSE_FAIL(R, "19001231T024904.");                   \
                                                                \
    /* TimeFractionalPart too long */                           \
    VERIFY_PARSE_FAIL(R, "1900-12-31 03:22:09,9876543219");     \
    VERIFY_PARSE_FAIL(R, "19001231T024904.1234567890");         \
                                                                \
    /* Legal Date with illegal TimeZoneUTCOffset */             \
    VERIFY_PARSE_FAIL(R, "1900-12-31+1");                       \
    VERIFY_PARSE_FAIL(R, "1900-12-31+12:2");                    \
    VERIFY_PARSE_FAIL(R, "1900-12-31+122");                     \
    VERIFY_PARSE_FAIL(R, "1900-12-31+12:23:3");                 \
    VERIFY_PARSE_FAIL(R, "1900-12-31+12233");                   \
    VERIFY_PARSE_FAIL(R, "1900-12-31+12:23:45.");               \
    VERIFY_PARSE_FAIL(R, "1900-12-31+122345,");                 \
    VERIFY_PARSE_FAIL(R, "1900-12-31+12:23:45.1234567890");     \
    VERIFY_PARSE_FAIL(R, "1900-12-31+122345,0987654321");       \
    /* Legal Date with illegal [TimeZoneIANAName] */            \
    VERIFY_PARSE_FAIL(R, "1900-12-31[.]");                      \
    VERIFY_PARSE_FAIL(R, "1900-12-31[..]");                     \
    VERIFY_PARSE_FAIL(R, "1900-12-31[abc/.]");                  \
    VERIFY_PARSE_FAIL(R, "1900-12-31[abc/..]");                 \
    VERIFY_PARSE_FAIL(R, "1900-12-31[abcdefghijklmno]");        \
    VERIFY_PARSE_FAIL(R, "1900-12-31[abcdefghijklmn/-abcde]");  \
    VERIFY_PARSE_FAIL(R, "1900-12-31[-bcdefghijklmn/abcde]");   \
    VERIFY_PARSE_FAIL(R, "1900-12-31[abcdefghi//abde]");        \
    /* Legal Date with illegal [Etc/GMT ASCIISign Hour] */      \
    VERIFY_PARSE_FAIL(R, "1900-12-31[ETC/GMT+10]");             \
    /* Wrong case for Etc */                                    \
    VERIFY_PARSE_FAIL(R, "1900-12-31[etc/GMT-10]");             \
    /* Wrong case for GMT */                                    \
    VERIFY_PARSE_FAIL(R, "1900-12-31[Etc/gmt+00]");             \
    VERIFY_PARSE_FAIL(R, "1900-12-31[Etc/gmt-00]");             \
    VERIFY_PARSE_FAIL(R, "1900-12-31[Etc/gMt+00]");             \
    VERIFY_PARSE_FAIL(R, "1900-12-31[Etc/gmT-00]");             \
    /* not ASCII sign */                                        \
    VERIFY_PARSE_FAIL(R, u8"1900-12-31[Etc/GMT\u221200]");      \
    /* Out of range */                                          \
    VERIFY_PARSE_FAIL(R, "1900-12-31[Etc/GMT+24]");             \
    VERIFY_PARSE_FAIL(R, "1900-12-31[Etc/GMT-24]");             \
    /* Single digit Hour */                                     \
    VERIFY_PARSE_FAIL(R, "1900-12-31[Etc/GMT+2]");              \
    VERIFY_PARSE_FAIL(R, "1900-12-31[Etc/GMT-0]");              \
    /* Three digit hour */                                      \
    VERIFY_PARSE_FAIL(R, "1900-12-31[Etc/GMT+201]");            \
    VERIFY_PARSE_FAIL(R, "1900-12-31[Etc/GMT-000]");            \
    /* With minute */                                           \
    VERIFY_PARSE_FAIL(R, "1900-12-31[Etc/GMT+05:30]");          \
    VERIFY_PARSE_FAIL(R, "1900-12-31[Etc/GMT+0530]");           \
    /* Legal Date with illegal [TimeZoneUTCOffsetName] */       \
    VERIFY_PARSE_FAIL(R, "1900-12-31[+]");                      \
    VERIFY_PARSE_FAIL(R, "1900-12-31[-]");                      \
    VERIFY_PARSE_FAIL(R, "1900-12-31[+0]");                     \
    VERIFY_PARSE_FAIL(R, "1900-12-31[-1]");                     \
    VERIFY_PARSE_FAIL(R, "1900-12-31[+01:]");                   \
    VERIFY_PARSE_FAIL(R, "1900-12-31[+24]");                    \
    VERIFY_PARSE_FAIL(R, "1900-12-31[-25]");                    \
    VERIFY_PARSE_FAIL(R, "1900-12-31[+01:2]");                  \
    VERIFY_PARSE_FAIL(R, "1900-12-31[-012]");                   \
    VERIFY_PARSE_FAIL(R, "1900-12-31[+01:60]");                 \
    VERIFY_PARSE_FAIL(R, "1900-12-31[-23:60]");                 \
    VERIFY_PARSE_FAIL(R, "1900-12-31[+2360]");                  \
    VERIFY_PARSE_FAIL(R, u8"1900-12-31[\u22121260]");           \
    VERIFY_PARSE_FAIL(R, "1900-12-31[+01:23:]");                \
    VERIFY_PARSE_FAIL(R, "1900-12-31[-01234]");                 \
    VERIFY_PARSE_FAIL(R, "1900-12-31[+01:23:4]");               \
    VERIFY_PARSE_FAIL(R, "1900-12-31[+01:23:61]");              \
    VERIFY_PARSE_FAIL(R, "1900-12-31[-012372]");                \
    VERIFY_PARSE_FAIL(R, "1900-12-31[+01:23:45.]");             \
    VERIFY_PARSE_FAIL(R, "1900-12-31[+01:23:45,]");             \
    VERIFY_PARSE_FAIL(R, "1900-12-31[+01:23:45.1234567890]");   \
    VERIFY_PARSE_FAIL(R, "1900-12-31[-01:23:45,0000000000]");   \
    VERIFY_PARSE_FAIL(R, "1900-12-31[+01:23:4a]");              \
    VERIFY_PARSE_FAIL(R, "1900-12-31[+01:b3:40]");              \
    VERIFY_PARSE_FAIL(R, "1900-12-31[+abcdefg]");               \
    /* Legal Date with illegal [CalendarName] */                \
    VERIFY_PARSE_FAIL(R, "1900-12-31[u-ca=]");                  \
    VERIFY_PARSE_FAIL(R, "1900-12-31[u-ca=123456789]");         \
    VERIFY_PARSE_FAIL(R, "1900-12-31[u-ca=a]");                 \
    VERIFY_PARSE_FAIL(R, "1900-12-31[u-ca=ab]");                \
    VERIFY_PARSE_FAIL(R, "1900-12-31[u-ca=abcdefghi]");         \
    VERIFY_PARSE_FAIL(R, "1900-12-31[u-ca=a-abcdefgh]");        \
    VERIFY_PARSE_FAIL(R, "1900-12-31[u-ca=ab-abcdefgh]");       \
    VERIFY_PARSE_FAIL(R, "1900-12-31[u-ca=abc-abcdefghi]");     \
    VERIFY_PARSE_FAIL(R, "1900-12-31[u-ca=abc-def-ghijklmno]"); \
  } while (false)

TEST(TemporalDateStringIllegal) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  VERIFY_PARSE_FAIL_ON_DATE(TemporalDateString);
  VERIFY_PARSE_FAIL(TemporalDateString, "+20210304");
  VERIFY_PARSE_FAIL(TemporalDateString, "-20210304");
  VERIFY_PARSE_FAIL(TemporalDateString, u8"\u221220210304");
  VERIFY_PARSE_FAIL(TemporalDateString, "210304");
}

void VerifyTemporalTimeStringTimeUndefined(Isolate* isolate, const char* str) {
  VerifyParseTemporalTimeStringSuccess(isolate, str, kUndefined, kUndefined,
                                       kUndefined, kUndefined, "");
}

TEST(TemporalTimeStringSuccess) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  // DateTime
  // DateYear - DateMonth - DateDay
  VerifyTemporalTimeStringTimeUndefined(isolate, "2021-11-03");
  // DateYear DateMonth DateDay
  VerifyTemporalTimeStringTimeUndefined(isolate, "20211103");
  // DateExtendedYear
  VerifyTemporalTimeStringTimeUndefined(isolate, "+002021-11-03");
  VerifyTemporalTimeStringTimeUndefined(isolate, "+000001-11-03");
  VerifyTemporalTimeStringTimeUndefined(isolate, "+0020211103");
  VerifyTemporalTimeStringTimeUndefined(isolate, "+0000011231");
  VerifyTemporalTimeStringTimeUndefined(isolate, "+0000000101");
  VerifyTemporalTimeStringTimeUndefined(isolate, "+0000000101");
  VerifyTemporalTimeStringTimeUndefined(isolate, "-0000000101");
  VerifyTemporalTimeStringTimeUndefined(isolate, u8"\u22120000000101");
  VerifyTemporalTimeStringTimeUndefined(isolate, "-0000000101");
  VerifyTemporalTimeStringTimeUndefined(isolate, "+654321-11-03");
  VerifyTemporalTimeStringTimeUndefined(isolate, "+999999-12-31");
  VerifyTemporalTimeStringTimeUndefined(isolate, "-654321-11-03");
  VerifyTemporalTimeStringTimeUndefined(isolate, "-999999-12-31");
  VerifyTemporalTimeStringTimeUndefined(isolate, u8"\u2212999999-12-31");
  VerifyTemporalTimeStringTimeUndefined(isolate, "+6543211103");
  VerifyTemporalTimeStringTimeUndefined(isolate, "+9999991231");
  VerifyTemporalTimeStringTimeUndefined(isolate, "-6543211103");
  VerifyTemporalTimeStringTimeUndefined(isolate, "-9999991231");
  VerifyTemporalTimeStringTimeUndefined(isolate, u8"\u22129999991231");

  // DateTime: Date TimeSpecSeparator_opt TimeZone_opt
  // Date TimeSpecSeparator
  // Differeent DateTimeSeparator: <S> T or t
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-11-09T01", 1, kUndefined,
                                       kUndefined, kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-12-07t23", 23, kUndefined,
                                       kUndefined, kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-09-31 02", 2, kUndefined,
                                       kUndefined, kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-11-09T0304", 3, 4,
                                       kUndefined, kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-12-07t05:16", 5, 16,
                                       kUndefined, kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-09-31 01:03:04", 1, 3, 4,
                                       kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-09-31 22:59:60", 22, 59,
                                       60, kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-09-31 215907", 21, 59, 7,
                                       kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-09-31 20:49:37.987654321",
                                       20, 49, 37, 987654321, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "1964-07-10 19:51:42,123", 19,
                                       51, 42, 123000000, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "1964-07-10 13:03:60,12345", 13,
                                       3, 60, 123450000, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "1964-07-10 01:03:04,123456789",
                                       1, 3, 4, 123456789, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "19640710 09:18:27,12345678", 9,
                                       18, 27, 123456780, "");
  // Date TimeZone
  // Date TimeZoneOffsetRequired
  // Date TimeZoneUTCOffset TimeZoneBracketedAnnotation_opt
  // Date TimeZoneNumericUTCOffset
  VerifyTemporalTimeStringTimeUndefined(isolate, "2021-11-09+11");
  VerifyTemporalTimeStringTimeUndefined(isolate, "2021-11-09-12:03");
  VerifyTemporalTimeStringTimeUndefined(isolate, "2021-11-09-1203");
  VerifyTemporalTimeStringTimeUndefined(isolate, "2021-11-09-12:03:04");
  VerifyTemporalTimeStringTimeUndefined(isolate, "2021-11-09-120304");
  VerifyTemporalTimeStringTimeUndefined(isolate,
                                        "2021-11-09-12:03:04,987654321");
  VerifyTemporalTimeStringTimeUndefined(isolate, "2021-11-09-120304.987654321");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-11-09T03+11", 3,
                                       kUndefined, kUndefined, kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-11-09t04:55-12:03", 4, 55,
                                       kUndefined, kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(
      isolate, "2021-11-09t06:22:01.987654321-12:03", 6, 22, 1, 987654321, "");
  VerifyParseTemporalTimeStringSuccess(
      isolate, "2021-11-09t073344,98765432-12:03", 7, 33, 44, 987654320, "");
  VerifyParseTemporalTimeStringSuccess(
      isolate, "2021-11-09t07:33:44,98765432-1203", 7, 33, 44, 987654320, "");
  VerifyParseTemporalTimeStringSuccess(
      isolate, "2021-11-09 075317,9876543-1203", 7, 53, 17, 987654300, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-11-09T12-13:03:04", 12,
                                       kUndefined, kUndefined, kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-11-09t1122-120304", 11,
                                       22, kUndefined, kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(isolate,
                                       "2021-11-09 223344-12:03:04.987654321",
                                       22, 33, 44, kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(
      isolate, "2021-11-09 234512.9876-12:03:04.987654321", 23, 45, 12,
      987600000, "");

  VerifyParseTemporalTimeStringSuccess(
      isolate, "2021-11-09T223344.987654321-120304.123456789", 22, 33, 44,
      987654321, "");
  VerifyParseTemporalTimeStringSuccess(
      isolate, "19670316T223344.987654321-120304.123456789", 22, 33, 44,
      987654321, "");
  // Date UTCDesignator
  // Date UTCDesignator
  VerifyTemporalTimeStringTimeUndefined(isolate, "2021-11-09z");
  VerifyTemporalTimeStringTimeUndefined(isolate, "2021-11-09Z");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-11-09T11z", 11,
                                       kUndefined, kUndefined, kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-11-09t12Z", 12,
                                       kUndefined, kUndefined, kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-11-09 01:23Z", 1, 23,
                                       kUndefined, kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-11-09 01:23:45Z", 1, 23,
                                       45, kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(
      isolate, "2021-11-09 01:23:45.678912345Z", 1, 23, 45, 678912345, "");
  VerifyParseTemporalTimeStringSuccess(
      isolate, "2021-11-09 01:23:45,567891234Z", 1, 23, 45, 567891234, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-11-09 0123Z", 1, 23,
                                       kUndefined, kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-11-09 012345Z", 1, 23, 45,
                                       kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-11-09t012345.678912345Z",
                                       1, 23, 45, 678912345, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-11-09 012345,891234Z", 1,
                                       23, 45, 891234000, "");
  VerifyParseTemporalTimeStringSuccess(isolate, "20211109T012345,891234567Z", 1,
                                       23, 45, 891234567, "");
  // Date TimeZoneNameRequired
  // Date TimeZoneBracketedAnnotation
  VerifyTemporalTimeStringTimeUndefined(isolate, "2021-11-09[Etc/GMT+01]");
  VerifyTemporalTimeStringTimeUndefined(isolate, "2021-11-09[Etc/GMT-23]");
  VerifyTemporalTimeStringTimeUndefined(isolate, "2021-11-09[Etc/GMT+23]");
  VerifyTemporalTimeStringTimeUndefined(isolate, "2021-11-09[Etc/GMT-00]");
  VerifyTemporalTimeStringTimeUndefined(isolate, "2021-11-09[Etc/GMT+01]");
  VerifyTemporalTimeStringTimeUndefined(isolate, "2021-11-09[Etc/GMT-23]");
  VerifyParseTemporalTimeStringSuccess(
      isolate, "2021-11-09 23:45:56.891234567Z[Etc/GMT+23]", 23, 45, 56,
      891234567, "");
  // TimeZoneIANAName
  VerifyTemporalTimeStringTimeUndefined(isolate, "2021-11-09[ABCDEFGHIJKLMN]");
  VerifyTemporalTimeStringTimeUndefined(
      isolate, "2021-11-09[ABCDEFGHIJKLMN/abcdefghijklmn/opeqrstuv]");
  VerifyTemporalTimeStringTimeUndefined(
      isolate, "2021-11-09[aBcDEfGHiJ.L_N/ABC...G_..KLMN]");
  VerifyTemporalTimeStringTimeUndefined(
      isolate, "2021-11-09[aBcDE-GHiJ.L_N/ABCbcdG-IJKLMN]");
  VerifyParseTemporalTimeStringSuccess(isolate,
                                       "2021-11-09T12z[.BCDEFGHIJKLMN]", 12,
                                       kUndefined, kUndefined, kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(
      isolate, "2021-11-09T23:45Z[ABCDEFGHIJKLMN/_bcde-ghij_lmn/.peqrstuv]", 23,
      45, kUndefined, kUndefined, "");
  VerifyParseTemporalTimeStringSuccess(
      isolate, "2021-11-09t234534.234+1234[aBcDEfGHiJ.L_N/ABC...G_..KLMN]", 23,
      45, 34, 234000000, "");
  VerifyParseTemporalTimeStringSuccess(
      isolate,
      "2021-11-09 "
      "123456.789123456-012345.789123456[aBcDEfGHiJ.L_N/ABCbcdGfIJKLMN]",
      12, 34, 56, 789123456, "");
  // TimeZoneUTCOffsetName
  VerifyTemporalTimeStringTimeUndefined(isolate, "2021-11-09[+12]");
  VerifyTemporalTimeStringTimeUndefined(isolate, "2021-11-09[+12:34]");
  VerifyTemporalTimeStringTimeUndefined(isolate, "2021-11-09[+12:34:56]");
  VerifyTemporalTimeStringTimeUndefined(isolate,
                                        "2021-11-09[+12:34:56,789123456]");
  VerifyTemporalTimeStringTimeUndefined(isolate,
                                        "2021-11-09[+12:34:56.789123456]");
  VerifyTemporalTimeStringTimeUndefined(
      isolate, u8"2021-11-09[\u221200:34:56.789123456]");

  VerifyParseTemporalTimeStringSuccess(
      isolate, "2021-11-09 01:23:45.678912345Z", 1, 23, 45, 678912345, "");

  VerifyParseTemporalTimeStringSuccess(isolate, "2021-03-11[u-ca=iso8601]",
                                       kUndefined, kUndefined, kUndefined,
                                       kUndefined, "iso8601");
  VerifyParseTemporalTimeStringSuccess(
      isolate, "2021-03-11[u-ca=abcdefgh-wxyzefg]", kUndefined, kUndefined,
      kUndefined, kUndefined, "abcdefgh-wxyzefg");
  VerifyParseTemporalTimeStringSuccess(
      isolate, "2021-03-11[u-ca=abcdefgh-wxyzefg-ijklmnop]", kUndefined,
      kUndefined, kUndefined, kUndefined, "abcdefgh-wxyzefg-ijklmnop");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-03-11T01[u-ca=iso8601]",
                                       1, kUndefined, kUndefined, kUndefined,
                                       "iso8601");
  VerifyParseTemporalTimeStringSuccess(
      isolate, "2021-03-11 02:34[u-ca=abcdefgh-wxyzefg]", 2, 34, kUndefined,
      kUndefined, "abcdefgh-wxyzefg");

  VerifyParseTemporalTimeStringSuccess(
      isolate,
      "2021-11-03 "
      "123456.789-012345.789123456[aBcDEfGHiJ.L_N/"
      "ABCbcdGfIJKLMN][u-ca=abc]",
      12, 34, 56, 789000000, "abc");

  VerifyParseTemporalTimeStringSuccess(
      isolate, "2021-03-11[+12:34:56,789123456][u-ca=abcdefgh-wxyzefg]",
      kUndefined, kUndefined, kUndefined, kUndefined, "abcdefgh-wxyzefg");
  VerifyParseTemporalTimeStringSuccess(
      isolate, "2021-03-11T23[+12:34:56,789123456][u-ca=abcdefgh-wxyzefg]", 23,
      kUndefined, kUndefined, kUndefined, "abcdefgh-wxyzefg");
  VerifyParseTemporalTimeStringSuccess(
      isolate,
      u8"20210311[\u221200:34:56.789123456][u-ca=abcdefgh-wxyzefg-ijklmnop]",
      kUndefined, kUndefined, kUndefined, kUndefined,
      "abcdefgh-wxyzefg-ijklmnop");
  VerifyParseTemporalTimeStringSuccess(
      isolate,
      u8"20210311T22:11[\u221200:34:56.789123456][u-ca=abcdefgh-"
      u8"wxyzefg-ijklmnop]",
      22, 11, kUndefined, kUndefined, "abcdefgh-wxyzefg-ijklmnop");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-11-03[u-ca=abc]",
                                       kUndefined, kUndefined, kUndefined,
                                       kUndefined, "abc");
  VerifyParseTemporalTimeStringSuccess(isolate,
                                       "2021-11-03T23:45:12.345[u-ca=abc]", 23,
                                       45, 12, 345000000, "abc");
  VerifyParseTemporalTimeStringSuccess(isolate, "2021-11-03[u-ca=iso-8601]",
                                       kUndefined, kUndefined, kUndefined,
                                       kUndefined, "iso-8601");
  VerifyParseTemporalTimeStringSuccess(isolate,
                                       "2021-11-03 234527[u-ca=iso-8601]", 23,
                                       45, 27, kUndefined, "iso-8601");

  VerifyParseTemporalTimeStringSuccess(isolate, "2021-11-03[u-ca=123456-789]",
                                       kUndefined, kUndefined, kUndefined,
                                       kUndefined, "123456-789");
  VerifyParseTemporalTimeStringSuccess(
      isolate, "2021-11-03t12[u-ca=123456-789]", 12, kUndefined, kUndefined,
      kUndefined, "123456-789");
}

TEST(TemporalTimeStringIllegal) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  VERIFY_PARSE_FAIL_ON_DATE(TemporalTimeString);
  VERIFY_PARSE_FAIL(TemporalTimeString, "");

  VERIFY_PARSE_FAIL(TemporalTimeString,
                    "2021-03-11t03:45.67[u-ca=abcdefgh-wxyzefg-ijklmnop]");
  // Single digit Hour
  VERIFY_PARSE_FAIL(TemporalTimeString, "0");
  VERIFY_PARSE_FAIL(TemporalTimeString, "9");
  // out of range Hour
  VERIFY_PARSE_FAIL(TemporalTimeString, "99");
  VERIFY_PARSE_FAIL(TemporalTimeString, "24");
  // Single digit Hour or TimeMinute
  VERIFY_PARSE_FAIL(TemporalTimeString, "000");
  VERIFY_PARSE_FAIL(TemporalTimeString, "111");
  VERIFY_PARSE_FAIL(TemporalTimeString, "00:0");
  VERIFY_PARSE_FAIL(TemporalTimeString, "11:1");
  VERIFY_PARSE_FAIL(TemporalTimeString, "0:00");
  VERIFY_PARSE_FAIL(TemporalTimeString, "1:11");
  // out of range Hour TimeMinute
  VERIFY_PARSE_FAIL(TemporalTimeString, "2400");
  VERIFY_PARSE_FAIL(TemporalTimeString, "2360");
  VERIFY_PARSE_FAIL(TemporalTimeString, "24:00");
  VERIFY_PARSE_FAIL(TemporalTimeString, "23:60");
  // out of range Hour TimeMinute or TimeSecond
  VERIFY_PARSE_FAIL(TemporalTimeString, "24:00:01");
  VERIFY_PARSE_FAIL(TemporalTimeString, "23:60:01");
  VERIFY_PARSE_FAIL(TemporalTimeString, "23:59:61");

  // Single digit Hour, TimeMinute or TimeSecond
  VERIFY_PARSE_FAIL(TemporalTimeString, "00000");
  VERIFY_PARSE_FAIL(TemporalTimeString, "22222");
  VERIFY_PARSE_FAIL(TemporalTimeString, "00:00:0");
  VERIFY_PARSE_FAIL(TemporalTimeString, "22:2:22");
  VERIFY_PARSE_FAIL(TemporalTimeString, "3:33:33");
  VERIFY_PARSE_FAIL(TemporalTimeString, "444:444");
  VERIFY_PARSE_FAIL(TemporalTimeString, "44444.567");
  VERIFY_PARSE_FAIL(TemporalTimeString, "44444,567");

  // wrong separator
  VERIFY_PARSE_FAIL(TemporalTimeString, "12:34:56 5678");

  // out of range Hour TimeMinute, TimeSecond or TimeFraction
  VERIFY_PARSE_FAIL(TemporalTimeString, "12:34:56.1234567890");
  VERIFY_PARSE_FAIL(TemporalTimeString, "24:01:02.123456789");
  VERIFY_PARSE_FAIL(TemporalTimeString, "23:60:02.123456789");
  VERIFY_PARSE_FAIL(TemporalTimeString, "23:59:61.123456789");
  VERIFY_PARSE_FAIL(TemporalTimeString, "23:33:44.0000000000");
}

#define IMPL_DATE_TIME_STRING_SUCCESS(R)                                       \
  do {                                                                         \
    /* CalendarDateTime : DateTime Calendaropt */                              \
    /* DateTime */                                                             \
    /* DateYear - DateMonth - DateDay */                                       \
    VerifyParse##R##Success(isolate, "2021-11-03", 2021, 11, 03, kUndefined,   \
                            kUndefined, kUndefined, kUndefined, "");           \
    /* DateYear DateMonth DateDay */                                           \
    VerifyParse##R##Success(isolate, "20211103", 2021, 11, 03, kUndefined,     \
                            kUndefined, kUndefined, kUndefined, "");           \
    /* DateExtendedYear */                                                     \
    VerifyParse##R##Success(isolate, "+002021-11-03", 2021, 11, 03,            \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, "+000001-11-03", 1, 11, 03, kUndefined,   \
                            kUndefined, kUndefined, kUndefined, "");           \
    VerifyParse##R##Success(isolate, "+0020211103", 2021, 11, 03, kUndefined,  \
                            kUndefined, kUndefined, kUndefined, "");           \
    VerifyParse##R##Success(isolate, "+0000011231", 1, 12, 31, kUndefined,     \
                            kUndefined, kUndefined, kUndefined, "");           \
    VerifyParse##R##Success(isolate, "+0000000101", 0, 1, 1, kUndefined,       \
                            kUndefined, kUndefined, kUndefined, "");           \
    VerifyParse##R##Success(isolate, "+0000000101", 0, 1, 1, kUndefined,       \
                            kUndefined, kUndefined, kUndefined, "");           \
    VerifyParse##R##Success(isolate, "-0000000101", 0, 1, 1, kUndefined,       \
                            kUndefined, kUndefined, kUndefined, "");           \
    VerifyParse##R##Success(isolate, u8"\u22120000000101", 0, 1, 1,            \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, "-0000000101", 0, 1, 1, kUndefined,       \
                            kUndefined, kUndefined, kUndefined, "");           \
    VerifyParse##R##Success(isolate, "+654321-11-03", 654321, 11, 3,           \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, "+999999-12-31", 999999, 12, 31,          \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, "-654321-11-03", -654321, 11, 3,          \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, "-999999-12-31", -999999, 12, 31,         \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, u8"\u2212999999-12-31", -999999, 12, 31,  \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, "+6543211103", 654321, 11, 3, kUndefined, \
                            kUndefined, kUndefined, kUndefined, "");           \
    VerifyParse##R##Success(isolate, "+9999991231", 999999, 12, 31,            \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, "-6543211103", -654321, 11, 3,            \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, "-9999991231", -999999, 12, 31,           \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, u8"\u22129999991231", -999999, 12, 31,    \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
                                                                               \
    /* DateTime: Date TimeSpecSeparator_opt TimeZone_opt */                    \
    /* Date TimeSpecSeparator */                                               \
    /* Differeent DateTimeSeparator: <S> T or t */                             \
    VerifyParse##R##Success(isolate, "2021-11-09T01", 2021, 11, 9, 1,          \
                            kUndefined, kUndefined, kUndefined, "");           \
    VerifyParse##R##Success(isolate, "2021-12-07t01", 2021, 12, 7, 1,          \
                            kUndefined, kUndefined, kUndefined, "");           \
    VerifyParse##R##Success(isolate, "2021-09-31 01", 2021, 9, 31, 1,          \
                            kUndefined, kUndefined, kUndefined, "");           \
    VerifyParse##R##Success(isolate, "2021-11-09T0102", 2021, 11, 9, 1, 2,     \
                            kUndefined, kUndefined, "");                       \
    VerifyParse##R##Success(isolate, "2021-12-07t01:02", 2021, 12, 7, 1, 2,    \
                            kUndefined, kUndefined, "");                       \
    VerifyParse##R##Success(isolate, "2021-09-31 01:03:04", 2021, 9, 31, 1, 3, \
                            4, kUndefined, "");                                \
    VerifyParse##R##Success(isolate, "2021-09-31 01:03:60", 2021, 9, 31, 1, 3, \
                            60, kUndefined, "");                               \
    VerifyParse##R##Success(isolate, "2021-09-31 010304", 2021, 9, 31, 1, 3,   \
                            4, kUndefined, "");                                \
    VerifyParse##R##Success(isolate, "2021-09-31 01:03:04.987654321", 2021, 9, \
                            31, 1, 3, 4, 987654321, "");                       \
    VerifyParse##R##Success(isolate, "1964-07-10 01:03:04,1", 1964, 7, 10, 1,  \
                            3, 4, 100000000, "");                              \
    VerifyParse##R##Success(isolate, "1964-07-10 01:03:60,1", 1964, 7, 10, 1,  \
                            3, 60, 100000000, "");                             \
    VerifyParse##R##Success(isolate, "1964-07-10 01:03:04,123456789", 1964, 7, \
                            10, 1, 3, 4, 123456789, "");                       \
    VerifyParse##R##Success(isolate, "19640710 01:03:04,123456789", 1964, 7,   \
                            10, 1, 3, 4, 123456789, "");                       \
    /* Date TimeZone */                                                        \
    /* Date TimeZoneOffsetRequired */                                          \
    /* Date TimeZoneUTCOffset TimeZoneBracketedAnnotation_opt */               \
    /* Date TimeZoneNumericUTCOffset */                                        \
    VerifyParse##R##Success(isolate, "2021-11-09+11", 2021, 11, 9, kUndefined, \
                            kUndefined, kUndefined, kUndefined, "");           \
    VerifyParse##R##Success(isolate, "2021-11-09-12:03", 2021, 11, 9,          \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, "2021-11-09-1203", 2021, 11, 9,           \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, "2021-11-09-12:03:04", 2021, 11, 9,       \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, "2021-11-09-120304", 2021, 11, 9,         \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, "2021-11-09-12:03:04,987654321", 2021,    \
                            11, 9, kUndefined, kUndefined, kUndefined,         \
                            kUndefined, "");                                   \
    VerifyParse##R##Success(isolate, "2021-11-09-120304.123456789", 2021, 11,  \
                            9, kUndefined, kUndefined, kUndefined, kUndefined, \
                            "");                                               \
    VerifyParse##R##Success(isolate, "2021-11-09T03+11", 2021, 11, 9, 3,       \
                            kUndefined, kUndefined, kUndefined, "");           \
    VerifyParse##R##Success(isolate, "2021-11-09t04:55-12:03", 2021, 11, 9, 4, \
                            55, kUndefined, kUndefined, "");                   \
    VerifyParse##R##Success(isolate, "2021-11-09t06:22:01.987654321", 2021,    \
                            11, 9, 6, 22, 1, 987654321, "");                   \
    VerifyParse##R##Success(isolate, "2021-11-09t062202,987654321", 2021, 11,  \
                            9, 6, 22, 2, 987654321, "");                       \
    VerifyParse##R##Success(isolate, "2021-11-09t06:22:03.987654321-1203",     \
                            2021, 11, 9, 6, 22, 3, 987654321, "");             \
    VerifyParse##R##Success(isolate, "2021-11-09 062204.987654321-1203", 2021, \
                            11, 9, 6, 22, 4, 987654321, "");                   \
                                                                               \
    VerifyParse##R##Success(isolate, "2021-11-09T12-12:03:04", 2021, 11, 9,    \
                            12, kUndefined, kUndefined, kUndefined, "");       \
    VerifyParse##R##Success(isolate, "2021-11-09t1122-120304", 2021, 11, 9,    \
                            11, 22, kUndefined, kUndefined, "");               \
    VerifyParse##R##Success(isolate, "2021-11-09 223344-12:03:04,987654321",   \
                            2021, 11, 9, 22, 33, 44, kUndefined, "");          \
    VerifyParse##R##Success(isolate,                                           \
                            "2021-11-09T223344.987654321-120304.123456789",    \
                            2021, 11, 9, 22, 33, 44, 987654321, "");           \
    VerifyParse##R##Success(isolate,                                           \
                            "19670316T223344.987654321-120304.123456789",      \
                            1967, 3, 16, 22, 33, 44, 987654321, "");           \
    /* Date UTCDesignator */                                                   \
    VerifyParse##R##Success(isolate, "2021-11-09z", 2021, 11, 9, kUndefined,   \
                            kUndefined, kUndefined, kUndefined, "");           \
    VerifyParse##R##Success(isolate, "2021-11-09Z", 2021, 11, 9, kUndefined,   \
                            kUndefined, kUndefined, kUndefined, "");           \
    VerifyParse##R##Success(isolate, "2021-11-09T11z", 2021, 11, 9, 11,        \
                            kUndefined, kUndefined, kUndefined, "");           \
    VerifyParse##R##Success(isolate, "2021-11-09t12Z", 2021, 11, 9, 12,        \
                            kUndefined, kUndefined, kUndefined, "");           \
    VerifyParse##R##Success(isolate, "2021-11-09 01:23Z", 2021, 11, 9, 1, 23,  \
                            kUndefined, kUndefined, "");                       \
    VerifyParse##R##Success(isolate, "2021-11-09 01:23:45Z", 2021, 11, 9, 1,   \
                            23, 45, kUndefined, "");                           \
    VerifyParse##R##Success(isolate, "2021-11-09 01:23:45.678912345Z", 2021,   \
                            11, 9, 1, 23, 45, 678912345, "");                  \
    VerifyParse##R##Success(isolate, "2021-11-09 01:23:45,567891234Z", 2021,   \
                            11, 9, 1, 23, 45, 567891234, "");                  \
    VerifyParse##R##Success(isolate, "2021-11-09 0123Z", 2021, 11, 9, 1, 23,   \
                            kUndefined, kUndefined, "");                       \
    VerifyParse##R##Success(isolate, "2021-11-09 012345Z", 2021, 11, 9, 1, 23, \
                            45, kUndefined, "");                               \
    VerifyParse##R##Success(isolate, "2021-11-09t012345.678912345Z", 2021, 11, \
                            9, 1, 23, 45, 678912345, "");                      \
    VerifyParse##R##Success(isolate, "2021-11-09 012345,891234567Z", 2021, 11, \
                            9, 1, 23, 45, 891234567, "");                      \
    VerifyParse##R##Success(isolate, "20211109T012345,891234567Z", 2021, 11,   \
                            9, 1, 23, 45, 891234567, "");                      \
    /* Date TimeZoneNameRequired */                                            \
    /* Date TimeZoneBracketedAnnotation */                                     \
    VerifyParse##R##Success(isolate, "2021-11-09[Etc/GMT+01]", 2021, 11, 9,    \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, "2021-11-09[Etc/GMT-23]", 2021, 11, 9,    \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, "2021-11-09[Etc/GMT+23]", 2021, 11, 9,    \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, "2021-11-09[Etc/GMT-00]", 2021, 11, 9,    \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, "2021-11-09Z[Etc/GMT+01]", 2021, 11, 9,   \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, "2021-11-09z[Etc/GMT-23]", 2021, 11, 9,   \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate,                                           \
                            "2021-11-09 23:45:56.891234567Z[Etc/GMT+23]",      \
                            2021, 11, 9, 23, 45, 56, 891234567, "");           \
    /* TimeZoneIANAName */                                                     \
    VerifyParse##R##Success(isolate, "2021-11-09[ABCDEFGHIJKLMN]", 2021, 11,   \
                            9, kUndefined, kUndefined, kUndefined, kUndefined, \
                            "");                                               \
    VerifyParse##R##Success(                                                   \
        isolate, "2021-11-09[ABCDEFGHIJKLMN/abcdefghijklmn/opeqrstuv]", 2021,  \
        11, 9, kUndefined, kUndefined, kUndefined, kUndefined, "");            \
    VerifyParse##R##Success(                                                   \
        isolate, "2021-11-09[aBcDEfGHiJ.L_N/ABC...G_..KLMN]", 2021, 11, 9,     \
        kUndefined, kUndefined, kUndefined, kUndefined, "");                   \
    VerifyParse##R##Success(                                                   \
        isolate, "2021-11-09[aBcDE-GHiJ.L_N/ABCbcdG-IJKLMN]", 2021, 11, 9,     \
        kUndefined, kUndefined, kUndefined, kUndefined, "");                   \
    VerifyParse##R##Success(isolate, "2021-11-09T12z[.BCDEFGHIJKLMN]", 2021,   \
                            11, 9, 12, kUndefined, kUndefined, kUndefined,     \
                            "");                                               \
    VerifyParse##R##Success(                                                   \
        isolate, "2021-11-09T23:45Z[ABCDEFGHIJKLMN/_bcde-ghij_lmn/.peqrstuv]", \
        2021, 11, 9, 23, 45, kUndefined, kUndefined, "");                      \
    VerifyParse##R##Success(                                                   \
        isolate, "2021-11-09t234534.234+1234[aBcDEfGHiJ.L_N/ABC...G_..KLMN]",  \
        2021, 11, 9, 23, 45, 34, 234000000, "");                               \
    VerifyParse##R##Success(                                                   \
        isolate,                                                               \
        "2021-11-09 "                                                          \
        "123456.789123456-012345.789123456[aBcDEfGHiJ.L_N/ABCbcdGfIJKLMN]",    \
        2021, 11, 9, 12, 34, 56, 789123456, "");                               \
    /* TimeZoneUTCOffsetName */                                                \
    VerifyParse##R##Success(isolate, "2021-11-09[+12]", 2021, 11, 9,           \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, "2021-11-09[+12:34]", 2021, 11, 9,        \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, "2021-11-09[-12:34:56]", 2021, 11, 9,     \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "");                                               \
    VerifyParse##R##Success(isolate, "2021-11-09[+12:34:56,789123456]", 2021,  \
                            11, 9, kUndefined, kUndefined, kUndefined,         \
                            kUndefined, "");                                   \
    VerifyParse##R##Success(isolate, "2021-11-09[+12:34:56.789123456]", 2021,  \
                            11, 9, kUndefined, kUndefined, kUndefined,         \
                            kUndefined, "");                                   \
    VerifyParse##R##Success(isolate, u8"2021-11-09[\u221200:34:56.789123456]", \
                            2021, 11, 9, kUndefined, kUndefined, kUndefined,   \
                            kUndefined, "");                                   \
                                                                               \
    /* Date TimeSpecSeparator TimeZone */                                      \
    /* DateTime Calendaropt */                                                 \
    VerifyParse##R##Success(isolate, "2021-11-03[u-ca=abc]", 2021, 11, 03,     \
                            kUndefined, kUndefined, kUndefined, kUndefined,    \
                            "abc");                                            \
    VerifyParse##R##Success(isolate, "2021-11-03[u-ca=iso-8601]", 2021, 11,    \
                            03, kUndefined, kUndefined, kUndefined,            \
                            kUndefined, "iso-8601");                           \
    VerifyParse##R##Success(isolate, "2021-11-03[u-ca=123456-789]", 2021, 11,  \
                            03, kUndefined, kUndefined, kUndefined,            \
                            kUndefined, "123456-789");                         \
    VerifyParse##R##Success(isolate, "2021-03-11[u-ca=abcdefgh-wxyzefg]",      \
                            2021, 3, 11, kUndefined, kUndefined, kUndefined,   \
                            kUndefined, "abcdefgh-wxyzefg");                   \
    VerifyParse##R##Success(isolate,                                           \
                            "2021-03-11[u-ca=abcdefgh-wxyzefg-ijklmnop]",      \
                            2021, 3, 11, kUndefined, kUndefined, kUndefined,   \
                            kUndefined, "abcdefgh-wxyzefg-ijklmnop");          \
                                                                               \
    VerifyParse##R##Success(                                                   \
        isolate,                                                               \
        "2021-11-03 "                                                          \
        "123456.789123456-012345.789123456[aBcDEfGHiJ.L_N/"                    \
        "ABCbcdGfIJKLMN][u-ca=abc]",                                           \
        2021, 11, 03, 12, 34, 56, 789123456, "abc");                           \
    VerifyParse##R##Success(                                                   \
        isolate, "2021-03-11[+12:34:56,789123456][u-ca=abcdefgh-wxyzefg]",     \
        2021, 3, 11, kUndefined, kUndefined, kUndefined, kUndefined,           \
        "abcdefgh-wxyzefg");                                                   \
    VerifyParse##R##Success(isolate,                                           \
                            u8"20210311[\u221200:34:56.789123456][u-ca="       \
                            u8"abcdefgh-wxyzefg-ijklmnop]",                    \
                            2021, 3, 11, kUndefined, kUndefined, kUndefined,   \
                            kUndefined, "abcdefgh-wxyzefg-ijklmnop");          \
    VerifyParse##R##Success(isolate, "2021-11-09 01:23:45.678912345Z", 2021,   \
                            11, 9, 1, 23, 45, 678912345, "");                  \
  } while (false)

TEST(TemporalDateTimeStringSuccess) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  IMPL_DATE_TIME_STRING_SUCCESS(TemporalDateTimeString);
}

TEST(TemporalDateTimeStringIllegal) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  VERIFY_PARSE_FAIL_ON_DATE(TemporalDateTimeString);
  VERIFY_PARSE_FAIL(TemporalDateTimeString, "+20210304");
  VERIFY_PARSE_FAIL(TemporalDateTimeString, "-20210304");
  VERIFY_PARSE_FAIL(TemporalDateTimeString, u8"\u221220210304");
  VERIFY_PARSE_FAIL(TemporalDateTimeString, "210304");
}

TEST(TemporalYearMonthStringSuccess) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  // TemporalYearMonthString :
  //   DateSpecYearMonth
  //   DateTime

  // DateSpecYearMonth:
  //   DateYear -opt DateMonth
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11", 2021, 11,
                                            kUndefined, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "202111", 2021, 11,
                                            kUndefined, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "+002021-11", 2021, 11,
                                            kUndefined, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "-002021-02", -2021, 02,
                                            kUndefined, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "-98765412", -987654, 12,
                                            kUndefined, "");

  // DateTime:
  // DateYear - DateMonth - DateDay
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-03", 2021, 11, 03,
                                            "");
  // DateYear DateMonth DateDay
  VerifyParseTemporalYearMonthStringSuccess(isolate, "20211103", 2021, 11, 03,
                                            "");
  // DateExtendedYear
  VerifyParseTemporalYearMonthStringSuccess(isolate, "+002021-11-03", 2021, 11,
                                            03, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "+000001-11-03", 1, 11, 03,
                                            "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "+0020211103", 2021, 11,
                                            03, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "+0000011231", 1, 12, 31,
                                            "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "+0000000101", 0, 1, 1,
                                            "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "+0000000101", 0, 1, 1,
                                            "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "-0000000101", 0, 1, 1,
                                            "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, u8"\u22120000000101", 0, 1,
                                            1, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "-0000000101", 0, 1, 1,
                                            "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "+654321-11-03", 654321,
                                            11, 3, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "+999999-12-31", 999999,
                                            12, 31, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "-654321-11-03", -654321,
                                            11, 3, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "-999999-12-31", -999999,
                                            12, 31, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, u8"\u2212999999-12-31",
                                            -999999, 12, 31, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "+6543211103", 654321, 11,
                                            3, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "+9999991231", 999999, 12,
                                            31, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "-6543211103", -654321, 11,
                                            3, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "-9999991231", -999999, 12,
                                            31, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, u8"\u22129999991231",
                                            -999999, 12, 31, "");

  // DateTime: Date TimeSpecSeparator_opt TimeZone_opt
  // Date TimeSpecSeparator
  // Differeent DateTimeSeparator: <S> T or t
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09T01", 2021, 11,
                                            9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-12-07t01", 2021, 12,
                                            7, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-09-31 01", 2021, 9,
                                            31, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09T0102", 2021,
                                            11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-12-07t01:02", 2021,
                                            12, 7, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-09-31 01:03:04",
                                            2021, 9, 31, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-09-31 01:03:60",
                                            2021, 9, 31, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-09-31 010304", 2021,
                                            9, 31, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-09-31 01:03:04.987654321", 2021, 9, 31, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "1964-07-10 01:03:04,1",
                                            1964, 7, 10, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "1964-07-10 01:03:60,1",
                                            1964, 7, 10, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "1964-07-10 01:03:04,123456789", 1964, 7, 10, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "19640710 01:03:04,123456789", 1964, 7, 10, "");
  // Date TimeZone
  // Date TimeZoneOffsetRequired
  // Date TimeZoneUTCOffset TimeZoneBracketedAnnotation_opt
  // Date TimeZoneNumericUTCOffset
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09+11", 2021, 11,
                                            9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09-12:03", 2021,
                                            11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09-1203", 2021,
                                            11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09-12:03:04",
                                            2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09-120304", 2021,
                                            11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09-12:03:04,987654321", 2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09-120304.123456789", 2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09T03+11", 2021,
                                            11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09t04:55-12:03",
                                            2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09t06:22:01.987654321", 2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09t062202,987654321", 2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09t06:22:03.987654321-1203", 2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09 062204.987654321-1203", 2021, 11, 9, "");

  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09T12-12:03:04",
                                            2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09t1122-120304",
                                            2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09 223344-12:03:04,987654321", 2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09T223344.987654321-120304.123456789", 2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "19670316T223344.987654321-120304.123456789", 1967, 3, 16, "");
  // Date UTCDesignator
  // Date UTCDesignator
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09z", 2021, 11, 9,
                                            "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09Z", 2021, 11, 9,
                                            "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09T11z", 2021, 11,
                                            9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09t12Z", 2021, 11,
                                            9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09 01:23Z", 2021,
                                            11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09 01:23:45Z",
                                            2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09 01:23:45.678912345Z", 2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09 01:23:45,567891234Z", 2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09 0123Z", 2021,
                                            11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09 012345Z", 2021,
                                            11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09t012345.678912345Z", 2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09 012345,891234567Z", 2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "20211109T012345,891234567Z", 2021, 11, 9, "");
  // Date TimeZoneNameRequired
  // Date TimeZoneBracketedAnnotation
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09[Etc/GMT+01]",
                                            2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09[Etc/GMT-23]",
                                            2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09[Etc/GMT+23]",
                                            2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09[Etc/GMT-00]",
                                            2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09Z[Etc/GMT+01]",
                                            2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09z[Etc/GMT-23]",
                                            2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09 23:45:56.891234567Z[Etc/GMT+23]", 2021, 11, 9, "");
  // TimeZoneIANAName
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09[ABCDEFGHIJKLMN]", 2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09[ABCDEFGHIJKLMN/abcdefghijklmn/opeqrstuv]", 2021, 11,
      9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09[aBcDEfGHiJ.L_N/ABC...G_..KLMN]", 2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09[aBcDE-GHiJ.L_N/ABCbcdG-IJKLMN]", 2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09T12z[.BCDEFGHIJKLMN]", 2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09T23:45Z[ABCDEFGHIJKLMN/_bcde-ghij_lmn/.peqrstuv]",
      2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09t234534.234+1234[aBcDEfGHiJ.L_N/ABC...G_..KLMN]",
      2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate,
      "2021-11-09 "
      "123456.789123456-012345.789123456[aBcDEfGHiJ.L_N/ABCbcdGfIJKLMN]",
      2021, 11, 9, "");
  // TimeZoneUTCOffsetName
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09[+12]", 2021,
                                            11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09[+12:34]", 2021,
                                            11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(isolate, "2021-11-09[-12:34:56]",
                                            2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09[+12:34:56,789123456]", 2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, "2021-11-09[+12:34:56.789123456]", 2021, 11, 9, "");
  VerifyParseTemporalYearMonthStringSuccess(
      isolate, u8"2021-11-09[\u221200:34:56.789123456]", 2021, 11, 9, "");
}

TEST(TemporalYearMonthStringIllegal) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  VERIFY_PARSE_FAIL_ON_DATE(TemporalYearMonthString);
  // DateYear -opt DateMonth
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "+2021-12");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "-2021-12");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, u8"2021\u221212");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, u8"\u22122021-12");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "2021-00");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "2021-13");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "202100");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "202113");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "+98765-12");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "-12345-12");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, u8"\u221212345-12");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "+9876-12");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "-1234-12");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, u8"\u22121234-12");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "+987-12");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "-123-12");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, u8"\u2212123-12");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "+98-12");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "-12-12");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, u8"\u221212-12");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "+9-12");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "-1-12");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, u8"\u22121-12");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "+9876512");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "-1234512");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, u8"\u22121234512");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "+987612");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "-123412");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, u8"\u2212123412");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "+98712");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, u8"\u221212312");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "+9812");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "-1212");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, u8"\u22121212");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "+912");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "-112");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, u8"\u2212112");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "+12");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "-12");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, u8"\u221212");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "+1");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, "-1");
  VERIFY_PARSE_FAIL(TemporalYearMonthString, u8"\u22121");
}

TEST(TemporalMonthDayStringSuccess) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  // TemporalMonthDayString :
  //   DateSpecMonthDay
  //   DateTime

  // DateSpecMonthDay:
  //   TwoDashesopt DateMonth -opt DateDay
  VerifyParseTemporalMonthDayStringSuccess(isolate, "--11-03", kUndefined, 11,
                                           3, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "--1231", kUndefined, 12,
                                           31, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "11-03", kUndefined, 11, 3,
                                           "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "0131", kUndefined, 1, 31,
                                           "");

  // DateTime:
  // DateYear - DateMonth - DateDay
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-03", 2021, 11, 03,
                                           "");
  // DateYear DateMonth DateDay
  VerifyParseTemporalMonthDayStringSuccess(isolate, "20211103", 2021, 11, 03,
                                           "");
  // DateExtendedYear
  VerifyParseTemporalMonthDayStringSuccess(isolate, "+002021-11-03", 2021, 11,
                                           03, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "+000001-11-03", 1, 11, 03,
                                           "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "+0020211103", 2021, 11, 03,
                                           "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "+0000011231", 1, 12, 31,
                                           "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "+0000000101", 0, 1, 1, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "+0000000101", 0, 1, 1, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "-0000000101", 0, 1, 1, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, u8"\u22120000000101", 0, 1,
                                           1, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "-0000000101", 0, 1, 1, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "+654321-11-03", 654321, 11,
                                           3, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "+999999-12-31", 999999, 12,
                                           31, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "-654321-11-03", -654321,
                                           11, 3, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "-999999-12-31", -999999,
                                           12, 31, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, u8"\u2212999999-12-31",
                                           -999999, 12, 31, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "+6543211103", 654321, 11,
                                           3, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "+9999991231", 999999, 12,
                                           31, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "-6543211103", -654321, 11,
                                           3, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "-9999991231", -999999, 12,
                                           31, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, u8"\u22129999991231",
                                           -999999, 12, 31, "");

  // DateTime: Date TimeSpecSeparator_opt TimeZone_opt
  // Date TimeSpecSeparator
  // Differeent DateTimeSeparator: <S> T or t
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09T01", 2021, 11,
                                           9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-12-07t01", 2021, 12,
                                           7, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-09-31 01", 2021, 9,
                                           31, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09T0102", 2021, 11,
                                           9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-12-07t01:02", 2021,
                                           12, 7, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-09-31 01:03:04", 2021,
                                           9, 31, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-09-31 01:03:60", 2021,
                                           9, 31, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-09-31 010304", 2021,
                                           9, 31, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-09-31 01:03:04.987654321", 2021, 9, 31, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "1964-07-10 01:03:04,1",
                                           1964, 7, 10, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "1964-07-10 01:03:60,1",
                                           1964, 7, 10, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "1964-07-10 01:03:04,123456789", 1964, 7, 10, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "19640710 01:03:04,123456789", 1964, 7, 10, "");
  // Date TimeZone
  // Date TimeZoneOffsetRequired
  // Date TimeZoneUTCOffset TimeZoneBracketedAnnotation_opt
  // Date TimeZoneNumericUTCOffset
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09+11", 2021, 11,
                                           9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09-12:03", 2021,
                                           11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09-1203", 2021, 11,
                                           9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09-12:03:04", 2021,
                                           11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09-120304", 2021,
                                           11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09-12:03:04,987654321", 2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09-120304.123456789", 2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09T03+11", 2021,
                                           11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09t04:55-12:03",
                                           2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09t06:22:01.987654321", 2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09t062202,987654321", 2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09t06:22:03.987654321-1203", 2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09 062204.987654321-1203", 2021, 11, 9, "");

  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09T12-12:03:04",
                                           2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09t1122-120304",
                                           2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09 223344-12:03:04,987654321", 2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09T223344.987654321-120304.123456789", 2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "19670316T223344.987654321-120304.123456789", 1967, 3, 16, "");
  // Date UTCDesignator
  // Date UTCDesignator
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09z", 2021, 11, 9,
                                           "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09Z", 2021, 11, 9,
                                           "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09T11z", 2021, 11,
                                           9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09t12Z", 2021, 11,
                                           9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09 01:23Z", 2021,
                                           11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09 01:23:45Z",
                                           2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09 01:23:45.678912345Z", 2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09 01:23:45,567891234Z", 2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09 0123Z", 2021,
                                           11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09 012345Z", 2021,
                                           11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09t012345.678912345Z", 2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09 012345,891234567Z", 2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "20211109T012345,891234567Z", 2021, 11, 9, "");
  // Date TimeZoneNameRequired
  // Date TimeZoneBracketedAnnotation
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09[Etc/GMT+01]",
                                           2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09[Etc/GMT-23]",
                                           2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09[Etc/GMT+23]",
                                           2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09[Etc/GMT-00]",
                                           2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09Z[Etc/GMT+01]",
                                           2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09z[Etc/GMT-23]",
                                           2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09 23:45:56.891234567Z[Etc/GMT+23]", 2021, 11, 9, "");
  // TimeZoneIANAName
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09[ABCDEFGHIJKLMN]", 2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09[ABCDEFGHIJKLMN/abcdefghijklmn/opeqrstuv]", 2021, 11,
      9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09[aBcDEfGHiJ.L_N/ABC...G_..KLMN]", 2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09[aBcDE-GHiJ.L_N/ABCbcdG-IJKLMN]", 2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09T12z[.BCDEFGHIJKLMN]", 2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09T23:45Z[ABCDEFGHIJKLMN/_bcde-ghij_lmn/.peqrstuv]",
      2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09t234534.234+1234[aBcDEfGHiJ.L_N/ABC...G_..KLMN]",
      2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate,
      "2021-11-09 "
      "123456.789123456-012345.789123456[aBcDEfGHiJ.L_N/ABCbcdGfIJKLMN]",
      2021, 11, 9, "");
  // TimeZoneUTCOffsetName
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09[+12]", 2021, 11,
                                           9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09[+12:34]", 2021,
                                           11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(isolate, "2021-11-09[-12:34:56]",
                                           2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09[+12:34:56,789123456]", 2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, "2021-11-09[+12:34:56.789123456]", 2021, 11, 9, "");
  VerifyParseTemporalMonthDayStringSuccess(
      isolate, u8"2021-11-09[\u221200:34:56.789123456]", 2021, 11, 9, "");
}

TEST(TemporalMonthDayStringIllegal) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  VERIFY_PARSE_FAIL_ON_DATE(TemporalMonthDayString);
  // TwoDashesopt DateMonth -opt DateDay
  VERIFY_PARSE_FAIL(TemporalMonthDayString, "--13-23");
  VERIFY_PARSE_FAIL(TemporalMonthDayString, "--12-32");
  VERIFY_PARSE_FAIL(TemporalMonthDayString, "--12-00");
  VERIFY_PARSE_FAIL(TemporalMonthDayString, "--00-02");
  VERIFY_PARSE_FAIL(TemporalMonthDayString, "00-02");
  VERIFY_PARSE_FAIL(TemporalMonthDayString, "10-00");
  VERIFY_PARSE_FAIL(TemporalMonthDayString, "0002");
  VERIFY_PARSE_FAIL(TemporalMonthDayString, "1000");
  VERIFY_PARSE_FAIL(TemporalMonthDayString, "-12-23");
  VERIFY_PARSE_FAIL(TemporalMonthDayString, "-1223");
  VERIFY_PARSE_FAIL(TemporalMonthDayString, "--1-23");
  VERIFY_PARSE_FAIL(TemporalMonthDayString, "--12-2");
  VERIFY_PARSE_FAIL(TemporalMonthDayString, "--122");
  VERIFY_PARSE_FAIL(TemporalMonthDayString, "--12-");
  VERIFY_PARSE_FAIL(TemporalMonthDayString, "-12-");
  VERIFY_PARSE_FAIL(TemporalMonthDayString, "--12");
  VERIFY_PARSE_FAIL(TemporalMonthDayString, "-12");
  VERIFY_PARSE_FAIL(TemporalMonthDayString, "-1");
  VERIFY_PARSE_FAIL(TemporalMonthDayString, "-1-2");
}

TEST(TemporalInstantStringSuccess) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  // Date TimeZoneOffsetRequired
  VerifyParseTemporalInstantStringSuccess(isolate, "2021-11-09z", true,
                                          kUndefined, kUndefined, kUndefined,
                                          kUndefined, kUndefined);
  VerifyParseTemporalInstantStringSuccess(isolate, "+002021-11-09z", true,
                                          kUndefined, kUndefined, kUndefined,
                                          kUndefined, kUndefined);
  VerifyParseTemporalInstantStringSuccess(isolate, "-002021-11-09z", true,
                                          kUndefined, kUndefined, kUndefined,
                                          kUndefined, kUndefined);
  VerifyParseTemporalInstantStringSuccess(isolate, "2021-11-09+00", false, 1, 0,
                                          kUndefined, kUndefined, kUndefined);
  VerifyParseTemporalInstantStringSuccess(isolate, "20211109+00", false, 1, 0,
                                          kUndefined, kUndefined, kUndefined);
  VerifyParseTemporalInstantStringSuccess(isolate, "20211109-23", false, -1, 23,
                                          kUndefined, kUndefined, kUndefined);
  VerifyParseTemporalInstantStringSuccess(isolate, "20211109+0059", false, 1, 0,
                                          59, kUndefined, kUndefined);
  VerifyParseTemporalInstantStringSuccess(isolate, "20211109-23:59", false, -1,
                                          23, 59, kUndefined, kUndefined);
  VerifyParseTemporalInstantStringSuccess(isolate, "20211109+005921", false, 1,
                                          0, 59, 21, kUndefined);
  VerifyParseTemporalInstantStringSuccess(isolate, "20211109-23:00:34", false,
                                          -1, 23, 0, 34, kUndefined);
  VerifyParseTemporalInstantStringSuccess(
      isolate, "20211109+00:59:21.000000001", false, 1, 0, 59, 21, 1);
  VerifyParseTemporalInstantStringSuccess(isolate, "20211109-230034.9", false,
                                          -1, 23, 0, 34, 900000000);
  VerifyParseTemporalInstantStringSuccess(isolate, "20211109-230035,89", false,
                                          -1, 23, 0, 35, 890000000);

  // Date DateTimeSeparator TimeSpec TimeZoneOffsetRequired
  VerifyParseTemporalInstantStringSuccess(
      isolate, "2021-11-09T12:34:56.987654321z", true, kUndefined, kUndefined,
      kUndefined, kUndefined, kUndefined);
  VerifyParseTemporalInstantStringSuccess(
      isolate, "2021-11-09 12:34:56.987654321z", true, kUndefined, kUndefined,
      kUndefined, kUndefined, kUndefined);
  VerifyParseTemporalInstantStringSuccess(
      isolate, "0001-11-09t12:34:56.987654321z", true, kUndefined, kUndefined,
      kUndefined, kUndefined, kUndefined);
  VerifyParseTemporalInstantStringSuccess(isolate, "2021-11-09T12+00", false, 1,
                                          0, kUndefined, kUndefined,
                                          kUndefined);
  VerifyParseTemporalInstantStringSuccess(isolate, "20211109t23+00", false, 1,
                                          0, kUndefined, kUndefined,
                                          kUndefined);
  VerifyParseTemporalInstantStringSuccess(isolate, "20211109 10-23", false, -1,
                                          23, kUndefined, kUndefined,
                                          kUndefined);
  VerifyParseTemporalInstantStringSuccess(isolate, "20211109T00:34+0059", false,
                                          1, 0, 59, kUndefined, kUndefined);
  VerifyParseTemporalInstantStringSuccess(isolate, "20211109t0233-23:59", false,
                                          -1, 23, 59, kUndefined, kUndefined);
  VerifyParseTemporalInstantStringSuccess(isolate, "20211109 091234+005921",
                                          false, 1, 0, 59, 21, kUndefined);
  VerifyParseTemporalInstantStringSuccess(isolate,
                                          "20211109T123456.789123456-23:00:34",
                                          false, -1, 23, 0, 34, kUndefined);
  VerifyParseTemporalInstantStringSuccess(
      isolate, "20211109t12:34:56.987654321+00:59:21.000000001", false, 1, 0,
      59, 21, 1);
  VerifyParseTemporalInstantStringSuccess(isolate,
                                          "20211109 235960,999999999-230034.9",
                                          false, -1, 23, 0, 34, 900000000);
  VerifyParseTemporalInstantStringSuccess(isolate,
                                          "20211109T000000.000000000-230035,89",
                                          false, -1, 23, 0, 35, 890000000);
}

TEST(TemporalInstantStringIllegal) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  VERIFY_PARSE_FAIL_ON_DATE(TemporalInstantString);

  // Without TimeZoneUTCOffsetSign
  VERIFY_PARSE_FAIL(TemporalInstantString, "202111090");
  VERIFY_PARSE_FAIL(TemporalInstantString, "202111099");
  VERIFY_PARSE_FAIL(TemporalInstantString, "2021110900");
  VERIFY_PARSE_FAIL(TemporalInstantString, "2021110901");
  VERIFY_PARSE_FAIL(TemporalInstantString, "2021110923");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109ff");

  // Wrong TimeZoneUTCOffsetHour
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+24");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109-24");
  VERIFY_PARSE_FAIL(TemporalInstantString, u8"20211109\u221224");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+ab");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109-2a");
  VERIFY_PARSE_FAIL(TemporalInstantString, u8"20211109\u22122Z");
  // Single digit is not TimeZoneUTCOffsetHour
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+0");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+2");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109-1");
  VERIFY_PARSE_FAIL(TemporalInstantString, u8"20211109\u22123");

  // Extra
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+23 ");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109 -22");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+23:");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109-22:");

  // Wrong TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour : TimeZoneUTCOffsetMinute
  // single digit TimeZoneUTCOffsetMinute
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+01:0");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+21:5");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109-20:4");
  VERIFY_PARSE_FAIL(TemporalInstantString, u8"20211109\u221219:3");
  // TimeZoneUTCOffsetMinute out of range
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+01:60");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+21:5a");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109-20:4f");
  VERIFY_PARSE_FAIL(TemporalInstantString, u8"20211109\u221219:a0");

  // Wrong TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour TimeZoneUTCOffsetMinute
  // single digit TimeZoneUTCOffsetMinute
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+010");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+215");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109-204");
  VERIFY_PARSE_FAIL(TemporalInstantString, u8"20211109\u2212193");
  // TimeZoneUTCOffsetMinute out of range
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+0160");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+215a");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109-204f");
  VERIFY_PARSE_FAIL(TemporalInstantString, u8"20211109\u221219a0");

  // TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour : TimeZoneUTCOffsetMinute :
  // TimeZoneUTCOffsetSecond TimeZoneUTCOffsetFractionopt with : here but not
  // there
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+01:0059");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+1534:33");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109-07:34:339");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109-07:34:.9");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109-07:34:,9");
  // fraction too long
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109-07:34:01.9876543219");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+07:34:01,9876543219");
  // fraction in hour or minute
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+01.0:00:59");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+01:00.1:59");

  // Wrong TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour TimeZoneUTCOffsetMinute
  // TimeZoneUTCOffsetSecond TimeZoneUTCOffsetFractionopt
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+0100.159");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+01.15009");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109-0100,159");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109-01,15009");
  // fraction too long
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109-073401.0000000000");
  VERIFY_PARSE_FAIL(TemporalInstantString, "20211109+073401,9876543219");
}

#define IMPL_ZONED_DATE_TIME_STRING_SUCCESS(R)                                \
  do {                                                                        \
    VerifyParse##R##Success(                                                  \
        isolate, "2021-11-03T02:03:04.56789z[Asia/Taipei]", 2021, 11, 03, 2,  \
        3, 4, 567890000, "", kUndefined, kUndefined, kUndefined, kUndefined,  \
        kUndefined, true, "Asia/Taipei");                                     \
    VerifyParse##R##Success(isolate, "1911-10-10[Asia/Shanghai][u-ca=roc]",   \
                            1911, 10, 10, kUndefined, kUndefined, kUndefined, \
                            kUndefined, "roc", kUndefined, kUndefined,        \
                            kUndefined, kUndefined, kUndefined, false,        \
                            "Asia/Shanghai");                                 \
    VerifyParse##R##Success(                                                  \
        isolate,                                                              \
        "+123456-12-31 "                                                      \
        "05:06:07+12:34:56.78901234[Europe/San_Marino][u-ca=hebrew]",         \
        123456, 12, 31, 5, 6, 7, kUndefined, "hebrew", 1, 12, 34, 56,         \
        789012340, false, "Europe/San_Marino");                               \
  } while (false)

TEST(TemporalZonedDateTimeStringSuccess) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  IMPL_ZONED_DATE_TIME_STRING_SUCCESS(TemporalZonedDateTimeString);
}

#define VERIFY_PARSE_FAIL_ON_ZONED_DATE_TIME(R)                            \
  do {                                                                     \
    VERIFY_PARSE_FAIL_ON_DATE(R);                                          \
    VERIFY_PARSE_FAIL(R, "+20210304");                                     \
    VERIFY_PARSE_FAIL(R, "-20210304");                                     \
    VERIFY_PARSE_FAIL(R, u8"\u221220210304");                              \
    VERIFY_PARSE_FAIL(R, "210304");                                        \
    VERIFY_PARSE_FAIL(R, "2021-03-04");                                    \
    VERIFY_PARSE_FAIL(R, "2021-03-04T23:59:20.1234");                      \
    VERIFY_PARSE_FAIL(R, "2021-03-04T23:59:20.1234z");                     \
    VERIFY_PARSE_FAIL(R, "2021-03-04[u-ca=roc]");                          \
    VERIFY_PARSE_FAIL(R, "2021-03-04T23:59:20.1234[u-ca=roc]");            \
    VERIFY_PARSE_FAIL(R, "2021-03-04T23:59:20.1234z[u-ca=roc]");           \
    VERIFY_PARSE_FAIL(R, "2021-03-04[]");                                  \
    VERIFY_PARSE_FAIL(R, "2021-03-04T23:59:20.1234[]");                    \
    VERIFY_PARSE_FAIL(R, "2021-03-04T23:59:20.1234z[]");                   \
    VERIFY_PARSE_FAIL(R, "2021-03-04[etc/gmt+00]");                        \
    VERIFY_PARSE_FAIL(R, "2021-03-04T23:59:20.1234[ETC/GMT+00]");          \
    VERIFY_PARSE_FAIL(R, "2021-03-04T23:59:20.1234z[Etc/GMT+24]");         \
    VERIFY_PARSE_FAIL(R, "2021-03-04[Etc/GMT+00:00]");                     \
    VERIFY_PARSE_FAIL(R, u8"2021-03-04[Etc/GMT\u221200]");                 \
    VERIFY_PARSE_FAIL(R, "2021-03-04T23:59:20.1234[.]");                   \
    VERIFY_PARSE_FAIL(R, "2021-03-04T23:59:20.1234z[..]");                 \
    VERIFY_PARSE_FAIL(R, "2021-03-04T23:59:20.1234[ABCD/.]");              \
    VERIFY_PARSE_FAIL(R, "2021-03-04T23:59:20.1234z[EFGH/..]");            \
    VERIFY_PARSE_FAIL(R, "2021-03-04T23:59:20.1234[abcdefghijklmno]");     \
    VERIFY_PARSE_FAIL(R, "2021-03-04 23:59:20.1234[abc/abcdefghijklmno]"); \
    VERIFY_PARSE_FAIL(R, "2021-03-04t23:59:20.1234[+1]");                  \
    VERIFY_PARSE_FAIL(R, "2021-03-04t23:59:20.1234[+123]");                \
    VERIFY_PARSE_FAIL(R, "2021-03-04t23:59:20.1234[+12345]");              \
    VERIFY_PARSE_FAIL(R, "2021-03-04t23:59:20.1234[-1]");                  \
    VERIFY_PARSE_FAIL(R, "2021-03-04t23:59:20.1234[-123]");                \
    VERIFY_PARSE_FAIL(R, "2021-03-04t23:59:20.1234[-12345]");              \
    VERIFY_PARSE_FAIL(R, "2021-03-04t23:59:20.1234[+12:3456]");            \
    VERIFY_PARSE_FAIL(R, "2021-03-04t23:59:20.1234[+1234:56]");            \
    VERIFY_PARSE_FAIL(R, "2021-03-04t23:59:20.1234[+123456.9876543210]");  \
    VERIFY_PARSE_FAIL(R, "2021-03-04t23:59:20.1234[+123456.]");            \
    VERIFY_PARSE_FAIL(R, "2021-03-04t23:59:20.1234[+123456,]");            \
  } while (false)

TEST(TemporalZonedDateTimeStringIllegal) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  VERIFY_PARSE_FAIL_ON_ZONED_DATE_TIME(TemporalZonedDateTimeString);
}

TEST(TemporalRelativeToStringSuccess) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  IMPL_DATE_TIME_STRING_SUCCESS(TemporalRelativeToString);
  IMPL_ZONED_DATE_TIME_STRING_SUCCESS(TemporalRelativeToString);
}

TEST(TemporalRelativeToStringIllegal) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  VERIFY_PARSE_FAIL_ON_DATE(TemporalRelativeToString);
  VERIFY_PARSE_FAIL(TemporalRelativeToString, "+20210304");
  VERIFY_PARSE_FAIL(TemporalRelativeToString, "-20210304");
  VERIFY_PARSE_FAIL(TemporalRelativeToString, u8"\u221220210304");
  VERIFY_PARSE_FAIL(TemporalRelativeToString, "210304");
}

TEST(TemporalCalendarStringSuccess) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  // CalendarName
  VerifyParseTemporalCalendarStringSuccess(isolate, "chinese", "chinese");
  VerifyParseTemporalCalendarStringSuccess(isolate, "roc", "roc");
  VerifyParseTemporalCalendarStringSuccess(isolate, "indian", "indian");
  VerifyParseTemporalCalendarStringSuccess(isolate, "persian", "persian");
  VerifyParseTemporalCalendarStringSuccess(isolate, "abcd-efghi", "abcd-efghi");
  VerifyParseTemporalCalendarStringSuccess(isolate, "abcd-efghi", "abcd-efghi");
  VerifyParseTemporalCalendarStringSuccess(
      isolate, "a2345678-b2345678-c2345678-d7654321",
      "a2345678-b2345678-c2345678-d7654321");
  // TemporalInstantString
  VerifyParseTemporalCalendarStringSuccess(isolate, "2021-11-08z[ABCD]", "");
  // CalendarDateTime
  VerifyParseTemporalCalendarStringSuccess(isolate, "2021-11-08[u-ca=chinese]",
                                           "chinese");
  VerifyParseTemporalCalendarStringSuccess(
      isolate, "2021-11-08[ABCDEFG][u-ca=chinese]", "chinese");
  VerifyParseTemporalCalendarStringSuccess(
      isolate, "2021-11-08[ABCDEFG/hijklmn][u-ca=roc]", "roc");
  // Time
  VerifyParseTemporalCalendarStringSuccess(isolate, "23:45:59", "");
  // DateSpecYearMonth
  VerifyParseTemporalCalendarStringSuccess(isolate, "2021-12", "");
  // DateSpecMonthDay
  VerifyParseTemporalCalendarStringSuccess(isolate, "--12-31", "");
  VerifyParseTemporalCalendarStringSuccess(isolate, "12-31", "");
  VerifyParseTemporalCalendarStringSuccess(isolate, "--1231", "");
}

TEST(TemporalCalendarStringIllegal) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  VERIFY_PARSE_FAIL(TemporalCalendarString, "20210304[u-ca=]");
  VERIFY_PARSE_FAIL(TemporalCalendarString, "20210304[u-ca=a]");
  VERIFY_PARSE_FAIL(TemporalCalendarString, "20210304[u-ca=ab]");
  VERIFY_PARSE_FAIL(TemporalCalendarString, "20210304[u-ca=abcdef-ab]");
  VERIFY_PARSE_FAIL(TemporalCalendarString, "20210304[u-ca=abcdefghijkl]");
}

void CheckDuration(const ParsedISO8601Duration& actual, int64_t sign,
                   int64_t years, int64_t months, int64_t weeks, int64_t days,
                   int64_t whole_hours, int64_t hours_fraction,
                   int64_t whole_minutes, int64_t minutes_fraction,
                   int64_t whole_seconds, int64_t seconds_fraction) {
  CHECK_EQ(sign, actual.sign);
  CHECK_EQ(years, actual.years);
  CHECK_EQ(months, actual.months);
  CHECK_EQ(weeks, actual.weeks);
  CHECK_EQ(days, actual.days);
  CHECK_EQ(whole_hours, actual.whole_hours);
  CHECK_EQ(hours_fraction, actual.hours_fraction);
  CHECK_EQ(whole_minutes, actual.whole_minutes);
  CHECK_EQ(minutes_fraction, actual.minutes_fraction);
  CHECK_EQ(whole_seconds, actual.whole_seconds);
  CHECK_EQ(seconds_fraction, actual.seconds_fraction);
}

void VerifyParseDurationSuccess(Isolate* isolate, const char* str, int64_t sign,
                                int64_t years, int64_t months, int64_t weeks,
                                int64_t days, int64_t whole_hours,
                                int64_t hours_fraction, int64_t whole_minutes,
                                int64_t minutes_fraction, int64_t whole_seconds,
                                int64_t seconds_fraction) {
  Handle<String> input = CcTest::MakeString(str);
  CheckDuration(
      TemporalParser::ParseTemporalDurationString(isolate, input).ToChecked(),
      sign, years, months, weeks, days, whole_hours, hours_fraction,
      whole_minutes, minutes_fraction, whole_seconds, seconds_fraction);
}

void VerifyParseDurationSuccess(Isolate* isolate, const char* str,
                                const ParsedISO8601Duration& expected) {
  VerifyParseDurationSuccess(isolate, str, expected.sign, expected.years,
                             expected.months, expected.weeks, expected.days,
                             expected.whole_hours, expected.hours_fraction,
                             expected.whole_minutes, expected.minutes_fraction,
                             expected.whole_seconds, expected.seconds_fraction);
}

void VerifyParseDurationWithPositiveSign(Isolate* isolate, const char* str) {
  Handle<String> input = CcTest::MakeString(str);
  ParsedISO8601Duration expected =
      TemporalParser::ParseTemporalDurationString(isolate, input).ToChecked();
  std::string with_sign("+");
  with_sign += str;
  VerifyParseDurationSuccess(isolate, with_sign.c_str(), expected);
}

void VerifyParseDurationWithMinusSign(Isolate* isolate, const char* str) {
  std::string with_sign("-");
  with_sign += str;
  Handle<String> input = CcTest::MakeString(with_sign.c_str());
  ParsedISO8601Duration expected =
      TemporalParser::ParseTemporalDurationString(isolate, input).ToChecked();
  with_sign = u8"\u2212";
  with_sign += str;
  VerifyParseDurationSuccess(isolate, with_sign.c_str(), expected);
}

char asciitolower(char in) {
  return (in <= 'Z' && in >= 'A') ? (in - ('Z' - 'z')) : in;
}

void VerifyParseDurationWithLowerCase(Isolate* isolate, const char* str) {
  Handle<String> input = CcTest::MakeString(str);
  ParsedISO8601Duration expected =
      TemporalParser::ParseTemporalDurationString(isolate, input).ToChecked();
  std::string lower(str);
  std::transform(lower.begin(), lower.end(), lower.begin(), asciitolower);
  VerifyParseDurationSuccess(isolate, lower.c_str(), expected);
}

char commatoperiod(char in) { return (in == ',') ? '.' : in; }
void VerifyParseDurationWithComma(Isolate* isolate, const char* str) {
  std::string period(str);
  std::transform(period.begin(), period.end(), period.begin(), commatoperiod);
  Handle<String> input = CcTest::MakeString(str);
  ParsedISO8601Duration expected =
      TemporalParser::ParseTemporalDurationString(isolate, input).ToChecked();
  VerifyParseDurationSuccess(isolate, str, expected);
}

// Test basic cases.
TEST(TemporalDurationStringBasic) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  VerifyParseDurationSuccess(isolate, "PT0S", 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "-PT0S", -1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                             0);

  VerifyParseDurationSuccess(isolate, "P1Y", 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "P2M", 1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "P3W", 1, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "P4D", 1, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "PT5H", 1, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "PT1.987654321H", 1, 0, 0, 0, 0, 1,
                             987654321, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "PT2.9H", 1, 0, 0, 0, 0, 2, 900000000, 0,
                             0, 0, 0);
  VerifyParseDurationSuccess(isolate, "PT6M", 1, 0, 0, 0, 0, 0, 0, 6, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "PT2.234567891M", 1, 0, 0, 0, 0, 0, 0, 2,
                             234567891, 0, 0);
  VerifyParseDurationSuccess(isolate, "PT3.23M", 1, 0, 0, 0, 0, 0, 0, 3,
                             230000000, 0, 0);
  VerifyParseDurationSuccess(isolate, "PT7S", 1, 0, 0, 0, 0, 0, 0, 0, 0, 7, 0);
  VerifyParseDurationSuccess(isolate, "PT3.345678912S", 1, 0, 0, 0, 0, 0, 0, 0,
                             0, 3, 345678912);
  VerifyParseDurationSuccess(isolate, "PT4.345S", 1, 0, 0, 0, 0, 0, 0, 0, 0, 4,
                             345000000);

  VerifyParseDurationSuccess(isolate, "P1Y2M3W4DT5.6H7.8M9.1S", 1, 1, 2, 3, 4,
                             5, 600000000, 7, 800000000, 9, 100000000);
  VerifyParseDurationSuccess(isolate, "-P9Y8M7W6DT5.4H3.2M1.9S", -1, 9, 8, 7, 6,
                             5, 400000000, 3, 200000000, 1, 900000000);

  VerifyParseDurationSuccess(isolate, "P0Y0M0W0DT0.0H0.0M0.0S", 1, 0, 0, 0, 0,
                             0, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "-P0Y0M0W0DT0.0H0.0M0.0S", -1, 0, 0, 0, 0,
                             0, 0, 0, 0, 0, 0);
}

// Test duration with ascii minus sign parsed correctly.
TEST(TemporalDurationStringNegative) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  VerifyParseDurationSuccess(isolate, "-P1Y", -1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "-P2M", -1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "-P3W", -1, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "-P4D", -1, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "-PT5H", -1, 0, 0, 0, 0, 5, 0, 0, 0, 0,
                             0);
  VerifyParseDurationSuccess(isolate, "-PT4.123H", -1, 0, 0, 0, 0, 4, 123000000,
                             0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "-PT3.123456H", -1, 0, 0, 0, 0, 3,
                             123456000, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "-PT6M", -1, 0, 0, 0, 0, 0, 0, 6, 0, 0,
                             0);
  VerifyParseDurationSuccess(isolate, "-PT5.2M", -1, 0, 0, 0, 0, 0, 0, 5,
                             200000000, 0, 0);
  VerifyParseDurationSuccess(isolate, "-PT4.3456M", -1, 0, 0, 0, 0, 0, 0, 4,
                             345600000, 0, 0);
  VerifyParseDurationSuccess(isolate, "-PT7S", -1, 0, 0, 0, 0, 0, 0, 0, 0, 7,
                             0);
  VerifyParseDurationSuccess(isolate, "-PT6.987S", -1, 0, 0, 0, 0, 0, 0, 0, 0,
                             6, 987000000);
}

// Test duration with + sign parsed the same as without + sign.
TEST(TemporalDurationStringPlus) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  // Check + Sign
  VerifyParseDurationWithPositiveSign(isolate, "P1Y");
  VerifyParseDurationWithPositiveSign(isolate, "P2M");
  VerifyParseDurationWithPositiveSign(isolate, "P3W");
  VerifyParseDurationWithPositiveSign(isolate, "P4D");
  VerifyParseDurationWithPositiveSign(isolate, "PT5H");
  VerifyParseDurationWithPositiveSign(isolate, "PT1.987654321H");
  VerifyParseDurationWithPositiveSign(isolate, "PT2.9H");
  VerifyParseDurationWithPositiveSign(isolate, "PT6M");
  VerifyParseDurationWithPositiveSign(isolate, "PT2.234567891M");
  VerifyParseDurationWithPositiveSign(isolate, "PT3.23M");
  VerifyParseDurationWithPositiveSign(isolate, "PT7S");
  VerifyParseDurationWithPositiveSign(isolate, "PT3.345678912S");
}

// Test duration with Unicode U+2212 minus sign parsed the same as ascii - sign.
TEST(TemporalDurationStringMinus) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  // Check + Sign
  VerifyParseDurationWithMinusSign(isolate, "P1Y");
  VerifyParseDurationWithMinusSign(isolate, "P2M");
  VerifyParseDurationWithMinusSign(isolate, "P3W");
  VerifyParseDurationWithMinusSign(isolate, "P4D");
  VerifyParseDurationWithMinusSign(isolate, "PT5H");
  VerifyParseDurationWithMinusSign(isolate, "PT1.987654321H");
  VerifyParseDurationWithMinusSign(isolate, "PT2.9H");
  VerifyParseDurationWithMinusSign(isolate, "PT6M");
  VerifyParseDurationWithMinusSign(isolate, "PT2.234567891M");
  VerifyParseDurationWithMinusSign(isolate, "PT3.23M");
  VerifyParseDurationWithMinusSign(isolate, "PT7S");
  VerifyParseDurationWithMinusSign(isolate, "PT3.345678912S");
}

// Test duration in lower case mark parsed the same as with upper case mark.
TEST(TemporalDurationStringLowerCase) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  // Check + Sign
  VerifyParseDurationWithLowerCase(isolate, "P1Y");
  VerifyParseDurationWithLowerCase(isolate, "P2M");
  VerifyParseDurationWithLowerCase(isolate, "P3W");
  VerifyParseDurationWithLowerCase(isolate, "P4D");
  VerifyParseDurationWithLowerCase(isolate, "PT5H");
  VerifyParseDurationWithLowerCase(isolate, "PT1.987654321H");
  VerifyParseDurationWithLowerCase(isolate, "PT2.9H");
  VerifyParseDurationWithLowerCase(isolate, "PT6M");
  VerifyParseDurationWithLowerCase(isolate, "PT2.234567891M");
  VerifyParseDurationWithLowerCase(isolate, "PT3.23M");
  VerifyParseDurationWithLowerCase(isolate, "PT7S");
  VerifyParseDurationWithLowerCase(isolate, "PT3.345678912S");
}

TEST(TemporalDurationStringComma) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  VerifyParseDurationWithComma(isolate, "PT1,987654321H");
  VerifyParseDurationWithComma(isolate, "PT2,9H");
  VerifyParseDurationWithComma(isolate, "PT2,234567891M");
  VerifyParseDurationWithComma(isolate, "PT3,23M");
  VerifyParseDurationWithComma(isolate, "PT3,345678912S");
}

TEST(TemporalDurationStringLongDigits) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  VerifyParseDurationSuccess(isolate, "P8999999999999999999Y", 1,
                             8999999999999999999, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "P8999999999999999998M", 1, 0,
                             8999999999999999998, 0, 0, 0, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "P8999999999999999997W", 1, 0, 0,
                             8999999999999999997, 0, 0, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "P8999999999999999996D", 1, 0, 0, 0,
                             8999999999999999996, 0, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "PT8999999999999999995H", 1, 0, 0, 0, 0,
                             8999999999999999995, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "PT8999999999999999994M", 1, 0, 0, 0, 0,
                             0, 0, 8999999999999999994, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "PT8999999999999999993S", 1, 0, 0, 0, 0,
                             0, 0, 0, 0, 8999999999999999993, 0);

  VerifyParseDurationSuccess(isolate, "PT0.999999999H", 1, 0, 0, 0, 0, 0,
                             999999999, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "PT0.999999999M", 1, 0, 0, 0, 0, 0, 0, 0,
                             999999999, 0, 0);
  VerifyParseDurationSuccess(isolate, "PT0.999999999S", 1, 0, 0, 0, 0, 0, 0, 0,
                             0, 0, 999999999);

  VerifyParseDurationSuccess(isolate, "-P8999999999999999999Y", -1,
                             8999999999999999999, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "-P8999999999999999998M", -1, 0,
                             8999999999999999998, 0, 0, 0, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "-P8999999999999999997W", -1, 0, 0,
                             8999999999999999997, 0, 0, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "-P8999999999999999996D", -1, 0, 0, 0,
                             8999999999999999996, 0, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "-PT8999999999999999995H", -1, 0, 0, 0, 0,
                             8999999999999999995, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "-PT8999999999999999995H", -1, 0, 0, 0, 0,
                             8999999999999999995, 0, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "-PT8999999999999999994M", -1, 0, 0, 0, 0,
                             0, 0, 8999999999999999994, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "-PT8999999999999999993S", -1, 0, 0, 0, 0,
                             0, 0, 0, 0, 8999999999999999993, 0);

  VerifyParseDurationSuccess(isolate, "-PT0.999999999H", -1, 0, 0, 0, 0, 0,
                             999999999, 0, 0, 0, 0);
  VerifyParseDurationSuccess(isolate, "-PT0.999999999M", -1, 0, 0, 0, 0, 0, 0,
                             0, 999999999, 0, 0);
  VerifyParseDurationSuccess(isolate, "-PT0.999999999S", -1, 0, 0, 0, 0, 0, 0,
                             0, 0, 0, 999999999);
}

TEST(TemporalDurationStringNotSatisfy) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());
  VERIFY_PARSE_FAIL(TemporalDurationString, "");

  // Missing P
  VERIFY_PARSE_FAIL(TemporalDurationString, "1Y");
  VERIFY_PARSE_FAIL(TemporalDurationString, "1M");
  VERIFY_PARSE_FAIL(TemporalDurationString, "+1W");
  VERIFY_PARSE_FAIL(TemporalDurationString, "-1D");

  // fraction with years, months, weeks or days
  VERIFY_PARSE_FAIL(TemporalDurationString, "P1.1Y");
  VERIFY_PARSE_FAIL(TemporalDurationString, "P2.2M");
  VERIFY_PARSE_FAIL(TemporalDurationString, "P3.3W");
  VERIFY_PARSE_FAIL(TemporalDurationString, "P4.4D");

  // Time without T
  VERIFY_PARSE_FAIL(TemporalDurationString, "P1H");
  VERIFY_PARSE_FAIL(TemporalDurationString, "P1S");

  // Sign after P
  VERIFY_PARSE_FAIL(TemporalDurationString, "P+1Y");
  VERIFY_PARSE_FAIL(TemporalDurationString, "P-2M");
  VERIFY_PARSE_FAIL(TemporalDurationString, u8"P\u22123W");
  VERIFY_PARSE_FAIL(TemporalDurationString, "P+4D");
  VERIFY_PARSE_FAIL(TemporalDurationString, "PT-4H");
  VERIFY_PARSE_FAIL(TemporalDurationString, "PT+5M");

  // with :
  VERIFY_PARSE_FAIL(TemporalDurationString, "PT01:22");

  // more than 9 digits in fraction
  VERIFY_PARSE_FAIL(TemporalDurationString, "PT1.9876543219H");
  VERIFY_PARSE_FAIL(TemporalDurationString, "PT0.9876543219M");
  VERIFY_PARSE_FAIL(TemporalDurationString, "PT2.9876543219S");

  // out of order
  VERIFY_PARSE_FAIL(TemporalDurationString, "P2M1Y");
  VERIFY_PARSE_FAIL(TemporalDurationString, "P3W4M");
  VERIFY_PARSE_FAIL(TemporalDurationString, "P5D6W");
  VERIFY_PARSE_FAIL(TemporalDurationString, "PT1H6Y");
  VERIFY_PARSE_FAIL(TemporalDurationString, "PT1M6W");
  VERIFY_PARSE_FAIL(TemporalDurationString, "PT1S6D");

  // Extra in the end
  VERIFY_PARSE_FAIL(TemporalDurationString, "P1Y ");
  VERIFY_PARSE_FAIL(TemporalDurationString, "P1Yp");
  VERIFY_PARSE_FAIL(TemporalDurationString, "P2M:");

  // Extra in the beginning
  VERIFY_PARSE_FAIL(TemporalDurationString, "pP1Y");
  VERIFY_PARSE_FAIL(TemporalDurationString, " P1Y");
  VERIFY_PARSE_FAIL(TemporalDurationString, ".P2M");

  // Fraction without digit
  VERIFY_PARSE_FAIL(TemporalDurationString, "PT.1H");
  VERIFY_PARSE_FAIL(TemporalDurationString, "PT.2M");
  VERIFY_PARSE_FAIL(TemporalDurationString, "PT.3S");

  // without date nor time
  VERIFY_PARSE_FAIL(TemporalDurationString, "P");
  VERIFY_PARSE_FAIL(TemporalDurationString, "PT");
}

void VerifyParseTimeZoneNumericUTCOffsetSuccess(
    Isolate* isolate, const char* str, int32_t tzuo_sign, int32_t tzuo_hour,
    int32_t tzuo_minute, int32_t tzuo_second, int32_t tzuo_nanosecond) {
  Handle<String> input = CcTest::MakeString(str);
  CheckTimeZoneNumericUTCOffset(
      TemporalParser::ParseTimeZoneNumericUTCOffset(isolate, input).ToChecked(),
      tzuo_sign, tzuo_hour, tzuo_minute, tzuo_second, tzuo_nanosecond);
}

TEST(TimeZoneNumericUTCOffsetBasic) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  // TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "+00", 1, 0, kUndefined,
                                             kUndefined, kUndefined);
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "+23", 1, 23, kUndefined,
                                             kUndefined, kUndefined);
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "-23", -1, 23, kUndefined,
                                             kUndefined, kUndefined);
  VerifyParseTimeZoneNumericUTCOffsetSuccess(
      isolate, u8"\u221223", -1, 23, kUndefined, kUndefined, kUndefined);

  // TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour : TimeZoneUTCOffsetMinute
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "+01:00", 1, 1, 0,
                                             kUndefined, kUndefined);
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "+21:59", 1, 21, 59,
                                             kUndefined, kUndefined);
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "-20:48", -1, 20, 48,
                                             kUndefined, kUndefined);
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, u8"\u221219:33", -1, 19,
                                             33, kUndefined, kUndefined);

  // TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour TimeZoneUTCOffsetMinute
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "+0100", 1, 1, 0,
                                             kUndefined, kUndefined);
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "+2159", 1, 21, 59,
                                             kUndefined, kUndefined);
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "-2048", -1, 20, 48,
                                             kUndefined, kUndefined);
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, u8"\u22121933", -1, 19,
                                             33, kUndefined, kUndefined);

  // TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour : TimeZoneUTCOffsetMinute :
  // TimeZoneUTCOffsetSecond TimeZoneUTCOffsetFractionopt
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "+01:00:59", 1, 1, 0, 59,
                                             kUndefined);
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "+15:34:33", 1, 15, 34,
                                             33, kUndefined);
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "-09:59:00", -1, 9, 59,
                                             00, kUndefined);
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, u8"\u221218:53:22", -1,
                                             18, 53, 22, kUndefined);

  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "+01:00:59.987654321", 1,
                                             1, 0, 59, 987654321);
  // ',' as DecimalSeparator
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "+01:00:59,123456789", 1,
                                             1, 0, 59, 123456789);
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "-09:59:00.9", -1, 9, 59,
                                             00, 900000000);
  // ',' as DecimalSeparator
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "-09:59:00,000000001", -1,
                                             9, 59, 00, 1);
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "-09:59:00.000000001", -1,
                                             9, 59, 00, 1);

  // TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour TimeZoneUTCOffsetMinute
  // TimeZoneUTCOffsetSecond TimeZoneUTCOffsetFractionopt
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "+010059", 1, 1, 0, 59,
                                             kUndefined);
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "+153433", 1, 15, 34, 33,
                                             kUndefined);
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "-095900", -1, 9, 59, 00,
                                             kUndefined);
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, u8"\u2212185322", -1, 18,
                                             53, 22, kUndefined);

  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "+010059.987654321", 1, 1,
                                             0, 59, 987654321);
  // ',' as DecimalSeparator
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "+010059,123456789", 1, 1,
                                             0, 59, 123456789);
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "-095900.9", -1, 9, 59,
                                             00, 900000000);
  // ',' as DecimalSeparator
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "-095900,000000001", -1,
                                             9, 59, 00, 1);
  VerifyParseTimeZoneNumericUTCOffsetSuccess(isolate, "-095900.000000001", -1,
                                             9, 59, 00, 1);
}

TEST(TimeZoneNumericUTCOffsetIllegal) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  v8::HandleScope scope(CcTest::isolate());

  // Without TimeZoneUTCOffsetSign
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "0");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "9");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "00");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "01");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "23");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "ff");

  // Wrong TimeZoneUTCOffsetHour
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+24");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "-24");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, u8"\u221224");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+ab");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "-2a");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, u8"\u22122Z");
  // Single digit is not TimeZoneUTCOffsetHour
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+0");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+2");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "-1");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, u8"\u22123");

  // Extra
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+23 ");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, " -22");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+23:");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "-22:");

  // Wrong TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour : TimeZoneUTCOffsetMinute
  // single digit TimeZoneUTCOffsetMinute
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+01:0");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+21:5");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "-20:4");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, u8"\u221219:3");
  // TimeZoneUTCOffsetMinute out of range
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+01:60");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+21:5a");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "-20:4f");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, u8"\u221219:a0");

  // Wrong TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour TimeZoneUTCOffsetMinute
  // single digit TimeZoneUTCOffsetMinute
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+010");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+215");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "-204");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, u8"\u2212193");
  // TimeZoneUTCOffsetMinute out of range
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+0160");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+215a");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "-204f");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, u8"\u221219a0");

  // TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour : TimeZoneUTCOffsetMinute :
  // TimeZoneUTCOffsetSecond TimeZoneUTCOffsetFractionopt with : here but not
  // there
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+01:0059");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+1534:33");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "-07:34:339");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "-07:34:.9");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "-07:34:,9");
  // fraction too long
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "-07:34:01.9876543219");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+07:34:01,9876543219");
  // fraction in hour or minute
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+01.0:00:59");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+01:00.1:59");

  // Wrong TimeZoneUTCOffsetSign TimeZoneUTCOffsetHour TimeZoneUTCOffsetMinute
  // TimeZoneUTCOffsetSecond TimeZoneUTCOffsetFractionopt
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+0100.159");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+01.15009");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "-0100,159");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "-01,15009");
  // fraction too long
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "-073401.0000000000");
  VERIFY_PARSE_FAIL(TimeZoneNumericUTCOffset, "+073401,9876543219");
}

}  // namespace internal
}  // namespace v8
