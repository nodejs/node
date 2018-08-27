// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_TRACING_CPU_PROFILER_H_
#define V8_PROFILER_TRACING_CPU_PROFILER_H_

#include "include/v8-platform.h"
#include "include/v8-profiler.h"
#include "src/base/atomic-utils.h"
#include "src/base/macros.h"
#include "src/base/platform/mutex.h"

namespace v8 {
namespace internal {

class CpuProfiler;
class Isolate;

class TracingCpuProfilerImpl final
    : public TracingCpuProfiler,
      private v8::TracingController::TraceStateObserver {
 public:
  explicit TracingCpuProfilerImpl(Isolate*);
  ~TracingCpuProfilerImpl();

  // v8::TracingController::TraceStateObserver
  void OnTraceEnabled() final;
  void OnTraceDisabled() final;

 private:
  void StartProfiling();
  void StopProfiling();

  Isolate* isolate_;
  std::unique_ptr<CpuProfiler> profiler_;
  bool profiling_enabled_;
  base::Mutex mutex_;

  DISALLOW_COPY_AND_ASSIGN(TracingCpuProfilerImpl);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_TRACING_CPU_PROFILER_H_
