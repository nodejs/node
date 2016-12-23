// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/profiler/tracing-cpu-profiler.h"

#include "src/v8.h"

namespace v8 {

std::unique_ptr<TracingCpuProfiler> TracingCpuProfiler::Create(
    v8::Isolate* isolate) {
  return std::unique_ptr<TracingCpuProfiler>(
      new internal::TracingCpuProfilerImpl(
          reinterpret_cast<internal::Isolate*>(isolate)));
}

namespace internal {

TracingCpuProfilerImpl::TracingCpuProfilerImpl(Isolate* isolate) {}

TracingCpuProfilerImpl::~TracingCpuProfilerImpl() {}

}  // namespace internal
}  // namespace v8
