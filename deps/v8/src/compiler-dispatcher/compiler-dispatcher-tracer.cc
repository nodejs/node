// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler-dispatcher/compiler-dispatcher-tracer.h"

#include "src/isolate.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

namespace {

double MonotonicallyIncreasingTimeInMs() {
  return V8::GetCurrentPlatform()->MonotonicallyIncreasingTime() *
         static_cast<double>(base::Time::kMillisecondsPerSecond);
}

const double kEstimatedRuntimeWithoutData = 1.0;

}  // namespace

CompilerDispatcherTracer::Scope::Scope(CompilerDispatcherTracer* tracer,
                                       ScopeID scope_id, size_t num)
    : tracer_(tracer), scope_id_(scope_id), num_(num) {
  start_time_ = MonotonicallyIncreasingTimeInMs();
}

CompilerDispatcherTracer::Scope::~Scope() {
  double elapsed = MonotonicallyIncreasingTimeInMs() - start_time_;
  switch (scope_id_) {
    case ScopeID::kPrepare:
      tracer_->RecordPrepare(elapsed);
      break;
    case ScopeID::kCompile:
      tracer_->RecordCompile(elapsed, num_);
      break;
    case ScopeID::kFinalize:
      tracer_->RecordFinalize(elapsed);
      break;
  }
}

// static
const char* CompilerDispatcherTracer::Scope::Name(ScopeID scope_id) {
  switch (scope_id) {
    case ScopeID::kPrepare:
      return "V8.BackgroundCompile_Prepare";
    case ScopeID::kCompile:
      return "V8.BackgroundCompile_Compile";
    case ScopeID::kFinalize:
      return "V8.BackgroundCompile_Finalize";
  }
  UNREACHABLE();
}

CompilerDispatcherTracer::CompilerDispatcherTracer(Isolate* isolate)
    : runtime_call_stats_(nullptr) {
  // isolate might be nullptr during unittests.
  if (isolate) {
    runtime_call_stats_ = isolate->counters()->runtime_call_stats();
  }
}

CompilerDispatcherTracer::~CompilerDispatcherTracer() = default;

void CompilerDispatcherTracer::RecordPrepare(double duration_ms) {
  base::LockGuard<base::Mutex> lock(&mutex_);
  prepare_events_.Push(duration_ms);
}

void CompilerDispatcherTracer::RecordCompile(double duration_ms,
                                             size_t source_length) {
  base::LockGuard<base::Mutex> lock(&mutex_);
  compile_events_.Push(std::make_pair(source_length, duration_ms));
}

void CompilerDispatcherTracer::RecordFinalize(double duration_ms) {
  base::LockGuard<base::Mutex> lock(&mutex_);
  finalize_events_.Push(duration_ms);
}

double CompilerDispatcherTracer::EstimatePrepareInMs() const {
  base::LockGuard<base::Mutex> lock(&mutex_);
  return Average(prepare_events_);
}

double CompilerDispatcherTracer::EstimateCompileInMs(
    size_t source_length) const {
  base::LockGuard<base::Mutex> lock(&mutex_);
  return Estimate(compile_events_, source_length);
}

double CompilerDispatcherTracer::EstimateFinalizeInMs() const {
  base::LockGuard<base::Mutex> lock(&mutex_);
  return Average(finalize_events_);
}

void CompilerDispatcherTracer::DumpStatistics() const {
  PrintF(
      "CompilerDispatcherTracer: "
      "prepare=%.2lfms compiling=%.2lfms/kb finalize=%.2lfms\n",
      EstimatePrepareInMs(), EstimateCompileInMs(1 * KB),
      EstimateFinalizeInMs());
}

double CompilerDispatcherTracer::Average(
    const base::RingBuffer<double>& buffer) {
  if (buffer.Count() == 0) return 0.0;
  double sum = buffer.Sum([](double a, double b) { return a + b; }, 0.0);
  return sum / buffer.Count();
}

double CompilerDispatcherTracer::Estimate(
    const base::RingBuffer<std::pair<size_t, double>>& buffer, size_t num) {
  if (buffer.Count() == 0) return kEstimatedRuntimeWithoutData;
  std::pair<size_t, double> sum = buffer.Sum(
      [](std::pair<size_t, double> a, std::pair<size_t, double> b) {
        return std::make_pair(a.first + b.first, a.second + b.second);
      },
      std::make_pair(0, 0.0));
  return num * (sum.second / sum.first);
}

}  // namespace internal
}  // namespace v8
