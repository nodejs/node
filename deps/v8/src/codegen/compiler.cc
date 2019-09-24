// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/compiler.h"

#include <algorithm>
#include <memory>

#include "src/api/api-inl.h"
#include "src/asmjs/asm-js.h"
#include "src/ast/prettyprinter.h"
#include "src/ast/scopes.h"
#include "src/base/optional.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/compilation-cache.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/codegen/pending-optimization-table.h"
#include "src/codegen/unoptimized-compilation-info.h"
#include "src/common/globals.h"
#include "src/common/message-template.h"
#include "src/compiler-dispatcher/compiler-dispatcher.h"
#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"
#include "src/compiler/pipeline.h"
#include "src/debug/debug.h"
#include "src/debug/liveedit.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/runtime-profiler.h"
#include "src/execution/vm-state-inl.h"
#include "src/heap/heap-inl.h"
#include "src/init/bootstrapper.h"
#include "src/interpreter/interpreter.h"
#include "src/logging/log-inl.h"
#include "src/objects/feedback-cell-inl.h"
#include "src/objects/map.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parser.h"
#include "src/parsing/parsing.h"
#include "src/parsing/rewriter.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/snapshot/code-serializer.h"
#include "src/utils/ostreams.h"
#include "src/zone/zone-list-inl.h"  // crbug.com/v8/8816

namespace v8 {
namespace internal {

// A wrapper around a OptimizedCompilationInfo that detaches the Handles from
// the underlying DeferredHandleScope and stores them in info_ on
// destruction.
class CompilationHandleScope final {
 public:
  explicit CompilationHandleScope(Isolate* isolate,
                                  OptimizedCompilationInfo* info)
      : deferred_(isolate), info_(info) {}
  ~CompilationHandleScope() { info_->set_deferred_handles(deferred_.Detach()); }

 private:
  DeferredHandleScope deferred_;
  OptimizedCompilationInfo* info_;
};

// Helper that times a scoped region and records the elapsed time.
struct ScopedTimer {
  explicit ScopedTimer(base::TimeDelta* location) : location_(location) {
    DCHECK_NOT_NULL(location_);
    timer_.Start();
  }

  ~ScopedTimer() { *location_ += timer_.Elapsed(); }

  base::ElapsedTimer timer_;
  base::TimeDelta* location_;
};

namespace {

void LogFunctionCompilation(CodeEventListener::LogEventsAndTags tag,
                            Handle<SharedFunctionInfo> shared,
                            Handle<Script> script,
                            Handle<AbstractCode> abstract_code, bool optimizing,
                            double time_taken_ms, Isolate* isolate) {
  DCHECK(!abstract_code.is_null());
  DCHECK(!abstract_code.is_identical_to(BUILTIN_CODE(isolate, CompileLazy)));

  // Log the code generation. If source information is available include
  // script name and line number. Check explicitly whether logging is
  // enabled as finding the line number is not free.
  if (!isolate->logger()->is_listening_to_code_events() &&
      !isolate->is_profiling() && !FLAG_log_function_events &&
      !isolate->code_event_dispatcher()->IsListeningToCodeEvents()) {
    return;
  }

  int line_num = Script::GetLineNumber(script, shared->StartPosition()) + 1;
  int column_num = Script::GetColumnNumber(script, shared->StartPosition()) + 1;
  String script_name = script->name().IsString()
                           ? String::cast(script->name())
                           : ReadOnlyRoots(isolate).empty_string();
  CodeEventListener::LogEventsAndTags log_tag =
      Logger::ToNativeByScript(tag, *script);
  PROFILE(isolate, CodeCreateEvent(log_tag, *abstract_code, *shared,
                                   script_name, line_num, column_num));
  if (!FLAG_log_function_events) return;

  DisallowHeapAllocation no_gc;

  std::string name = optimizing ? "optimize" : "compile";
  switch (tag) {
    case CodeEventListener::EVAL_TAG:
      name += "-eval";
      break;
    case CodeEventListener::SCRIPT_TAG:
      break;
    case CodeEventListener::LAZY_COMPILE_TAG:
      name += "-lazy";
      break;
    case CodeEventListener::FUNCTION_TAG:
      break;
    default:
      UNREACHABLE();
  }

  LOG(isolate, FunctionEvent(name.c_str(), script->id(), time_taken_ms,
                             shared->StartPosition(), shared->EndPosition(),
                             shared->DebugName()));
}

ScriptOriginOptions OriginOptionsForEval(Object script) {
  if (!script.IsScript()) return ScriptOriginOptions();

  const auto outer_origin_options = Script::cast(script).origin_options();
  return ScriptOriginOptions(outer_origin_options.IsSharedCrossOrigin(),
                             outer_origin_options.IsOpaque());
}

}  // namespace

// ----------------------------------------------------------------------------
// Implementation of UnoptimizedCompilationJob

CompilationJob::Status UnoptimizedCompilationJob::ExecuteJob() {
  DisallowHeapAccess no_heap_access;
  // Delegate to the underlying implementation.
  DCHECK_EQ(state(), State::kReadyToExecute);
  ScopedTimer t(&time_taken_to_execute_);
  return UpdateState(ExecuteJobImpl(), State::kReadyToFinalize);
}

CompilationJob::Status UnoptimizedCompilationJob::FinalizeJob(
    Handle<SharedFunctionInfo> shared_info, Isolate* isolate) {
  DCHECK_EQ(ThreadId::Current(), isolate->thread_id());
  DisallowCodeDependencyChange no_dependency_change;
  DisallowJavascriptExecution no_js(isolate);

  // Delegate to the underlying implementation.
  DCHECK_EQ(state(), State::kReadyToFinalize);
  ScopedTimer t(&time_taken_to_finalize_);
  return UpdateState(FinalizeJobImpl(shared_info, isolate), State::kSucceeded);
}

void UnoptimizedCompilationJob::RecordCompilationStats(Isolate* isolate) const {
  int code_size;
  if (compilation_info()->has_bytecode_array()) {
    code_size = compilation_info()->bytecode_array()->SizeIncludingMetadata();
  } else {
    DCHECK(compilation_info()->has_asm_wasm_data());
    code_size = compilation_info()->asm_wasm_data()->Size();
  }

  Counters* counters = isolate->counters();
  // TODO(4280): Rename counters from "baseline" to "unoptimized" eventually.
  counters->total_baseline_code_size()->Increment(code_size);
  counters->total_baseline_compile_count()->Increment(1);

  // TODO(5203): Add timers for each phase of compilation.
  // Also add total time (there's now already timer_ on the base class).
}

void UnoptimizedCompilationJob::RecordFunctionCompilation(
    CodeEventListener::LogEventsAndTags tag, Handle<SharedFunctionInfo> shared,
    Isolate* isolate) const {
  Handle<AbstractCode> abstract_code;
  if (compilation_info()->has_bytecode_array()) {
    abstract_code =
        Handle<AbstractCode>::cast(compilation_info()->bytecode_array());
  } else {
    DCHECK(compilation_info()->has_asm_wasm_data());
    abstract_code =
        Handle<AbstractCode>::cast(BUILTIN_CODE(isolate, InstantiateAsmJs));
  }

  double time_taken_ms = time_taken_to_execute_.InMillisecondsF() +
                         time_taken_to_finalize_.InMillisecondsF();

  LogFunctionCompilation(tag, shared, parse_info()->script(), abstract_code,
                         false, time_taken_ms, isolate);
}

// ----------------------------------------------------------------------------
// Implementation of OptimizedCompilationJob

CompilationJob::Status OptimizedCompilationJob::PrepareJob(Isolate* isolate) {
  DCHECK_EQ(ThreadId::Current(), isolate->thread_id());
  DisallowJavascriptExecution no_js(isolate);

  if (FLAG_trace_opt && compilation_info()->IsOptimizing()) {
    StdoutStream os;
    os << "[compiling method " << Brief(*compilation_info()->closure())
       << " using " << compiler_name_;
    if (compilation_info()->is_osr()) os << " OSR";
    os << "]" << std::endl;
  }

  // Delegate to the underlying implementation.
  DCHECK_EQ(state(), State::kReadyToPrepare);
  ScopedTimer t(&time_taken_to_prepare_);
  return UpdateState(PrepareJobImpl(isolate), State::kReadyToExecute);
}

CompilationJob::Status OptimizedCompilationJob::ExecuteJob() {
  DisallowHeapAccess no_heap_access;
  // Delegate to the underlying implementation.
  DCHECK_EQ(state(), State::kReadyToExecute);
  ScopedTimer t(&time_taken_to_execute_);
  return UpdateState(ExecuteJobImpl(), State::kReadyToFinalize);
}

CompilationJob::Status OptimizedCompilationJob::FinalizeJob(Isolate* isolate) {
  DCHECK_EQ(ThreadId::Current(), isolate->thread_id());
  DisallowJavascriptExecution no_js(isolate);

  // Delegate to the underlying implementation.
  DCHECK_EQ(state(), State::kReadyToFinalize);
  ScopedTimer t(&time_taken_to_finalize_);
  return UpdateState(FinalizeJobImpl(isolate), State::kSucceeded);
}

CompilationJob::Status OptimizedCompilationJob::RetryOptimization(
    BailoutReason reason) {
  DCHECK(compilation_info_->IsOptimizing());
  compilation_info_->RetryOptimization(reason);
  return UpdateState(FAILED, State::kFailed);
}

CompilationJob::Status OptimizedCompilationJob::AbortOptimization(
    BailoutReason reason) {
  DCHECK(compilation_info_->IsOptimizing());
  compilation_info_->AbortOptimization(reason);
  return UpdateState(FAILED, State::kFailed);
}

void OptimizedCompilationJob::RecordCompilationStats(CompilationMode mode,
                                                     Isolate* isolate) const {
  DCHECK(compilation_info()->IsOptimizing());
  Handle<JSFunction> function = compilation_info()->closure();
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
    code_size += function->shared().SourceSize();
    PrintF("Compiled: %d functions with %d byte source size in %fms.\n",
           compiled_functions, code_size, compilation_time);
  }
  // Don't record samples from machines without high-resolution timers,
  // as that can cause serious reporting issues. See the thread at
  // http://g/chrome-metrics-team/NwwJEyL8odU/discussion for more details.
  if (base::TimeTicks::IsHighResolution()) {
    Counters* const counters = isolate->counters();
    if (compilation_info()->is_osr()) {
      counters->turbofan_osr_prepare()->AddSample(
          static_cast<int>(time_taken_to_prepare_.InMicroseconds()));
      counters->turbofan_osr_execute()->AddSample(
          static_cast<int>(time_taken_to_execute_.InMicroseconds()));
      counters->turbofan_osr_finalize()->AddSample(
          static_cast<int>(time_taken_to_finalize_.InMicroseconds()));
      counters->turbofan_osr_total_time()->AddSample(
          static_cast<int>(ElapsedTime().InMicroseconds()));
    } else {
      counters->turbofan_optimize_prepare()->AddSample(
          static_cast<int>(time_taken_to_prepare_.InMicroseconds()));
      counters->turbofan_optimize_execute()->AddSample(
          static_cast<int>(time_taken_to_execute_.InMicroseconds()));
      counters->turbofan_optimize_finalize()->AddSample(
          static_cast<int>(time_taken_to_finalize_.InMicroseconds()));
      counters->turbofan_optimize_total_time()->AddSample(
          static_cast<int>(ElapsedTime().InMicroseconds()));

      // Compute foreground / background time.
      base::TimeDelta time_background;
      base::TimeDelta time_foreground =
          time_taken_to_prepare_ + time_taken_to_finalize_;
      switch (mode) {
        case OptimizedCompilationJob::kConcurrent:
          time_background += time_taken_to_execute_;
          counters->turbofan_optimize_concurrent_total_time()->AddSample(
              static_cast<int>(ElapsedTime().InMicroseconds()));
          break;
        case OptimizedCompilationJob::kSynchronous:
          counters->turbofan_optimize_non_concurrent_total_time()->AddSample(
              static_cast<int>(ElapsedTime().InMicroseconds()));
          time_foreground += time_taken_to_execute_;
          break;
      }
      counters->turbofan_optimize_total_background()->AddSample(
          static_cast<int>(time_background.InMicroseconds()));
      counters->turbofan_optimize_total_foreground()->AddSample(
          static_cast<int>(time_foreground.InMicroseconds()));
    }
    counters->turbofan_ticks()->AddSample(static_cast<int>(
        compilation_info()->tick_counter().CurrentTicks() / 1000));
  }
}

void OptimizedCompilationJob::RecordFunctionCompilation(
    CodeEventListener::LogEventsAndTags tag, Isolate* isolate) const {
  Handle<AbstractCode> abstract_code =
      Handle<AbstractCode>::cast(compilation_info()->code());

  double time_taken_ms = time_taken_to_prepare_.InMillisecondsF() +
                         time_taken_to_execute_.InMillisecondsF() +
                         time_taken_to_finalize_.InMillisecondsF();

  Handle<Script> script(
      Script::cast(compilation_info()->shared_info()->script()), isolate);
  LogFunctionCompilation(tag, compilation_info()->shared_info(), script,
                         abstract_code, true, time_taken_ms, isolate);
}

// ----------------------------------------------------------------------------
// Local helper methods that make up the compilation pipeline.

namespace {

bool UseAsmWasm(FunctionLiteral* literal, bool asm_wasm_broken) {
  // Check whether asm.js validation is enabled.
  if (!FLAG_validate_asm) return false;

  // Modules that have validated successfully, but were subsequently broken by
  // invalid module instantiation attempts are off limit forever.
  if (asm_wasm_broken) return false;

  // In stress mode we want to run the validator on everything.
  if (FLAG_stress_validate_asm) return true;

  // In general, we respect the "use asm" directive.
  return literal->scope()->IsAsmModule();
}

void InstallBytecodeArray(Handle<BytecodeArray> bytecode_array,
                          Handle<SharedFunctionInfo> shared_info,
                          ParseInfo* parse_info, Isolate* isolate) {
  if (!FLAG_interpreted_frames_native_stack) {
    shared_info->set_bytecode_array(*bytecode_array);
    return;
  }

  Handle<Code> code = isolate->factory()->CopyCode(Handle<Code>::cast(
      isolate->factory()->interpreter_entry_trampoline_for_profiling()));

  Handle<InterpreterData> interpreter_data =
      Handle<InterpreterData>::cast(isolate->factory()->NewStruct(
          INTERPRETER_DATA_TYPE, AllocationType::kOld));

  interpreter_data->set_bytecode_array(*bytecode_array);
  interpreter_data->set_interpreter_trampoline(*code);

  shared_info->set_interpreter_data(*interpreter_data);

  Handle<Script> script = parse_info->script();
  Handle<AbstractCode> abstract_code = Handle<AbstractCode>::cast(code);
  int line_num =
      Script::GetLineNumber(script, shared_info->StartPosition()) + 1;
  int column_num =
      Script::GetColumnNumber(script, shared_info->StartPosition()) + 1;
  String script_name = script->name().IsString()
                           ? String::cast(script->name())
                           : ReadOnlyRoots(isolate).empty_string();
  CodeEventListener::LogEventsAndTags log_tag = Logger::ToNativeByScript(
      CodeEventListener::INTERPRETED_FUNCTION_TAG, *script);
  PROFILE(isolate, CodeCreateEvent(log_tag, *abstract_code, *shared_info,
                                   script_name, line_num, column_num));
}

void InstallUnoptimizedCode(UnoptimizedCompilationInfo* compilation_info,
                            Handle<SharedFunctionInfo> shared_info,
                            ParseInfo* parse_info, Isolate* isolate) {
  DCHECK_EQ(shared_info->language_mode(),
            compilation_info->literal()->language_mode());

  // Update the shared function info with the scope info.
  Handle<ScopeInfo> scope_info = compilation_info->scope()->scope_info();
  shared_info->set_scope_info(*scope_info);

  if (compilation_info->has_bytecode_array()) {
    DCHECK(!shared_info->HasBytecodeArray());  // Only compiled once.
    DCHECK(!compilation_info->has_asm_wasm_data());
    DCHECK(!shared_info->HasFeedbackMetadata());

    // If the function failed asm-wasm compilation, mark asm_wasm as broken
    // to ensure we don't try to compile as asm-wasm.
    if (compilation_info->literal()->scope()->IsAsmModule()) {
      shared_info->set_is_asm_wasm_broken(true);
    }

    InstallBytecodeArray(compilation_info->bytecode_array(), shared_info,
                         parse_info, isolate);

    Handle<FeedbackMetadata> feedback_metadata = FeedbackMetadata::New(
        isolate, compilation_info->feedback_vector_spec());
    shared_info->set_feedback_metadata(*feedback_metadata);
  } else {
    DCHECK(compilation_info->has_asm_wasm_data());
    shared_info->set_asm_wasm_data(*compilation_info->asm_wasm_data());
    shared_info->set_feedback_metadata(
        ReadOnlyRoots(isolate).empty_feedback_metadata());
  }

  // Install coverage info on the shared function info.
  if (compilation_info->has_coverage_info() &&
      !shared_info->HasCoverageInfo()) {
    DCHECK(isolate->is_block_code_coverage());
    isolate->debug()->InstallCoverageInfo(shared_info,
                                          compilation_info->coverage_info());
  }
}

void EnsureSharedFunctionInfosArrayOnScript(ParseInfo* parse_info,
                                            Isolate* isolate) {
  DCHECK(parse_info->is_toplevel());
  DCHECK(!parse_info->script().is_null());
  if (parse_info->script()->shared_function_infos().length() > 0) {
    DCHECK_EQ(parse_info->script()->shared_function_infos().length(),
              parse_info->max_function_literal_id() + 1);
    return;
  }
  Handle<WeakFixedArray> infos(isolate->factory()->NewWeakFixedArray(
      parse_info->max_function_literal_id() + 1));
  parse_info->script()->set_shared_function_infos(*infos);
}

void SetSharedFunctionFlagsFromLiteral(FunctionLiteral* literal,
                                       Handle<SharedFunctionInfo> shared_info) {
  shared_info->set_has_duplicate_parameters(
      literal->has_duplicate_parameters());
  shared_info->set_is_oneshot_iife(literal->is_oneshot_iife());
  shared_info->UpdateAndFinalizeExpectedNofPropertiesFromEstimate(literal);
  if (literal->dont_optimize_reason() != BailoutReason::kNoReason) {
    shared_info->DisableOptimization(literal->dont_optimize_reason());
  }
  shared_info->set_is_safe_to_skip_arguments_adaptor(
      literal->SafeToSkipArgumentsAdaptor());
}

CompilationJob::Status FinalizeUnoptimizedCompilationJob(
    UnoptimizedCompilationJob* job, Handle<SharedFunctionInfo> shared_info,
    Isolate* isolate) {
  UnoptimizedCompilationInfo* compilation_info = job->compilation_info();
  ParseInfo* parse_info = job->parse_info();

  SetSharedFunctionFlagsFromLiteral(compilation_info->literal(), shared_info);

  CompilationJob::Status status = job->FinalizeJob(shared_info, isolate);
  if (status == CompilationJob::SUCCEEDED) {
    InstallUnoptimizedCode(compilation_info, shared_info, parse_info, isolate);
    CodeEventListener::LogEventsAndTags log_tag;
    if (parse_info->is_toplevel()) {
      log_tag = compilation_info->is_eval() ? CodeEventListener::EVAL_TAG
                                            : CodeEventListener::SCRIPT_TAG;
    } else {
      log_tag = parse_info->lazy_compile() ? CodeEventListener::LAZY_COMPILE_TAG
                                           : CodeEventListener::FUNCTION_TAG;
    }
    job->RecordFunctionCompilation(log_tag, shared_info, isolate);
    job->RecordCompilationStats(isolate);
  }
  return status;
}

std::unique_ptr<UnoptimizedCompilationJob> ExecuteUnoptimizedCompileJobs(
    ParseInfo* parse_info, FunctionLiteral* literal,
    AccountingAllocator* allocator,
    UnoptimizedCompilationJobList* inner_function_jobs) {
  if (UseAsmWasm(literal, parse_info->is_asm_wasm_broken())) {
    std::unique_ptr<UnoptimizedCompilationJob> asm_job(
        AsmJs::NewCompilationJob(parse_info, literal, allocator));
    if (asm_job->ExecuteJob() == CompilationJob::SUCCEEDED) {
      return asm_job;
    }
    // asm.js validation failed, fall through to standard unoptimized compile.
    // Note: we rely on the fact that AsmJs jobs have done all validation in the
    // PrepareJob and ExecuteJob phases and can't fail in FinalizeJob with
    // with a validation error or another error that could be solve by falling
    // through to standard unoptimized compile.
  }
  std::vector<FunctionLiteral*> eager_inner_literals;
  std::unique_ptr<UnoptimizedCompilationJob> job(
      interpreter::Interpreter::NewCompilationJob(
          parse_info, literal, allocator, &eager_inner_literals));

  if (job->ExecuteJob() != CompilationJob::SUCCEEDED) {
    // Compilation failed, return null.
    return std::unique_ptr<UnoptimizedCompilationJob>();
  }

  // Recursively compile eager inner literals.
  for (FunctionLiteral* inner_literal : eager_inner_literals) {
    std::unique_ptr<UnoptimizedCompilationJob> inner_job(
        ExecuteUnoptimizedCompileJobs(parse_info, inner_literal, allocator,
                                      inner_function_jobs));
    // Compilation failed, return null.
    if (!inner_job) return std::unique_ptr<UnoptimizedCompilationJob>();
    inner_function_jobs->emplace_front(std::move(inner_job));
  }

  return job;
}

std::unique_ptr<UnoptimizedCompilationJob> GenerateUnoptimizedCode(
    ParseInfo* parse_info, AccountingAllocator* allocator,
    UnoptimizedCompilationJobList* inner_function_jobs) {
  DisallowHeapAccess no_heap_access;
  DCHECK(inner_function_jobs->empty());

  std::unique_ptr<UnoptimizedCompilationJob> job;
  if (Compiler::Analyze(parse_info)) {
    job = ExecuteUnoptimizedCompileJobs(parse_info, parse_info->literal(),
                                        allocator, inner_function_jobs);
  }

  // Character stream shouldn't be used again.
  parse_info->ResetCharacterStream();

  return job;
}

MaybeHandle<SharedFunctionInfo> GenerateUnoptimizedCodeForToplevel(
    Isolate* isolate, ParseInfo* parse_info, AccountingAllocator* allocator,
    IsCompiledScope* is_compiled_scope) {
  EnsureSharedFunctionInfosArrayOnScript(parse_info, isolate);
  parse_info->ast_value_factory()->Internalize(isolate);

  if (!Compiler::Analyze(parse_info)) return MaybeHandle<SharedFunctionInfo>();
  DeclarationScope::AllocateScopeInfos(parse_info, isolate);

  // Prepare and execute compilation of the outer-most function.
  // Create the SharedFunctionInfo and add it to the script's list.
  Handle<Script> script = parse_info->script();
  Handle<SharedFunctionInfo> top_level =
      isolate->factory()->NewSharedFunctionInfoForLiteral(parse_info->literal(),
                                                          script, true);

  std::vector<FunctionLiteral*> functions_to_compile;
  functions_to_compile.push_back(parse_info->literal());

  while (!functions_to_compile.empty()) {
    FunctionLiteral* literal = functions_to_compile.back();
    functions_to_compile.pop_back();
    Handle<SharedFunctionInfo> shared_info =
        Compiler::GetSharedFunctionInfo(literal, script, isolate);
    if (shared_info->is_compiled()) continue;
    if (UseAsmWasm(literal, parse_info->is_asm_wasm_broken())) {
      std::unique_ptr<UnoptimizedCompilationJob> asm_job(
          AsmJs::NewCompilationJob(parse_info, literal, allocator));
      if (asm_job->ExecuteJob() == CompilationJob::SUCCEEDED &&
          FinalizeUnoptimizedCompilationJob(asm_job.get(), shared_info,
                                            isolate) ==
              CompilationJob::SUCCEEDED) {
        continue;
      }
      // asm.js validation failed, fall through to standard unoptimized compile.
      // Note: we rely on the fact that AsmJs jobs have done all validation in
      // the PrepareJob and ExecuteJob phases and can't fail in FinalizeJob with
      // with a validation error or another error that could be solve by falling
      // through to standard unoptimized compile.
    }

    std::unique_ptr<UnoptimizedCompilationJob> job(
        interpreter::Interpreter::NewCompilationJob(
            parse_info, literal, allocator, &functions_to_compile));

    if (job->ExecuteJob() == CompilationJob::FAILED ||
        FinalizeUnoptimizedCompilationJob(job.get(), shared_info, isolate) ==
            CompilationJob::FAILED) {
      return MaybeHandle<SharedFunctionInfo>();
    }

    if (FLAG_stress_lazy_source_positions) {
      // Collect source positions immediately to try and flush out bytecode
      // mismatches.
      SharedFunctionInfo::EnsureSourcePositionsAvailable(isolate, shared_info);
    }

    if (shared_info.is_identical_to(top_level)) {
      // Ensure that the top level function is retained.
      *is_compiled_scope = shared_info->is_compiled_scope();
      DCHECK(is_compiled_scope->is_compiled());
    }
  }

  // Character stream shouldn't be used again.
  parse_info->ResetCharacterStream();

  return top_level;
}

bool FinalizeUnoptimizedCode(
    ParseInfo* parse_info, Isolate* isolate,
    Handle<SharedFunctionInfo> shared_info,
    UnoptimizedCompilationJob* outer_function_job,
    UnoptimizedCompilationJobList* inner_function_jobs) {
  DCHECK(AllowCompilation::IsAllowed(isolate));

  // TODO(rmcilroy): Clear native context in debug once AsmJS generates doesn't
  // rely on accessing native context during finalization.

  // Allocate scope infos for the literal.
  DeclarationScope::AllocateScopeInfos(parse_info, isolate);

  // Finalize the outer-most function's compilation job.
  if (FinalizeUnoptimizedCompilationJob(outer_function_job, shared_info,
                                        isolate) != CompilationJob::SUCCEEDED) {
    return false;
  }

  // Finalize the inner functions' compilation jobs.
  for (auto&& inner_job : *inner_function_jobs) {
    Handle<SharedFunctionInfo> inner_shared_info =
        Compiler::GetSharedFunctionInfo(
            inner_job->compilation_info()->literal(), parse_info->script(),
            isolate);
    // The inner function might be compiled already if compiling for debug.
    if (inner_shared_info->is_compiled()) continue;
    if (FinalizeUnoptimizedCompilationJob(inner_job.get(), inner_shared_info,
                                          isolate) !=
        CompilationJob::SUCCEEDED) {
      return false;
    }
  }

  // Report any warnings generated during compilation.
  if (parse_info->pending_error_handler()->has_pending_warnings()) {
    parse_info->pending_error_handler()->ReportWarnings(isolate,
                                                        parse_info->script());
  }

  return true;
}

V8_WARN_UNUSED_RESULT MaybeHandle<Code> GetCodeFromOptimizedCodeCache(
    Handle<JSFunction> function, BailoutId osr_offset) {
  RuntimeCallTimerScope runtimeTimer(
      function->GetIsolate(),
      RuntimeCallCounterId::kCompileGetFromOptimizedCodeMap);
  Handle<SharedFunctionInfo> shared(function->shared(), function->GetIsolate());
  DisallowHeapAllocation no_gc;
  if (osr_offset.IsNone()) {
    if (function->has_feedback_vector()) {
      FeedbackVector feedback_vector = function->feedback_vector();
      feedback_vector.EvictOptimizedCodeMarkedForDeoptimization(
          function->shared(), "GetCodeFromOptimizedCodeCache");
      Code code = feedback_vector.optimized_code();

      if (!code.is_null()) {
        // Caching of optimized code enabled and optimized code found.
        DCHECK(!code.marked_for_deoptimization());
        DCHECK(function->shared().is_compiled());
        return Handle<Code>(code, feedback_vector.GetIsolate());
      }
    }
  }
  return MaybeHandle<Code>();
}

void ClearOptimizedCodeCache(OptimizedCompilationInfo* compilation_info) {
  Handle<JSFunction> function = compilation_info->closure();
  if (compilation_info->osr_offset().IsNone()) {
    Handle<FeedbackVector> vector =
        handle(function->feedback_vector(), function->GetIsolate());
    vector->ClearOptimizationMarker();
  }
}

void InsertCodeIntoOptimizedCodeCache(
    OptimizedCompilationInfo* compilation_info) {
  Handle<Code> code = compilation_info->code();
  if (code->kind() != Code::OPTIMIZED_FUNCTION) return;  // Nothing to do.

  // Function context specialization folds-in the function context,
  // so no sharing can occur.
  if (compilation_info->is_function_context_specializing()) {
    // Native context specialized code is not shared, so make sure the optimized
    // code cache is clear.
    ClearOptimizedCodeCache(compilation_info);
    return;
  }

  // Cache optimized context-specific code.
  Handle<JSFunction> function = compilation_info->closure();
  Handle<SharedFunctionInfo> shared(function->shared(), function->GetIsolate());
  Handle<Context> native_context(function->context().native_context(),
                                 function->GetIsolate());
  if (compilation_info->osr_offset().IsNone()) {
    Handle<FeedbackVector> vector =
        handle(function->feedback_vector(), function->GetIsolate());
    FeedbackVector::SetOptimizedCode(vector, code);
  }
}

bool GetOptimizedCodeNow(OptimizedCompilationJob* job, Isolate* isolate) {
  TimerEventScope<TimerEventRecompileSynchronous> timer(isolate);
  RuntimeCallTimerScope runtimeTimer(
      isolate, RuntimeCallCounterId::kRecompileSynchronous);
  OptimizedCompilationInfo* compilation_info = job->compilation_info();
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.RecompileSynchronous");

  if (job->PrepareJob(isolate) != CompilationJob::SUCCEEDED ||
      job->ExecuteJob() != CompilationJob::SUCCEEDED ||
      job->FinalizeJob(isolate) != CompilationJob::SUCCEEDED) {
    if (FLAG_trace_opt) {
      PrintF("[aborted optimizing ");
      compilation_info->closure()->ShortPrint();
      PrintF(" because: %s]\n",
             GetBailoutReason(compilation_info->bailout_reason()));
    }
    return false;
  }

  // Success!
  job->RecordCompilationStats(OptimizedCompilationJob::kSynchronous, isolate);
  DCHECK(!isolate->has_pending_exception());
  InsertCodeIntoOptimizedCodeCache(compilation_info);
  job->RecordFunctionCompilation(CodeEventListener::LAZY_COMPILE_TAG, isolate);
  return true;
}

bool GetOptimizedCodeLater(OptimizedCompilationJob* job, Isolate* isolate) {
  OptimizedCompilationInfo* compilation_info = job->compilation_info();
  if (!isolate->optimizing_compile_dispatcher()->IsQueueAvailable()) {
    if (FLAG_trace_concurrent_recompilation) {
      PrintF("  ** Compilation queue full, will retry optimizing ");
      compilation_info->closure()->ShortPrint();
      PrintF(" later.\n");
    }
    return false;
  }

  if (isolate->heap()->HighMemoryPressure()) {
    if (FLAG_trace_concurrent_recompilation) {
      PrintF("  ** High memory pressure, will retry optimizing ");
      compilation_info->closure()->ShortPrint();
      PrintF(" later.\n");
    }
    return false;
  }

  TimerEventScope<TimerEventRecompileSynchronous> timer(isolate);
  RuntimeCallTimerScope runtimeTimer(
      isolate, RuntimeCallCounterId::kRecompileSynchronous);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.RecompileSynchronous");

  if (job->PrepareJob(isolate) != CompilationJob::SUCCEEDED) return false;
  isolate->optimizing_compile_dispatcher()->QueueForOptimization(job);

  if (FLAG_trace_concurrent_recompilation) {
    PrintF("  ** Queued ");
    compilation_info->closure()->ShortPrint();
    PrintF(" for concurrent optimization.\n");
  }
  return true;
}

MaybeHandle<Code> GetOptimizedCode(Handle<JSFunction> function,
                                   ConcurrencyMode mode,
                                   BailoutId osr_offset = BailoutId::None(),
                                   JavaScriptFrame* osr_frame = nullptr) {
  Isolate* isolate = function->GetIsolate();
  Handle<SharedFunctionInfo> shared(function->shared(), isolate);

  // Make sure we clear the optimization marker on the function so that we
  // don't try to re-optimize.
  if (function->HasOptimizationMarker()) {
    function->ClearOptimizationMarker();
  }

  if (shared->optimization_disabled() &&
      shared->disable_optimization_reason() == BailoutReason::kNeverOptimize) {
    return MaybeHandle<Code>();
  }

  if (isolate->debug()->needs_check_on_function_call()) {
    // Do not optimize when debugger needs to hook into every call.
    return MaybeHandle<Code>();
  }

  // If code was pending optimization for testing, delete remove the entry
  // from the table that was preventing the bytecode from being flushed
  if (V8_UNLIKELY(FLAG_testing_d8_test_runner)) {
    PendingOptimizationTable::FunctionWasOptimized(isolate, function);
  }

  Handle<Code> cached_code;
  if (GetCodeFromOptimizedCodeCache(function, osr_offset)
          .ToHandle(&cached_code)) {
    if (FLAG_trace_opt) {
      PrintF("[found optimized code for ");
      function->ShortPrint();
      if (!osr_offset.IsNone()) {
        PrintF(" at OSR AST id %d", osr_offset.ToInt());
      }
      PrintF("]\n");
    }
    return cached_code;
  }

  // Reset profiler ticks, function is no longer considered hot.
  DCHECK(shared->is_compiled());
  function->feedback_vector().set_profiler_ticks(0);

  VMState<COMPILER> state(isolate);
  TimerEventScope<TimerEventOptimizeCode> optimize_code_timer(isolate);
  RuntimeCallTimerScope runtimeTimer(isolate,
                                     RuntimeCallCounterId::kOptimizeCode);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.OptimizeCode");

  DCHECK(!isolate->has_pending_exception());
  PostponeInterruptsScope postpone(isolate);
  bool has_script = shared->script().IsScript();
  // BUG(5946): This DCHECK is necessary to make certain that we won't
  // tolerate the lack of a script without bytecode.
  DCHECK_IMPLIES(!has_script, shared->HasBytecodeArray());
  std::unique_ptr<OptimizedCompilationJob> job(
      compiler::Pipeline::NewCompilationJob(isolate, function, has_script));
  OptimizedCompilationInfo* compilation_info = job->compilation_info();

  compilation_info->SetOptimizingForOsr(osr_offset, osr_frame);

  // Do not use TurboFan if we need to be able to set break points.
  if (compilation_info->shared_info()->HasBreakInfo()) {
    compilation_info->AbortOptimization(BailoutReason::kFunctionBeingDebugged);
    return MaybeHandle<Code>();
  }

  // Do not use TurboFan if optimization is disabled or function doesn't pass
  // turbo_filter.
  if (!FLAG_opt || !shared->PassesFilter(FLAG_turbo_filter)) {
    compilation_info->AbortOptimization(BailoutReason::kOptimizationDisabled);
    return MaybeHandle<Code>();
  }

  // In case of concurrent recompilation, all handles below this point will be
  // allocated in a deferred handle scope that is detached and handed off to
  // the background thread when we return.
  base::Optional<CompilationHandleScope> compilation;
  if (mode == ConcurrencyMode::kConcurrent) {
    compilation.emplace(isolate, compilation_info);
  }

  // All handles below will be canonicalized.
  CanonicalHandleScope canonical(isolate);

  // Reopen handles in the new CompilationHandleScope.
  compilation_info->ReopenHandlesInNewHandleScope(isolate);

  if (mode == ConcurrencyMode::kConcurrent) {
    if (GetOptimizedCodeLater(job.get(), isolate)) {
      job.release();  // The background recompile job owns this now.

      // Set the optimization marker and return a code object which checks it.
      function->SetOptimizationMarker(OptimizationMarker::kInOptimizationQueue);
      DCHECK(function->IsInterpreted() ||
             (!function->is_compiled() && function->shared().IsInterpreted()));
      DCHECK(function->shared().HasBytecodeArray());
      return BUILTIN_CODE(isolate, InterpreterEntryTrampoline);
    }
  } else {
    if (GetOptimizedCodeNow(job.get(), isolate))
      return compilation_info->code();
  }

  if (isolate->has_pending_exception()) isolate->clear_pending_exception();
  return MaybeHandle<Code>();
}

bool FailWithPendingException(Isolate* isolate, ParseInfo* parse_info,
                              Compiler::ClearExceptionFlag flag) {
  if (flag == Compiler::CLEAR_EXCEPTION) {
    isolate->clear_pending_exception();
  } else if (!isolate->has_pending_exception()) {
    if (parse_info->pending_error_handler()->has_pending_error()) {
      parse_info->pending_error_handler()->ReportErrors(
          isolate, parse_info->script(), parse_info->ast_value_factory());
    } else {
      isolate->StackOverflow();
    }
  }
  return false;
}

void FinalizeScriptCompilation(Isolate* isolate, ParseInfo* parse_info) {
  Handle<Script> script = parse_info->script();
  script->set_compilation_state(Script::COMPILATION_STATE_COMPILED);

  // Register any pending parallel tasks with the associated SFI.
  if (parse_info->parallel_tasks()) {
    CompilerDispatcher* dispatcher = parse_info->parallel_tasks()->dispatcher();
    for (auto& it : *parse_info->parallel_tasks()) {
      FunctionLiteral* literal = it.first;
      CompilerDispatcher::JobId job_id = it.second;
      MaybeHandle<SharedFunctionInfo> maybe_shared_for_task =
          script->FindSharedFunctionInfo(isolate, literal);
      Handle<SharedFunctionInfo> shared_for_task;
      if (maybe_shared_for_task.ToHandle(&shared_for_task)) {
        dispatcher->RegisterSharedFunctionInfo(job_id, *shared_for_task);
      } else {
        dispatcher->AbortJob(job_id);
      }
    }
  }
}

MaybeHandle<SharedFunctionInfo> FinalizeTopLevel(
    ParseInfo* parse_info, Isolate* isolate,
    UnoptimizedCompilationJob* outer_function_job,
    UnoptimizedCompilationJobList* inner_function_jobs) {
  // Internalize ast values onto the heap.
  parse_info->ast_value_factory()->Internalize(isolate);

  // Create shared function infos for top level and shared function infos array
  // for inner functions.
  EnsureSharedFunctionInfosArrayOnScript(parse_info, isolate);
  DCHECK_EQ(kNoSourcePosition,
            parse_info->literal()->function_token_position());
  Handle<SharedFunctionInfo> shared_info =
      isolate->factory()->NewSharedFunctionInfoForLiteral(
          parse_info->literal(), parse_info->script(), true);

  // Finalize compilation of the unoptimized bytecode or asm-js data.
  if (!FinalizeUnoptimizedCode(parse_info, isolate, shared_info,
                               outer_function_job, inner_function_jobs)) {
    FailWithPendingException(isolate, parse_info,
                             Compiler::ClearExceptionFlag::KEEP_EXCEPTION);
    return MaybeHandle<SharedFunctionInfo>();
  }

  FinalizeScriptCompilation(isolate, parse_info);

  return shared_info;
}

MaybeHandle<SharedFunctionInfo> CompileToplevel(
    ParseInfo* parse_info, Isolate* isolate,
    IsCompiledScope* is_compiled_scope) {
  TimerEventScope<TimerEventCompileCode> top_level_timer(isolate);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.CompileCode");
  DCHECK_EQ(ThreadId::Current(), isolate->thread_id());

  PostponeInterruptsScope postpone(isolate);
  DCHECK(!isolate->native_context().is_null());
  RuntimeCallTimerScope runtimeTimer(
      isolate, parse_info->is_eval() ? RuntimeCallCounterId::kCompileEval
                                     : RuntimeCallCounterId::kCompileScript);
  VMState<BYTECODE_COMPILER> state(isolate);
  if (parse_info->literal() == nullptr &&
      !parsing::ParseProgram(parse_info, isolate)) {
    return MaybeHandle<SharedFunctionInfo>();
  }
  // Measure how long it takes to do the compilation; only take the
  // rest of the function into account to avoid overlap with the
  // parsing statistics.
  HistogramTimer* rate = parse_info->is_eval()
                             ? isolate->counters()->compile_eval()
                             : isolate->counters()->compile();
  HistogramTimerScope timer(rate);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               parse_info->is_eval() ? "V8.CompileEval" : "V8.Compile");

  // Generate the unoptimized bytecode or asm-js data.
  MaybeHandle<SharedFunctionInfo> shared_info =
      GenerateUnoptimizedCodeForToplevel(
          isolate, parse_info, isolate->allocator(), is_compiled_scope);
  if (shared_info.is_null()) {
    FailWithPendingException(isolate, parse_info,
                             Compiler::ClearExceptionFlag::KEEP_EXCEPTION);
    return MaybeHandle<SharedFunctionInfo>();
  }

  FinalizeScriptCompilation(isolate, parse_info);
  return shared_info;
}

std::unique_ptr<UnoptimizedCompilationJob> CompileOnBackgroundThread(
    ParseInfo* parse_info, AccountingAllocator* allocator,
    UnoptimizedCompilationJobList* inner_function_jobs) {
  DisallowHeapAccess no_heap_access;
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.CompileCodeBackground");
  RuntimeCallTimerScope runtimeTimer(
      parse_info->runtime_call_stats(),
      parse_info->is_toplevel()
          ? parse_info->is_eval()
                ? RuntimeCallCounterId::kCompileBackgroundEval
                : RuntimeCallCounterId::kCompileBackgroundScript
          : RuntimeCallCounterId::kCompileBackgroundFunction);

  // Generate the unoptimized bytecode or asm-js data.
  std::unique_ptr<UnoptimizedCompilationJob> outer_function_job(
      GenerateUnoptimizedCode(parse_info, allocator, inner_function_jobs));
  return outer_function_job;
}

}  // namespace

BackgroundCompileTask::BackgroundCompileTask(ScriptStreamingData* streamed_data,
                                             Isolate* isolate)
    : info_(new ParseInfo(isolate)),
      stack_size_(i::FLAG_stack_size),
      worker_thread_runtime_call_stats_(
          isolate->counters()->worker_thread_runtime_call_stats()),
      allocator_(isolate->allocator()),
      timer_(isolate->counters()->compile_script_on_background()) {
  VMState<PARSER> state(isolate);

  // Prepare the data for the internalization phase and compilation phase, which
  // will happen in the main thread after parsing.
  LOG(isolate, ScriptEvent(Logger::ScriptEventType::kStreamingCompile,
                           info_->script_id()));
  info_->set_toplevel();
  info_->set_allow_lazy_parsing();
  if (V8_UNLIKELY(info_->block_coverage_enabled())) {
    info_->AllocateSourceRangeMap();
  }
  LanguageMode language_mode = construct_language_mode(FLAG_use_strict);
  info_->set_language_mode(
      stricter_language_mode(info_->language_mode(), language_mode));

  std::unique_ptr<Utf16CharacterStream> stream(ScannerStream::For(
      streamed_data->source_stream.get(), streamed_data->encoding));
  info_->set_character_stream(std::move(stream));
}

BackgroundCompileTask::BackgroundCompileTask(
    AccountingAllocator* allocator, const ParseInfo* outer_parse_info,
    const AstRawString* function_name, const FunctionLiteral* function_literal,
    WorkerThreadRuntimeCallStats* worker_thread_runtime_stats,
    TimedHistogram* timer, int max_stack_size)
    : info_(ParseInfo::FromParent(outer_parse_info, allocator, function_literal,
                                  function_name)),
      stack_size_(max_stack_size),
      worker_thread_runtime_call_stats_(worker_thread_runtime_stats),
      allocator_(allocator),
      timer_(timer) {
  DCHECK(outer_parse_info->is_toplevel());
  DCHECK(!function_literal->is_toplevel());

  // Clone the character stream so both can be accessed independently.
  std::unique_ptr<Utf16CharacterStream> character_stream =
      outer_parse_info->character_stream()->Clone();
  character_stream->Seek(function_literal->start_position());
  info_->set_character_stream(std::move(character_stream));

  // Get preparsed scope data from the function literal.
  if (function_literal->produced_preparse_data()) {
    ZonePreparseData* serialized_data =
        function_literal->produced_preparse_data()->Serialize(info_->zone());
    info_->set_consumed_preparse_data(
        ConsumedPreparseData::For(info_->zone(), serialized_data));
  }
}

BackgroundCompileTask::~BackgroundCompileTask() = default;

namespace {

// A scope object that ensures a parse info's runtime call stats, stack limit
// and on_background_thread fields is set correctly during worker-thread
// compile, and restores it after going out of scope.
class OffThreadParseInfoScope {
 public:
  OffThreadParseInfoScope(
      ParseInfo* parse_info,
      WorkerThreadRuntimeCallStats* worker_thread_runtime_stats, int stack_size)
      : parse_info_(parse_info),
        original_runtime_call_stats_(parse_info_->runtime_call_stats()),
        original_stack_limit_(parse_info_->stack_limit()),
        worker_thread_scope_(worker_thread_runtime_stats) {
    parse_info_->set_on_background_thread(true);
    parse_info_->set_runtime_call_stats(worker_thread_scope_.Get());
    parse_info_->set_stack_limit(GetCurrentStackPosition() - stack_size * KB);
  }

  ~OffThreadParseInfoScope() {
    parse_info_->set_stack_limit(original_stack_limit_);
    parse_info_->set_runtime_call_stats(original_runtime_call_stats_);
    parse_info_->set_on_background_thread(false);
  }

 private:
  ParseInfo* parse_info_;
  RuntimeCallStats* original_runtime_call_stats_;
  uintptr_t original_stack_limit_;
  WorkerThreadRuntimeCallStatsScope worker_thread_scope_;

  DISALLOW_COPY_AND_ASSIGN(OffThreadParseInfoScope);
};

}  // namespace

void BackgroundCompileTask::Run() {
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHeapAccess no_heap_access;

  TimedHistogramScope timer(timer_);
  OffThreadParseInfoScope off_thread_scope(
      info_.get(), worker_thread_runtime_call_stats_, stack_size_);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "BackgroundCompileTask::Run");
  RuntimeCallTimerScope runtimeTimer(
      info_->runtime_call_stats(),
      RuntimeCallCounterId::kCompileBackgroundCompileTask);

  // Update the character stream's runtime call stats.
  info_->character_stream()->set_runtime_call_stats(
      info_->runtime_call_stats());

  // Parser needs to stay alive for finalizing the parsing on the main
  // thread.
  parser_.reset(new Parser(info_.get()));
  parser_->InitializeEmptyScopeChain(info_.get());

  parser_->ParseOnBackground(info_.get());
  if (info_->literal() != nullptr) {
    // Parsing has succeeded, compile.
    outer_function_job_ = CompileOnBackgroundThread(info_.get(), allocator_,
                                                    &inner_function_jobs_);
  }
}

// ----------------------------------------------------------------------------
// Implementation of Compiler

bool Compiler::Analyze(ParseInfo* parse_info) {
  DCHECK_NOT_NULL(parse_info->literal());
  RuntimeCallTimerScope runtimeTimer(
      parse_info->runtime_call_stats(),
      parse_info->on_background_thread()
          ? RuntimeCallCounterId::kCompileBackgroundAnalyse
          : RuntimeCallCounterId::kCompileAnalyse);
  if (!Rewriter::Rewrite(parse_info)) return false;
  if (!DeclarationScope::Analyze(parse_info)) return false;
  return true;
}

bool Compiler::ParseAndAnalyze(ParseInfo* parse_info,
                               Handle<SharedFunctionInfo> shared_info,
                               Isolate* isolate) {
  if (!parsing::ParseAny(parse_info, shared_info, isolate)) {
    return false;
  }
  return Compiler::Analyze(parse_info);
}

// static
bool Compiler::CollectSourcePositions(Isolate* isolate,
                                      Handle<SharedFunctionInfo> shared_info) {
  DCHECK(shared_info->is_compiled());
  DCHECK(shared_info->HasBytecodeArray());
  DCHECK(!shared_info->GetBytecodeArray().HasSourcePositionTable());

  // Source position collection should be context independent.
  NullContextScope null_context_scope(isolate);

  // Collecting source positions requires allocating a new source position
  // table.
  DCHECK(AllowHeapAllocation::IsAllowed());

  Handle<BytecodeArray> bytecode =
      handle(shared_info->GetBytecodeArray(), isolate);

  // TODO(v8:8510): Push the CLEAR_EXCEPTION flag or something like it down into
  // the parser so it aborts without setting a pending exception, which then
  // gets thrown. This would avoid the situation where potentially we'd reparse
  // several times (running out of stack each time) before hitting this limit.
  if (GetCurrentStackPosition() < isolate->stack_guard()->real_climit()) {
    // Stack is already exhausted.
    bytecode->SetSourcePositionsFailedToCollect();
    return false;
  }

  DCHECK(AllowCompilation::IsAllowed(isolate));
  DCHECK_EQ(ThreadId::Current(), isolate->thread_id());
  DCHECK(!isolate->has_pending_exception());
  VMState<BYTECODE_COMPILER> state(isolate);
  PostponeInterruptsScope postpone(isolate);
  RuntimeCallTimerScope runtimeTimer(
      isolate, RuntimeCallCounterId::kCompileCollectSourcePositions);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.CollectSourcePositions");
  HistogramTimerScope timer(isolate->counters()->collect_source_positions());

  // Set up parse info.
  ParseInfo parse_info(isolate, shared_info);
  parse_info.set_lazy_compile();
  parse_info.set_collect_source_positions();
  if (FLAG_allow_natives_syntax) parse_info.set_allow_natives_syntax();

  // Parse and update ParseInfo with the results. Don't update parsing
  // statistics since we've already parsed the code before.
  if (!parsing::ParseAny(&parse_info, shared_info, isolate,
                         parsing::ReportErrorsAndStatisticsMode::kNo)) {
    // Parsing failed probably as a result of stack exhaustion.
    bytecode->SetSourcePositionsFailedToCollect();
    return FailWithPendingException(
        isolate, &parse_info, Compiler::ClearExceptionFlag::CLEAR_EXCEPTION);
  }

  // Character stream shouldn't be used again.
  parse_info.ResetCharacterStream();

  // Generate the unoptimized bytecode.
  // TODO(v8:8510): Consider forcing preparsing of inner functions to avoid
  // wasting time fully parsing them when they won't ever be used.
  std::unique_ptr<UnoptimizedCompilationJob> job;
  {
    if (!Compiler::Analyze(&parse_info)) {
      // Recompiling failed probably as a result of stack exhaustion.
      bytecode->SetSourcePositionsFailedToCollect();
      return FailWithPendingException(
          isolate, &parse_info, Compiler::ClearExceptionFlag::CLEAR_EXCEPTION);
    }

    job = interpreter::Interpreter::NewSourcePositionCollectionJob(
        &parse_info, parse_info.literal(), bytecode, isolate->allocator());

    if (!job || job->ExecuteJob() != CompilationJob::SUCCEEDED ||
        job->FinalizeJob(shared_info, isolate) != CompilationJob::SUCCEEDED) {
      // Recompiling failed probably as a result of stack exhaustion.
      bytecode->SetSourcePositionsFailedToCollect();
      return FailWithPendingException(
          isolate, &parse_info, Compiler::ClearExceptionFlag::CLEAR_EXCEPTION);
    }
  }

  DCHECK(job->compilation_info()->collect_source_positions());

  // If debugging, make sure that instrumented bytecode has the source position
  // table set on it as well.
  if (shared_info->HasDebugInfo() &&
      shared_info->GetDebugInfo().HasInstrumentedBytecodeArray()) {
    ByteArray source_position_table =
        job->compilation_info()->bytecode_array()->SourcePositionTable();
    shared_info->GetDebugBytecodeArray().set_source_position_table(
        source_position_table);
  }

  DCHECK(!isolate->has_pending_exception());
  DCHECK(shared_info->is_compiled_scope().is_compiled());
  return true;
}

bool Compiler::Compile(Handle<SharedFunctionInfo> shared_info,
                       ClearExceptionFlag flag,
                       IsCompiledScope* is_compiled_scope) {
  // We should never reach here if the function is already compiled.
  DCHECK(!shared_info->is_compiled());
  DCHECK(!is_compiled_scope->is_compiled());

  Isolate* isolate = shared_info->GetIsolate();
  DCHECK(AllowCompilation::IsAllowed(isolate));
  DCHECK_EQ(ThreadId::Current(), isolate->thread_id());
  DCHECK(!isolate->has_pending_exception());
  DCHECK(!shared_info->HasBytecodeArray());
  VMState<BYTECODE_COMPILER> state(isolate);
  PostponeInterruptsScope postpone(isolate);
  TimerEventScope<TimerEventCompileCode> compile_timer(isolate);
  RuntimeCallTimerScope runtimeTimer(isolate,
                                     RuntimeCallCounterId::kCompileFunction);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.CompileCode");
  AggregatedHistogramTimerScope timer(isolate->counters()->compile_lazy());

  // Set up parse info.
  ParseInfo parse_info(isolate, shared_info);
  parse_info.set_lazy_compile();

  // Check if the compiler dispatcher has shared_info enqueued for compile.
  CompilerDispatcher* dispatcher = isolate->compiler_dispatcher();
  if (dispatcher->IsEnqueued(shared_info)) {
    if (!dispatcher->FinishNow(shared_info)) {
      return FailWithPendingException(isolate, &parse_info, flag);
    }
    *is_compiled_scope = shared_info->is_compiled_scope();
    DCHECK(is_compiled_scope->is_compiled());
    return true;
  }

  if (shared_info->HasUncompiledDataWithPreparseData()) {
    parse_info.set_consumed_preparse_data(ConsumedPreparseData::For(
        isolate,
        handle(
            shared_info->uncompiled_data_with_preparse_data().preparse_data(),
            isolate)));
  }

  // Parse and update ParseInfo with the results.
  if (!parsing::ParseAny(&parse_info, shared_info, isolate)) {
    return FailWithPendingException(isolate, &parse_info, flag);
  }

  // Generate the unoptimized bytecode or asm-js data.
  UnoptimizedCompilationJobList inner_function_jobs;
  std::unique_ptr<UnoptimizedCompilationJob> outer_function_job(
      GenerateUnoptimizedCode(&parse_info, isolate->allocator(),
                              &inner_function_jobs));
  if (!outer_function_job) {
    return FailWithPendingException(isolate, &parse_info, flag);
  }

  // Internalize ast values onto the heap.
  parse_info.ast_value_factory()->Internalize(isolate);

  // Finalize compilation of the unoptimized bytecode or asm-js data.
  if (!FinalizeUnoptimizedCode(&parse_info, isolate, shared_info,
                               outer_function_job.get(),
                               &inner_function_jobs)) {
    return FailWithPendingException(isolate, &parse_info, flag);
  }

  DCHECK(!isolate->has_pending_exception());
  *is_compiled_scope = shared_info->is_compiled_scope();
  DCHECK(is_compiled_scope->is_compiled());

  if (FLAG_stress_lazy_source_positions) {
    // Collect source positions immediately to try and flush out bytecode
    // mismatches.
    SharedFunctionInfo::EnsureSourcePositionsAvailable(isolate, shared_info);

    // Do the same for eagerly compiled inner functions.
    for (auto&& inner_job : inner_function_jobs) {
      Handle<SharedFunctionInfo> inner_shared_info =
          Compiler::GetSharedFunctionInfo(
              inner_job->compilation_info()->literal(), parse_info.script(),
              isolate);
      SharedFunctionInfo::EnsureSourcePositionsAvailable(isolate,
                                                         inner_shared_info);
    }
  }

  return true;
}

bool Compiler::Compile(Handle<JSFunction> function, ClearExceptionFlag flag,
                       IsCompiledScope* is_compiled_scope) {
  // We should never reach here if the function is already compiled or optimized
  DCHECK(!function->is_compiled());
  DCHECK(!function->IsOptimized());
  DCHECK(!function->HasOptimizationMarker());
  DCHECK(!function->HasOptimizedCode());

  // Reset the JSFunction if we are recompiling due to the bytecode having been
  // flushed.
  function->ResetIfBytecodeFlushed();

  Isolate* isolate = function->GetIsolate();
  Handle<SharedFunctionInfo> shared_info = handle(function->shared(), isolate);

  // Ensure shared function info is compiled.
  *is_compiled_scope = shared_info->is_compiled_scope();
  if (!is_compiled_scope->is_compiled() &&
      !Compile(shared_info, flag, is_compiled_scope)) {
    return false;
  }
  DCHECK(is_compiled_scope->is_compiled());
  Handle<Code> code = handle(shared_info->GetCode(), isolate);

  // Initialize the feedback cell for this JSFunction.
  JSFunction::InitializeFeedbackCell(function);

  // Optimize now if --always-opt is enabled.
  if (FLAG_always_opt && !function->shared().HasAsmWasmData()) {
    if (FLAG_trace_opt) {
      PrintF("[optimizing ");
      function->ShortPrint();
      PrintF(" because --always-opt]\n");
    }
    Handle<Code> opt_code;
    if (GetOptimizedCode(function, ConcurrencyMode::kNotConcurrent)
            .ToHandle(&opt_code)) {
      code = opt_code;
    }
  }

  // Install code on closure.
  function->set_code(*code);

  // Check postconditions on success.
  DCHECK(!isolate->has_pending_exception());
  DCHECK(function->shared().is_compiled());
  DCHECK(function->is_compiled());
  return true;
}

bool Compiler::FinalizeBackgroundCompileTask(
    BackgroundCompileTask* task, Handle<SharedFunctionInfo> shared_info,
    Isolate* isolate, ClearExceptionFlag flag) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.FinalizeBackgroundCompileTask");
  RuntimeCallTimerScope runtimeTimer(
      isolate, RuntimeCallCounterId::kCompileFinalizeBackgroundCompileTask);
  HandleScope scope(isolate);
  ParseInfo* parse_info = task->info();
  DCHECK(!parse_info->is_toplevel());
  DCHECK(!shared_info->is_compiled());

  Handle<Script> script(Script::cast(shared_info->script()), isolate);
  parse_info->set_script(script);

  task->parser()->UpdateStatistics(isolate, script);
  task->parser()->HandleSourceURLComments(isolate, script);

  if (parse_info->literal() == nullptr || !task->outer_function_job()) {
    // Parsing or compile failed on background thread - report error messages.
    return FailWithPendingException(isolate, parse_info, flag);
  }

  // Parsing has succeeded - finalize compilation.
  parse_info->ast_value_factory()->Internalize(isolate);
  if (!FinalizeUnoptimizedCode(parse_info, isolate, shared_info,
                               task->outer_function_job(),
                               task->inner_function_jobs())) {
    // Finalization failed - throw an exception.
    return FailWithPendingException(isolate, parse_info, flag);
  }

  DCHECK(!isolate->has_pending_exception());
  DCHECK(shared_info->is_compiled());
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
    DCHECK(function->shared().is_compiled());
    DCHECK(function->shared().IsInterpreted());
    code = BUILTIN_CODE(isolate, InterpreterEntryTrampoline);
  }

  // Install code on closure.
  function->set_code(*code);

  // Check postconditions on success.
  DCHECK(!isolate->has_pending_exception());
  DCHECK(function->shared().is_compiled());
  DCHECK(function->is_compiled());
  DCHECK_IMPLIES(function->HasOptimizationMarker(),
                 function->IsInOptimizationQueue());
  DCHECK_IMPLIES(function->HasOptimizationMarker(),
                 function->ChecksOptimizationMarker());
  DCHECK_IMPLIES(function->IsInOptimizationQueue(),
                 mode == ConcurrencyMode::kConcurrent);
  return true;
}

MaybeHandle<SharedFunctionInfo> Compiler::CompileForLiveEdit(
    ParseInfo* parse_info, Isolate* isolate) {
  IsCompiledScope is_compiled_scope;
  return CompileToplevel(parse_info, isolate, &is_compiled_scope);
}

MaybeHandle<JSFunction> Compiler::GetFunctionFromEval(
    Handle<String> source, Handle<SharedFunctionInfo> outer_info,
    Handle<Context> context, LanguageMode language_mode,
    ParseRestriction restriction, int parameters_end_pos,
    int eval_scope_position, int eval_position) {
  Isolate* isolate = context->GetIsolate();
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
  if (restriction == ONLY_SINGLE_FUNCTION_LITERAL &&
      parameters_end_pos != kNoSourcePosition) {
    // use the parameters_end_pos as the eval_scope_position in the eval cache.
    DCHECK_EQ(eval_scope_position, 0);
    eval_scope_position = -parameters_end_pos;
  }
  CompilationCache* compilation_cache = isolate->compilation_cache();
  InfoCellPair eval_result = compilation_cache->LookupEval(
      source, outer_info, context, language_mode, eval_scope_position);
  Handle<FeedbackCell> feedback_cell;
  if (eval_result.has_feedback_cell()) {
    feedback_cell = handle(eval_result.feedback_cell(), isolate);
  }

  Handle<SharedFunctionInfo> shared_info;
  Handle<Script> script;
  IsCompiledScope is_compiled_scope;
  bool allow_eval_cache;
  if (eval_result.has_shared()) {
    shared_info = Handle<SharedFunctionInfo>(eval_result.shared(), isolate);
    script = Handle<Script>(Script::cast(shared_info->script()), isolate);
    is_compiled_scope = shared_info->is_compiled_scope();
    allow_eval_cache = true;
  } else {
    ParseInfo parse_info(isolate);
    script = parse_info.CreateScript(
        isolate, source, OriginOptionsForEval(outer_info->script()));
    script->set_compilation_type(Script::COMPILATION_TYPE_EVAL);
    script->set_eval_from_shared(*outer_info);
    if (eval_position == kNoSourcePosition) {
      // If the position is missing, attempt to get the code offset by
      // walking the stack. Do not translate the code offset into source
      // position, but store it as negative value for lazy translation.
      StackTraceFrameIterator it(isolate);
      if (!it.done() && it.is_javascript()) {
        FrameSummary summary = FrameSummary::GetTop(it.javascript_frame());
        script->set_eval_from_shared(
            summary.AsJavaScript().function()->shared());
        script->set_origin_options(OriginOptionsForEval(*summary.script()));
        eval_position = -summary.code_offset();
      } else {
        eval_position = 0;
      }
    }
    script->set_eval_from_position(eval_position);
    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
        TRACE_DISABLED_BY_DEFAULT("v8.compile"), "Script",
        TRACE_ID_WITH_SCOPE(Script::kTraceScope, script->id()),
        script->ToTracedValue());

    parse_info.set_eval();
    parse_info.set_language_mode(language_mode);
    parse_info.set_parse_restriction(restriction);
    parse_info.set_parameters_end_pos(parameters_end_pos);
    if (!context->IsNativeContext()) {
      parse_info.set_outer_scope_info(handle(context->scope_info(), isolate));
    }
    DCHECK(!parse_info.is_module());

    if (!CompileToplevel(&parse_info, isolate, &is_compiled_scope)
             .ToHandle(&shared_info)) {
      return MaybeHandle<JSFunction>();
    }
    allow_eval_cache = parse_info.allow_eval_cache();
  }

  // If caller is strict mode, the result must be in strict mode as well.
  DCHECK(is_sloppy(language_mode) || is_strict(shared_info->language_mode()));

  Handle<JSFunction> result;
  if (eval_result.has_shared()) {
    if (eval_result.has_feedback_cell()) {
      result = isolate->factory()->NewFunctionFromSharedFunctionInfo(
          shared_info, context, feedback_cell, AllocationType::kYoung);
    } else {
      result = isolate->factory()->NewFunctionFromSharedFunctionInfo(
          shared_info, context, AllocationType::kYoung);
      JSFunction::InitializeFeedbackCell(result);
      if (allow_eval_cache) {
        // Make sure to cache this result.
        Handle<FeedbackCell> new_feedback_cell(result->raw_feedback_cell(),
                                               isolate);
        compilation_cache->PutEval(source, outer_info, context, shared_info,
                                   new_feedback_cell, eval_scope_position);
      }
    }
  } else {
    result = isolate->factory()->NewFunctionFromSharedFunctionInfo(
        shared_info, context, AllocationType::kYoung);
    JSFunction::InitializeFeedbackCell(result);
    if (allow_eval_cache) {
      // Add the SharedFunctionInfo and the LiteralsArray to the eval cache if
      // we didn't retrieve from there.
      Handle<FeedbackCell> new_feedback_cell(result->raw_feedback_cell(),
                                             isolate);
      compilation_cache->PutEval(source, outer_info, context, shared_info,
                                 new_feedback_cell, eval_scope_position);
    }
  }
  DCHECK(is_compiled_scope.is_compiled());

  return result;
}

// Check whether embedder allows code generation in this context.
// (via v8::Isolate::SetAllowCodeGenerationFromStringsCallback)
bool CodeGenerationFromStringsAllowed(Isolate* isolate, Handle<Context> context,
                                      Handle<String> source) {
  DCHECK(context->allow_code_gen_from_strings().IsFalse(isolate));
  DCHECK(isolate->allow_code_gen_callback());

  // Callback set. Let it decide if code generation is allowed.
  VMState<EXTERNAL> state(isolate);
  RuntimeCallTimerScope timer(
      isolate, RuntimeCallCounterId::kCodeGenerationFromStringsCallbacks);
  AllowCodeGenerationFromStringsCallback callback =
      isolate->allow_code_gen_callback();
  return callback(v8::Utils::ToLocal(context), v8::Utils::ToLocal(source));
}

// Check whether embedder allows code generation in this context.
// (via v8::Isolate::SetModifyCodeGenerationFromStringsCallback)
bool ModifyCodeGenerationFromStrings(Isolate* isolate, Handle<Context> context,
                                     Handle<i::Object>* source) {
  DCHECK(context->allow_code_gen_from_strings().IsFalse(isolate));
  DCHECK(isolate->modify_code_gen_callback());
  DCHECK(source);

  // Callback set. Run it, and use the return value as source, or block
  // execution if it's not set.
  VMState<EXTERNAL> state(isolate);
  ModifyCodeGenerationFromStringsCallback modify_callback =
      isolate->modify_code_gen_callback();
  RuntimeCallTimerScope timer(
      isolate, RuntimeCallCounterId::kCodeGenerationFromStringsCallbacks);
  MaybeLocal<v8::String> modified_source =
      modify_callback(v8::Utils::ToLocal(context), v8::Utils::ToLocal(*source));
  if (modified_source.IsEmpty()) return false;

  // Use the new source (which might be the same as the old source) and return.
  *source = Utils::OpenHandle(*modified_source.ToLocalChecked(), false);
  return true;
}

// Run Embedder-mandated checks before generating code from a string.
//
// Returns a string to be used for compilation, or a flag that an object type
// was encountered that is neither a string, nor something the embedder knows
// how to handle.
//
// Returns: (assuming: std::tie(source, unknown_object))
// - !source.is_null(): compilation allowed, source contains the source string.
// - unknown_object is true: compilation allowed, but we don't know how to
//                           deal with source_object.
// - source.is_null() && !unknown_object: compilation should be blocked.
//
// - !source_is_null() and unknown_object can't be true at the same time.
std::pair<MaybeHandle<String>, bool> Compiler::ValidateDynamicCompilationSource(
    Isolate* isolate, Handle<Context> context,
    Handle<i::Object> source_object) {
  Handle<String> source;
  if (source_object->IsString()) source = Handle<String>::cast(source_object);

  // Check if the context unconditionally allows code gen from strings.
  // allow_code_gen_from_strings can be many things, so we'll always check
  // against the 'false' literal, so that e.g. undefined and 'true' are treated
  // the same.
  if (!context->allow_code_gen_from_strings().IsFalse(isolate)) {
    return {source, !source_object->IsString()};
  }

  // Check if the context allows code generation for this string.
  // allow_code_gen_callback only allows proper strings.
  // (I.e., let allow_code_gen_callback decide, if it has been set.)
  if (isolate->allow_code_gen_callback()) {
    if (source_object->IsString() &&
        CodeGenerationFromStringsAllowed(isolate, context, source)) {
      return {source, !source_object->IsString()};
    }
  }

  // Check if the context wants to block or modify this source object.
  // Double-check that we really have a string now.
  // (Let modify_code_gen_callback decide, if it's been set.)
  if (isolate->modify_code_gen_callback()) {
    if (ModifyCodeGenerationFromStrings(isolate, context, &source_object) &&
        source_object->IsString())
      return {Handle<String>::cast(source_object), false};
  }

  return {MaybeHandle<String>(), !source_object->IsString()};
}

MaybeHandle<JSFunction> Compiler::GetFunctionFromValidatedString(
    Handle<Context> context, MaybeHandle<String> source,
    ParseRestriction restriction, int parameters_end_pos) {
  Isolate* const isolate = context->GetIsolate();
  Handle<Context> native_context(context->native_context(), isolate);

  // Raise an EvalError if we did not receive a string.
  if (source.is_null()) {
    Handle<Object> error_message =
        native_context->ErrorMessageForCodeGenerationFromStrings();
    THROW_NEW_ERROR(
        isolate,
        NewEvalError(MessageTemplate::kCodeGenFromStrings, error_message),
        JSFunction);
  }

  // Compile source string in the native context.
  int eval_scope_position = 0;
  int eval_position = kNoSourcePosition;
  Handle<SharedFunctionInfo> outer_info(
      native_context->empty_function().shared(), isolate);
  return Compiler::GetFunctionFromEval(source.ToHandleChecked(), outer_info,
                                       native_context, LanguageMode::kSloppy,
                                       restriction, parameters_end_pos,
                                       eval_scope_position, eval_position);
}

MaybeHandle<JSFunction> Compiler::GetFunctionFromString(
    Handle<Context> context, Handle<Object> source,
    ParseRestriction restriction, int parameters_end_pos) {
  Isolate* const isolate = context->GetIsolate();
  Handle<Context> native_context(context->native_context(), isolate);
  return GetFunctionFromValidatedString(
      context, ValidateDynamicCompilationSource(isolate, context, source).first,
      restriction, parameters_end_pos);
}

namespace {

struct ScriptCompileTimerScope {
 public:
  // TODO(leszeks): There are too many blink-specific entries in this enum,
  // figure out a way to push produce/hit-isolate-cache/consume/consume-failed
  // back up the API and log them in blink instead.
  enum class CacheBehaviour {
    kProduceCodeCache,
    kHitIsolateCacheWhenNoCache,
    kConsumeCodeCache,
    kConsumeCodeCacheFailed,
    kNoCacheBecauseInlineScript,
    kNoCacheBecauseScriptTooSmall,
    kNoCacheBecauseCacheTooCold,
    kNoCacheNoReason,
    kNoCacheBecauseNoResource,
    kNoCacheBecauseInspector,
    kNoCacheBecauseCachingDisabled,
    kNoCacheBecauseModule,
    kNoCacheBecauseStreamingSource,
    kNoCacheBecauseV8Extension,
    kHitIsolateCacheWhenProduceCodeCache,
    kHitIsolateCacheWhenConsumeCodeCache,
    kNoCacheBecauseExtensionModule,
    kNoCacheBecausePacScript,
    kNoCacheBecauseInDocumentWrite,
    kNoCacheBecauseResourceWithNoCacheHandler,
    kHitIsolateCacheWhenStreamingSource,
    kCount
  };

  explicit ScriptCompileTimerScope(
      Isolate* isolate, ScriptCompiler::NoCacheReason no_cache_reason)
      : isolate_(isolate),
        all_scripts_histogram_scope_(isolate->counters()->compile_script(),
                                     true),
        no_cache_reason_(no_cache_reason),
        hit_isolate_cache_(false),
        producing_code_cache_(false),
        consuming_code_cache_(false),
        consuming_code_cache_failed_(false) {}

  ~ScriptCompileTimerScope() {
    CacheBehaviour cache_behaviour = GetCacheBehaviour();

    Histogram* cache_behaviour_histogram =
        isolate_->counters()->compile_script_cache_behaviour();
    // Sanity check that the histogram has exactly one bin per enum entry.
    DCHECK_EQ(0, cache_behaviour_histogram->min());
    DCHECK_EQ(static_cast<int>(CacheBehaviour::kCount),
              cache_behaviour_histogram->max() + 1);
    DCHECK_EQ(static_cast<int>(CacheBehaviour::kCount),
              cache_behaviour_histogram->num_buckets());
    cache_behaviour_histogram->AddSample(static_cast<int>(cache_behaviour));

    histogram_scope_.set_histogram(
        GetCacheBehaviourTimedHistogram(cache_behaviour));
  }

  void set_hit_isolate_cache() { hit_isolate_cache_ = true; }

  void set_producing_code_cache() { producing_code_cache_ = true; }

  void set_consuming_code_cache() { consuming_code_cache_ = true; }

  void set_consuming_code_cache_failed() {
    consuming_code_cache_failed_ = true;
  }

 private:
  Isolate* isolate_;
  LazyTimedHistogramScope histogram_scope_;
  // TODO(leszeks): This timer is the sum of the other times, consider removing
  // it to save space.
  HistogramTimerScope all_scripts_histogram_scope_;
  ScriptCompiler::NoCacheReason no_cache_reason_;
  bool hit_isolate_cache_;
  bool producing_code_cache_;
  bool consuming_code_cache_;
  bool consuming_code_cache_failed_;

  CacheBehaviour GetCacheBehaviour() {
    if (producing_code_cache_) {
      if (hit_isolate_cache_) {
        return CacheBehaviour::kHitIsolateCacheWhenProduceCodeCache;
      } else {
        return CacheBehaviour::kProduceCodeCache;
      }
    }

    if (consuming_code_cache_) {
      if (hit_isolate_cache_) {
        return CacheBehaviour::kHitIsolateCacheWhenConsumeCodeCache;
      } else if (consuming_code_cache_failed_) {
        return CacheBehaviour::kConsumeCodeCacheFailed;
      }
      return CacheBehaviour::kConsumeCodeCache;
    }

    if (hit_isolate_cache_) {
      if (no_cache_reason_ == ScriptCompiler::kNoCacheBecauseStreamingSource) {
        return CacheBehaviour::kHitIsolateCacheWhenStreamingSource;
      }
      return CacheBehaviour::kHitIsolateCacheWhenNoCache;
    }

    switch (no_cache_reason_) {
      case ScriptCompiler::kNoCacheBecauseInlineScript:
        return CacheBehaviour::kNoCacheBecauseInlineScript;
      case ScriptCompiler::kNoCacheBecauseScriptTooSmall:
        return CacheBehaviour::kNoCacheBecauseScriptTooSmall;
      case ScriptCompiler::kNoCacheBecauseCacheTooCold:
        return CacheBehaviour::kNoCacheBecauseCacheTooCold;
      case ScriptCompiler::kNoCacheNoReason:
        return CacheBehaviour::kNoCacheNoReason;
      case ScriptCompiler::kNoCacheBecauseNoResource:
        return CacheBehaviour::kNoCacheBecauseNoResource;
      case ScriptCompiler::kNoCacheBecauseInspector:
        return CacheBehaviour::kNoCacheBecauseInspector;
      case ScriptCompiler::kNoCacheBecauseCachingDisabled:
        return CacheBehaviour::kNoCacheBecauseCachingDisabled;
      case ScriptCompiler::kNoCacheBecauseModule:
        return CacheBehaviour::kNoCacheBecauseModule;
      case ScriptCompiler::kNoCacheBecauseStreamingSource:
        return CacheBehaviour::kNoCacheBecauseStreamingSource;
      case ScriptCompiler::kNoCacheBecauseV8Extension:
        return CacheBehaviour::kNoCacheBecauseV8Extension;
      case ScriptCompiler::kNoCacheBecauseExtensionModule:
        return CacheBehaviour::kNoCacheBecauseExtensionModule;
      case ScriptCompiler::kNoCacheBecausePacScript:
        return CacheBehaviour::kNoCacheBecausePacScript;
      case ScriptCompiler::kNoCacheBecauseInDocumentWrite:
        return CacheBehaviour::kNoCacheBecauseInDocumentWrite;
      case ScriptCompiler::kNoCacheBecauseResourceWithNoCacheHandler:
        return CacheBehaviour::kNoCacheBecauseResourceWithNoCacheHandler;
      case ScriptCompiler::kNoCacheBecauseDeferredProduceCodeCache: {
        if (hit_isolate_cache_) {
          return CacheBehaviour::kHitIsolateCacheWhenProduceCodeCache;
        } else {
          return CacheBehaviour::kProduceCodeCache;
        }
      }
    }
    UNREACHABLE();
  }

  TimedHistogram* GetCacheBehaviourTimedHistogram(
      CacheBehaviour cache_behaviour) {
    switch (cache_behaviour) {
      case CacheBehaviour::kProduceCodeCache:
      // Even if we hit the isolate's compilation cache, we currently recompile
      // when we want to produce the code cache.
      case CacheBehaviour::kHitIsolateCacheWhenProduceCodeCache:
        return isolate_->counters()->compile_script_with_produce_cache();
      case CacheBehaviour::kHitIsolateCacheWhenNoCache:
      case CacheBehaviour::kHitIsolateCacheWhenConsumeCodeCache:
      case CacheBehaviour::kHitIsolateCacheWhenStreamingSource:
        return isolate_->counters()->compile_script_with_isolate_cache_hit();
      case CacheBehaviour::kConsumeCodeCacheFailed:
        return isolate_->counters()->compile_script_consume_failed();
      case CacheBehaviour::kConsumeCodeCache:
        return isolate_->counters()->compile_script_with_consume_cache();

      case CacheBehaviour::kNoCacheBecauseInlineScript:
        return isolate_->counters()
            ->compile_script_no_cache_because_inline_script();
      case CacheBehaviour::kNoCacheBecauseScriptTooSmall:
        return isolate_->counters()
            ->compile_script_no_cache_because_script_too_small();
      case CacheBehaviour::kNoCacheBecauseCacheTooCold:
        return isolate_->counters()
            ->compile_script_no_cache_because_cache_too_cold();

      // Aggregate all the other "no cache" counters into a single histogram, to
      // save space.
      case CacheBehaviour::kNoCacheNoReason:
      case CacheBehaviour::kNoCacheBecauseNoResource:
      case CacheBehaviour::kNoCacheBecauseInspector:
      case CacheBehaviour::kNoCacheBecauseCachingDisabled:
      // TODO(leszeks): Consider counting separately once modules are more
      // common.
      case CacheBehaviour::kNoCacheBecauseModule:
      // TODO(leszeks): Count separately or remove entirely once we have
      // background compilation.
      case CacheBehaviour::kNoCacheBecauseStreamingSource:
      case CacheBehaviour::kNoCacheBecauseV8Extension:
      case CacheBehaviour::kNoCacheBecauseExtensionModule:
      case CacheBehaviour::kNoCacheBecausePacScript:
      case CacheBehaviour::kNoCacheBecauseInDocumentWrite:
      case CacheBehaviour::kNoCacheBecauseResourceWithNoCacheHandler:
        return isolate_->counters()->compile_script_no_cache_other();

      case CacheBehaviour::kCount:
        UNREACHABLE();
    }
    UNREACHABLE();
  }
};

Handle<Script> NewScript(Isolate* isolate, ParseInfo* parse_info,
                         Handle<String> source,
                         Compiler::ScriptDetails script_details,
                         ScriptOriginOptions origin_options,
                         NativesFlag natives) {
  // Create a script object describing the script to be compiled.
  Handle<Script> script =
      parse_info->CreateScript(isolate, source, origin_options, natives);
  Handle<Object> script_name;
  if (script_details.name_obj.ToHandle(&script_name)) {
    script->set_name(*script_name);
    script->set_line_offset(script_details.line_offset);
    script->set_column_offset(script_details.column_offset);
  }
  Handle<Object> source_map_url;
  if (script_details.source_map_url.ToHandle(&source_map_url)) {
    script->set_source_mapping_url(*source_map_url);
  }
  Handle<FixedArray> host_defined_options;
  if (script_details.host_defined_options.ToHandle(&host_defined_options)) {
    script->set_host_defined_options(*host_defined_options);
  }
  LOG(isolate, ScriptDetails(*script));
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("v8.compile"), "Script",
      TRACE_ID_WITH_SCOPE(Script::kTraceScope, script->id()),
      script->ToTracedValue());
  return script;
}

}  // namespace

MaybeHandle<SharedFunctionInfo> Compiler::GetSharedFunctionInfoForScript(
    Isolate* isolate, Handle<String> source,
    const Compiler::ScriptDetails& script_details,
    ScriptOriginOptions origin_options, v8::Extension* extension,
    ScriptData* cached_data, ScriptCompiler::CompileOptions compile_options,
    ScriptCompiler::NoCacheReason no_cache_reason, NativesFlag natives) {
  ScriptCompileTimerScope compile_timer(isolate, no_cache_reason);

  if (compile_options == ScriptCompiler::kNoCompileOptions ||
      compile_options == ScriptCompiler::kEagerCompile) {
    DCHECK_NULL(cached_data);
  } else {
    DCHECK(compile_options == ScriptCompiler::kConsumeCodeCache);
    DCHECK(cached_data);
    DCHECK_NULL(extension);
  }
  int source_length = source->length();
  isolate->counters()->total_load_size()->Increment(source_length);
  isolate->counters()->total_compile_size()->Increment(source_length);

  LanguageMode language_mode = construct_language_mode(FLAG_use_strict);
  CompilationCache* compilation_cache = isolate->compilation_cache();

  // Do a lookup in the compilation cache but not for extensions.
  MaybeHandle<SharedFunctionInfo> maybe_result;
  IsCompiledScope is_compiled_scope;
  if (extension == nullptr) {
    bool can_consume_code_cache =
        compile_options == ScriptCompiler::kConsumeCodeCache;
    if (can_consume_code_cache) {
      compile_timer.set_consuming_code_cache();
    }

    // First check per-isolate compilation cache.
    maybe_result = compilation_cache->LookupScript(
        source, script_details.name_obj, script_details.line_offset,
        script_details.column_offset, origin_options, isolate->native_context(),
        language_mode);
    if (!maybe_result.is_null()) {
      compile_timer.set_hit_isolate_cache();
    } else if (can_consume_code_cache) {
      compile_timer.set_consuming_code_cache();
      // Then check cached code provided by embedder.
      HistogramTimerScope timer(isolate->counters()->compile_deserialize());
      RuntimeCallTimerScope runtimeTimer(
          isolate, RuntimeCallCounterId::kCompileDeserialize);
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                   "V8.CompileDeserialize");
      Handle<SharedFunctionInfo> inner_result;
      if (CodeSerializer::Deserialize(isolate, cached_data, source,
                                      origin_options)
              .ToHandle(&inner_result) &&
          inner_result->is_compiled()) {
        // Promote to per-isolate compilation cache.
        is_compiled_scope = inner_result->is_compiled_scope();
        DCHECK(is_compiled_scope.is_compiled());
        compilation_cache->PutScript(source, isolate->native_context(),
                                     language_mode, inner_result);
        Handle<Script> script(Script::cast(inner_result->script()), isolate);
        maybe_result = inner_result;
      } else {
        // Deserializer failed. Fall through to compile.
        compile_timer.set_consuming_code_cache_failed();
      }
    }
  }

  if (maybe_result.is_null()) {
    ParseInfo parse_info(isolate);
    // No cache entry found compile the script.
    NewScript(isolate, &parse_info, source, script_details, origin_options,
              natives);

    // Compile the function and add it to the isolate cache.
    if (origin_options.IsModule()) parse_info.set_module();
    parse_info.set_extension(extension);
    parse_info.set_eager(compile_options == ScriptCompiler::kEagerCompile);

    parse_info.set_language_mode(
        stricter_language_mode(parse_info.language_mode(), language_mode));
    maybe_result = CompileToplevel(&parse_info, isolate, &is_compiled_scope);
    Handle<SharedFunctionInfo> result;
    if (extension == nullptr && maybe_result.ToHandle(&result)) {
      DCHECK(is_compiled_scope.is_compiled());
      compilation_cache->PutScript(source, isolate->native_context(),
                                   language_mode, result);
    } else if (maybe_result.is_null() && natives != EXTENSION_CODE) {
      isolate->ReportPendingMessages();
    }
  }

  return maybe_result;
}

MaybeHandle<JSFunction> Compiler::GetWrappedFunction(
    Handle<String> source, Handle<FixedArray> arguments,
    Handle<Context> context, const Compiler::ScriptDetails& script_details,
    ScriptOriginOptions origin_options, ScriptData* cached_data,
    v8::ScriptCompiler::CompileOptions compile_options,
    v8::ScriptCompiler::NoCacheReason no_cache_reason) {
  Isolate* isolate = context->GetIsolate();
  ScriptCompileTimerScope compile_timer(isolate, no_cache_reason);

  if (compile_options == ScriptCompiler::kNoCompileOptions ||
      compile_options == ScriptCompiler::kEagerCompile) {
    DCHECK_NULL(cached_data);
  } else {
    DCHECK(compile_options == ScriptCompiler::kConsumeCodeCache);
    DCHECK(cached_data);
  }

  int source_length = source->length();
  isolate->counters()->total_compile_size()->Increment(source_length);

  LanguageMode language_mode = construct_language_mode(FLAG_use_strict);

  MaybeHandle<SharedFunctionInfo> maybe_result;
  bool can_consume_code_cache =
      compile_options == ScriptCompiler::kConsumeCodeCache;
  if (can_consume_code_cache) {
    compile_timer.set_consuming_code_cache();
    // Then check cached code provided by embedder.
    HistogramTimerScope timer(isolate->counters()->compile_deserialize());
    RuntimeCallTimerScope runtimeTimer(
        isolate, RuntimeCallCounterId::kCompileDeserialize);
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                 "V8.CompileDeserialize");
    maybe_result = CodeSerializer::Deserialize(isolate, cached_data, source,
                                               origin_options);
    if (maybe_result.is_null()) {
      // Deserializer failed. Fall through to compile.
      compile_timer.set_consuming_code_cache_failed();
    }
  }

  Handle<SharedFunctionInfo> wrapped;
  Handle<Script> script;
  IsCompiledScope is_compiled_scope;
  if (!maybe_result.ToHandle(&wrapped)) {
    ParseInfo parse_info(isolate);
    script = NewScript(isolate, &parse_info, source, script_details,
                       origin_options, NOT_NATIVES_CODE);
    script->set_wrapped_arguments(*arguments);

    parse_info.set_eval();  // Use an eval scope as declaration scope.
    parse_info.set_function_syntax_kind(FunctionSyntaxKind::kWrapped);
    // TODO(delphick): Remove this and instead make the wrapped and wrapper
    // functions fully non-lazy instead thus preventing source positions from
    // being omitted.
    parse_info.set_collect_source_positions(true);
    // parse_info.set_eager(compile_options == ScriptCompiler::kEagerCompile);
    if (!context->IsNativeContext()) {
      parse_info.set_outer_scope_info(handle(context->scope_info(), isolate));
    }
    parse_info.set_language_mode(
        stricter_language_mode(parse_info.language_mode(), language_mode));

    Handle<SharedFunctionInfo> top_level;
    maybe_result = CompileToplevel(&parse_info, isolate, &is_compiled_scope);
    if (maybe_result.is_null()) isolate->ReportPendingMessages();
    ASSIGN_RETURN_ON_EXCEPTION(isolate, top_level, maybe_result, JSFunction);

    SharedFunctionInfo::ScriptIterator infos(isolate, *script);
    for (SharedFunctionInfo info = infos.Next(); !info.is_null();
         info = infos.Next()) {
      if (info.is_wrapped()) {
        wrapped = Handle<SharedFunctionInfo>(info, isolate);
        break;
      }
    }
    DCHECK(!wrapped.is_null());
  } else {
    is_compiled_scope = wrapped->is_compiled_scope();
    script = Handle<Script>(Script::cast(wrapped->script()), isolate);
  }
  DCHECK(is_compiled_scope.is_compiled());

  return isolate->factory()->NewFunctionFromSharedFunctionInfo(
      wrapped, context, AllocationType::kYoung);
}

MaybeHandle<SharedFunctionInfo>
Compiler::GetSharedFunctionInfoForStreamedScript(
    Isolate* isolate, Handle<String> source,
    const ScriptDetails& script_details, ScriptOriginOptions origin_options,
    ScriptStreamingData* streaming_data) {
  ScriptCompileTimerScope compile_timer(
      isolate, ScriptCompiler::kNoCacheBecauseStreamingSource);
  PostponeInterruptsScope postpone(isolate);

  int source_length = source->length();
  isolate->counters()->total_load_size()->Increment(source_length);
  isolate->counters()->total_compile_size()->Increment(source_length);

  BackgroundCompileTask* task = streaming_data->task.get();
  ParseInfo* parse_info = task->info();
  DCHECK(parse_info->is_toplevel());
  // Check if compile cache already holds the SFI, if so no need to finalize
  // the code compiled on the background thread.
  CompilationCache* compilation_cache = isolate->compilation_cache();
  MaybeHandle<SharedFunctionInfo> maybe_result =
      compilation_cache->LookupScript(
          source, script_details.name_obj, script_details.line_offset,
          script_details.column_offset, origin_options,
          isolate->native_context(), parse_info->language_mode());
  if (!maybe_result.is_null()) {
    compile_timer.set_hit_isolate_cache();
  }

  if (maybe_result.is_null()) {
    // No cache entry found, finalize compilation of the script and add it to
    // the isolate cache.
    Handle<Script> script =
        NewScript(isolate, parse_info, source, script_details, origin_options,
                  NOT_NATIVES_CODE);
    task->parser()->UpdateStatistics(isolate, script);
    task->parser()->HandleSourceURLComments(isolate, script);

    if (parse_info->literal() == nullptr || !task->outer_function_job()) {
      // Parsing has failed - report error messages.
      FailWithPendingException(isolate, parse_info,
                               Compiler::ClearExceptionFlag::KEEP_EXCEPTION);
    } else {
      // Parsing has succeeded - finalize compilation.
      maybe_result =
          FinalizeTopLevel(parse_info, isolate, task->outer_function_job(),
                           task->inner_function_jobs());
      if (maybe_result.is_null()) {
        // Finalization failed - throw an exception.
        FailWithPendingException(isolate, parse_info,
                                 Compiler::ClearExceptionFlag::KEEP_EXCEPTION);
      }
    }

    // Add compiled code to the isolate cache.
    Handle<SharedFunctionInfo> result;
    if (maybe_result.ToHandle(&result)) {
      compilation_cache->PutScript(source, isolate->native_context(),
                                   parse_info->language_mode(), result);
    }
  }

  streaming_data->Release();
  return maybe_result;
}

Handle<SharedFunctionInfo> Compiler::GetSharedFunctionInfo(
    FunctionLiteral* literal, Handle<Script> script, Isolate* isolate) {
  // Precondition: code has been parsed and scopes have been analyzed.
  MaybeHandle<SharedFunctionInfo> maybe_existing;

  // Find any previously allocated shared function info for the given literal.
  maybe_existing = script->FindSharedFunctionInfo(isolate, literal);

  // If we found an existing shared function info, return it.
  Handle<SharedFunctionInfo> existing;
  if (maybe_existing.ToHandle(&existing)) {
    // If the function has been uncompiled (bytecode flushed) it will have lost
    // any preparsed data. If we produced preparsed data during this compile for
    // this function, replace the uncompiled data with one that includes it.
    if (literal->produced_preparse_data() != nullptr &&
        existing->HasUncompiledDataWithoutPreparseData()) {
      DCHECK(literal->inferred_name()->Equals(
          existing->uncompiled_data().inferred_name()));
      DCHECK_EQ(literal->start_position(),
                existing->uncompiled_data().start_position());
      DCHECK_EQ(literal->end_position(),
                existing->uncompiled_data().end_position());
      Handle<PreparseData> preparse_data =
          literal->produced_preparse_data()->Serialize(isolate);
      Handle<UncompiledData> new_uncompiled_data =
          isolate->factory()->NewUncompiledDataWithPreparseData(
              literal->inferred_name(), literal->start_position(),
              literal->end_position(), preparse_data);
      existing->set_uncompiled_data(*new_uncompiled_data);
    }
    return existing;
  }

  // Allocate a shared function info object which will be compiled lazily.
  Handle<SharedFunctionInfo> result =
      isolate->factory()->NewSharedFunctionInfoForLiteral(literal, script,
                                                          false);
  return result;
}

MaybeHandle<Code> Compiler::GetOptimizedCodeForOSR(Handle<JSFunction> function,
                                                   BailoutId osr_offset,
                                                   JavaScriptFrame* osr_frame) {
  DCHECK(!osr_offset.IsNone());
  DCHECK_NOT_NULL(osr_frame);
  return GetOptimizedCode(function, ConcurrencyMode::kNotConcurrent, osr_offset,
                          osr_frame);
}

bool Compiler::FinalizeOptimizedCompilationJob(OptimizedCompilationJob* job,
                                               Isolate* isolate) {
  VMState<COMPILER> state(isolate);
  // Take ownership of compilation job.  Deleting job also tears down the zone.
  std::unique_ptr<OptimizedCompilationJob> job_scope(job);
  OptimizedCompilationInfo* compilation_info = job->compilation_info();

  TimerEventScope<TimerEventRecompileSynchronous> timer(isolate);
  RuntimeCallTimerScope runtimeTimer(
      isolate, RuntimeCallCounterId::kRecompileSynchronous);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.RecompileSynchronous");

  Handle<SharedFunctionInfo> shared = compilation_info->shared_info();

  // Reset profiler ticks, function is no longer considered hot.
  compilation_info->closure()->feedback_vector().set_profiler_ticks(0);

  DCHECK(!shared->HasBreakInfo());

  // 1) Optimization on the concurrent thread may have failed.
  // 2) The function may have already been optimized by OSR.  Simply continue.
  //    Except when OSR already disabled optimization for some reason.
  // 3) The code may have already been invalidated due to dependency change.
  // 4) Code generation may have failed.
  if (job->state() == CompilationJob::State::kReadyToFinalize) {
    if (shared->optimization_disabled()) {
      job->RetryOptimization(BailoutReason::kOptimizationDisabled);
    } else if (job->FinalizeJob(isolate) == CompilationJob::SUCCEEDED) {
      job->RecordCompilationStats(OptimizedCompilationJob::kConcurrent,
                                  isolate);
      job->RecordFunctionCompilation(CodeEventListener::LAZY_COMPILE_TAG,
                                     isolate);
      InsertCodeIntoOptimizedCodeCache(compilation_info);
      if (FLAG_trace_opt) {
        PrintF("[completed optimizing ");
        compilation_info->closure()->ShortPrint();
        PrintF("]\n");
      }
      compilation_info->closure()->set_code(*compilation_info->code());
      return CompilationJob::SUCCEEDED;
    }
  }

  DCHECK_EQ(job->state(), CompilationJob::State::kFailed);
  if (FLAG_trace_opt) {
    PrintF("[aborted optimizing ");
    compilation_info->closure()->ShortPrint();
    PrintF(" because: %s]\n",
           GetBailoutReason(compilation_info->bailout_reason()));
  }
  compilation_info->closure()->set_code(shared->GetCode());
  // Clear the InOptimizationQueue marker, if it exists.
  if (compilation_info->closure()->IsInOptimizationQueue()) {
    compilation_info->closure()->ClearOptimizationMarker();
  }
  return CompilationJob::FAILED;
}

void Compiler::PostInstantiation(Handle<JSFunction> function) {
  Isolate* isolate = function->GetIsolate();
  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  IsCompiledScope is_compiled_scope(shared->is_compiled_scope());

  // If code is compiled to bytecode (i.e., isn't asm.js), then allocate a
  // feedback and check for optimized code.
  if (is_compiled_scope.is_compiled() && shared->HasBytecodeArray()) {
    JSFunction::InitializeFeedbackCell(function);

    Code code = function->has_feedback_vector()
                    ? function->feedback_vector().optimized_code()
                    : Code();
    if (!code.is_null()) {
      // Caching of optimized code enabled and optimized code found.
      DCHECK(!code.marked_for_deoptimization());
      DCHECK(function->shared().is_compiled());
      function->set_code(code);
    }

    if (FLAG_always_opt && shared->allows_lazy_compilation() &&
        !shared->optimization_disabled() && !function->IsOptimized() &&
        !function->HasOptimizedCode()) {
      JSFunction::EnsureFeedbackVector(function);
      function->MarkForOptimization(ConcurrencyMode::kNotConcurrent);
    }
  }

  if (shared->is_toplevel() || shared->is_wrapped()) {
    // If it's a top-level script, report compilation to the debugger.
    Handle<Script> script(Script::cast(shared->script()), isolate);
    isolate->debug()->OnAfterCompile(script);
  }
}

// ----------------------------------------------------------------------------
// Implementation of ScriptStreamingData

ScriptStreamingData::ScriptStreamingData(
    std::unique_ptr<ScriptCompiler::ExternalSourceStream> source_stream,
    ScriptCompiler::StreamedSource::Encoding encoding)
    : source_stream(std::move(source_stream)), encoding(encoding) {}

ScriptStreamingData::~ScriptStreamingData() = default;

void ScriptStreamingData::Release() { task.reset(); }

}  // namespace internal
}  // namespace v8
