// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_DISPATCHER_UNOPTIMIZED_COMPILE_JOB_H_
#define V8_COMPILER_DISPATCHER_UNOPTIMIZED_COMPILE_JOB_H_

#include <memory>

#include "include/v8.h"
#include "src/base/macros.h"
#include "src/compiler-dispatcher/compiler-dispatcher-job.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class AccountingAllocator;
class AstRawString;
class AstValueFactory;
class AstStringConstants;
class BackgroundCompileTask;
class CompilerDispatcherTracer;
class DeferredHandles;
class FunctionLiteral;
class Isolate;
class ParseInfo;
class Parser;
class SharedFunctionInfo;
class String;
class TimedHistogram;
class UnicodeCache;
class UnoptimizedCompilationJob;
class WorkerThreadRuntimeCallStats;

// TODO(rmcilroy): Remove this class entirely and just have CompilerDispatcher
// manage BackgroundCompileTasks.
class V8_EXPORT_PRIVATE UnoptimizedCompileJob : public CompilerDispatcherJob {
 public:
  // Creates a UnoptimizedCompileJob in the initial state.
  UnoptimizedCompileJob(
      CompilerDispatcherTracer* tracer, AccountingAllocator* allocator,
      const ParseInfo* outer_parse_info, const AstRawString* function_name,
      const FunctionLiteral* function_literal,
      WorkerThreadRuntimeCallStats* worker_thread_runtime_stats,
      TimedHistogram* timer, size_t max_stack_size);
  ~UnoptimizedCompileJob() override;

  // CompilerDispatcherJob implementation.
  void Compile(bool on_background_thread) override;
  void FinalizeOnMainThread(Isolate* isolate,
                            Handle<SharedFunctionInfo> shared) override;
  void ResetOnMainThread(Isolate* isolate) override;
  double EstimateRuntimeOfNextStepInMs() const override;

 private:
  friend class CompilerDispatcherTest;
  friend class UnoptimizedCompileJobTest;

  void ResetDataOnMainThread(Isolate* isolate);

  CompilerDispatcherTracer* tracer_;
  std::unique_ptr<BackgroundCompileTask> task_;

  DISALLOW_COPY_AND_ASSIGN(UnoptimizedCompileJob);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_DISPATCHER_UNOPTIMIZED_COMPILE_JOB_H_
