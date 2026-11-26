// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRACING_TRACING_CATEGORY_OBSERVER_H_
#define V8_TRACING_TRACING_CATEGORY_OBSERVER_H_

namespace v8 {
namespace tracing {

// Keep the observer base class (perfetto::TrackEventSessionObserver /
// v8::TracingController::TraceStateObserver) in the .cc file so that headers
// including this file for Mode constants (such as runtime-call-stats.h) do not
// pull in trace-event.h and the full Perfetto SDK headers.
class TracingCategoryObserver {
 public:
  enum Mode {
    ENABLED_BY_NATIVE = 1 << 0,
    ENABLED_BY_TRACING = 1 << 1,
    ENABLED_BY_SAMPLING = 1 << 2,
  };

  static void SetUp();
  static void TearDown();
};

}  // namespace tracing
}  // namespace v8

#endif  // V8_TRACING_TRACING_CATEGORY_OBSERVER_H_
