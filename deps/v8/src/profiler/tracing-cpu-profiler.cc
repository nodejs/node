// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/tracing-cpu-profiler.h"

#include "src/profiler/cpu-profiler.h"
#include "src/tracing/trace-event.h"
#include "src/v8.h"

#define PROFILER_TRACE_CATEGORY_ENABLED(cat) \
  (*TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT(cat)))

namespace v8 {

std::unique_ptr<TracingCpuProfiler> TracingCpuProfiler::Create(
    v8::Isolate* isolate) {
  return std::unique_ptr<TracingCpuProfiler>(
      new internal::TracingCpuProfilerImpl(
          reinterpret_cast<internal::Isolate*>(isolate)));
}

namespace internal {

TracingCpuProfilerImpl::TracingCpuProfilerImpl(Isolate* isolate)
    : isolate_(isolate), profiling_enabled_(false) {
  // Make sure tracing system notices profiler categories.
  PROFILER_TRACE_CATEGORY_ENABLED("v8.cpu_profiler");
  PROFILER_TRACE_CATEGORY_ENABLED("v8.cpu_profiler.hires");
  V8::GetCurrentPlatform()->AddTraceStateObserver(this);
}

TracingCpuProfilerImpl::~TracingCpuProfilerImpl() {
  StopProfiling();
  V8::GetCurrentPlatform()->RemoveTraceStateObserver(this);
}

void TracingCpuProfilerImpl::OnTraceEnabled() {
  if (!PROFILER_TRACE_CATEGORY_ENABLED("v8.cpu_profiler")) return;
  profiling_enabled_ = true;
  isolate_->RequestInterrupt(
      [](v8::Isolate*, void* data) {
        reinterpret_cast<TracingCpuProfilerImpl*>(data)->StartProfiling();
      },
      this);
}

void TracingCpuProfilerImpl::OnTraceDisabled() {
  base::LockGuard<base::Mutex> lock(&mutex_);
  if (!profiling_enabled_) return;
  profiling_enabled_ = false;
  isolate_->RequestInterrupt(
      [](v8::Isolate*, void* data) {
        reinterpret_cast<TracingCpuProfilerImpl*>(data)->StopProfiling();
      },
      this);
}

void TracingCpuProfilerImpl::StartProfiling() {
  base::LockGuard<base::Mutex> lock(&mutex_);
  if (!profiling_enabled_ || profiler_) return;
  int sampling_interval_us =
      PROFILER_TRACE_CATEGORY_ENABLED("v8.cpu_profiler.hires") ? 100 : 1000;
  profiler_.reset(new CpuProfiler(isolate_));
  profiler_->set_sampling_interval(
      base::TimeDelta::FromMicroseconds(sampling_interval_us));
  profiler_->StartProfiling("", true);
}

void TracingCpuProfilerImpl::StopProfiling() {
  base::LockGuard<base::Mutex> lock(&mutex_);
  if (!profiler_) return;
  profiler_->StopProfiling("");
  profiler_.reset();
}

}  // namespace internal
}  // namespace v8
