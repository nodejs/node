// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler.h"

#include <algorithm>
#include <memory>

#include "src/asmjs/asm-js.h"
#include "src/assembler-inl.h"
#include "src/ast/ast-numbering.h"
#include "src/ast/prettyprinter.h"
#include "src/ast/scopes.h"
#include "src/base/optional.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/compilation-cache.h"
#include "src/compilation-info.h"
#include "src/compiler-dispatcher/compiler-dispatcher.h"
#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"
#include "src/compiler/pipeline.h"
#include "src/debug/debug.h"
#include "src/debug/liveedit.h"
#include "src/frames-inl.h"
#include "src/full-codegen/full-codegen.h"
#include "src/globals.h"
#include "src/heap/heap.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate-inl.h"
#include "src/log-inl.h"
#include "src/messages.h"
#include "src/objects/map.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parsing.h"
#include "src/parsing/rewriter.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/runtime-profiler.h"
#include "src/snapshot/code-serializer.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

// A wrapper around a ParseInfo that detaches the parser handles from the
// underlying DeferredHandleScope and stores them in info_ on destruction.
class ParseHandleScope final {
 public:
  explicit ParseHandleScope(ParseInfo* info, Isolate* isolate)
      : deferred_(isolate), info_(info) {}
  ~ParseHandleScope() { info_->set_deferred_handles(deferred_.Detach()); }

 private:
  DeferredHandleScope deferred_;
  ParseInfo* info_;
};

// A wrapper around a CompilationInfo that detaches the Handles from
// the underlying DeferredHandleScope and stores them in info_ on
// destruction.
class CompilationHandleScope final {
 public:
  explicit CompilationHandleScope(CompilationInfo* info)
      : deferred_(info->isolate()), info_(info) {}
  ~CompilationHandleScope() { info_->set_deferred_handles(deferred_.Detach()); }

 private:
  DeferredHandleScope deferred_;
  CompilationInfo* info_;
};

// Helper that times a scoped region and records the elapsed time.
struct ScopedTimer {
  explicit ScopedTimer(base::TimeDelta* location) : location_(location) {
    DCHECK(location_ != NULL);
    timer_.Start();
  }

  ~ScopedTimer() { *location_ += timer_.Elapsed(); }

  base::ElapsedTimer timer_;
  base::TimeDelta* location_;
};

// ----------------------------------------------------------------------------
// Implementation of CompilationJob

CompilationJob::CompilationJob(Isolate* isolate, CompilationInfo* info,
                               const char* compiler_name, State initial_state)
    : info_(info),
      isolate_thread_id_(isolate->thread_id()),
      compiler_name_(compiler_name),
      state_(initial_state),
      stack_limit_(isolate->stack_guard()->real_climit()),
      executed_on_background_thread_(false) {}

CompilationJob::Status CompilationJob::PrepareJob() {
  DCHECK(ThreadId::Current().Equals(info()->isolate()->thread_id()));
  DisallowJavascriptExecution no_js(isolate());

  if (FLAG_trace_opt && info()->IsOptimizing()) {
    OFStream os(stdout);
    os << "[compiling method " << Brief(*info()->closure()) << " using "
       << compiler_name_;
    if (info()->is_osr()) os << " OSR";
    os << "]" << std::endl;
  }

  // Delegate to the underlying implementation.
  DCHECK(state() == State::kReadyToPrepare);
  ScopedTimer t(&time_taken_to_prepare_);
  return UpdateState(PrepareJobImpl(), State::kReadyToExecute);
}

CompilationJob::Status CompilationJob::ExecuteJob() {
  base::Optional<DisallowHeapAllocation> no_allocation;
  base::Optional<DisallowHandleAllocation> no_handles;
  base::Optional<DisallowHandleDereference> no_deref;
  base::Optional<DisallowCodeDependencyChange> no_dependency_change;
  if (can_execute_on_background_thread()) {
    no_allocation.emplace();
    no_handles.emplace();
    no_deref.emplace();
    no_dependency_change.emplace();
    executed_on_background_thread_ =
        !ThreadId::Current().Equals(isolate_thread_id_);
  } else {
    DCHECK(ThreadId::Current().Equals(isolate_thread_id_));
  }

  // Delegate to the underlying implementation.
  DCHECK(state() == State::kReadyToExecute);
  ScopedTimer t(&time_taken_to_execute_);
  return UpdateState(ExecuteJobImpl(), State::kReadyToFinalize);
}

CompilationJob::Status CompilationJob::FinalizeJob() {
  DCHECK(ThreadId::Current().Equals(info()->isolate()->thread_id()));
  DisallowCodeDependencyChange no_dependency_change;
  DisallowJavascriptExecution no_js(isolate());
  DCHECK(!info()->dependencies()->HasAborted());

  // Delegate to the underlying implementation.
  DCHECK(state() == State::kReadyToFinalize);
  ScopedTimer t(&time_taken_to_finalize_);
  return UpdateState(FinalizeJobImpl(), State::kSucceeded);
}

CompilationJob::Status CompilationJob::RetryOptimization(BailoutReason reason) {
  DCHECK(info_->IsOptimizing());
  info_->RetryOptimization(reason);
  state_ = State::kFailed;
  return FAILED;
}

CompilationJob::Status CompilationJob::AbortOptimization(BailoutReason reason) {
  DCHECK(info_->IsOptimizing());
  info_->AbortOptimization(reason);
  state_ = State::kFailed;
  return FAILED;
}

void CompilationJob::RecordUnoptimizedCompilationStats() const {
  int code_size;
  if (info()->has_bytecode_array()) {
    code_size = info()->bytecode_array()->SizeIncludingMetadata();
  } else {
    code_size = info()->code()->SizeIncludingMetadata();
  }

  Counters* counters = isolate()->counters();
  // TODO(4280): Rename counters from "baseline" to "unoptimized" eventually.
  counters->total_baseline_code_size()->Increment(code_size);
  counters->total_baseline_compile_count()->Increment(1);

  // TODO(5203): Add timers for each phase of compilation.
}

void CompilationJob::RecordOptimizedCompilationStats() const {
  DCHECK(info()->IsOptimizing());
  Handle<JSFunction> function = info()->closure();
  if (!function->IsOptimized()) {
    // Concurrent recompilation and OSR may race.  Increment only once.
    int opt_count = function->shared()->opt_count();
    function->shared()->set_opt_count(opt_count + 1);
  }
  double ms_creategraph = time_taken_to_prepare_.InMillisecondsF();
  double ms_optimize = time_taken_to_execute_.InMillisecondsF();
  double ms_codegen = time_taken_to_finalize_.InMillisecondsF();
  if (FLAG_trace_opt) {
    PrintF("[optimizing ");
    function->ShortPrint();
    PrintF(" - took %0.3f, %0.3f, %0.3f ms]\n", ms_creategraph, ms_optimize,
           ms_codegen);
  }
  if (FLAG_trace_opt_stats) {
    static double compilation_time = 0.0;
    static int compiled_functions = 0;
    static int code_size = 0;

    compilation_time += (ms_creategraph + ms_optimize + ms_codegen);
    compiled_functions++;
    code_size += function->shared()->SourceSize();
    PrintF("Compiled: %d functions with %d byte source size in %fms.\n",
           compiled_functions, code_size, compilation_time);
  }
}

Isolate* CompilationJob::isolate() const { return info()->isolate(); }

// ----------------------------------------------------------------------------
// Local helper methods that make up the compilation pipeline.

namespace {

void RecordFunctionCompilation(CodeEventListener::LogEventsAndTags tag,
                               CompilationInfo* info) {
  // Log the code generation. If source information is available include
  // script name and line number. Check explicitly whether logging is
  // enabled as finding the line number is not free.
  if (info->isolate()->logger()->is_logging_code_events() ||
      info->isolate()->is_profiling()) {
    Handle<SharedFunctionInfo> shared = info->shared_info();
    Handle<Script> script = info->parse_info()->script();
    Handle<AbstractCode> abstract_code =
        info->has_bytecode_array()
            ? Handle<AbstractCode>::cast(info->bytecode_array())
            : Handle<AbstractCode>::cast(info->code());
    if (abstract_code.is_identical_to(
            info->isolate()->builtins()->CompileLazy())) {
      return;
    }
    int line_num = Script::GetLineNumber(script, shared->start_position()) + 1;
    int column_num =
        Script::GetColumnNumber(script, shared->start_position()) + 1;
    String* script_name = script->name()->IsString()
                              ? String::cast(script->name())
                              : info->isolate()->heap()->empty_string();
    CodeEventListener::LogEventsAndTags log_tag =
        Logger::ToNativeByScript(tag, *script);
    PROFILE(info->isolate(),
            CodeCreateEvent(log_tag, *abstract_code, *shared, script_name,
                            line_num, column_num));
  }
}

void EnsureFeedbackMetadata(CompilationInfo* info) {
  DCHECK(info->has_shared_info());

  // If no type feedback metadata exists, create it. At this point the
  // AstNumbering pass has already run. Note the snapshot can contain outdated
  // vectors for a different configuration, hence we also recreate a new vector
  // when the function is not compiled (i.e. no code was serialized).

  // TODO(mvstanton): reintroduce is_empty() predicate to feedback_metadata().
  if (info->shared_info()->feedback_metadata()->length() == 0 ||
      !info->shared_info()->is_compiled()) {
    Handle<FeedbackMetadata> feedback_metadata = FeedbackMetadata::New(
        info->isolate(), info->literal()->feedback_vector_spec());
    info->shared_info()->set_feedback_metadata(*feedback_metadata);
  }

  // It's very important that recompiles do not alter the structure of the type
  // feedback vector. Verify that the structure fits the function literal.
  CHECK(!info->shared_info()->feedback_metadata()->SpecDiffersFrom(
      info->literal()->feedback_vector_spec()));
}

bool ShouldUseFullCodegen(FunctionLiteral* literal) {
  // Code which can't be supported by the old pipeline should use Ignition.
  if (literal->must_use_ignition()) return false;

  // Resumable functions are not supported by {FullCodeGenerator}, suspended
  // activations stored as {JSGeneratorObject} on the heap always assume the
  // underlying code to be based on the bytecode array.
  DCHECK(!IsResumableFunction(literal->kind()));

  // Use full-codegen for asm.js functions.
  if (literal->scope()->asm_function()) return true;

  // If stressing full-codegen then use it for all functions it can support.
  return FLAG_stress_fullcodegen;
}

bool UseAsmWasm(DeclarationScope* scope, Handle<SharedFunctionInfo> shared_info,
                bool is_debug) {
  // Check whether asm.js validation is enabled.
  if (!FLAG_validate_asm) return false;

  // Modules that have validated successfully, but were subsequently broken by
  // invalid module instantiation attempts are off limit forever.
  if (!shared_info.is_null() && shared_info->is_asm_wasm_broken()) return false;

  // Compiling for debugging is not supported, fall back.
  if (is_debug) return false;

  // In stress mode we want to run the validator on everything.
  if (FLAG_stress_validate_asm) return true;

  // In general, we respect the "use asm" directive.
  return scope->asm_module();
}

bool UseCompilerDispatcher(ConcurrencyMode inner_function_mode,
                           CompilerDispatcher* dispatcher,
                           DeclarationScope* scope,
                           Handle<SharedFunctionInfo> shared_info,
                           bool is_debug, bool will_serialize) {
  return inner_function_mode == ConcurrencyMode::kConcurrent &&
         dispatcher->IsEnabled() && !is_debug && !will_serialize &&
         !UseAsmWasm(scope, shared_info, is_debug);
}

CompilationJob* GetUnoptimizedCompilationJob(CompilationInfo* info) {
  // Function should have been parsed and analyzed before creating a compilation
  // job.
  DCHECK_NOT_NULL(info->literal());
  DCHECK_NOT_NULL(info->scope());

  if (ShouldUseFullCodegen(info->literal())) {
    return FullCodeGenerator::NewCompilationJob(info);
  } else {
    return interpreter::Interpreter::NewCompilationJob(info);
  }
}

void InstallUnoptimizedCode(CompilationInfo* info) {
  Handle<SharedFunctionInfo> shared = info->shared_info();
  DCHECK_EQ(info->shared_info()->language_mode(),
            info->literal()->language_mode());

  // Ensure feedback metadata is installed.
  EnsureFeedbackMetadata(info);

  // Mark code to be executed once before being aged if necessary.
  // TODO(6409): Remove when full-codegen dies.
  DCHECK(!info->code().is_null());
  if (info->parse_info()->literal()->should_be_used_once_hint()) {
    info->code()->MarkToBeExecutedOnce(info->isolate());
  }

  // Update the shared function info with the scope info.
  Handle<ScopeInfo> scope_info = info->scope()->scope_info();
  shared->set_scope_info(*scope_info);
  Scope* outer_scope = info->scope()->GetOuterScopeWithContext();
  if (outer_scope) {
    shared->set_outer_scope_info(*outer_scope->scope_info());
  }

  // Install compilation result on the shared function info.
  // TODO(mstarzinger): Compiling for debug code might be used to reveal inner
  // functions via {FindSharedFunctionInfoInScript}, in which case we end up
  // regenerating existing bytecode. Fix this!
  if (info->is_debug() && info->has_bytecode_array()) {
    shared->ClearBytecodeArray();
  }
  DCHECK(!info->code().is_null());
  shared->ReplaceCode(*info->code());
  if (info->has_bytecode_array()) {
    DCHECK(!shared->HasBytecodeArray());  // Only compiled once.
    shared->set_bytecode_array(*info->bytecode_array());
  }

  // Install coverage info on the shared function info.
  if (info->has_coverage_info()) {
    DCHECK(info->is_block_coverage_enabled());
    info->isolate()->debug()->InstallCoverageInfo(info->shared_info(),
                                                  info->coverage_info());
  }
}

void EnsureSharedFunctionInfosArrayOnScript(CompilationInfo* info) {
  DCHECK(info->parse_info()->is_toplevel());
  DCHECK(!info->script().is_null());
  if (info->script()->shared_function_infos()->length() > 0) {
    DCHECK_EQ(info->script()->shared_function_infos()->length(),
              info->parse_info()->max_function_literal_id() + 1);
    return;
  }
  Handle<FixedArray> infos(info->isolate()->factory()->NewFixedArray(
      info->parse_info()->max_function_literal_id() + 1));
  info->script()->set_shared_function_infos(*infos);
}

void SetSharedFunctionFlagsFromLiteral(FunctionLiteral* literal,
                                       Handle<SharedFunctionInfo> shared_info) {
  // Don't overwrite values set by the bootstrapper.
  if (!shared_info->HasLength()) {
    shared_info->set_length(literal->function_length());
  }
  shared_info->set_ast_node_count(literal->ast_node_count());
  shared_info->set_has_duplicate_parameters(
      literal->has_duplicate_parameters());
  shared_info->SetExpectedNofPropertiesFromEstimate(literal);
  if (literal->dont_optimize_reason() != kNoReason) {
    shared_info->DisableOptimization(literal->dont_optimize_reason());
  }
}

CompilationJob::Status FinalizeUnoptimizedCompilationJob(CompilationJob* job) {
  CompilationInfo* info = job->info();
  ParseInfo* parse_info = info->parse_info();
  Isolate* isolate = info->isolate();

  if (parse_info->is_toplevel()) {
    // Allocate a shared function info and an array for shared function infos
    // for inner functions.
    EnsureSharedFunctionInfosArrayOnScript(info);
    DCHECK_EQ(kNoSourcePosition, info->literal()->function_token_position());
    if (!info->has_shared_info()) {
      Handle<SharedFunctionInfo> shared =
          isolate->factory()->NewSharedFunctionInfoForLiteral(info->literal(),
                                                              info->script());
      shared->set_is_toplevel(true);
      parse_info->set_shared_info(shared);
    }
  }
  SetSharedFunctionFlagsFromLiteral(info->literal(), info->shared_info());

  CompilationJob::Status status = job->FinalizeJob();
  if (status == CompilationJob::SUCCEEDED) {
    InstallUnoptimizedCode(info);
    CodeEventListener::LogEventsAndTags log_tags =
        parse_info->is_toplevel() ? parse_info->is_eval()
                                        ? CodeEventListener::EVAL_TAG
                                        : CodeEventListener::SCRIPT_TAG
                                  : CodeEventListener::FUNCTION_TAG;
    RecordFunctionCompilation(log_tags, info);
    job->RecordUnoptimizedCompilationStats();
  }
  return status;
}

bool Renumber(ParseInfo* parse_info,
              Compiler::EagerInnerFunctionLiterals* eager_literals) {
  RuntimeCallTimerScope runtimeTimer(parse_info->runtime_call_stats(),
                                     &RuntimeCallStats::CompileRenumber);

  // CollectTypeProfile uses its own feedback slots. If we have existing
  // FeedbackMetadata, we can only collect type profile, if the feedback vector
  // has the appropriate slots.
  bool collect_type_profile;
  if (parse_info->shared_info().is_null() ||
      parse_info->shared_info()->feedback_metadata()->length() == 0) {
    collect_type_profile =
        FLAG_type_profile && parse_info->script()->IsUserJavaScript();
  } else {
    collect_type_profile =
        parse_info->shared_info()->feedback_metadata()->HasTypeProfileSlot();
  }

  if (!AstNumbering::Renumber(parse_info->stack_limit(), parse_info->zone(),
                              parse_info->literal(), eager_literals,
                              collect_type_profile)) {
    return false;
  }
  return true;
}

bool GenerateUnoptimizedCode(CompilationInfo* info) {
  if (UseAsmWasm(info->scope(), info->shared_info(), info->is_debug())) {
    MaybeHandle<FixedArray> wasm_data;
    wasm_data = AsmJs::CompileAsmViaWasm(info);
    if (!wasm_data.is_null()) {
      SetSharedFunctionFlagsFromLiteral(info->literal(), info->shared_info());
      info->shared_info()->set_asm_wasm_data(*wasm_data.ToHandleChecked());
      info->SetCode(info->isolate()->builtins()->InstantiateAsmJs());
      InstallUnoptimizedCode(info);
      return true;
    }
  }

  std::unique_ptr<CompilationJob> job(GetUnoptimizedCompilationJob(info));
  if (job->PrepareJob() != CompilationJob::SUCCEEDED) return false;
  if (job->ExecuteJob() != CompilationJob::SUCCEEDED) return false;
  if (FinalizeUnoptimizedCompilationJob(job.get()) !=
      CompilationJob::SUCCEEDED) {
    return false;
  }
  return true;
}

bool CompileUnoptimizedInnerFunctions(
    Compiler::EagerInnerFunctionLiterals* literals,
    ConcurrencyMode inner_function_mode, std::shared_ptr<Zone> parse_zone,
    CompilationInfo* outer_info) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.CompileUnoptimizedInnerFunctions");
  Isolate* isolate = outer_info->isolate();
  Handle<Script> script = outer_info->script();
  bool is_debug = outer_info->is_debug();
  bool will_serialize = outer_info->will_serialize();
  RuntimeCallTimerScope runtimeTimer(isolate,
                                     &RuntimeCallStats::CompileInnerFunction);

  for (auto it : *literals) {
    FunctionLiteral* literal = it->value();
    Handle<SharedFunctionInfo> shared =
        Compiler::GetSharedFunctionInfo(literal, script, outer_info);
    if (shared->is_compiled()) continue;

    // Try to enqueue the eager function on the compiler dispatcher.
    CompilerDispatcher* dispatcher = isolate->compiler_dispatcher();
    if (UseCompilerDispatcher(inner_function_mode, dispatcher, literal->scope(),
                              shared, is_debug, will_serialize) &&
        dispatcher->EnqueueAndStep(outer_info->script(), shared, literal,
                                   parse_zone,
                                   outer_info->parse_info()->deferred_handles(),
                                   outer_info->deferred_handles())) {
      // If we have successfully queued up the function for compilation on the
      // compiler dispatcher then we are done.
      continue;
    } else {
      // Otherwise generate unoptimized code now.
      ParseInfo parse_info(script);
      CompilationInfo info(parse_info.zone(), &parse_info, isolate,
                           Handle<JSFunction>::null());
      parse_info.set_toplevel(false);
      parse_info.set_literal(literal);
      parse_info.set_shared_info(shared);
      parse_info.set_function_literal_id(shared->function_literal_id());
      parse_info.set_language_mode(literal->scope()->language_mode());
      parse_info.set_ast_value_factory(
          outer_info->parse_info()->ast_value_factory());
      parse_info.set_ast_value_factory_owned(false);
      parse_info.set_source_range_map(
          outer_info->parse_info()->source_range_map());

      if (will_serialize) info.PrepareForSerializing();
      if (is_debug) info.MarkAsDebug();

      if (!GenerateUnoptimizedCode(&info)) {
        if (!isolate->has_pending_exception()) isolate->StackOverflow();
        return false;
      }
    }
  }
  return true;
}

bool InnerFunctionShouldUseFullCodegen(
    ThreadedList<ThreadedListZoneEntry<FunctionLiteral*>>* literals) {
  for (auto it : *literals) {
    FunctionLiteral* literal = it->value();
    if (ShouldUseFullCodegen(literal)) return true;
  }
  return false;
}

bool CompileUnoptimizedCode(CompilationInfo* info,
                            ConcurrencyMode inner_function_mode) {
  Isolate* isolate = info->isolate();
  DCHECK(AllowCompilation::IsAllowed(isolate));

  Compiler::EagerInnerFunctionLiterals inner_literals;
  {
    base::Optional<CompilationHandleScope> compilation_handle_scope;
    if (inner_function_mode == ConcurrencyMode::kConcurrent) {
      compilation_handle_scope.emplace(info);
    }
    if (!Compiler::Analyze(info, &inner_literals)) {
      if (!isolate->has_pending_exception()) isolate->StackOverflow();
      return false;
    }
  }

  if (info->parse_info()->is_toplevel() &&
      (ShouldUseFullCodegen(info->literal()) ||
       InnerFunctionShouldUseFullCodegen(&inner_literals))) {
    // Full-codegen needs to access SFI when compiling, so allocate the array
    // now.
    EnsureSharedFunctionInfosArrayOnScript(info);
    inner_function_mode = ConcurrencyMode::kNotConcurrent;
  }

  std::shared_ptr<Zone> parse_zone;
  if (inner_function_mode == ConcurrencyMode::kConcurrent) {
    // Seal the parse zone so that it can be shared by parallel inner function
    // compilation jobs.
    DCHECK_NE(info->parse_info()->zone(), info->zone());
    parse_zone = info->parse_info()->zone_shared();
    parse_zone->Seal();
  }

  if (!GenerateUnoptimizedCode(info) ||
      !CompileUnoptimizedInnerFunctions(&inner_literals, inner_function_mode,
                                        parse_zone, info)) {
    if (!isolate->has_pending_exception()) isolate->StackOverflow();
    return false;
  }

  return true;
}

MUST_USE_RESULT MaybeHandle<Code> GetUnoptimizedCode(
    CompilationInfo* info, ConcurrencyMode inner_function_mode) {
  RuntimeCallTimerScope runtimeTimer(
      info->isolate(), &RuntimeCallStats::CompileGetUnoptimizedCode);
  VMState<COMPILER> state(info->isolate());
  PostponeInterruptsScope postpone(info->isolate());

  // Parse and update ParseInfo with the results.
  {
    if (!parsing::ParseAny(
            info->parse_info(), info->isolate(),
            inner_function_mode != ConcurrencyMode::kConcurrent)) {
      return MaybeHandle<Code>();
    }

    if (inner_function_mode == ConcurrencyMode::kConcurrent) {
      ParseHandleScope parse_handles(info->parse_info(), info->isolate());
      info->parse_info()->ReopenHandlesInNewHandleScope();
      info->parse_info()->ast_value_factory()->Internalize(info->isolate());
    }
  }

  // Compile either unoptimized code or bytecode for the interpreter.
  if (!CompileUnoptimizedCode(info, inner_function_mode)) {
    return MaybeHandle<Code>();
  }

  // Record the function compilation event.
  RecordFunctionCompilation(CodeEventListener::LAZY_COMPILE_TAG, info);

  return info->code();
}

MUST_USE_RESULT MaybeHandle<Code> GetCodeFromOptimizedCodeCache(
    Handle<JSFunction> function, BailoutId osr_ast_id) {
  RuntimeCallTimerScope runtimeTimer(
      function->GetIsolate(),
      &RuntimeCallStats::CompileGetFromOptimizedCodeMap);
  Handle<SharedFunctionInfo> shared(function->shared());
  DisallowHeapAllocation no_gc;
  if (osr_ast_id.IsNone()) {
    if (function->feedback_vector_cell()->value()->IsFeedbackVector()) {
      FeedbackVector* feedback_vector = function->feedback_vector();
      feedback_vector->EvictOptimizedCodeMarkedForDeoptimization(
          function->shared(), "GetCodeFromOptimizedCodeCache");
      Code* code = feedback_vector->optimized_code();

      if (code != nullptr) {
        // Caching of optimized code enabled and optimized code found.
        DCHECK(!code->marked_for_deoptimization());
        DCHECK(function->shared()->is_compiled());
        return Handle<Code>(code);
      }
    }
  }
  return MaybeHandle<Code>();
}

void ClearOptimizedCodeCache(CompilationInfo* info) {
  Handle<JSFunction> function = info->closure();
  if (info->osr_ast_id().IsNone()) {
    Handle<FeedbackVector> vector =
        handle(function->feedback_vector(), function->GetIsolate());
    vector->ClearOptimizedCode();
  }
}

void InsertCodeIntoOptimizedCodeCache(CompilationInfo* info) {
  Handle<Code> code = info->code();
  if (code->kind() != Code::OPTIMIZED_FUNCTION) return;  // Nothing to do.

  // Function context specialization folds-in the function context,
  // so no sharing can occur.
  if (info->is_function_context_specializing()) {
    // Native context specialized code is not shared, so make sure the optimized
    // code cache is clear.
    ClearOptimizedCodeCache(info);
    return;
  }
  // Frame specialization implies function context specialization.
  DCHECK(!info->is_frame_specializing());

  // Cache optimized context-specific code.
  Handle<JSFunction> function = info->closure();
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<Context> native_context(function->context()->native_context());
  if (info->osr_ast_id().IsNone()) {
    Handle<FeedbackVector> vector =
        handle(function->feedback_vector(), function->GetIsolate());
    FeedbackVector::SetOptimizedCode(vector, code);
  }
}

bool GetOptimizedCodeNow(CompilationJob* job) {
  CompilationInfo* info = job->info();
  Isolate* isolate = info->isolate();

  // Parsing is not required when optimizing from existing bytecode.
  if (!info->is_optimizing_from_bytecode()) {
    if (!Compiler::ParseAndAnalyze(info)) return false;
    EnsureFeedbackMetadata(info);
  }

  JSFunction::EnsureLiterals(info->closure());

  TimerEventScope<TimerEventRecompileSynchronous> timer(isolate);
  RuntimeCallTimerScope runtimeTimer(isolate,
                                     &RuntimeCallStats::RecompileSynchronous);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.RecompileSynchronous");

  if (job->PrepareJob() != CompilationJob::SUCCEEDED ||
      job->ExecuteJob() != CompilationJob::SUCCEEDED ||
      job->FinalizeJob() != CompilationJob::SUCCEEDED) {
    if (FLAG_trace_opt) {
      PrintF("[aborted optimizing ");
      info->closure()->ShortPrint();
      PrintF(" because: %s]\n", GetBailoutReason(info->bailout_reason()));
    }
    return false;
  }

  // Success!
  job->RecordOptimizedCompilationStats();
  DCHECK(!isolate->has_pending_exception());
  InsertCodeIntoOptimizedCodeCache(info);
  RecordFunctionCompilation(CodeEventListener::LAZY_COMPILE_TAG, info);
  return true;
}

bool GetOptimizedCodeLater(CompilationJob* job) {
  CompilationInfo* info = job->info();
  Isolate* isolate = info->isolate();

  if (!isolate->optimizing_compile_dispatcher()->IsQueueAvailable()) {
    if (FLAG_trace_concurrent_recompilation) {
      PrintF("  ** Compilation queue full, will retry optimizing ");
      info->closure()->ShortPrint();
      PrintF(" later.\n");
    }
    return false;
  }

  if (isolate->heap()->HighMemoryPressure()) {
    if (FLAG_trace_concurrent_recompilation) {
      PrintF("  ** High memory pressure, will retry optimizing ");
      info->closure()->ShortPrint();
      PrintF(" later.\n");
    }
    return false;
  }

  // Parsing is not required when optimizing from existing bytecode.
  if (!info->is_optimizing_from_bytecode()) {
    if (!Compiler::ParseAndAnalyze(info)) return false;
    EnsureFeedbackMetadata(info);
  }

  JSFunction::EnsureLiterals(info->closure());

  TimerEventScope<TimerEventRecompileSynchronous> timer(info->isolate());
  RuntimeCallTimerScope runtimeTimer(info->isolate(),
                                     &RuntimeCallStats::RecompileSynchronous);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.RecompileSynchronous");

  if (job->PrepareJob() != CompilationJob::SUCCEEDED) return false;
  isolate->optimizing_compile_dispatcher()->QueueForOptimization(job);

  if (FLAG_trace_concurrent_recompilation) {
    PrintF("  ** Queued ");
    info->closure()->ShortPrint();
    PrintF(" for concurrent optimization.\n");
  }
  return true;
}

MaybeHandle<Code> GetOptimizedCode(Handle<JSFunction> function,
                                   ConcurrencyMode mode,
                                   BailoutId osr_ast_id = BailoutId::None(),
                                   JavaScriptFrame* osr_frame = nullptr) {
  Isolate* isolate = function->GetIsolate();
  Handle<SharedFunctionInfo> shared(function->shared(), isolate);

  bool ignition_osr = osr_frame && osr_frame->is_interpreted();
  USE(ignition_osr);
  DCHECK_IMPLIES(ignition_osr, !osr_ast_id.IsNone());
  DCHECK_IMPLIES(ignition_osr, FLAG_ignition_osr);

  // Make sure we clear the optimization marker on the function so that we
  // don't try to re-optimize.
  if (function->HasOptimizationMarker()) {
    function->ClearOptimizationMarker();
  }

  Handle<Code> cached_code;
  if (GetCodeFromOptimizedCodeCache(function, osr_ast_id)
          .ToHandle(&cached_code)) {
    if (FLAG_trace_opt) {
      PrintF("[found optimized code for ");
      function->ShortPrint();
      if (!osr_ast_id.IsNone()) {
        PrintF(" at OSR AST id %d", osr_ast_id.ToInt());
      }
      PrintF("]\n");
    }
    return cached_code;
  }

  // Reset profiler ticks, function is no longer considered hot.
  DCHECK(shared->is_compiled());
  shared->set_profiler_ticks(0);

  VMState<COMPILER> state(isolate);
  DCHECK(!isolate->has_pending_exception());
  PostponeInterruptsScope postpone(isolate);
  bool has_script = shared->script()->IsScript();
  // BUG(5946): This DCHECK is necessary to make certain that we won't
  // tolerate the lack of a script without bytecode.
  DCHECK_IMPLIES(!has_script, shared->HasBytecodeArray());
  std::unique_ptr<CompilationJob> job(
      compiler::Pipeline::NewCompilationJob(function, has_script));
  CompilationInfo* info = job->info();
  ParseInfo* parse_info = info->parse_info();

  info->SetOptimizingForOsr(osr_ast_id, osr_frame);

  // Do not use TurboFan if we need to be able to set break points.
  if (info->shared_info()->HasBreakInfo()) {
    info->AbortOptimization(kFunctionBeingDebugged);
    return MaybeHandle<Code>();
  }

  // Do not use TurboFan when %NeverOptimizeFunction was applied.
  if (shared->optimization_disabled() &&
      shared->disable_optimization_reason() == kOptimizationDisabledForTest) {
    info->AbortOptimization(kOptimizationDisabledForTest);
    return MaybeHandle<Code>();
  }

  // Do not use TurboFan if optimization is disabled or function doesn't pass
  // turbo_filter.
  if (!FLAG_opt || !shared->PassesFilter(FLAG_turbo_filter)) {
    info->AbortOptimization(kOptimizationDisabled);
    return MaybeHandle<Code>();
  }

  // Limit the number of times we try to optimize functions.
  const int kMaxDeoptCount =
      FLAG_deopt_every_n_times == 0 ? FLAG_max_deopt_count : 1000;
  if (info->shared_info()->deopt_count() > kMaxDeoptCount) {
    info->AbortOptimization(kDeoptimizedTooManyTimes);
    return MaybeHandle<Code>();
  }

  TimerEventScope<TimerEventOptimizeCode> optimize_code_timer(isolate);
  RuntimeCallTimerScope runtimeTimer(isolate, &RuntimeCallStats::OptimizeCode);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.OptimizeCode");

  // TurboFan can optimize directly from existing bytecode.
  if (shared->HasBytecodeArray()) {
    info->MarkAsOptimizeFromBytecode();
  }

  // Verify that OSR compilations are delegated to the correct graph builder.
  // Depending on the underlying frame the semantics of the {BailoutId} differ
  // and the various graph builders hard-code a certain semantic:
  //  - Interpreter : The BailoutId represents a bytecode offset.
  //  - FullCodegen : The BailoutId represents the id of an AST node.
  DCHECK_IMPLIES(info->is_osr() && ignition_osr,
                 info->is_optimizing_from_bytecode());
  DCHECK_IMPLIES(info->is_osr() && !ignition_osr,
                 !info->is_optimizing_from_bytecode());

  // In case of concurrent recompilation, all handles below this point will be
  // allocated in a deferred handle scope that is detached and handed off to
  // the background thread when we return.
  base::Optional<CompilationHandleScope> compilation;
  if (mode == ConcurrencyMode::kConcurrent) {
    compilation.emplace(info);
  }

  // All handles below will be canonicalized.
  CanonicalHandleScope canonical(info->isolate());

  // Reopen handles in the new CompilationHandleScope.
  info->ReopenHandlesInNewHandleScope();
  parse_info->ReopenHandlesInNewHandleScope();

  if (mode == ConcurrencyMode::kConcurrent) {
    if (GetOptimizedCodeLater(job.get())) {
      job.release();  // The background recompile job owns this now.

      // Set the optimization marker and return a code object which checks it.
      function->SetOptimizationMarker(OptimizationMarker::kInOptimizationQueue);
      if (function->IsInterpreted()) {
        return isolate->builtins()->InterpreterEntryTrampoline();
      } else {
        return isolate->builtins()->CheckOptimizationMarker();
      }
    }
  } else {
    if (GetOptimizedCodeNow(job.get())) return info->code();
  }

  if (isolate->has_pending_exception()) isolate->clear_pending_exception();
  return MaybeHandle<Code>();
}

CompilationJob::Status FinalizeOptimizedCompilationJob(CompilationJob* job) {
  CompilationInfo* info = job->info();
  Isolate* isolate = info->isolate();

  TimerEventScope<TimerEventRecompileSynchronous> timer(info->isolate());
  RuntimeCallTimerScope runtimeTimer(isolate,
                                     &RuntimeCallStats::RecompileSynchronous);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.RecompileSynchronous");

  Handle<SharedFunctionInfo> shared = info->shared_info();

  // Reset profiler ticks, function is no longer considered hot.
  shared->set_profiler_ticks(0);

  DCHECK(!shared->HasBreakInfo());

  // 1) Optimization on the concurrent thread may have failed.
  // 2) The function may have already been optimized by OSR.  Simply continue.
  //    Except when OSR already disabled optimization for some reason.
  // 3) The code may have already been invalidated due to dependency change.
  // 4) Code generation may have failed.
  if (job->state() == CompilationJob::State::kReadyToFinalize) {
    if (shared->optimization_disabled()) {
      job->RetryOptimization(kOptimizationDisabled);
    } else if (info->dependencies()->HasAborted()) {
      job->RetryOptimization(kBailedOutDueToDependencyChange);
    } else if (job->FinalizeJob() == CompilationJob::SUCCEEDED) {
      job->RecordOptimizedCompilationStats();
      RecordFunctionCompilation(CodeEventListener::LAZY_COMPILE_TAG, info);
      InsertCodeIntoOptimizedCodeCache(info);
      if (FLAG_trace_opt) {
        PrintF("[completed optimizing ");
        info->closure()->ShortPrint();
        PrintF("]\n");
      }
      info->closure()->ReplaceCode(*info->code());
      return CompilationJob::SUCCEEDED;
    }
  }

  DCHECK(job->state() == CompilationJob::State::kFailed);
  if (FLAG_trace_opt) {
    PrintF("[aborted optimizing ");
    info->closure()->ShortPrint();
    PrintF(" because: %s]\n", GetBailoutReason(info->bailout_reason()));
  }
  info->closure()->ReplaceCode(shared->code());
  // Clear the InOptimizationQueue marker, if it exists.
  if (info->closure()->IsInOptimizationQueue()) {
    info->closure()->ClearOptimizationMarker();
  }
  return CompilationJob::FAILED;
}

MaybeHandle<Code> GetLazyCode(Handle<JSFunction> function) {
  Isolate* isolate = function->GetIsolate();
  DCHECK(!isolate->has_pending_exception());
  DCHECK(!function->is_compiled());
  TimerEventScope<TimerEventCompileCode> compile_timer(isolate);
  RuntimeCallTimerScope runtimeTimer(isolate,
                                     &RuntimeCallStats::CompileFunction);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.CompileCode");
  AggregatedHistogramTimerScope timer(isolate->counters()->compile_lazy());

  if (function->shared()->is_compiled()) {
    // Function has already been compiled. Normally we'd expect the CompileLazy
    // builtin to catch cases where we already have compiled code or optimized
    // code, but there are paths that call the CompileLazy runtime function
    // directly (e.g. failed asm.js compilations), so we include a check for
    // those.
    Handle<Code> cached_code;
    if (GetCodeFromOptimizedCodeCache(function, BailoutId::None())
            .ToHandle(&cached_code)) {
      if (FLAG_trace_opt) {
        PrintF("[found optimized code for ");
        function->ShortPrint();
        PrintF(" during unoptimized compile]\n");
      }
      return cached_code;
    }
    // TODO(leszeks): Either handle optimization markers here, or DCHECK that
    // there aren't any.

    return Handle<Code>(function->shared()->code());
  } else {
    // Function doesn't have any baseline compiled code, compile now.
    DCHECK(!function->shared()->HasBytecodeArray());

    ParseInfo parse_info(handle(function->shared()));
    Zone compile_zone(isolate->allocator(), ZONE_NAME);
    CompilationInfo info(&compile_zone, &parse_info, isolate, function);
    if (FLAG_experimental_preparser_scope_analysis) {
      Handle<SharedFunctionInfo> shared(function->shared());
      if (shared->HasPreParsedScopeData()) {
        Handle<PreParsedScopeData> data(
            PreParsedScopeData::cast(shared->preparsed_scope_data()));
        parse_info.consumed_preparsed_scope_data()->SetData(data);
        // After we've compiled the function, we don't need data about its
        // skippable functions any more.
        shared->set_preparsed_scope_data(isolate->heap()->null_value());
      }
    }
    ConcurrencyMode inner_function_mode = FLAG_compiler_dispatcher_eager_inner
                                              ? ConcurrencyMode::kConcurrent
                                              : ConcurrencyMode::kNotConcurrent;
    Handle<Code> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result, GetUnoptimizedCode(&info, inner_function_mode), Code);

    if (FLAG_always_opt && !info.shared_info()->HasAsmWasmData()) {
      if (FLAG_trace_opt) {
        PrintF("[optimizing ");
        function->ShortPrint();
        PrintF(" because --always-opt]\n");
      }
      Handle<Code> opt_code;
      if (GetOptimizedCode(function, ConcurrencyMode::kNotConcurrent)
              .ToHandle(&opt_code)) {
        result = opt_code;
      }
    }

    return result;
  }
}

Handle<SharedFunctionInfo> CompileToplevel(CompilationInfo* info) {
  Isolate* isolate = info->isolate();
  TimerEventScope<TimerEventCompileCode> timer(isolate);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.CompileCode");
  PostponeInterruptsScope postpone(isolate);
  DCHECK(!isolate->native_context().is_null());
  ParseInfo* parse_info = info->parse_info();
  ConcurrencyMode inner_function_mode = FLAG_compiler_dispatcher_eager_inner
                                            ? ConcurrencyMode::kConcurrent
                                            : ConcurrencyMode::kNotConcurrent;

  RuntimeCallTimerScope runtimeTimer(
      isolate, parse_info->is_eval() ? &RuntimeCallStats::CompileEval
                                     : &RuntimeCallStats::CompileScript);

  Handle<Script> script = parse_info->script();

  Handle<SharedFunctionInfo> result;

  { VMState<COMPILER> state(info->isolate());
    if (parse_info->literal() == nullptr) {
      if (!parsing::ParseProgram(
              parse_info, info->isolate(),
              inner_function_mode != ConcurrencyMode::kConcurrent)) {
        return Handle<SharedFunctionInfo>::null();
      }

      if (inner_function_mode == ConcurrencyMode::kConcurrent) {
        ParseHandleScope parse_handles(parse_info, info->isolate());
        parse_info->ReopenHandlesInNewHandleScope();
        parse_info->ast_value_factory()->Internalize(info->isolate());
      }
    }

    // Measure how long it takes to do the compilation; only take the
    // rest of the function into account to avoid overlap with the
    // parsing statistics.
    HistogramTimer* rate = parse_info->is_eval()
                               ? info->isolate()->counters()->compile_eval()
                               : info->isolate()->counters()->compile();
    HistogramTimerScope timer(rate);
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                 parse_info->is_eval() ? "V8.CompileEval" : "V8.Compile");

    // Compile the code.
    if (!CompileUnoptimizedCode(info, inner_function_mode)) {
      return Handle<SharedFunctionInfo>::null();
    }

    if (!script.is_null()) {
      script->set_compilation_state(Script::COMPILATION_STATE_COMPILED);
    }
  }

  return info->shared_info();
}

}  // namespace

// ----------------------------------------------------------------------------
// Implementation of Compiler

bool Compiler::Analyze(ParseInfo* info, Isolate* isolate,
                       EagerInnerFunctionLiterals* eager_literals) {
  DCHECK_NOT_NULL(info->literal());
  RuntimeCallTimerScope runtimeTimer(isolate,
                                     &RuntimeCallStats::CompileAnalyse);
  if (!Rewriter::Rewrite(info, isolate)) return false;
  DeclarationScope::Analyze(info, isolate, AnalyzeMode::kRegular);
  if (!Renumber(info, eager_literals)) {
    return false;
  }
  DCHECK_NOT_NULL(info->scope());
  return true;
}

bool Compiler::Analyze(CompilationInfo* info,
                       EagerInnerFunctionLiterals* eager_literals) {
  return Compiler::Analyze(info->parse_info(), info->isolate(), eager_literals);
}

bool Compiler::ParseAndAnalyze(ParseInfo* info, Isolate* isolate) {
  if (!parsing::ParseAny(info, isolate)) return false;
  if (!Compiler::Analyze(info, isolate)) return false;
  DCHECK_NOT_NULL(info->literal());
  DCHECK_NOT_NULL(info->scope());
  return true;
}

bool Compiler::ParseAndAnalyze(CompilationInfo* info) {
  return Compiler::ParseAndAnalyze(info->parse_info(), info->isolate());
}

bool Compiler::Compile(Handle<JSFunction> function, ClearExceptionFlag flag) {
  if (function->is_compiled()) return true;
  Isolate* isolate = function->GetIsolate();
  DCHECK(AllowCompilation::IsAllowed(isolate));

  CompilerDispatcher* dispatcher = isolate->compiler_dispatcher();
  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  Handle<Code> code;
  if (dispatcher->IsEnqueued(shared)) {
    if (!dispatcher->FinishNow(shared)) {
      if (flag == CLEAR_EXCEPTION) {
        isolate->clear_pending_exception();
      }
      return false;
    }
    code = handle(shared->code(), isolate);
  } else {
    // Start a compilation.
    if (!GetLazyCode(function).ToHandle(&code)) {
      if (flag == CLEAR_EXCEPTION) {
        isolate->clear_pending_exception();
      }
      return false;
    }
  }

  // Install code on closure.
  function->ReplaceCode(*code);
  JSFunction::EnsureLiterals(function);

  // Check postconditions on success.
  DCHECK(!isolate->has_pending_exception());
  DCHECK(function->shared()->is_compiled());
  DCHECK(function->is_compiled());
  return true;
}

bool Compiler::CompileOptimized(Handle<JSFunction> function,
                                ConcurrencyMode mode) {
  if (function->IsOptimized()) return true;
  Isolate* isolate = function->GetIsolate();
  DCHECK(AllowCompilation::IsAllowed(isolate));

  // Start a compilation.
  Handle<Code> code;
  if (!GetOptimizedCode(function, mode).ToHandle(&code)) {
    // Optimization failed, get unoptimized code. Unoptimized code must exist
    // already if we are optimizing.
    DCHECK(!isolate->has_pending_exception());
    DCHECK(function->shared()->is_compiled());
    code = handle(function->shared()->code(), isolate);
  }

  // Install code on closure.
  function->ReplaceCode(*code);
  JSFunction::EnsureLiterals(function);

  // Check postconditions on success.
  DCHECK(!isolate->has_pending_exception());
  DCHECK(function->shared()->is_compiled());
  DCHECK(function->is_compiled());
  DCHECK_IMPLIES(function->HasOptimizationMarker(),
                 function->IsInOptimizationQueue());
  DCHECK_IMPLIES(function->HasOptimizationMarker(),
                 function->ChecksOptimizationMarker());
  DCHECK_IMPLIES(function->IsInOptimizationQueue(),
                 mode == ConcurrencyMode::kConcurrent);
  return true;
}

bool Compiler::CompileDebugCode(Handle<SharedFunctionInfo> shared) {
  Isolate* isolate = shared->GetIsolate();
  DCHECK(AllowCompilation::IsAllowed(isolate));

  // Start a compilation.
  ParseInfo parse_info(shared);
  CompilationInfo info(parse_info.zone(), &parse_info, isolate,
                       Handle<JSFunction>::null());
  info.MarkAsDebug();
  if (GetUnoptimizedCode(&info, ConcurrencyMode::kNotConcurrent).is_null()) {
    isolate->clear_pending_exception();
    return false;
  }

  // Check postconditions on success.
  DCHECK(!isolate->has_pending_exception());
  DCHECK(shared->is_compiled());
  DCHECK(shared->HasDebugCode());
  return true;
}

MaybeHandle<JSArray> Compiler::CompileForLiveEdit(Handle<Script> script) {
  Isolate* isolate = script->GetIsolate();
  DCHECK(AllowCompilation::IsAllowed(isolate));

  // In order to ensure that live edit function info collection finds the newly
  // generated shared function infos, clear the script's list temporarily
  // and restore it at the end of this method.
  Handle<FixedArray> old_function_infos(script->shared_function_infos(),
                                        isolate);
  script->set_shared_function_infos(isolate->heap()->empty_fixed_array());

  // Start a compilation.
  ParseInfo parse_info(script);
  Zone compile_zone(isolate->allocator(), ZONE_NAME);
  CompilationInfo info(&compile_zone, &parse_info, isolate,
                       Handle<JSFunction>::null());
  info.MarkAsDebug();

  // TODO(635): support extensions.
  const bool compilation_succeeded = !CompileToplevel(&info).is_null();
  Handle<JSArray> infos;
  if (compilation_succeeded) {
    // Check postconditions on success.
    DCHECK(!isolate->has_pending_exception());
    infos = LiveEditFunctionTracker::Collect(parse_info.literal(), script,
                                             parse_info.zone(), isolate);
  }

  // Restore the original function info list in order to remain side-effect
  // free as much as possible, since some code expects the old shared function
  // infos to stick around.
  script->set_shared_function_infos(*old_function_infos);

  return infos;
}

bool Compiler::EnsureBytecode(CompilationInfo* info) {
  if (!info->shared_info()->is_compiled()) {
    CompilerDispatcher* dispatcher = info->isolate()->compiler_dispatcher();
    if (dispatcher->IsEnqueued(info->shared_info())) {
      if (!dispatcher->FinishNow(info->shared_info())) return false;
    } else if (GetUnoptimizedCode(info, ConcurrencyMode::kNotConcurrent)
                   .is_null()) {
      return false;
    }
  }
  DCHECK(info->shared_info()->is_compiled());
  if (info->shared_info()->HasAsmWasmData()) return false;
  return info->shared_info()->HasBytecodeArray();
}

MaybeHandle<JSFunction> Compiler::GetFunctionFromEval(
    Handle<String> source, Handle<SharedFunctionInfo> outer_info,
    Handle<Context> context, LanguageMode language_mode,
    ParseRestriction restriction, int parameters_end_pos,
    int eval_scope_position, int eval_position, int line_offset,
    int column_offset, Handle<Object> script_name,
    ScriptOriginOptions options) {
  Isolate* isolate = source->GetIsolate();
  int source_length = source->length();
  isolate->counters()->total_eval_size()->Increment(source_length);
  isolate->counters()->total_compile_size()->Increment(source_length);

  // The cache lookup key needs to be aware of the separation between the
  // parameters and the body to prevent this valid invocation:
  //   Function("", "function anonymous(\n/**/) {\n}");
  // from adding an entry that falsely approves this invalid invocation:
  //   Function("\n/**/) {\nfunction anonymous(", "}");
  // The actual eval_scope_position for indirect eval and CreateDynamicFunction
  // is unused (just 0), which means it's an available field to use to indicate
  // this separation. But to make sure we're not causing other false hits, we
  // negate the scope position.
  int position = eval_scope_position;
  if (FLAG_harmony_function_tostring &&
      restriction == ONLY_SINGLE_FUNCTION_LITERAL &&
      parameters_end_pos != kNoSourcePosition) {
    // use the parameters_end_pos as the eval_scope_position in the eval cache.
    DCHECK_EQ(eval_scope_position, 0);
    position = -parameters_end_pos;
  }
  CompilationCache* compilation_cache = isolate->compilation_cache();
  InfoVectorPair eval_result = compilation_cache->LookupEval(
      source, outer_info, context, language_mode, position);
  Handle<Cell> vector;
  if (eval_result.has_vector()) {
    vector = Handle<Cell>(eval_result.vector(), isolate);
  }

  Handle<SharedFunctionInfo> shared_info;
  Handle<Script> script;
  if (eval_result.has_shared()) {
    shared_info = Handle<SharedFunctionInfo>(eval_result.shared(), isolate);
    script = Handle<Script>(Script::cast(shared_info->script()), isolate);
  } else {
    script = isolate->factory()->NewScript(source);
    if (isolate->NeedsSourcePositionsForProfiling()) {
      Script::InitLineEnds(script);
    }
    if (!script_name.is_null()) {
      script->set_name(*script_name);
      script->set_line_offset(line_offset);
      script->set_column_offset(column_offset);
    }
    script->set_origin_options(options);
    script->set_compilation_type(Script::COMPILATION_TYPE_EVAL);
    Script::SetEvalOrigin(script, outer_info, eval_position);

    ParseInfo parse_info(script);
    Zone compile_zone(isolate->allocator(), ZONE_NAME);
    CompilationInfo info(&compile_zone, &parse_info, isolate,
                         Handle<JSFunction>::null());
    parse_info.set_eval();
    parse_info.set_language_mode(language_mode);
    parse_info.set_parse_restriction(restriction);
    parse_info.set_parameters_end_pos(parameters_end_pos);
    if (!context->IsNativeContext()) {
      parse_info.set_outer_scope_info(handle(context->scope_info()));
    }

    shared_info = CompileToplevel(&info);
    if (shared_info.is_null()) {
      return MaybeHandle<JSFunction>();
    }
  }

  // If caller is strict mode, the result must be in strict mode as well.
  DCHECK(is_sloppy(language_mode) || is_strict(shared_info->language_mode()));

  Handle<JSFunction> result;
  if (eval_result.has_shared()) {
    if (eval_result.has_vector()) {
      result = isolate->factory()->NewFunctionFromSharedFunctionInfo(
          shared_info, context, vector, NOT_TENURED);
    } else {
      result = isolate->factory()->NewFunctionFromSharedFunctionInfo(
          shared_info, context, NOT_TENURED);
      JSFunction::EnsureLiterals(result);
      // Make sure to cache this result.
      Handle<Cell> new_vector(result->feedback_vector_cell(), isolate);
      compilation_cache->PutEval(source, outer_info, context, shared_info,
                                 new_vector, eval_scope_position);
    }
  } else {
    result = isolate->factory()->NewFunctionFromSharedFunctionInfo(
        shared_info, context, NOT_TENURED);
    JSFunction::EnsureLiterals(result);
    // Add the SharedFunctionInfo and the LiteralsArray to the eval cache if
    // we didn't retrieve from there.
    Handle<Cell> vector(result->feedback_vector_cell(), isolate);
    compilation_cache->PutEval(source, outer_info, context, shared_info, vector,
                               eval_scope_position);
  }

  // OnAfterCompile has to be called after we create the JSFunction, which we
  // may require to recompile the eval for debugging, if we find a function
  // that contains break points in the eval script.
  isolate->debug()->OnAfterCompile(script);

  return result;
}

namespace {

bool ContainsAsmModule(Handle<Script> script) {
  DisallowHeapAllocation no_gc;
  SharedFunctionInfo::ScriptIterator iter(script);
  while (SharedFunctionInfo* info = iter.Next()) {
    if (info->HasAsmWasmData()) return true;
  }
  return false;
}

}  // namespace

bool Compiler::CodeGenerationFromStringsAllowed(Isolate* isolate,
                                                Handle<Context> context,
                                                Handle<String> source) {
  DCHECK(context->allow_code_gen_from_strings()->IsFalse(isolate));
  // Check with callback if set.
  AllowCodeGenerationFromStringsCallback callback =
      isolate->allow_code_gen_callback();
  if (callback == NULL) {
    // No callback set and code generation disallowed.
    return false;
  } else {
    // Callback set. Let it decide if code generation is allowed.
    VMState<EXTERNAL> state(isolate);
    return callback(v8::Utils::ToLocal(context), v8::Utils::ToLocal(source));
  }
}

MaybeHandle<JSFunction> Compiler::GetFunctionFromString(
    Handle<Context> context, Handle<String> source,
    ParseRestriction restriction, int parameters_end_pos) {
  Isolate* const isolate = context->GetIsolate();
  Handle<Context> native_context(context->native_context(), isolate);

  // Check if native context allows code generation from
  // strings. Throw an exception if it doesn't.
  if (native_context->allow_code_gen_from_strings()->IsFalse(isolate) &&
      !CodeGenerationFromStringsAllowed(isolate, native_context, source)) {
    Handle<Object> error_message =
        native_context->ErrorMessageForCodeGenerationFromStrings();
    THROW_NEW_ERROR(isolate, NewEvalError(MessageTemplate::kCodeGenFromStrings,
                                          error_message),
                    JSFunction);
  }

  // Compile source string in the native context.
  int eval_scope_position = 0;
  int eval_position = kNoSourcePosition;
  Handle<SharedFunctionInfo> outer_info(native_context->closure()->shared());
  return Compiler::GetFunctionFromEval(source, outer_info, native_context,
                                       SLOPPY, restriction, parameters_end_pos,
                                       eval_scope_position, eval_position);
}

Handle<SharedFunctionInfo> Compiler::GetSharedFunctionInfoForScript(
    Handle<String> source, Handle<Object> script_name, int line_offset,
    int column_offset, ScriptOriginOptions resource_options,
    Handle<Object> source_map_url, Handle<Context> context,
    v8::Extension* extension, ScriptData** cached_data,
    ScriptCompiler::CompileOptions compile_options, NativesFlag natives) {
  Isolate* isolate = source->GetIsolate();
  if (compile_options == ScriptCompiler::kNoCompileOptions) {
    cached_data = NULL;
  } else if (compile_options == ScriptCompiler::kProduceParserCache ||
             compile_options == ScriptCompiler::kProduceCodeCache) {
    DCHECK(cached_data && !*cached_data);
    DCHECK(extension == NULL);
    DCHECK(!isolate->debug()->is_loaded());
  } else {
    DCHECK(compile_options == ScriptCompiler::kConsumeParserCache ||
           compile_options == ScriptCompiler::kConsumeCodeCache);
    DCHECK(cached_data && *cached_data);
    DCHECK(extension == NULL);
  }
  int source_length = source->length();
  isolate->counters()->total_load_size()->Increment(source_length);
  isolate->counters()->total_compile_size()->Increment(source_length);

  LanguageMode language_mode = construct_language_mode(FLAG_use_strict);
  CompilationCache* compilation_cache = isolate->compilation_cache();

  // Do a lookup in the compilation cache but not for extensions.
  Handle<SharedFunctionInfo> result;
  Handle<Cell> vector;
  if (extension == NULL) {
    // First check per-isolate compilation cache.
    InfoVectorPair pair = compilation_cache->LookupScript(
        source, script_name, line_offset, column_offset, resource_options,
        context, language_mode);
    if (!pair.has_shared() && FLAG_serialize_toplevel &&
        compile_options == ScriptCompiler::kConsumeCodeCache &&
        !isolate->debug()->is_loaded()) {
      // Then check cached code provided by embedder.
      HistogramTimerScope timer(isolate->counters()->compile_deserialize());
      RuntimeCallTimerScope runtimeTimer(isolate,
                                         &RuntimeCallStats::CompileDeserialize);
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                   "V8.CompileDeserialize");
      Handle<SharedFunctionInfo> inner_result;
      if (CodeSerializer::Deserialize(isolate, *cached_data, source)
              .ToHandle(&inner_result)) {
        // Promote to per-isolate compilation cache.
        DCHECK(inner_result->is_compiled());
        Handle<FeedbackVector> feedback_vector =
            FeedbackVector::New(isolate, inner_result);
        vector = isolate->factory()->NewCell(feedback_vector);
        compilation_cache->PutScript(source, context, language_mode,
                                     inner_result, vector);
        Handle<Script> script(Script::cast(inner_result->script()), isolate);
        isolate->debug()->OnAfterCompile(script);
        return inner_result;
      }
      // Deserializer failed. Fall through to compile.
    } else {
      if (pair.has_shared()) {
        result = Handle<SharedFunctionInfo>(pair.shared(), isolate);
      }
      if (pair.has_vector()) {
        vector = Handle<Cell>(pair.vector(), isolate);
      }
    }
  }

  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization && FLAG_serialize_toplevel &&
      compile_options == ScriptCompiler::kProduceCodeCache) {
    timer.Start();
  }

  if (result.is_null() ||
      (FLAG_serialize_toplevel &&
       compile_options == ScriptCompiler::kProduceCodeCache)) {
    // No cache entry found, or embedder wants a code cache. Compile the script.

    // Create a script object describing the script to be compiled.
    Handle<Script> script = isolate->factory()->NewScript(source);
    if (isolate->NeedsSourcePositionsForProfiling()) {
      Script::InitLineEnds(script);
    }
    if (natives == NATIVES_CODE) {
      script->set_type(Script::TYPE_NATIVE);
    } else if (natives == EXTENSION_CODE) {
      script->set_type(Script::TYPE_EXTENSION);
    } else if (natives == INSPECTOR_CODE) {
      script->set_type(Script::TYPE_INSPECTOR);
    }
    if (!script_name.is_null()) {
      script->set_name(*script_name);
      script->set_line_offset(line_offset);
      script->set_column_offset(column_offset);
    }
    script->set_origin_options(resource_options);
    if (!source_map_url.is_null()) {
      script->set_source_mapping_url(*source_map_url);
    }

    // Compile the function and add it to the cache.
    ParseInfo parse_info(script);
    Zone compile_zone(isolate->allocator(), ZONE_NAME);
    CompilationInfo info(&compile_zone, &parse_info, isolate,
                         Handle<JSFunction>::null());
    if (resource_options.IsModule()) parse_info.set_module();
    if (compile_options != ScriptCompiler::kNoCompileOptions) {
      parse_info.set_cached_data(cached_data);
    }
    parse_info.set_compile_options(compile_options);
    parse_info.set_extension(extension);
    if (!context->IsNativeContext()) {
      parse_info.set_outer_scope_info(handle(context->scope_info()));
    }
    if (FLAG_serialize_toplevel &&
        compile_options == ScriptCompiler::kProduceCodeCache) {
      info.PrepareForSerializing();
    }

    parse_info.set_language_mode(
        static_cast<LanguageMode>(parse_info.language_mode() | language_mode));
    result = CompileToplevel(&info);
    if (extension == NULL && !result.is_null()) {
      // We need a feedback vector.
      DCHECK(result->is_compiled());
      Handle<FeedbackVector> feedback_vector =
          FeedbackVector::New(isolate, result);
      vector = isolate->factory()->NewCell(feedback_vector);
      compilation_cache->PutScript(source, context, language_mode, result,
                                   vector);
      if (FLAG_serialize_toplevel &&
          compile_options == ScriptCompiler::kProduceCodeCache &&
          !ContainsAsmModule(script)) {
        HistogramTimerScope histogram_timer(
            isolate->counters()->compile_serialize());
        RuntimeCallTimerScope runtimeTimer(isolate,
                                           &RuntimeCallStats::CompileSerialize);
        TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                     "V8.CompileSerialize");
        *cached_data = CodeSerializer::Serialize(isolate, result, source);
        if (FLAG_profile_deserialization) {
          PrintF("[Compiling and serializing took %0.3f ms]\n",
                 timer.Elapsed().InMillisecondsF());
        }
      }
    }

    if (result.is_null()) {
      if (natives != EXTENSION_CODE && natives != NATIVES_CODE) {
        isolate->ReportPendingMessages();
      }
    } else {
      isolate->debug()->OnAfterCompile(script);
    }
  } else if (result->ic_age() != isolate->heap()->global_ic_age()) {
    result->ResetForNewContext(isolate->heap()->global_ic_age());
  }
  return result;
}

Handle<SharedFunctionInfo> Compiler::GetSharedFunctionInfoForStreamedScript(
    Handle<Script> script, ParseInfo* parse_info, int source_length) {
  Isolate* isolate = script->GetIsolate();
  // TODO(titzer): increment the counters in caller.
  isolate->counters()->total_load_size()->Increment(source_length);
  isolate->counters()->total_compile_size()->Increment(source_length);

  LanguageMode language_mode = construct_language_mode(FLAG_use_strict);
  parse_info->set_language_mode(
      static_cast<LanguageMode>(parse_info->language_mode() | language_mode));

  Zone compile_zone(isolate->allocator(), ZONE_NAME);
  CompilationInfo compile_info(&compile_zone, parse_info, isolate,
                               Handle<JSFunction>::null());

  // The source was parsed lazily, so compiling for debugging is not possible.
  DCHECK(!compile_info.is_debug());

  Handle<SharedFunctionInfo> result = CompileToplevel(&compile_info);
  if (!result.is_null()) isolate->debug()->OnAfterCompile(script);
  return result;
}

Handle<SharedFunctionInfo> Compiler::GetSharedFunctionInfo(
    FunctionLiteral* literal, Handle<Script> script,
    CompilationInfo* outer_info) {
  // Precondition: code has been parsed and scopes have been analyzed.
  Isolate* isolate = outer_info->isolate();
  MaybeHandle<SharedFunctionInfo> maybe_existing;

  // Find any previously allocated shared function info for the given literal.
  maybe_existing = script->FindSharedFunctionInfo(isolate, literal);

  // If we found an existing shared function info, return it.
  Handle<SharedFunctionInfo> existing;
  if (maybe_existing.ToHandle(&existing)) {
    DCHECK(!existing->is_toplevel());
    return existing;
  }

  // Allocate a shared function info object which will be compiled lazily.
  Handle<SharedFunctionInfo> result =
      isolate->factory()->NewSharedFunctionInfoForLiteral(literal, script);
  result->set_is_toplevel(false);
  Scope* outer_scope = literal->scope()->GetOuterScopeWithContext();
  if (outer_scope) {
    result->set_outer_scope_info(*outer_scope->scope_info());
  }
  return result;
}

Handle<SharedFunctionInfo> Compiler::GetSharedFunctionInfoForNative(
    v8::Extension* extension, Handle<String> name) {
  Isolate* isolate = name->GetIsolate();
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate);

  // Compute the function template for the native function.
  v8::Local<v8::FunctionTemplate> fun_template =
      extension->GetNativeFunctionTemplate(v8_isolate,
                                           v8::Utils::ToLocal(name));
  DCHECK(!fun_template.IsEmpty());

  // Instantiate the function and create a shared function info from it.
  Handle<JSFunction> fun = Handle<JSFunction>::cast(Utils::OpenHandle(
      *fun_template->GetFunction(v8_isolate->GetCurrentContext())
           .ToLocalChecked()));
  Handle<Code> code = Handle<Code>(fun->shared()->code());
  Handle<Code> construct_stub = Handle<Code>(fun->shared()->construct_stub());
  Handle<SharedFunctionInfo> shared = isolate->factory()->NewSharedFunctionInfo(
      name, FunctionKind::kNormalFunction, code,
      Handle<ScopeInfo>(fun->shared()->scope_info()));
  shared->set_outer_scope_info(fun->shared()->outer_scope_info());
  shared->SetConstructStub(*construct_stub);
  shared->set_feedback_metadata(fun->shared()->feedback_metadata());

  // Copy the function data to the shared function info.
  shared->set_function_data(fun->shared()->function_data());
  int parameters = fun->shared()->internal_formal_parameter_count();
  shared->set_internal_formal_parameter_count(parameters);

  return shared;
}

MaybeHandle<Code> Compiler::GetOptimizedCodeForOSR(Handle<JSFunction> function,
                                                   BailoutId osr_ast_id,
                                                   JavaScriptFrame* osr_frame) {
  DCHECK(!osr_ast_id.IsNone());
  DCHECK_NOT_NULL(osr_frame);
  return GetOptimizedCode(function, ConcurrencyMode::kNotConcurrent, osr_ast_id,
                          osr_frame);
}

CompilationJob* Compiler::PrepareUnoptimizedCompilationJob(
    CompilationInfo* info) {
  VMState<COMPILER> state(info->isolate());
  std::unique_ptr<CompilationJob> job(GetUnoptimizedCompilationJob(info));
  if (job->PrepareJob() != CompilationJob::SUCCEEDED) {
    return nullptr;
  }
  return job.release();
}

bool Compiler::FinalizeCompilationJob(CompilationJob* raw_job) {
  // Take ownership of compilation job.  Deleting job also tears down the zone.
  std::unique_ptr<CompilationJob> job(raw_job);

  VMState<COMPILER> state(job->info()->isolate());
  if (job->info()->IsOptimizing()) {
    return FinalizeOptimizedCompilationJob(job.get()) ==
           CompilationJob::SUCCEEDED;
  } else {
    return FinalizeUnoptimizedCompilationJob(job.get()) ==
           CompilationJob::SUCCEEDED;
  }
}

void Compiler::PostInstantiation(Handle<JSFunction> function,
                                 PretenureFlag pretenure) {
  Handle<SharedFunctionInfo> shared(function->shared());

  if (FLAG_always_opt && shared->allows_lazy_compilation() &&
      !function->shared()->HasAsmWasmData() &&
      function->shared()->is_compiled()) {
    // TODO(mvstanton): pass pretenure flag to EnsureLiterals.
    JSFunction::EnsureLiterals(function);

    if (!function->IsOptimized()) {
      // Only mark for optimization if we don't already have optimized code.
      if (!function->HasOptimizedCode()) {
        function->MarkForOptimization(ConcurrencyMode::kNotConcurrent);
      }
    }
  }

  if (shared->is_compiled()) {
    // TODO(mvstanton): pass pretenure flag to EnsureLiterals.
    JSFunction::EnsureLiterals(function);

    Code* code = function->feedback_vector()->optimized_code();
    if (code != nullptr) {
      // Caching of optimized code enabled and optimized code found.
      DCHECK(!code->marked_for_deoptimization());
      DCHECK(function->shared()->is_compiled());
      function->ReplaceCode(code);
    }
  }
}

}  // namespace internal
}  // namespace v8
