// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRACING_TRACING_CATEGORY_OBSERVER_H_
#define V8_TRACING_TRACING_CATEGORY_OBSERVER_H_

#include "include/v8-platform.h"
#include "src/tracing/trace-event.h"

namespace v8 {
namespace tracing {

class TracingCategoryObserver
#if defined(V8_USE_PERFETTO)
    : public perfetto::TrackEventSessionObserver {
#else
    : public TracingController::TraceStateObserver {
#endif
 public:
  enum Mode {
    ENABLED_BY_NATIVE = 1 << 0,
    ENABLED_BY_TRACING = 1 << 1,
    ENABLED_BY_SAMPLING = 1 << 2,
  };

  static void SetUp();
  static void TearDown();

#if defined(V8_USE_PERFETTO)
  // perfetto::TrackEventSessionObserver
  void OnStart(const perfetto::DataSourceBase::StartArgs&) override;
  void OnStop(const perfetto::DataSourceBase::StopArgs&) override;
#else
  // v8::TracingController::TraceStateObserver
  void OnTraceEnabled() final;
  void OnTraceDisabled() final;
#endif

 private:
  static TracingCategoryObserver* instance_;
};

}  // namespace tracing
}  // namespace v8

#endif  // V8_TRACING_TRACING_CATEGORY_OBSERVER_H_
