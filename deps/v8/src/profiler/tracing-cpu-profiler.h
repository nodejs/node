// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_TRACING_CPU_PROFILER_H_
#define V8_PROFILER_TRACING_CPU_PROFILER_H_

#include <memory>

#include "include/v8-platform.h"
#include "src/base/atomic-utils.h"
#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/tracing/trace-event.h"

namespace v8 {
namespace internal {

class CpuProfiler;
class Isolate;

class TracingCpuProfilerImpl final
#if defined(V8_USE_PERFETTO)
    : public perfetto::TrackEventSessionObserver {
#else
    : private v8::TracingController::TraceStateObserver {
#endif
 public:
  explicit TracingCpuProfilerImpl(Isolate*);
  ~TracingCpuProfilerImpl() override;
  TracingCpuProfilerImpl(const TracingCpuProfilerImpl&) = delete;
  TracingCpuProfilerImpl& operator=(const TracingCpuProfilerImpl&) = delete;

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
  void StartProfiling();
  void StopProfiling();

  Isolate* isolate_;
  std::unique_ptr<CpuProfiler> profiler_;
  bool profiling_enabled_;
  base::Mutex mutex_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_TRACING_CPU_PROFILER_H_
