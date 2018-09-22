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

namespace {

// A scope object that ensures a parse info's runtime call stats field is set
// correctly during worker-thread compile, and restores it after going out of
// scope.
class OffThreadRuntimeCallStatsScope {
 public:
  OffThreadRuntimeCallStatsScope(
      ParseInfo* parse_info,
      WorkerThreadRuntimeCallStats* worker_thread_runtime_stats)
      : parse_info_(parse_info),
        original_runtime_call_stats_(parse_info_->runtime_call_stats()),
        worker_thread_scope_(worker_thread_runtime_stats) {
    parse_info_->set_runtime_call_stats(worker_thread_scope_.Get());
  }

  ~OffThreadRuntimeCallStatsScope() {
    parse_info_->set_runtime_call_stats(original_runtime_call_stats_);
  }

 private:
  ParseInfo* parse_info_;
  RuntimeCallStats* original_runtime_call_stats_;
  WorkerThreadRuntimeCallStatsScope worker_thread_scope_;
};

}  // namespace

UnoptimizedCompileJob::UnoptimizedCompileJob(
    CompilerDispatcherTracer* tracer, AccountingAllocator* allocator,
    const ParseInfo* outer_parse_info, const AstRawString* function_name,
    const FunctionLiteral* function_literal,
    WorkerThreadRuntimeCallStats* worker_thread_runtime_stats,
    size_t max_stack_size)
    : CompilerDispatcherJob(Type::kUnoptimizedCompile),
      tracer_(tracer),
      allocator_(allocator),
      worker_thread_runtime_stats_(worker_thread_runtime_stats),
      max_stack_size_(max_stack_size),
      trace_compiler_dispatcher_jobs_(FLAG_trace_compiler_dispatcher_jobs) {
  DCHECK(outer_parse_info->is_toplevel());
  DCHECK(!function_literal->is_toplevel());

  // Initialize parse_info for the given function details.
  parse_info_ = ParseInfo::FromParent(outer_parse_info, allocator_,
                                      function_literal, function_name);

  // Clone the character stream so both can be accessed independently.
  std::unique_ptr<Utf16CharacterStream> character_stream =
      outer_parse_info->character_stream()->Clone();
  character_stream->Seek(function_literal->start_position());
  parse_info_->set_character_stream(std::move(character_stream));

  // Get preparsed scope data from the function literal.
  if (function_literal->produced_preparsed_scope_data()) {
    DCHECK(FLAG_preparser_scope_analysis);
    ZonePreParsedScopeData* serialized_data =
        function_literal->produced_preparsed_scope_data()->Serialize(
            parse_info_->zone());
    parse_info_->set_consumed_preparsed_scope_data(
        ConsumedPreParsedScopeData::For(parse_info_->zone(), serialized_data));
  }

  // Create a unicode cache for the parse-info.
  // TODO(rmcilroy): Try to reuse an existing one.
  unicode_cache_.reset(new UnicodeCache());
  parse_info_->set_unicode_cache(unicode_cache_.get());

  parser_.reset(new Parser(parse_info_.get()));

  if (trace_compiler_dispatcher_jobs_) {
    PrintF(
        "UnoptimizedCompileJob[%p] created for function literal id %d in "
        "initial state.\n",
        static_cast<void*>(this), function_literal->function_literal_id());
  }
}

UnoptimizedCompileJob::~UnoptimizedCompileJob() {
  DCHECK(status() == Status::kInitial || status() == Status::kDone);
}

void UnoptimizedCompileJob::Compile(bool on_background_thread) {
  DCHECK_EQ(status(), Status::kInitial);
  COMPILER_DISPATCHER_TRACE_SCOPE_WITH_NUM(
      tracer_, kCompile,
      parse_info_->end_position() - parse_info_->start_position());
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.UnoptimizedCompileJob::Compile");
  if (trace_compiler_dispatcher_jobs_) {
    PrintF("UnoptimizedCompileJob[%p]: Compiling\n", static_cast<void*>(this));
  }
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  base::Optional<OffThreadRuntimeCallStatsScope> runtime_call_stats_scope;
  if (V8_UNLIKELY(FLAG_runtime_stats && on_background_thread)) {
    runtime_call_stats_scope.emplace(parse_info_.get(),
                                     worker_thread_runtime_stats_);
  }

  RuntimeCallTimerScope runtimeTimer(
      parse_info_->runtime_call_stats(),
      on_background_thread
          ? RuntimeCallCounterId::kCompileBackgroundUnoptimizedCompileJob
          : RuntimeCallCounterId::kCompileUnoptimizedCompileJob);

  parse_info_->set_on_background_thread(on_background_thread);

  uintptr_t stack_limit = GetCurrentStackPosition() - max_stack_size_ * KB;
  parse_info_->set_stack_limit(stack_limit);

  parser_.reset(new Parser(parse_info_.get()));
  parser_->set_stack_limit(stack_limit);

  // We only support compilation of functions with no outer scope info
  // therefore it is correct to use an empty scope chain.
  DCHECK(parse_info_->maybe_outer_scope_info().is_null());
  parser_->InitializeEmptyScopeChain(parse_info_.get());
  parser_->ParseOnBackground(parse_info_.get());

  if (parse_info_->literal() == nullptr) {
    // Parser sets error in pending error handler.
    set_status(Status::kReadyToFinalize);
    return;
  }

  if (!Compiler::Analyze(parse_info_.get())) {
    parse_info_->pending_error_handler()->set_stack_overflow();
    set_status(Status::kReadyToFinalize);
    return;
  }

  compilation_job_.reset(interpreter::Interpreter::NewCompilationJob(
      parse_info_.get(), parse_info_->literal(), allocator_, nullptr));

  if (!compilation_job_.get()) {
    parse_info_->pending_error_handler()->set_stack_overflow();
    set_status(Status::kReadyToFinalize);
    return;
  }

  if (compilation_job_->ExecuteJob() != CompilationJob::SUCCEEDED) {
    parse_info_->pending_error_handler()->set_stack_overflow();
    set_status(Status::kReadyToFinalize);
    return;
  }

  set_status(Status::kReadyToFinalize);
}

void UnoptimizedCompileJob::FinalizeOnMainThread(
    Isolate* isolate, Handle<SharedFunctionInfo> shared) {
  DCHECK_EQ(ThreadId::Current().ToInteger(), isolate->thread_id().ToInteger());
  DCHECK_EQ(status(), Status::kReadyToFinalize);
  COMPILER_DISPATCHER_TRACE_SCOPE(tracer_, kFinalize);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.UnoptimizedCompileJob::FinalizeOnMainThread");
  RuntimeCallTimerScope runtimeTimer(
      isolate, RuntimeCallCounterId::kCompileFinalizeUnoptimizedCompileJob);

  if (trace_compiler_dispatcher_jobs_) {
    PrintF("UnoptimizedCompileJob[%p]: Finalizing compiling\n",
           static_cast<void*>(this));
  }

  HandleScope scope(isolate);
  Handle<Script> script(Script::cast(shared->script()), isolate);
  parse_info_->set_script(script);

  parser_->UpdateStatistics(isolate, script);
  parser_->HandleSourceURLComments(isolate, script);

  if (!parse_info_->literal() || !compilation_job_.get()) {
    // Parse or compile failed on the main thread, report errors.
    parse_info_->pending_error_handler()->ReportErrors(
        isolate, script, parse_info_->ast_value_factory());
    ResetDataOnMainThread(isolate);
    set_status(Status::kFailed);
    return;
  }

  // Internalize ast values onto the heap.
  parse_info_->ast_value_factory()->Internalize(isolate);
  // Allocate scope infos for the literal.
  DeclarationScope::AllocateScopeInfos(parse_info_.get(), isolate);
  if (compilation_job_->state() == CompilationJob::State::kFailed ||
      !Compiler::FinalizeCompilationJob(compilation_job_.release(), shared,
                                        isolate)) {
    if (!isolate->has_pending_exception()) isolate->StackOverflow();
    ResetDataOnMainThread(isolate);
    set_status(Status::kFailed);
    return;
  }

  ResetDataOnMainThread(isolate);
  set_status(Status::kDone);
}

void UnoptimizedCompileJob::ResetDataOnMainThread(Isolate* isolate) {
  DCHECK_EQ(ThreadId::Current().ToInteger(), isolate->thread_id().ToInteger());
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.UnoptimizedCompileJob::ResetDataOnMainThread");
  compilation_job_.reset();
  parser_.reset();
  unicode_cache_.reset();
  parse_info_.reset();
}

void UnoptimizedCompileJob::ResetOnMainThread(Isolate* isolate) {
  if (trace_compiler_dispatcher_jobs_) {
    PrintF("UnoptimizedCompileJob[%p]: Resetting\n", static_cast<void*>(this));
  }

  ResetDataOnMainThread(isolate);
  set_status(Status::kInitial);
}

double UnoptimizedCompileJob::EstimateRuntimeOfNextStepInMs() const {
  switch (status()) {
    case Status::kInitial:
      return tracer_->EstimateCompileInMs(parse_info_->end_position() -
                                          parse_info_->start_position());
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
