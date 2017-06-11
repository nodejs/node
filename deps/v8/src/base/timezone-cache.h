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
  // TODO(littledan): Make more accurate with another parameter along the
  // lines of this spec change:
  // https://github.com/tc39/ecma262/pull/778
  virtual double LocalTimeOffset() = 0;

  // Called when the local timezone changes
  virtual void Clear() = 0;

  // Called when tearing down the isolate
  virtual ~TimezoneCache() {}
};

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_TIMEZONE_CACHE_H_
