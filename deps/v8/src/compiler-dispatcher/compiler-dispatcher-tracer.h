// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_DISPATCHER_COMPILER_DISPATCHER_TRACER_H_
#define V8_COMPILER_DISPATCHER_COMPILER_DISPATCHER_TRACER_H_

#include <utility>

#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/base/ring-buffer.h"
#include "src/counters.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class Isolate;
class RuntimeCallStats;

#define COMPILER_DISPATCHER_TRACE_SCOPE_WITH_NUM(tracer, scope_id, num)      \
  CompilerDispatcherTracer::ScopeID tracer_scope_id(                         \
      CompilerDispatcherTracer::ScopeID::scope_id);                          \
  CompilerDispatcherTracer::Scope trace_scope(tracer, tracer_scope_id, num); \
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),                      \
               CompilerDispatcherTracer::Scope::Name(tracer_scope_id))

#define COMPILER_DISPATCHER_TRACE_SCOPE(tracer, scope_id) \
  COMPILER_DISPATCHER_TRACE_SCOPE_WITH_NUM(tracer, scope_id, 0)

class V8_EXPORT_PRIVATE CompilerDispatcherTracer {
 public:
  enum class ScopeID {
    kPrepareToParse,
    kParse,
    kFinalizeParsing,
    kAnalyze,
    kPrepareToCompile,
    kCompile,
    kFinalizeCompiling
  };

  class Scope {
   public:
    Scope(CompilerDispatcherTracer* tracer, ScopeID scope_id, size_t num = 0);
    ~Scope();

    static const char* Name(ScopeID scoped_id);

   private:
    CompilerDispatcherTracer* tracer_;
    ScopeID scope_id_;
    size_t num_;
    double start_time_;

    DISALLOW_COPY_AND_ASSIGN(Scope);
  };

  explicit CompilerDispatcherTracer(Isolate* isolate);
  ~CompilerDispatcherTracer();

  void RecordPrepareToParse(double duration_ms);
  void RecordParse(double duration_ms, size_t source_length);
  void RecordFinalizeParsing(double duration_ms);
  void RecordAnalyze(double duration_ms);
  void RecordPrepareToCompile(double duration_ms);
  void RecordCompile(double duration_ms, size_t ast_size_in_bytes);
  void RecordFinalizeCompiling(double duration_ms);

  double EstimatePrepareToParseInMs() const;
  double EstimateParseInMs(size_t source_length) const;
  double EstimateFinalizeParsingInMs() const;
  double EstimateAnalyzeInMs() const;
  double EstimatePrepareToCompileInMs() const;
  double EstimateCompileInMs(size_t ast_size_in_bytes) const;
  double EstimateFinalizeCompilingInMs() const;

  void DumpStatistics() const;

 private:
  static double Average(const base::RingBuffer<double>& buffer);
  static double Estimate(
      const base::RingBuffer<std::pair<size_t, double>>& buffer, size_t num);

  mutable base::Mutex mutex_;
  base::RingBuffer<double> prepare_parse_events_;
  base::RingBuffer<std::pair<size_t, double>> parse_events_;
  base::RingBuffer<double> finalize_parsing_events_;
  base::RingBuffer<double> analyze_events_;
  base::RingBuffer<double> prepare_compile_events_;
  base::RingBuffer<std::pair<size_t, double>> compile_events_;
  base::RingBuffer<double> finalize_compiling_events_;

  RuntimeCallStats* runtime_call_stats_;

  DISALLOW_COPY_AND_ASSIGN(CompilerDispatcherTracer);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_DISPATCHER_COMPILER_DISPATCHER_TRACER_H_
