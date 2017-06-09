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
    case ScopeID::kPrepareToParse:
      tracer_->RecordPrepareToParse(elapsed);
      break;
    case ScopeID::kParse:
      tracer_->RecordParse(elapsed, num_);
      break;
    case ScopeID::kFinalizeParsing:
      tracer_->RecordFinalizeParsing(elapsed);
      break;
    case ScopeID::kAnalyze:
      tracer_->RecordAnalyze(elapsed);
      break;
    case ScopeID::kPrepareToCompile:
      tracer_->RecordPrepareToCompile(elapsed);
      break;
    case ScopeID::kCompile:
      tracer_->RecordCompile(elapsed, num_);
      break;
    case ScopeID::kFinalizeCompiling:
      tracer_->RecordFinalizeCompiling(elapsed);
      break;
  }
}

// static
const char* CompilerDispatcherTracer::Scope::Name(ScopeID scope_id) {
  switch (scope_id) {
    case ScopeID::kPrepareToParse:
      return "V8.BackgroundCompile_PrepareToParse";
    case ScopeID::kParse:
      return "V8.BackgroundCompile_Parse";
    case ScopeID::kFinalizeParsing:
      return "V8.BackgroundCompile_FinalizeParsing";
    case ScopeID::kAnalyze:
      return "V8.BackgroundCompile_Analyze";
    case ScopeID::kPrepareToCompile:
      return "V8.BackgroundCompile_PrepareToCompile";
    case ScopeID::kCompile:
      return "V8.BackgroundCompile_Compile";
    case ScopeID::kFinalizeCompiling:
      return "V8.BackgroundCompile_FinalizeCompiling";
  }
  UNREACHABLE();
  return nullptr;
}

CompilerDispatcherTracer::CompilerDispatcherTracer(Isolate* isolate)
    : runtime_call_stats_(nullptr) {
  // isolate might be nullptr during unittests.
  if (isolate) {
    runtime_call_stats_ = isolate->counters()->runtime_call_stats();
  }
}

CompilerDispatcherTracer::~CompilerDispatcherTracer() {}

void CompilerDispatcherTracer::RecordPrepareToParse(double duration_ms) {
  base::LockGuard<base::Mutex> lock(&mutex_);
  prepare_parse_events_.Push(duration_ms);
}

void CompilerDispatcherTracer::RecordParse(double duration_ms,
                                           size_t source_length) {
  base::LockGuard<base::Mutex> lock(&mutex_);
  parse_events_.Push(std::make_pair(source_length, duration_ms));
}

void CompilerDispatcherTracer::RecordFinalizeParsing(double duration_ms) {
  base::LockGuard<base::Mutex> lock(&mutex_);
  finalize_parsing_events_.Push(duration_ms);
}

void CompilerDispatcherTracer::RecordAnalyze(double duration_ms) {
  base::LockGuard<base::Mutex> lock(&mutex_);
  analyze_events_.Push(duration_ms);
}

void CompilerDispatcherTracer::RecordPrepareToCompile(double duration_ms) {
  base::LockGuard<base::Mutex> lock(&mutex_);
  prepare_compile_events_.Push(duration_ms);
}

void CompilerDispatcherTracer::RecordCompile(double duration_ms,
                                             size_t ast_size_in_bytes) {
  base::LockGuard<base::Mutex> lock(&mutex_);
  compile_events_.Push(std::make_pair(ast_size_in_bytes, duration_ms));
}

void CompilerDispatcherTracer::RecordFinalizeCompiling(double duration_ms) {
  base::LockGuard<base::Mutex> lock(&mutex_);
  finalize_compiling_events_.Push(duration_ms);
}

double CompilerDispatcherTracer::EstimatePrepareToParseInMs() const {
  base::LockGuard<base::Mutex> lock(&mutex_);
  return Average(prepare_parse_events_);
}

double CompilerDispatcherTracer::EstimateParseInMs(size_t source_length) const {
  base::LockGuard<base::Mutex> lock(&mutex_);
  return Estimate(parse_events_, source_length);
}

double CompilerDispatcherTracer::EstimateFinalizeParsingInMs() const {
  base::LockGuard<base::Mutex> lock(&mutex_);
  return Average(finalize_parsing_events_);
}

double CompilerDispatcherTracer::EstimateAnalyzeInMs() const {
  base::LockGuard<base::Mutex> lock(&mutex_);
  return Average(analyze_events_);
}

double CompilerDispatcherTracer::EstimatePrepareToCompileInMs() const {
  base::LockGuard<base::Mutex> lock(&mutex_);
  return Average(prepare_compile_events_);
}

double CompilerDispatcherTracer::EstimateCompileInMs(
    size_t ast_size_in_bytes) const {
  base::LockGuard<base::Mutex> lock(&mutex_);
  return Estimate(compile_events_, ast_size_in_bytes);
}

double CompilerDispatcherTracer::EstimateFinalizeCompilingInMs() const {
  base::LockGuard<base::Mutex> lock(&mutex_);
  return Average(finalize_compiling_events_);
}

void CompilerDispatcherTracer::DumpStatistics() const {
  PrintF(
      "CompilerDispatcherTracer: "
      "prepare_parsing=%.2lfms parsing=%.2lfms/kb finalize_parsing=%.2lfms "
      "analyze=%.2lfms prepare_compiling=%.2lfms compiling=%.2lfms/kb "
      "finalize_compiling=%.2lfms\n",
      EstimatePrepareToParseInMs(), EstimateParseInMs(1 * KB),
      EstimateFinalizeParsingInMs(), EstimateAnalyzeInMs(),
      EstimatePrepareToCompileInMs(), EstimateCompileInMs(1 * KB),
      EstimateFinalizeCompilingInMs());
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
