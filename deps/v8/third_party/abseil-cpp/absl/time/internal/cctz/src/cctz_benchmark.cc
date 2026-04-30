// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <algorithm>
#include <cassert>
#include <chrono>
#include <ctime>
#include <random>
#include <string>
#include <vector>

#include "benchmark/benchmark.h"
#include "absl/time/internal/cctz/include/cctz/civil_time.h"
#include "absl/time/internal/cctz/include/cctz/time_zone.h"
#include "absl/time/internal/cctz/src/test_time_zone_names.h"
#include "absl/time/internal/cctz/src/time_zone_impl.h"

namespace {

namespace cctz = absl::time_internal::cctz;

void BM_Difference_Days(benchmark::State& state) {
  const cctz::civil_day c(2014, 8, 22);
  const cctz::civil_day epoch(1970, 1, 1);
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(c - epoch);
  }
}
BENCHMARK(BM_Difference_Days);

void BM_Step_Days(benchmark::State& state) {
  const cctz::civil_day kStart(2014, 8, 22);
  cctz::civil_day c = kStart;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(++c);
  }
}
BENCHMARK(BM_Step_Days);

void BM_GetWeekday(benchmark::State& state) {
  const cctz::civil_day c(2014, 8, 22);
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(cctz::get_weekday(c));
  }
}
BENCHMARK(BM_GetWeekday);

void BM_NextWeekday(benchmark::State& state) {
  const cctz::civil_day kStart(2014, 8, 22);
  const cctz::civil_day kDays[7] = {
      kStart + 0, kStart + 1, kStart + 2, kStart + 3,
      kStart + 4, kStart + 5, kStart + 6,
  };
  const cctz::weekday kWeekdays[7] = {
      cctz::weekday::monday,   cctz::weekday::tuesday, cctz::weekday::wednesday,
      cctz::weekday::thursday, cctz::weekday::friday,  cctz::weekday::saturday,
      cctz::weekday::sunday,
  };
  while (state.KeepRunningBatch(7 * 7)) {
    for (const auto from : kDays) {
      for (const auto to : kWeekdays) {
        benchmark::DoNotOptimize(cctz::next_weekday(from, to));
      }
    }
  }
}
BENCHMARK(BM_NextWeekday);

void BM_PrevWeekday(benchmark::State& state) {
  const cctz::civil_day kStart(2014, 8, 22);
  const cctz::civil_day kDays[7] = {
      kStart + 0, kStart + 1, kStart + 2, kStart + 3,
      kStart + 4, kStart + 5, kStart + 6,
  };
  const cctz::weekday kWeekdays[7] = {
      cctz::weekday::monday,   cctz::weekday::tuesday, cctz::weekday::wednesday,
      cctz::weekday::thursday, cctz::weekday::friday,  cctz::weekday::saturday,
      cctz::weekday::sunday,
  };
  while (state.KeepRunningBatch(7 * 7)) {
    for (const auto from : kDays) {
      for (const auto to : kWeekdays) {
        benchmark::DoNotOptimize(cctz::prev_weekday(from, to));
      }
    }
  }
}
BENCHMARK(BM_PrevWeekday);

const char RFC3339_full[] = "%Y-%m-%d%ET%H:%M:%E*S%Ez";
const char RFC3339_sec[] = "%Y-%m-%d%ET%H:%M:%S%Ez";

const char RFC1123_full[] = "%a, %d %b %Y %H:%M:%S %z";
const char RFC1123_no_wday[] = "%d %b %Y %H:%M:%S %z";

std::vector<std::string> AllTimeZoneNames() {
  std::vector<std::string> names;
  for (const char* const* namep = cctz::kTimeZoneNames; *namep != nullptr;
       ++namep) {
    names.push_back(std::string("file:") + *namep);
  }
  assert(!names.empty());

  std::mt19937 urbg(42);  // a UniformRandomBitGenerator with fixed seed
  std::shuffle(names.begin(), names.end(), urbg);
  return names;
}

cctz::time_zone TestTimeZone() {
  cctz::time_zone tz;
  cctz::load_time_zone("America/Los_Angeles", &tz);
  return tz;
}

void BM_Zone_LoadUTCTimeZoneFirst(benchmark::State& state) {
  cctz::time_zone tz;
  cctz::load_time_zone("UTC", &tz);  // in case we're first
  cctz::time_zone::Impl::ClearTimeZoneMapTestOnly();
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(cctz::load_time_zone("UTC", &tz));
  }
}
BENCHMARK(BM_Zone_LoadUTCTimeZoneFirst);

void BM_Zone_LoadUTCTimeZoneLast(benchmark::State& state) {
  cctz::time_zone tz;
  for (const auto& name : AllTimeZoneNames()) {
    cctz::load_time_zone(name, &tz);  // prime cache
  }
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(cctz::load_time_zone("UTC", &tz));
  }
}
BENCHMARK(BM_Zone_LoadUTCTimeZoneLast);

void BM_Zone_LoadTimeZoneFirst(benchmark::State& state) {
  cctz::time_zone tz = cctz::utc_time_zone();  // in case we're first
  const std::string name = "file:America/Los_Angeles";
  while (state.KeepRunning()) {
    state.PauseTiming();
    cctz::time_zone::Impl::ClearTimeZoneMapTestOnly();
    state.ResumeTiming();
    benchmark::DoNotOptimize(cctz::load_time_zone(name, &tz));
  }
}
BENCHMARK(BM_Zone_LoadTimeZoneFirst);

void BM_Zone_LoadTimeZoneCached(benchmark::State& state) {
  cctz::time_zone tz = cctz::utc_time_zone();  // in case we're first
  cctz::time_zone::Impl::ClearTimeZoneMapTestOnly();
  const std::string name = "file:America/Los_Angeles";
  cctz::load_time_zone(name, &tz);  // prime cache
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(cctz::load_time_zone(name, &tz));
  }
}
BENCHMARK(BM_Zone_LoadTimeZoneCached);

void BM_Zone_LoadLocalTimeZoneCached(benchmark::State& state) {
  cctz::utc_time_zone();  // in case we're first
  cctz::time_zone::Impl::ClearTimeZoneMapTestOnly();
  cctz::local_time_zone();  // prime cache
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(cctz::local_time_zone());
  }
}
BENCHMARK(BM_Zone_LoadLocalTimeZoneCached);

void BM_Zone_LoadAllTimeZonesFirst(benchmark::State& state) {
  cctz::time_zone tz;
  const std::vector<std::string> names = AllTimeZoneNames();
  for (auto index = names.size(); state.KeepRunning(); ++index) {
    if (index == names.size()) {
      index = 0;
    }
    if (index == 0) {
      state.PauseTiming();
      cctz::time_zone::Impl::ClearTimeZoneMapTestOnly();
      state.ResumeTiming();
    }
    benchmark::DoNotOptimize(cctz::load_time_zone(names[index], &tz));
  }
}
BENCHMARK(BM_Zone_LoadAllTimeZonesFirst);

void BM_Zone_LoadAllTimeZonesCached(benchmark::State& state) {
  cctz::time_zone tz;
  const std::vector<std::string> names = AllTimeZoneNames();
  for (const auto& name : names) {
    cctz::load_time_zone(name, &tz);  // prime cache
  }
  for (auto index = names.size(); state.KeepRunning(); ++index) {
    if (index == names.size()) {
      index = 0;
    }
    benchmark::DoNotOptimize(cctz::load_time_zone(names[index], &tz));
  }
}
BENCHMARK(BM_Zone_LoadAllTimeZonesCached);

void BM_Zone_TimeZoneEqualityImplicit(benchmark::State& state) {
  cctz::time_zone tz;  // implicit UTC
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(tz == tz);
  }
}
BENCHMARK(BM_Zone_TimeZoneEqualityImplicit);

void BM_Zone_TimeZoneEqualityExplicit(benchmark::State& state) {
  cctz::time_zone tz = cctz::utc_time_zone();  // explicit UTC
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(tz == tz);
  }
}
BENCHMARK(BM_Zone_TimeZoneEqualityExplicit);

void BM_Zone_UTCTimeZone(benchmark::State& state) {
  cctz::time_zone tz;
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(cctz::utc_time_zone());
  }
}
BENCHMARK(BM_Zone_UTCTimeZone);

// In each "ToCivil" benchmark we switch between two instants separated
// by at least one transition in order to defeat any internal caching of
// previous results (e.g., see local_time_hint_).
//
// The "UTC" variants use UTC instead of the Google/local time zone.

void BM_Time_ToCivil_CCTZ(benchmark::State& state) {
  const cctz::time_zone tz = TestTimeZone();
  std::chrono::system_clock::time_point tp =
      std::chrono::system_clock::from_time_t(1384569027);
  std::chrono::system_clock::time_point tp2 =
      std::chrono::system_clock::from_time_t(1418962578);
  while (state.KeepRunning()) {
    std::swap(tp, tp2);
    tp += std::chrono::seconds(1);
    benchmark::DoNotOptimize(cctz::convert(tp, tz));
  }
}
BENCHMARK(BM_Time_ToCivil_CCTZ);

void BM_Time_ToCivil_Libc(benchmark::State& state) {
  // No timezone support, so just use localtime.
  time_t t = 1384569027;
  time_t t2 = 1418962578;
  struct tm tm;
  while (state.KeepRunning()) {
    std::swap(t, t2);
    t += 1;
#if defined(_WIN32) || defined(_WIN64)
    benchmark::DoNotOptimize(localtime_s(&tm, &t));
#else
    benchmark::DoNotOptimize(localtime_r(&t, &tm));
#endif
  }
}
BENCHMARK(BM_Time_ToCivil_Libc);

void BM_Time_ToCivilUTC_CCTZ(benchmark::State& state) {
  const cctz::time_zone tz = cctz::utc_time_zone();
  std::chrono::system_clock::time_point tp =
      std::chrono::system_clock::from_time_t(1384569027);
  while (state.KeepRunning()) {
    tp += std::chrono::seconds(1);
    benchmark::DoNotOptimize(cctz::convert(tp, tz));
  }
}
BENCHMARK(BM_Time_ToCivilUTC_CCTZ);

void BM_Time_ToCivilUTC_Libc(benchmark::State& state) {
  time_t t = 1384569027;
  struct tm tm;
  while (state.KeepRunning()) {
    t += 1;
#if defined(_WIN32) || defined(_WIN64)
    benchmark::DoNotOptimize(gmtime_s(&tm, &t));
#else
    benchmark::DoNotOptimize(gmtime_r(&t, &tm));
#endif
  }
}
BENCHMARK(BM_Time_ToCivilUTC_Libc);

// In each "FromCivil" benchmark we switch between two YMDhms values
// separated by at least one transition in order to defeat any internal
// caching of previous results (e.g., see time_local_hint_).
//
// The "UTC" variants use UTC instead of the Google/local time zone.
// The "Day0" variants require normalization of the day of month.

void BM_Time_FromCivil_CCTZ(benchmark::State& state) {
  const cctz::time_zone tz = TestTimeZone();
  int i = 0;
  while (state.KeepRunning()) {
    if ((i++ & 1) == 0) {
      benchmark::DoNotOptimize(
          cctz::convert(cctz::civil_second(2014, 12, 18, 20, 16, 18), tz));
    } else {
      benchmark::DoNotOptimize(
          cctz::convert(cctz::civil_second(2013, 11, 15, 18, 30, 27), tz));
    }
  }
}
BENCHMARK(BM_Time_FromCivil_CCTZ);

void BM_Time_FromCivil_Libc(benchmark::State& state) {
  // No timezone support, so just use localtime.
  int i = 0;
  while (state.KeepRunning()) {
    struct tm tm;
    if ((i++ & 1) == 0) {
      tm.tm_year = 2014 - 1900;
      tm.tm_mon = 12 - 1;
      tm.tm_mday = 18;
      tm.tm_hour = 20;
      tm.tm_min = 16;
      tm.tm_sec = 18;
    } else {
      tm.tm_year = 2013 - 1900;
      tm.tm_mon = 11 - 1;
      tm.tm_mday = 15;
      tm.tm_hour = 18;
      tm.tm_min = 30;
      tm.tm_sec = 27;
    }
    tm.tm_isdst = -1;
    benchmark::DoNotOptimize(mktime(&tm));
  }
}
BENCHMARK(BM_Time_FromCivil_Libc);

void BM_Time_FromCivilUTC_CCTZ(benchmark::State& state) {
  const cctz::time_zone tz = cctz::utc_time_zone();
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(
        cctz::convert(cctz::civil_second(2014, 12, 18, 20, 16, 18), tz));
  }
}
BENCHMARK(BM_Time_FromCivilUTC_CCTZ);

// There is no BM_Time_FromCivilUTC_Libc.

void BM_Time_FromCivilDay0_CCTZ(benchmark::State& state) {
  const cctz::time_zone tz = TestTimeZone();
  int i = 0;
  while (state.KeepRunning()) {
    if ((i++ & 1) == 0) {
      benchmark::DoNotOptimize(
          cctz::convert(cctz::civil_second(2014, 12, 0, 20, 16, 18), tz));
    } else {
      benchmark::DoNotOptimize(
          cctz::convert(cctz::civil_second(2013, 11, 0, 18, 30, 27), tz));
    }
  }
}
BENCHMARK(BM_Time_FromCivilDay0_CCTZ);

void BM_Time_FromCivilDay0_Libc(benchmark::State& state) {
  // No timezone support, so just use localtime.
  int i = 0;
  while (state.KeepRunning()) {
    struct tm tm;
    if ((i++ & 1) == 0) {
      tm.tm_year = 2014 - 1900;
      tm.tm_mon = 12 - 1;
      tm.tm_mday = 0;
      tm.tm_hour = 20;
      tm.tm_min = 16;
      tm.tm_sec = 18;
    } else {
      tm.tm_year = 2013 - 1900;
      tm.tm_mon = 11 - 1;
      tm.tm_mday = 0;
      tm.tm_hour = 18;
      tm.tm_min = 30;
      tm.tm_sec = 27;
    }
    tm.tm_isdst = -1;
    benchmark::DoNotOptimize(mktime(&tm));
  }
}
BENCHMARK(BM_Time_FromCivilDay0_Libc);

const char* const kFormats[] = {
    RFC1123_full,           // 0
    RFC1123_no_wday,        // 1
    RFC3339_full,           // 2
    RFC3339_sec,            // 3
    "%Y-%m-%d%ET%H:%M:%S",  // 4
    "%Y-%m-%d",             // 5
    "%F%ET%T",              // 6
};
const int kNumFormats = sizeof(kFormats) / sizeof(kFormats[0]);

void BM_Format_FormatTime(benchmark::State& state) {
  const std::string fmt = kFormats[state.range(0)];
  state.SetLabel(fmt);
  const cctz::time_zone tz = TestTimeZone();
  const std::chrono::system_clock::time_point tp =
      cctz::convert(cctz::civil_second(1977, 6, 28, 9, 8, 7), tz) +
      std::chrono::microseconds(1);
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(cctz::format(fmt, tp, tz));
  }
}
BENCHMARK(BM_Format_FormatTime)->DenseRange(0, kNumFormats - 1);

void BM_Format_ParseTime(benchmark::State& state) {
  const std::string fmt = kFormats[state.range(0)];
  state.SetLabel(fmt);
  const cctz::time_zone tz = TestTimeZone();
  std::chrono::system_clock::time_point tp =
      cctz::convert(cctz::civil_second(1977, 6, 28, 9, 8, 7), tz) +
      std::chrono::microseconds(1);
  const std::string when = cctz::format(fmt, tp, tz);
  while (state.KeepRunning()) {
    benchmark::DoNotOptimize(cctz::parse(fmt, when, tz, &tp));
  }
}
BENCHMARK(BM_Format_ParseTime)->DenseRange(0, kNumFormats - 1);

}  // namespace
