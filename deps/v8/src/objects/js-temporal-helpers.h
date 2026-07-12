// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_TEMPORAL_HELPERS_H_
#define V8_OBJECTS_JS_TEMPORAL_HELPERS_H_

#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/managed.h"
#include "src/objects/objects.h"

// This file exists to hold helpers that are a part of Temporal but needed in
// non-Temporal code. Once V8_TEMPORAL_SUPPORT is removed, this can be merged
// with js-temporal-objects.h

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define AT __FILE__ ":" TOSTRING(__LINE__)

#ifdef DEBUG
#define TEMPORAL_DEBUG_INFO AT
#else
#define TEMPORAL_DEBUG_INFO ""
#endif  // DEBUG

#define NEW_TEMPORAL_TYPE_ERROR(str)       \
  NewTypeError(MessageTemplate::kTemporal, \
               isolate->factory()->NewStringFromStaticChars(str))
#define NEW_TEMPORAL_RANGE_ERROR(str)       \
  NewRangeError(MessageTemplate::kTemporal, \
                isolate->factory()->NewStringFromStaticChars(str))

#define NEW_TEMPORAL_RANGE_ERROR_WITH_ARG(str, arg) \
  NewRangeError(MessageTemplate::kTemporalWithArg,  \
                isolate->factory()->NewStringFromStaticChars(str), arg)

#define NEW_TEMPORAL_TYPE_ERROR_WITH_ARG(str, arg) \
  NewTypeError(MessageTemplate::kTemporalWithArg,  \
               isolate->factory()->NewStringFromStaticChars(str), arg)

namespace v8 {
namespace internal {
namespace temporal {

// For Intl.DurationFormat

// #sec-temporal-time-duration-records
struct TimeDurationRecord {
  double days;
  double hours;
  double minutes;
  double seconds;
  double milliseconds;
  double microseconds;
  double nanoseconds;

  // #sec-temporal-createtimedurationrecord
  static Maybe<TimeDurationRecord> Create(Isolate* isolate, double days,
                                          double hours, double minutes,
                                          double seconds, double milliseconds,
                                          double microseconds,
                                          double nanoseconds);
};

// #sec-temporal-duration-records
struct DurationRecord {
  double years;
  double months;
  double weeks;
  TimeDurationRecord time_duration;
  // #sec-temporal-createdurationrecord
  static Maybe<DurationRecord> Create(Isolate* isolate, double years,
                                      double months, double weeks, double days,
                                      double hours, double minutes,
                                      double seconds, double milliseconds,
                                      double microseconds, double nanoseconds);

  static int32_t Sign(const DurationRecord& dur);
};

// https://tc39.es/proposal-temporal/#sec-todurationrecord-deleted
Maybe<DurationRecord> ToDurationRecord(
    Isolate* isolate, DirectHandle<Object> temporal_duration_like_obj,
    const DurationRecord& input);

// #sec-temporal-isvalidduration
bool IsValidDuration(Isolate* isolate, const DurationRecord& dur);

}  // namespace temporal
}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_JS_TEMPORAL_HELPERS_H_
