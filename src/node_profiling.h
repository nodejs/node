#ifndef SRC_NODE_PROFILING_H_
#define SRC_NODE_PROFILING_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8-profiler.h"
#include "v8.h"

#include <cstdint>
#include <sstream>

namespace node {

struct HeapProfileOptions {
  uint64_t sample_interval = 512 * 1024;
  int stack_depth = 16;
  v8::HeapProfiler::SamplingFlags flags =
      v8::HeapProfiler::SamplingFlags::kSamplingNoFlags;
};

HeapProfileOptions ParseHeapProfileOptions(
    const v8::FunctionCallbackInfo<v8::Value>& args);

bool SerializeHeapProfile(v8::Isolate* isolate, std::ostringstream& out_stream);

struct CpuProfileOptions {
  int sampling_interval_us = 0;
  uint32_t max_samples = v8::CpuProfilingOptions::kNoSampleLimit;
};

CpuProfileOptions ParseCpuProfileOptions(
    const v8::FunctionCallbackInfo<v8::Value>& args);

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_PROFILING_H_
