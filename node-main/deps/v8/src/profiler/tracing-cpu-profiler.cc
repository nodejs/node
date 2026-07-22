// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/tracing-cpu-profiler.h"

#include "src/execution/isolate.h"
#include "src/init/v8.h"
#include "src/profiler/cpu-profiler.h"
#include "src/tracing/trace-event.h"

namespace v8 {
namespace internal {

TracingCpuProfilerImpl::TracingCpuProfilerImpl(Isolate* isolate)
    : isolate_(isolate), profiling_enabled_(false) {
#if defined(V8_USE_PERFETTO)
  TrackEvent::AddSessionObserver(this);
  // Fire the observer if tracing is already in progress.
  if (TrackEvent::IsEnabled()) OnStart({});
#else
  V8::GetCurrentPlatform()->GetTracingController()->AddTraceStateObserver(this);
#endif
}

TracingCpuProfilerImpl::~TracingCpuProfilerImpl() {
  StopProfiling();
#if defined(V8_USE_PERFETTO)
  TrackEvent::RemoveSessionObserver(this);
#else
  V8::GetCurrentPlatform()->GetTracingController()->RemoveTraceStateObserver(
      this);
#endif
}

#if defined(V8_USE_PERFETTO)
void TracingCpuProfilerImpl::OnStart(
    const perfetto::DataSourceBase::StartArgs&) {
#else
void TracingCpuProfilerImpl::OnTraceEnabled() {
#endif
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("v8.cpu_profiler"), &enabled);
  if (!enabled) return;
  profiling_enabled_ = true;
  isolate_->RequestInterrupt(
      [](v8::Isolate*, void* data) {
        reinterpret_cast<TracingCpuProfilerImpl*>(data)->StartProfiling();
      },
      this);
}

namespace {
class RunInterruptsTask : public v8::Task {
 public:
  explicit RunInterruptsTask(v8::internal::Isolate* isolate)
      : isolate_(isolate) {}
  void Run() override { isolate_->stack_guard()->HandleInterrupts(); }

 private:
  v8::internal::Isolate* isolate_;
};
}  // namespace

#if defined(V8_USE_PERFETTO)
void TracingCpuProfilerImpl::OnStop(const perfetto::DataSourceBase::StopArgs&) {
#else
void TracingCpuProfilerImpl::OnTraceDisabled() {
#endif
  base::MutexGuard lock(&mutex_);
  if (!profiling_enabled_) return;
  profiling_enabled_ = false;
  isolate_->RequestInterrupt(
      [](v8::Isolate*, void* data) {
        reinterpret_cast<TracingCpuProfilerImpl*>(data)->StopProfiling();
      },
      this);
  // It could be a long time until the Isolate next runs any JS which could be
  // interrupted, and we'd rather not leave the sampler thread running during
  // that time, so also post a task to run any interrupts.
  V8::GetCurrentPlatform()
      ->GetForegroundTaskRunner(reinterpret_cast<v8::Isolate*>(isolate_))
      ->PostTask(std::make_unique<RunInterruptsTask>(isolate_));
}

void TracingCpuProfilerImpl::StartProfiling() {
  base::MutexGuard lock(&mutex_);
  if (!profiling_enabled_ || profiler_) return;
  int sampling_interval_us = 100;
  profiler_.reset(new CpuProfiler(isolate_, kDebugNaming));
  profiler_->set_sampling_interval(
      base::TimeDelta::FromMicroseconds(sampling_interval_us));
  profiler_->StartProfiling("", {kLeafNodeLineNumbers});
}

void TracingCpuProfilerImpl::StopProfiling() {
  base::MutexGuard lock(&mutex_);
  if (!profiler_) return;
  profiler_->StopProfiling("");
  profiler_.reset();
}

}  // namespace internal
}  // namespace v8
