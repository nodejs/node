// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
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

#include "src/date.h"
#include "src/global-handles.h"
#include "src/isolate.h"
#include "src/v8.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;

class DateCacheMock: public DateCache {
 public:
  struct Rule {
    int year, start_month, start_day, end_month, end_day, offset_sec;
  };

  DateCacheMock(int local_offset, Rule* rules, int rules_count)
      : local_offset_(local_offset), rules_(rules), rules_count_(rules_count) {}

 protected:
  virtual int GetDaylightSavingsOffsetFromOS(int64_t time_sec) {
    int days = DaysFromTime(time_sec * 1000);
    int time_in_day_sec = TimeInDay(time_sec * 1000, days) / 1000;
    int year, month, day;
    YearMonthDayFromDays(days, &year, &month, &day);
    Rule* rule = FindRuleFor(year, month, day, time_in_day_sec);
    return rule == NULL ? 0 : rule->offset_sec * 1000;
  }


  virtual int GetLocalOffsetFromOS() {
    return local_offset_;
  }

 private:
  Rule* FindRuleFor(int year, int month, int day, int time_in_day_sec) {
    Rule* result = NULL;
    for (int i = 0; i < rules_count_; i++)
      if (Match(&rules_[i], year, month, day, time_in_day_sec)) {
        result = &rules_[i];
      }
    return result;
  }


  bool Match(Rule* rule, int year, int month, int day, int time_in_day_sec) {
    if (rule->year != 0 && rule->year != year) return false;
    if (rule->start_month > month) return false;
    if (rule->end_month < month) return false;
    int start_day = ComputeRuleDay(year, rule->start_month, rule->start_day);
    if (rule->start_month == month && start_day > day) return false;
    if (rule->start_month == month && start_day == day &&
        2 * 3600 > time_in_day_sec)
      return false;
    int end_day = ComputeRuleDay(year, rule->end_month, rule->end_day);
    if (rule->end_month == month && end_day < day) return false;
    if (rule->end_month == month && end_day == day &&
        2 * 3600 <= time_in_day_sec)
      return false;
    return true;
  }


  int ComputeRuleDay(int year, int month, int day) {
    if (day != 0) return day;
    int days = DaysFromYearMonth(year, month);
    // Find the first Sunday of the month.
    while (Weekday(days + day) != 6) day++;
    return day + 1;
  }

  int local_offset_;
  Rule* rules_;
  int rules_count_;
};

static int64_t TimeFromYearMonthDay(DateCache* date_cache,
                                    int year,
                                    int month,
                                    int day) {
  int64_t result = date_cache->DaysFromYearMonth(year, month);
  return (result + day - 1) * DateCache::kMsPerDay;
}


static void CheckDST(int64_t time) {
  Isolate* isolate = CcTest::i_isolate();
  DateCache* date_cache = isolate->date_cache();
  int64_t actual = date_cache->ToLocal(time);
  int64_t expected = time + date_cache->GetLocalOffsetFromOS() +
                     date_cache->GetDaylightSavingsOffsetFromOS(time / 1000);
  CHECK_EQ(actual, expected);
}


TEST(DaylightSavingsTime) {
  LocalContext context;
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope scope(isolate);
  DateCacheMock::Rule rules[] = {
    {0, 2, 0, 10, 0, 3600},  // DST from March to November in any year.
    {2010, 2, 0, 7, 20, 3600},  // DST from March to August 20 in 2010.
    {2010, 7, 20, 8, 10, 0},  // No DST from August 20 to September 10 in 2010.
    {2010, 8, 10, 10, 0, 3600},  // DST from September 10 to November in 2010.
  };

  int local_offset_ms = -36000000;  // -10 hours.

  DateCacheMock* date_cache =
    new DateCacheMock(local_offset_ms, rules, arraysize(rules));

  reinterpret_cast<Isolate*>(isolate)->set_date_cache(date_cache);

  int64_t start_of_2010 = TimeFromYearMonthDay(date_cache, 2010, 0, 1);
  int64_t start_of_2011 = TimeFromYearMonthDay(date_cache, 2011, 0, 1);
  int64_t august_20 = TimeFromYearMonthDay(date_cache, 2010, 7, 20);
  int64_t september_10 = TimeFromYearMonthDay(date_cache, 2010, 8, 10);
  CheckDST((august_20 + september_10) / 2);
  CheckDST(september_10);
  CheckDST(september_10 + 2 * 3600);
  CheckDST(september_10 + 2 * 3600 - 1000);
  CheckDST(august_20 + 2 * 3600);
  CheckDST(august_20 + 2 * 3600 - 1000);
  CheckDST(august_20);
  // Check each day of 2010.
  for (int64_t time = start_of_2011 + 2 * 3600;
       time >= start_of_2010;
       time -= DateCache::kMsPerDay) {
    CheckDST(time);
    CheckDST(time - 1000);
    CheckDST(time + 1000);
  }
  // Check one day from 2010 to 2100.
  for (int year = 2100; year >= 2010; year--) {
    CheckDST(TimeFromYearMonthDay(date_cache, year, 5, 5));
  }
  CheckDST((august_20 + september_10) / 2);
  CheckDST(september_10);
  CheckDST(september_10 + 2 * 3600);
  CheckDST(september_10 + 2 * 3600 - 1000);
  CheckDST(august_20 + 2 * 3600);
  CheckDST(august_20 + 2 * 3600 - 1000);
  CheckDST(august_20);
}

namespace {
int legacy_parse_count = 0;
void DateParseLegacyCounterCallback(v8::Isolate* isolate,
                                    v8::Isolate::UseCounterFeature feature) {
  if (feature == v8::Isolate::kLegacyDateParser) legacy_parse_count++;
}
}  // anonymous namespace

TEST(DateParseLegacyUseCounter) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  LocalContext context;
  CcTest::isolate()->SetUseCounterCallback(DateParseLegacyCounterCallback);
  CHECK_EQ(0, legacy_parse_count);
  CompileRun("Date.parse('2015-02-31')");
  CHECK_EQ(0, legacy_parse_count);
  CompileRun("Date.parse('2015-02-31T11:22:33.444Z01:23')");
  CHECK_EQ(0, legacy_parse_count);
  CompileRun("Date.parse('2015-02-31T11:22:33.444')");
  CHECK_EQ(0, legacy_parse_count);
  CompileRun("Date.parse('2000 01 01')");
  CHECK_EQ(1, legacy_parse_count);
  CompileRun("Date.parse('2015-02-31T11:22:33.444     ')");
  CHECK_EQ(1, legacy_parse_count);
}

#ifdef V8_INTL_SUPPORT
TEST(DateCacheVersion) {
  FLAG_allow_natives_syntax = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);
  v8::Local<v8::Number> date_cache_version =
      v8::Local<v8::Number>::Cast(CompileRun("%DateCacheVersion()"));

  CHECK(date_cache_version->IsNumber());
  CHECK_EQ(0.0, date_cache_version->NumberValue(context).FromMaybe(-1.0));

  v8::Date::DateTimeConfigurationChangeNotification(isolate);

  date_cache_version =
      v8::Local<v8::Number>::Cast(CompileRun("%DateCacheVersion()"));
  CHECK(date_cache_version->IsNumber());
  CHECK_EQ(1.0, date_cache_version->NumberValue(context).FromMaybe(-1.0));
}
#endif  // V8_INTL_SUPPORT
