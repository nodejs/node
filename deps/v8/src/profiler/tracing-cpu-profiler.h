// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_TRACING_CPU_PROFILER_H
#define V8_PROFILER_TRACING_CPU_PROFILER_H

#include "include/v8-profiler.h"
#include "src/base/macros.h"

namespace v8 {
namespace internal {

class TracingCpuProfilerImpl final : public TracingCpuProfiler {
 public:
  explicit TracingCpuProfilerImpl(Isolate*);
  ~TracingCpuProfilerImpl();

 private:
  DISALLOW_COPY_AND_ASSIGN(TracingCpuProfilerImpl);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_TRACING_CPU_PROFILER_H
