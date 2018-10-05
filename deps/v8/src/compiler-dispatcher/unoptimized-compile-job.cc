// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler-dispatcher/unoptimized-compile-job.h"

#include "src/assert-scope.h"
#include "src/compiler-dispatcher/compiler-dispatcher-tracer.h"
#include "src/compiler.h"
#include "src/flags.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parser.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/unicode-cache.h"
#include "src/unoptimized-compilation-info.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

UnoptimizedCompileJob::UnoptimizedCompileJob(
    CompilerDispatcherTracer* tracer, AccountingAllocator* allocator,
    const ParseInfo* outer_parse_info, const AstRawString* function_name,
    const FunctionLiteral* function_literal,
    WorkerThreadRuntimeCallStats* worker_thread_runtime_stats,
    TimedHistogram* timer, size_t max_stack_size)
    : CompilerDispatcherJob(Type::kUnoptimizedCompile),
      tracer_(tracer),
      task_(new BackgroundCompileTask(allocator, outer_parse_info,
                                      function_name, function_literal,
                                      worker_thread_runtime_stats, timer,
                                      static_cast<int>(max_stack_size))) {}

UnoptimizedCompileJob::~UnoptimizedCompileJob() {
  DCHECK(status() == Status::kInitial || status() == Status::kDone);
}

void UnoptimizedCompileJob::Compile(bool on_background_thread) {
  DCHECK_EQ(status(), Status::kInitial);
  COMPILER_DISPATCHER_TRACE_SCOPE_WITH_NUM(
      tracer_, kCompile,
      task_->info()->end_position() - task_->info()->start_position());
  task_->Run();
  set_status(Status::kReadyToFinalize);
}

void UnoptimizedCompileJob::FinalizeOnMainThread(
    Isolate* isolate, Handle<SharedFunctionInfo> shared) {
  DCHECK_EQ(ThreadId::Current().ToInteger(), isolate->thread_id().ToInteger());
  DCHECK_EQ(status(), Status::kReadyToFinalize);
  COMPILER_DISPATCHER_TRACE_SCOPE(tracer_, kFinalize);

  bool succeeded = Compiler::FinalizeBackgroundCompileTask(
      task_.get(), shared, isolate, Compiler::KEEP_EXCEPTION);

  ResetDataOnMainThread(isolate);
  set_status(succeeded ? Status::kDone : Status::kFailed);
}

void UnoptimizedCompileJob::ResetDataOnMainThread(Isolate* isolate) {
  DCHECK_EQ(ThreadId::Current().ToInteger(), isolate->thread_id().ToInteger());
  task_.reset();
}

void UnoptimizedCompileJob::ResetOnMainThread(Isolate* isolate) {
  ResetDataOnMainThread(isolate);
  set_status(Status::kInitial);
}

double UnoptimizedCompileJob::EstimateRuntimeOfNextStepInMs() const {
  switch (status()) {
    case Status::kInitial:
      return tracer_->EstimateCompileInMs(task_->info()->end_position() -
                                          task_->info()->start_position());
    case Status::kReadyToFinalize:
      // TODO(rmcilroy): Pass size of bytecode to tracer to get better estimate.
      return tracer_->EstimateFinalizeInMs();

    case Status::kFailed:
    case Status::kDone:
      return 0.0;
  }

  UNREACHABLE();
}

}  // namespace internal
}  // namespace v8
