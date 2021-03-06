// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_TIMEZONE_CACHE_H_
#define V8_BASE_TIMEZONE_CACHE_H_

namespace v8 {
namespace base {

class TimezoneCache {
 public:
  // Short name of the local timezone (e.g., EST)
  virtual const char* LocalTimezone(double time_ms) = 0;

  // ES #sec-daylight-saving-time-adjustment
  // Daylight Saving Time Adjustment
  virtual double DaylightSavingsOffset(double time_ms) = 0;

  // ES #sec-local-time-zone-adjustment
  // Local Time Zone Adjustment
  //
  // https://github.com/tc39/ecma262/pull/778
  virtual double LocalTimeOffset(double time_ms, bool is_utc) = 0;

  /**
   * Time zone redetection indicator for Clear function.
   *
   * kSkip indicates host time zone doesn't have to be redetected.
   * kRedetect indicates host time zone should be redetected, and used to set
   * the default time zone.
   *
   * The host time zone detection may require file system access or similar
   * operations unlikely to be available inside a sandbox. If v8 is run inside a
   * sandbox, the host time zone has to be detected outside the sandbox
   * separately.
   */
  enum class TimeZoneDetection { kSkip, kRedetect };

  // Called when the local timezone changes
  virtual void Clear(TimeZoneDetection time_zone_detection) = 0;

  // Called when tearing down the isolate
  virtual ~TimezoneCache() = default;
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_TIMEZONE_CACHE_H_
