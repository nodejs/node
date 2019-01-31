// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/tracing-cpu-profiler.h"

#include "src/profiler/cpu-profiler.h"
#include "src/tracing/trace-event.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

TracingCpuProfilerImpl::TracingCpuProfilerImpl(Isolate* isolate)
    : isolate_(isolate), profiling_enabled_(false) {
  V8::GetCurrentPlatform()->GetTracingController()->AddTraceStateObserver(this);
}

TracingCpuProfilerImpl::~TracingCpuProfilerImpl() {
  StopProfiling();
  V8::GetCurrentPlatform()->GetTracingController()->RemoveTraceStateObserver(
      this);
}

void TracingCpuProfilerImpl::OnTraceEnabled() {
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

void TracingCpuProfilerImpl::OnTraceDisabled() {
  base::MutexGuard lock(&mutex_);
  if (!profiling_enabled_) return;
  profiling_enabled_ = false;
  isolate_->RequestInterrupt(
      [](v8::Isolate*, void* data) {
        reinterpret_cast<TracingCpuProfilerImpl*>(data)->StopProfiling();
      },
      this);
}

void TracingCpuProfilerImpl::StartProfiling() {
  base::MutexGuard lock(&mutex_);
  if (!profiling_enabled_ || profiler_) return;
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("v8.cpu_profiler.hires"), &enabled);
  int sampling_interval_us = enabled ? 100 : 1000;
  profiler_.reset(new CpuProfiler(isolate_));
  profiler_->set_sampling_interval(
      base::TimeDelta::FromMicroseconds(sampling_interval_us));
  profiler_->StartProfiling("", true);
}

void TracingCpuProfilerImpl::StopProfiling() {
  base::MutexGuard lock(&mutex_);
  if (!profiler_) return;
  profiler_->StopProfiling("");
  profiler_.reset();
}

}  // namespace internal
}  // namespace v8
