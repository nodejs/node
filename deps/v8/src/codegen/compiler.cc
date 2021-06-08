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
#include "src/base/logging.h"
#include "src/base/optional.h"
#include "src/base/platform/time.h"
#include "src/baseline/baseline.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/compilation-cache.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/codegen/pending-optimization-table.h"
#include "src/codegen/unoptimized-compilation-info.h"
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/common/message-template.h"
#include "src/compiler-dispatcher/compiler-dispatcher.h"
#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"
#include "src/compiler/pipeline.h"
#include "src/debug/debug.h"
#include "src/debug/liveedit.h"
#include "src/diagnostics/code-tracer.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/isolate.h"
#include "src/execution/local-isolate.h"
#include "src/execution/runtime-profiler.h"
#include "src/execution/vm-state-inl.h"
#include "src/handles/maybe-handles.h"
#include "src/heap/heap-inl.h"
#include "src/heap/local-factory-inl.h"
#include "src/heap/local-heap-inl.h"
#include "src/heap/local-heap.h"
#include "src/heap/parked-scope.h"
#include "src/init/bootstrapper.h"
#include "src/interpreter/interpreter.h"
#include "src/logging/log-inl.h"
#include "src/objects/feedback-cell-inl.h"
#include "src/objects/js-function-inl.h"
#include "src/objects/map.h"
#include "src/objects/object-list-macros.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/string.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parser.h"
#include "src/parsing/parsing.h"
#include "src/parsing/pending-compilation-error-handler.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/snapshot/code-serializer.h"
#include "src/utils/ostreams.h"
#include "src/zone/zone-list-inl.h"  // crbug.com/v8/8816

namespace v8 {
namespace internal {

namespace {

class CompilerTracer : public AllStatic {
 public:
  static void TracePrepareJob(Isolate* isolate, OptimizedCompilationInfo* info,
                              const char* compiler_name) {
    if (!FLAG_trace_opt || !info->IsOptimizing()) return;
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintTracePrefix(scope, "compiling method", info);
    PrintF(scope.file(), " using %s%s", compiler_name,
           info->is_osr() ? " OSR" : "");
    PrintTraceSuffix(scope);
  }

  static void TraceStartBaselineCompile(Isolate* isolate,
                                        Handle<SharedFunctionInfo> shared) {
    if (!FLAG_trace_baseline) return;
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintTracePrefix(scope, "compiling method", shared, CodeKind::BASELINE);
    PrintF(scope.file(), " using Sparkplug");
    PrintTraceSuffix(scope);
  }

  static void TraceCompilationStats(Isolate* isolate,
                                    OptimizedCompilationInfo* info,
                                    double ms_creategraph, double ms_optimize,
                                    double ms_codegen) {
    if (!FLAG_trace_opt || !info->IsOptimizing()) return;
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintTracePrefix(scope, "optimizing", info);
    PrintF(scope.file(), " - took %0.3f, %0.3f, %0.3f ms", ms_creategraph,
           ms_optimize, ms_codegen);
    PrintTraceSuffix(scope);
  }

  static void TraceFinishBaselineCompile(Isolate* isolate,
                                         Handle<SharedFunctionInfo> shared,
                                         double ms_timetaken) {
    if (!FLAG_trace_baseline) return;
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintTracePrefix(scope, "compiling", shared, CodeKind::BASELINE);
    PrintF(scope.file(), " - took %0.3f ms", ms_timetaken);
    PrintTraceSuffix(scope);
  }

  static void TraceCompletedJob(Isolate* isolate,
                                OptimizedCompilationInfo* info) {
    if (!FLAG_trace_opt) return;
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintTracePrefix(scope, "completed optimizing", info);
    PrintTraceSuffix(scope);
  }

  static void TraceAbortedJob(Isolate* isolate,
                              OptimizedCompilationInfo* info) {
    if (!FLAG_trace_opt) return;
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintTracePrefix(scope, "aborted optimizing", info);
    PrintF(scope.file(), " because: %s",
           GetBailoutReason(info->bailout_reason()));
    PrintTraceSuffix(scope);
  }

  static void TraceOptimizedCodeCacheHit(Isolate* isolate,
                                         Handle<JSFunction> function,
                                         BytecodeOffset osr_offset,
                                         CodeKind code_kind) {
    if (!FLAG_trace_opt) return;
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintTracePrefix(scope, "found optimized code for", function, code_kind);
    if (!osr_offset.IsNone()) {
      PrintF(scope.file(), " at OSR bytecode offset %d", osr_offset.ToInt());
    }
    PrintTraceSuffix(scope);
  }

  static void TraceOptimizeForAlwaysOpt(Isolate* isolate,
                                        Handle<JSFunction> function,
                                        CodeKind code_kind) {
    if (!FLAG_trace_opt) return;
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintTracePrefix(scope, "optimizing", function, code_kind);
    PrintF(scope.file(), " because --always-opt");
    PrintTraceSuffix(scope);
  }

  static void TraceMarkForAlwaysOpt(Isolate* isolate,
                                    Handle<JSFunction> function) {
    if (!FLAG_trace_opt) return;
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintF(scope.file(), "[marking ");
    function->ShortPrint(scope.file());
    PrintF(scope.file(), " for optimized recompilation because --always-opt");
    PrintF(scope.file(), "]\n");
  }

 private:
  static void PrintTracePrefix(const CodeTracer::Scope& scope,
                               const char* header,
                               OptimizedCompilationInfo* info) {
    PrintTracePrefix(scope, header, info->closure(), info->code_kind());
  }

  static void PrintTracePrefix(const CodeTracer::Scope& scope,
                               const char* header, Handle<JSFunction> function,
                               CodeKind code_kind) {
    PrintF(scope.file(), "[%s ", header);
    function->ShortPrint(scope.file());
    PrintF(scope.file(), " (target %s)", CodeKindToString(code_kind));
  }

  static void PrintTracePrefix(const CodeTracer::Scope& scope,
                               const char* header,
                               Handle<SharedFunctionInfo> shared,
                               CodeKind code_kind) {
    PrintF(scope.file(), "[%s ", header);
    shared->ShortPrint(scope.file());
    PrintF(scope.file(), " (target %s)", CodeKindToString(code_kind));
  }

  static void PrintTraceSuffix(const CodeTracer::Scope& scope) {
    PrintF(scope.file(), "]\n");
  }
};

}  // namespace

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

// static
void Compiler::LogFunctionCompilation(Isolate* isolate,
                                      CodeEventListener::LogEventsAndTags tag,
                                      Handle<SharedFunctionInfo> shared,
                                      Handle<Script> script,
                                      Handle<AbstractCode> abstract_code,
                                      CodeKind kind, double time_taken_ms) {
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
  Handle<String> script_name(script->name().IsString()
                                 ? String::cast(script->name())
                                 : ReadOnlyRoots(isolate).empty_string(),
                             isolate);
  CodeEventListener::LogEventsAndTags log_tag =
      Logger::ToNativeByScript(tag, *script);
  PROFILE(isolate, CodeCreateEvent(log_tag, abstract_code, shared, script_name,
                                   line_num, column_num));
  if (!FLAG_log_function_events) return;

  std::string name;
  switch (kind) {
    case CodeKind::INTERPRETED_FUNCTION:
      name = "interpreter";
      break;
    case CodeKind::BASELINE:
      name = "baseline";
      break;
    case CodeKind::TURBOPROP:
      name = "turboprop";
      break;
    case CodeKind::TURBOFAN:
      name = "optimize";
      break;
    default:
      UNREACHABLE();
  }
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

  Handle<String> debug_name = SharedFunctionInfo::DebugName(shared);
  DisallowGarbageCollection no_gc;
  LOG(isolate, FunctionEvent(name.c_str(), script->id(), time_taken_ms,
                             shared->StartPosition(), shared->EndPosition(),
                             *debug_name));
}

namespace {

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

CompilationJob::Status UnoptimizedCompilationJob::FinalizeJob(
    Handle<SharedFunctionInfo> shared_info, LocalIsolate* isolate) {
  // Delegate to the underlying implementation.
  DCHECK_EQ(state(), State::kReadyToFinalize);
  ScopedTimer t(&time_taken_to_finalize_);
  return UpdateState(FinalizeJobImpl(shared_info, isolate), State::kSucceeded);
}

namespace {

void RecordUnoptimizedCompilationStats(Isolate* isolate,
                                       Handle<SharedFunctionInfo> shared_info) {
#if V8_ENABLE_WEBASSEMBLY
  int code_size =
      shared_info->HasBytecodeArray()
          ? shared_info->GetBytecodeArray(isolate).SizeIncludingMetadata()
          : shared_info->asm_wasm_data().Size();
#else
  int code_size =
      shared_info->GetBytecodeArray(isolate).SizeIncludingMetadata();
#endif  // V8_ENABLE_WEBASSEMBLY

  Counters* counters = isolate->counters();
  // TODO(4280): Rename counters from "baseline" to "unoptimized" eventually.
  counters->total_baseline_code_size()->Increment(code_size);
  counters->total_baseline_compile_count()->Increment(1);

  // TODO(5203): Add timers for each phase of compilation.
  // Also add total time (there's now already timer_ on the base class).
}

void RecordUnoptimizedFunctionCompilation(
    Isolate* isolate, CodeEventListener::LogEventsAndTags tag,
    Handle<SharedFunctionInfo> shared, base::TimeDelta time_taken_to_execute,
    base::TimeDelta time_taken_to_finalize) {
  Handle<AbstractCode> abstract_code;
  if (shared->HasBytecodeArray()) {
    abstract_code =
        handle(AbstractCode::cast(shared->GetBytecodeArray(isolate)), isolate);
  } else {
#if V8_ENABLE_WEBASSEMBLY
    DCHECK(shared->HasAsmWasmData());
    abstract_code =
        Handle<AbstractCode>::cast(BUILTIN_CODE(isolate, InstantiateAsmJs));
#else
    UNREACHABLE();
#endif  // V8_ENABLE_WEBASSEMBLY
  }

  double time_taken_ms = time_taken_to_execute.InMillisecondsF() +
                         time_taken_to_finalize.InMillisecondsF();

  Handle<Script> script(Script::cast(shared->script()), isolate);
  Compiler::LogFunctionCompilation(isolate, tag, shared, script, abstract_code,
                                   CodeKind::INTERPRETED_FUNCTION,
                                   time_taken_ms);
}

}  // namespace

// ----------------------------------------------------------------------------
// Implementation of OptimizedCompilationJob

CompilationJob::Status OptimizedCompilationJob::PrepareJob(Isolate* isolate) {
  DCHECK_EQ(ThreadId::Current(), isolate->thread_id());
  DisallowJavascriptExecution no_js(isolate);
  CompilerTracer::TracePrepareJob(isolate, compilation_info(), compiler_name_);

  // Delegate to the underlying implementation.
  DCHECK_EQ(state(), State::kReadyToPrepare);
  ScopedTimer t(&time_taken_to_prepare_);
  return UpdateState(PrepareJobImpl(isolate), State::kReadyToExecute);
}

CompilationJob::Status OptimizedCompilationJob::ExecuteJob(
    RuntimeCallStats* stats, LocalIsolate* local_isolate) {
  DCHECK_IMPLIES(local_isolate, local_isolate->heap()->IsParked());
  // Delegate to the underlying implementation.
  DCHECK_EQ(state(), State::kReadyToExecute);
  ScopedTimer t(&time_taken_to_execute_);
  return UpdateState(ExecuteJobImpl(stats, local_isolate),
                     State::kReadyToFinalize);
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
  CompilerTracer::TraceCompilationStats(
      isolate, compilation_info(), ms_creategraph, ms_optimize, ms_codegen);
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
  Compiler::LogFunctionCompilation(
      isolate, tag, compilation_info()->shared_info(), script, abstract_code,
      compilation_info()->code_kind(), time_taken_ms);
}

// ----------------------------------------------------------------------------
// Local helper methods that make up the compilation pipeline.

namespace {

#if V8_ENABLE_WEBASSEMBLY
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
#endif

void InstallInterpreterTrampolineCopy(
    Isolate* isolate, Handle<SharedFunctionInfo> shared_info,
    CodeEventListener::LogEventsAndTags log_tag) {
  DCHECK(FLAG_interpreted_frames_native_stack);
  if (!shared_info->function_data(kAcquireLoad).IsBytecodeArray()) {
    DCHECK(!shared_info->HasBytecodeArray());
    return;
  }
  Handle<BytecodeArray> bytecode_array(shared_info->GetBytecodeArray(isolate),
                                       isolate);

  Handle<Code> code = isolate->factory()->CopyCode(Handle<Code>::cast(
      isolate->factory()->interpreter_entry_trampoline_for_profiling()));

  Handle<InterpreterData> interpreter_data =
      Handle<InterpreterData>::cast(isolate->factory()->NewStruct(
          INTERPRETER_DATA_TYPE, AllocationType::kOld));

  interpreter_data->set_bytecode_array(*bytecode_array);
  interpreter_data->set_interpreter_trampoline(*code);

  shared_info->set_interpreter_data(*interpreter_data);

  Handle<Script> script(Script::cast(shared_info->script()), isolate);
  Handle<AbstractCode> abstract_code = Handle<AbstractCode>::cast(code);
  int line_num =
      Script::GetLineNumber(script, shared_info->StartPosition()) + 1;
  int column_num =
      Script::GetColumnNumber(script, shared_info->StartPosition()) + 1;
  Handle<String> script_name =
      handle(script->name().IsString() ? String::cast(script->name())
                                       : ReadOnlyRoots(isolate).empty_string(),
             isolate);
  PROFILE(isolate, CodeCreateEvent(log_tag, abstract_code, shared_info,
                                   script_name, line_num, column_num));
}

template <typename LocalIsolate>
void InstallUnoptimizedCode(UnoptimizedCompilationInfo* compilation_info,
                            Handle<SharedFunctionInfo> shared_info,
                            LocalIsolate* isolate) {
  if (compilation_info->has_bytecode_array()) {
    DCHECK(!shared_info->HasBytecodeArray());  // Only compiled once.
    DCHECK(!compilation_info->has_asm_wasm_data());
    DCHECK(!shared_info->HasFeedbackMetadata());

#if V8_ENABLE_WEBASSEMBLY
    // If the function failed asm-wasm compilation, mark asm_wasm as broken
    // to ensure we don't try to compile as asm-wasm.
    if (compilation_info->literal()->scope()->IsAsmModule()) {
      shared_info->set_is_asm_wasm_broken(true);
    }
#endif  // V8_ENABLE_WEBASSEMBLY

    shared_info->set_bytecode_array(*compilation_info->bytecode_array());

    Handle<FeedbackMetadata> feedback_metadata = FeedbackMetadata::New(
        isolate, compilation_info->feedback_vector_spec());
    shared_info->set_feedback_metadata(*feedback_metadata);
  } else {
#if V8_ENABLE_WEBASSEMBLY
    DCHECK(compilation_info->has_asm_wasm_data());
    // We should only have asm/wasm data when finalizing on the main thread.
    DCHECK((std::is_same<LocalIsolate, Isolate>::value));
    shared_info->set_asm_wasm_data(*compilation_info->asm_wasm_data());
    shared_info->set_feedback_metadata(
        ReadOnlyRoots(isolate).empty_feedback_metadata());
#else
    UNREACHABLE();
#endif  // V8_ENABLE_WEBASSEMBLY
  }
}

void LogUnoptimizedCompilation(Isolate* isolate,
                               Handle<SharedFunctionInfo> shared_info,
                               CodeEventListener::LogEventsAndTags log_tag,
                               base::TimeDelta time_taken_to_execute,
                               base::TimeDelta time_taken_to_finalize) {
  RecordUnoptimizedFunctionCompilation(isolate, log_tag, shared_info,
                                       time_taken_to_execute,
                                       time_taken_to_finalize);
  RecordUnoptimizedCompilationStats(isolate, shared_info);
}

template <typename LocalIsolate>
void EnsureSharedFunctionInfosArrayOnScript(Handle<Script> script,
                                            ParseInfo* parse_info,
                                            LocalIsolate* isolate) {
  DCHECK(parse_info->flags().is_toplevel());
  if (script->shared_function_infos().length() > 0) {
    DCHECK_EQ(script->shared_function_infos().length(),
              parse_info->max_function_literal_id() + 1);
    return;
  }
  Handle<WeakFixedArray> infos(isolate->factory()->NewWeakFixedArray(
      parse_info->max_function_literal_id() + 1, AllocationType::kOld));
  script->set_shared_function_infos(*infos);
}

void UpdateSharedFunctionFlagsAfterCompilation(FunctionLiteral* literal,
                                               SharedFunctionInfo shared_info) {
  DCHECK_EQ(shared_info.language_mode(), literal->language_mode());

  shared_info.set_has_duplicate_parameters(literal->has_duplicate_parameters());
  shared_info.set_is_oneshot_iife(literal->is_oneshot_iife());
  shared_info.UpdateAndFinalizeExpectedNofPropertiesFromEstimate(literal);
  if (literal->dont_optimize_reason() != BailoutReason::kNoReason) {
    shared_info.DisableOptimization(literal->dont_optimize_reason());
  }

  shared_info.set_class_scope_has_private_brand(
      literal->class_scope_has_private_brand());
  shared_info.set_has_static_private_methods_or_accessors(
      literal->has_static_private_methods_or_accessors());

  shared_info.SetScopeInfo(*literal->scope()->scope_info());
}

bool CanCompileWithBaseline(Isolate* isolate,
                            Handle<SharedFunctionInfo> shared) {
  // Check if we actually have bytecode.
  if (!shared->HasBytecodeArray()) return false;

  // Do not optimize when debugger needs to hook into every call.
  if (isolate->debug()->needs_check_on_function_call()) return false;

  // Functions with breakpoints have to stay interpreted.
  if (shared->HasBreakInfo()) return false;

  // Do not baseline compile if sparkplug is disabled or function doesn't pass
  // sparkplug_filter.
  if (!FLAG_sparkplug || !shared->PassesFilter(FLAG_sparkplug_filter)) {
    return false;
  }

  return true;
}

bool CompileSharedWithBaseline(Isolate* isolate,
                               Handle<SharedFunctionInfo> shared,
                               Compiler::ClearExceptionFlag flag,
                               IsCompiledScope* is_compiled_scope) {
  // We shouldn't be passing uncompiled functions into this function.
  DCHECK(is_compiled_scope->is_compiled());

  // Early return for already baseline-compiled functions.
  if (shared->HasBaselineData()) return true;

  // Check if we actually can compile with baseline.
  if (!CanCompileWithBaseline(isolate, shared)) return false;

  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed(kStackSpaceRequiredForCompilation * KB)) {
    if (flag == Compiler::KEEP_EXCEPTION) {
      isolate->StackOverflow();
    }
    return false;
  }

  CompilerTracer::TraceStartBaselineCompile(isolate, shared);
  Handle<Code> code;
  base::TimeDelta time_taken;
  {
    ScopedTimer timer(&time_taken);
    if (!GenerateBaselineCode(isolate, shared).ToHandle(&code)) {
      // TODO(leszeks): This can only fail because of an OOM. Do we want to
      // report these somehow, or silently ignore them?
      return false;
    }

    Handle<HeapObject> function_data =
        handle(HeapObject::cast(shared->function_data(kAcquireLoad)), isolate);
    Handle<BaselineData> baseline_data =
        isolate->factory()->NewBaselineData(code, function_data);
    shared->set_baseline_data(*baseline_data);
  }
  double time_taken_ms = time_taken.InMillisecondsF();

  CompilerTracer::TraceFinishBaselineCompile(isolate, shared, time_taken_ms);

  if (shared->script().IsScript()) {
    Compiler::LogFunctionCompilation(
        isolate, CodeEventListener::FUNCTION_TAG, shared,
        handle(Script::cast(shared->script()), isolate),
        Handle<AbstractCode>::cast(code), CodeKind::BASELINE, time_taken_ms);
  }
  return true;
}

// Finalize a single compilation job. This function can return
// RETRY_ON_MAIN_THREAD if the job cannot be finalized off-thread, in which case
// it should be safe to call it again on the main thread with the same job.
template <typename LocalIsolate>
CompilationJob::Status FinalizeSingleUnoptimizedCompilationJob(
    UnoptimizedCompilationJob* job, Handle<SharedFunctionInfo> shared_info,
    LocalIsolate* isolate,
    FinalizeUnoptimizedCompilationDataList*
        finalize_unoptimized_compilation_data_list) {
  UnoptimizedCompilationInfo* compilation_info = job->compilation_info();

  CompilationJob::Status status = job->FinalizeJob(shared_info, isolate);
  if (status == CompilationJob::SUCCEEDED) {
    InstallUnoptimizedCode(compilation_info, shared_info, isolate);

    MaybeHandle<CoverageInfo> coverage_info;
    if (compilation_info->has_coverage_info() &&
        !shared_info->HasCoverageInfo()) {
      coverage_info = compilation_info->coverage_info();
    }

    finalize_unoptimized_compilation_data_list->emplace_back(
        isolate, shared_info, coverage_info, job->time_taken_to_execute(),
        job->time_taken_to_finalize());
  }
  DCHECK_IMPLIES(
      status == CompilationJob::RETRY_ON_MAIN_THREAD,
      (std::is_same<LocalIsolate, v8::internal::LocalIsolate>::value));
  return status;
}

std::unique_ptr<UnoptimizedCompilationJob>
ExecuteSingleUnoptimizedCompilationJob(
    ParseInfo* parse_info, FunctionLiteral* literal,
    AccountingAllocator* allocator,
    std::vector<FunctionLiteral*>* eager_inner_literals,
    LocalIsolate* local_isolate) {
#if V8_ENABLE_WEBASSEMBLY
  if (UseAsmWasm(literal, parse_info->flags().is_asm_wasm_broken())) {
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
#endif
  std::unique_ptr<UnoptimizedCompilationJob> job(
      interpreter::Interpreter::NewCompilationJob(
          parse_info, literal, allocator, eager_inner_literals, local_isolate));

  if (job->ExecuteJob() != CompilationJob::SUCCEEDED) {
    // Compilation failed, return null.
    return std::unique_ptr<UnoptimizedCompilationJob>();
  }

  return job;
}

bool RecursivelyExecuteUnoptimizedCompilationJobs(
    ParseInfo* parse_info, FunctionLiteral* literal,
    AccountingAllocator* allocator,
    UnoptimizedCompilationJobList* function_jobs) {
  std::vector<FunctionLiteral*> eager_inner_literals;

  // We need to pass nullptr here because we are on the background
  // thread but don't have a LocalIsolate.
  DCHECK_NULL(LocalHeap::Current());
  std::unique_ptr<UnoptimizedCompilationJob> job =
      ExecuteSingleUnoptimizedCompilationJob(parse_info, literal, allocator,
                                             &eager_inner_literals, nullptr);

  if (!job) return false;

  // Recursively compile eager inner literals.
  for (FunctionLiteral* inner_literal : eager_inner_literals) {
    if (!RecursivelyExecuteUnoptimizedCompilationJobs(
            parse_info, inner_literal, allocator, function_jobs)) {
      return false;
    }
  }

  function_jobs->emplace_front(std::move(job));
  return true;
}

template <typename LocalIsolate>
bool IterativelyExecuteAndFinalizeUnoptimizedCompilationJobs(
    LocalIsolate* isolate, Handle<SharedFunctionInfo> outer_shared_info,
    Handle<Script> script, ParseInfo* parse_info,
    AccountingAllocator* allocator, IsCompiledScope* is_compiled_scope,
    FinalizeUnoptimizedCompilationDataList*
        finalize_unoptimized_compilation_data_list,
    DeferredFinalizationJobDataList*
        jobs_to_retry_finalization_on_main_thread) {
  DeclarationScope::AllocateScopeInfos(parse_info, isolate);

  std::vector<FunctionLiteral*> functions_to_compile;
  functions_to_compile.push_back(parse_info->literal());

  while (!functions_to_compile.empty()) {
    FunctionLiteral* literal = functions_to_compile.back();
    functions_to_compile.pop_back();
    Handle<SharedFunctionInfo> shared_info =
        Compiler::GetSharedFunctionInfo(literal, script, isolate);
    if (shared_info->is_compiled()) continue;

    std::unique_ptr<UnoptimizedCompilationJob> job =
        ExecuteSingleUnoptimizedCompilationJob(parse_info, literal, allocator,
                                               &functions_to_compile,
                                               isolate->AsLocalIsolate());

    if (!job) return false;

    UpdateSharedFunctionFlagsAfterCompilation(literal, *shared_info);

    auto finalization_status = FinalizeSingleUnoptimizedCompilationJob(
        job.get(), shared_info, isolate,
        finalize_unoptimized_compilation_data_list);

    switch (finalization_status) {
      case CompilationJob::SUCCEEDED:
        if (shared_info.is_identical_to(outer_shared_info)) {
          // Ensure that the top level function is retained.
          *is_compiled_scope = shared_info->is_compiled_scope(isolate);
          DCHECK(is_compiled_scope->is_compiled());
        }
        break;

      case CompilationJob::FAILED:
        return false;

      case CompilationJob::RETRY_ON_MAIN_THREAD:
        // This should not happen on the main thread.
        DCHECK((!std::is_same<LocalIsolate, Isolate>::value));
        DCHECK_NOT_NULL(jobs_to_retry_finalization_on_main_thread);

        // Clear the literal and ParseInfo to prevent further attempts to
        // access them.
        job->compilation_info()->ClearLiteral();
        job->ClearParseInfo();
        jobs_to_retry_finalization_on_main_thread->emplace_back(
            isolate, shared_info, std::move(job));
        break;
    }
  }

  // Report any warnings generated during compilation.
  if (parse_info->pending_error_handler()->has_pending_warnings()) {
    parse_info->pending_error_handler()->PrepareWarnings(isolate);
  }

  return true;
}

bool FinalizeAllUnoptimizedCompilationJobs(
    ParseInfo* parse_info, Isolate* isolate, Handle<Script> script,
    UnoptimizedCompilationJobList* compilation_jobs,
    FinalizeUnoptimizedCompilationDataList*
        finalize_unoptimized_compilation_data_list) {
  DCHECK(AllowCompilation::IsAllowed(isolate));
  DCHECK(!compilation_jobs->empty());

  // TODO(rmcilroy): Clear native context in debug once AsmJS generates doesn't
  // rely on accessing native context during finalization.

  // Allocate scope infos for the literal.
  DeclarationScope::AllocateScopeInfos(parse_info, isolate);

  // Finalize the functions' compilation jobs.
  for (auto&& job : *compilation_jobs) {
    FunctionLiteral* literal = job->compilation_info()->literal();
    Handle<SharedFunctionInfo> shared_info =
        Compiler::GetSharedFunctionInfo(literal, script, isolate);
    // The inner function might be compiled already if compiling for debug.
    if (shared_info->is_compiled()) continue;
    UpdateSharedFunctionFlagsAfterCompilation(literal, *shared_info);
    if (FinalizeSingleUnoptimizedCompilationJob(
            job.get(), shared_info, isolate,
            finalize_unoptimized_compilation_data_list) !=
        CompilationJob::SUCCEEDED) {
      return false;
    }
  }

  // Report any warnings generated during compilation.
  if (parse_info->pending_error_handler()->has_pending_warnings()) {
    parse_info->pending_error_handler()->PrepareWarnings(isolate);
  }

  return true;
}

bool FinalizeDeferredUnoptimizedCompilationJobs(
    Isolate* isolate, Handle<Script> script,
    DeferredFinalizationJobDataList* deferred_jobs,
    PendingCompilationErrorHandler* pending_error_handler,
    FinalizeUnoptimizedCompilationDataList*
        finalize_unoptimized_compilation_data_list) {
  DCHECK(AllowCompilation::IsAllowed(isolate));

  if (deferred_jobs->empty()) return true;

  // TODO(rmcilroy): Clear native context in debug once AsmJS generates doesn't
  // rely on accessing native context during finalization.

  // Finalize the deferred compilation jobs.
  for (auto&& job : *deferred_jobs) {
    Handle<SharedFunctionInfo> shared_info = job.function_handle();
    if (FinalizeSingleUnoptimizedCompilationJob(
            job.job(), shared_info, isolate,
            finalize_unoptimized_compilation_data_list) !=
        CompilationJob::SUCCEEDED) {
      return false;
    }
  }

  // Report any warnings generated during deferred finalization.
  if (pending_error_handler->has_pending_warnings()) {
    pending_error_handler->PrepareWarnings(isolate);
  }

  return true;
}

V8_WARN_UNUSED_RESULT MaybeHandle<Code> GetCodeFromOptimizedCodeCache(
    Handle<JSFunction> function, BytecodeOffset osr_offset,
    CodeKind code_kind) {
  RuntimeCallTimerScope runtimeTimer(
      function->GetIsolate(),
      RuntimeCallCounterId::kCompileGetFromOptimizedCodeMap);
  Handle<SharedFunctionInfo> shared(function->shared(), function->GetIsolate());
  Isolate* isolate = function->GetIsolate();
  DisallowGarbageCollection no_gc;
  Code code;
  if (osr_offset.IsNone() && function->has_feedback_vector()) {
    FeedbackVector feedback_vector = function->feedback_vector();
    feedback_vector.EvictOptimizedCodeMarkedForDeoptimization(
        function->raw_feedback_cell(), function->shared(),
        "GetCodeFromOptimizedCodeCache");
    code = feedback_vector.optimized_code();
  } else if (!osr_offset.IsNone()) {
    code = function->context()
               .native_context()
               .GetOSROptimizedCodeCache()
               .GetOptimizedCode(shared, osr_offset, isolate);
  }
  DCHECK_IMPLIES(!code.is_null(), code.kind() <= code_kind);
  if (!code.is_null() && code.kind() == code_kind) {
    // Caching of optimized code enabled and optimized code found.
    DCHECK(!code.marked_for_deoptimization());
    DCHECK(function->shared().is_compiled());
    DCHECK(CodeKindIsStoredInOptimizedCodeCache(code.kind()));
    DCHECK_IMPLIES(!osr_offset.IsNone(), CodeKindCanOSR(code.kind()));
    return Handle<Code>(code, isolate);
  }
  return MaybeHandle<Code>();
}

void ClearOptimizedCodeCache(OptimizedCompilationInfo* compilation_info) {
  DCHECK(!CodeKindIsNativeContextIndependentJSFunction(
      compilation_info->code_kind()));
  Handle<JSFunction> function = compilation_info->closure();
  if (compilation_info->osr_offset().IsNone()) {
    Handle<FeedbackVector> vector =
        handle(function->feedback_vector(), function->GetIsolate());
    vector->ClearOptimizationMarker();
  }
}

void InsertCodeIntoOptimizedCodeCache(
    OptimizedCompilationInfo* compilation_info) {
  const CodeKind kind = compilation_info->code_kind();
  if (!CodeKindIsStoredInOptimizedCodeCache(kind)) return;

  if (compilation_info->function_context_specializing()) {
    // Function context specialization folds-in the function context, so no
    // sharing can occur. Make sure the optimized code cache is cleared.
    ClearOptimizedCodeCache(compilation_info);
    return;
  }

  // Cache optimized code.
  Handle<Code> code = compilation_info->code();
  Handle<JSFunction> function = compilation_info->closure();
  Handle<SharedFunctionInfo> shared(function->shared(), function->GetIsolate());
  Handle<NativeContext> native_context(function->context().native_context(),
                                       function->GetIsolate());
  if (compilation_info->osr_offset().IsNone()) {
    Handle<FeedbackVector> vector =
        handle(function->feedback_vector(), function->GetIsolate());
    FeedbackVector::SetOptimizedCode(vector, code,
                                     function->raw_feedback_cell());
  } else {
    DCHECK(CodeKindCanOSR(kind));
    OSROptimizedCodeCache::AddOptimizedCode(native_context, shared, code,
                                            compilation_info->osr_offset());
  }
}

void InsertCodeIntoCompilationCache(Isolate* isolate,
                                    OptimizedCompilationInfo* info) {
  if (!CodeKindIsNativeContextIndependentJSFunction(info->code_kind())) return;

  DCHECK(info->osr_offset().IsNone());

  Handle<Code> code = info->code();
  DCHECK(!info->function_context_specializing());

  Handle<SharedFunctionInfo> sfi = info->shared_info();
  CompilationCache* cache = isolate->compilation_cache();
  cache->PutCode(sfi, code);
  DCHECK(!cache->LookupCode(sfi).is_null());

  sfi->set_may_have_cached_code(true);

  if (FLAG_trace_turbo_nci) CompilationCacheCode::TraceInsertion(sfi, code);
}

V8_WARN_UNUSED_RESULT MaybeHandle<Code> GetCodeFromCompilationCache(
    Isolate* isolate, Handle<SharedFunctionInfo> shared) {
  if (!shared->may_have_cached_code()) return {};
  return shared->TryGetCachedCode(isolate);
}

// Runs PrepareJob in the proper compilation & canonical scopes. Handles will be
// allocated in a persistent handle scope that is detached and handed off to the
// {compilation_info} after PrepareJob.
bool PrepareJobWithHandleScope(OptimizedCompilationJob* job, Isolate* isolate,
                               OptimizedCompilationInfo* compilation_info) {
  CompilationHandleScope compilation(isolate, compilation_info);
  CanonicalHandleScope canonical(isolate, compilation_info);
  compilation_info->ReopenHandlesInNewHandleScope(isolate);
  return job->PrepareJob(isolate) == CompilationJob::SUCCEEDED;
}

bool GetOptimizedCodeNow(OptimizedCompilationJob* job, Isolate* isolate,
                         OptimizedCompilationInfo* compilation_info) {
  TimerEventScope<TimerEventRecompileSynchronous> timer(isolate);
  RuntimeCallTimerScope runtimeTimer(
      isolate, RuntimeCallCounterId::kOptimizeNonConcurrent);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.OptimizeNonConcurrent");

  if (!PrepareJobWithHandleScope(job, isolate, compilation_info)) {
    CompilerTracer::TraceAbortedJob(isolate, compilation_info);
    return false;
  }

  {
    // Park main thread here to be in the same state as background threads.
    ParkedScope parked_scope(isolate->main_thread_local_isolate());
    if (job->ExecuteJob(isolate->counters()->runtime_call_stats(),
                        isolate->main_thread_local_isolate())) {
      UnparkedScope unparked_scope(isolate->main_thread_local_isolate());
      CompilerTracer::TraceAbortedJob(isolate, compilation_info);
      return false;
    }
  }

  if (job->FinalizeJob(isolate) != CompilationJob::SUCCEEDED) {
    CompilerTracer::TraceAbortedJob(isolate, compilation_info);
    return false;
  }

  // Success!
  job->RecordCompilationStats(OptimizedCompilationJob::kSynchronous, isolate);
  DCHECK(!isolate->has_pending_exception());
  InsertCodeIntoOptimizedCodeCache(compilation_info);
  job->RecordFunctionCompilation(CodeEventListener::LAZY_COMPILE_TAG, isolate);
  return true;
}

bool GetOptimizedCodeLater(std::unique_ptr<OptimizedCompilationJob> job,
                           Isolate* isolate,
                           OptimizedCompilationInfo* compilation_info,
                           CodeKind code_kind, Handle<JSFunction> function) {
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
      isolate, RuntimeCallCounterId::kOptimizeConcurrentPrepare);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.OptimizeConcurrentPrepare");

  if (!PrepareJobWithHandleScope(job.get(), isolate, compilation_info)) {
    return false;
  }

  // The background recompile will own this job.
  isolate->optimizing_compile_dispatcher()->QueueForOptimization(job.get());
  job.release();

  if (FLAG_trace_concurrent_recompilation) {
    PrintF("  ** Queued ");
    compilation_info->closure()->ShortPrint();
    PrintF(" for concurrent optimization.\n");
  }

  if (CodeKindIsStoredInOptimizedCodeCache(code_kind)) {
    function->SetOptimizationMarker(OptimizationMarker::kInOptimizationQueue);
  }

  // Note: Usually the active tier is expected to be Ignition or NCI at this
  // point (in other words we don't expect to optimize if the function is
  // already TF-optimized). There is a special case for OSR though, for which
  // we *can* reach this point even if we've already generated non-OSR'd TF
  // code.
  DCHECK(function->shared().HasBytecodeArray());
  return true;
}

// Returns the code object at which execution continues after a concurrent
// optimization job has been started (but not finished).
Handle<Code> ContinuationForConcurrentOptimization(
    Isolate* isolate, Handle<JSFunction> function) {
  Handle<Code> cached_code;
  if (FLAG_turbo_nci && function->NextTier() == CodeKindForTopTier() &&
      GetCodeFromCompilationCache(isolate, handle(function->shared(), isolate))
          .ToHandle(&cached_code)) {
    // Tiering up to Turbofan and cached optimized code exists. Continue
    // execution there until TF optimization has finished.
    return cached_code;
  } else if (FLAG_turboprop && function->HasAvailableOptimizedCode()) {
    DCHECK(!FLAG_turboprop_as_toptier);
    DCHECK(function->NextTier() == CodeKind::TURBOFAN);
    // It is possible that we have marked a closure for TurboFan optimization
    // but the marker is processed by another closure that doesn't have
    // optimized code yet. So heal the closure here and return the optimized
    // code.
    if (!function->HasAttachedOptimizedCode()) {
      DCHECK(function->feedback_vector().has_optimized_code());
      // Release store isn't required here because it was done on store
      // into the feedback vector.
      STATIC_ASSERT(
          FeedbackVector::kFeedbackVectorMaybeOptimizedCodeIsStoreRelease);
      function->set_code(function->feedback_vector().optimized_code());
    }
    return handle(function->code(), isolate);
  } else if (function->shared().HasBaselineData()) {
    Code baseline_code = function->shared().baseline_data().baseline_code();
    function->set_code(baseline_code);
    return handle(baseline_code, isolate);
  }
  DCHECK(function->ActiveTierIsIgnition());
  return BUILTIN_CODE(isolate, InterpreterEntryTrampoline);
}

MaybeHandle<Code> GetOptimizedCode(
    Handle<JSFunction> function, ConcurrencyMode mode, CodeKind code_kind,
    BytecodeOffset osr_offset = BytecodeOffset::None(),
    JavaScriptFrame* osr_frame = nullptr) {
  DCHECK(CodeKindIsOptimizedJSFunction(code_kind));

  Isolate* isolate = function->GetIsolate();
  Handle<SharedFunctionInfo> shared(function->shared(), isolate);

  // Make sure we clear the optimization marker on the function so that we
  // don't try to re-optimize.
  // If compiling for NCI (which does not use the optimization marker), don't
  // touch the marker to avoid interfering with Turbofan compilation.
  if (!CodeKindIsNativeContextIndependentJSFunction(code_kind) &&
      function->HasOptimizationMarker()) {
    function->ClearOptimizationMarker();
  }

  if (shared->optimization_disabled() &&
      shared->disable_optimization_reason() == BailoutReason::kNeverOptimize) {
    return {};
  }

  // Do not optimize when debugger needs to hook into every call.
  if (isolate->debug()->needs_check_on_function_call()) return {};

  // Do not use TurboFan if we need to be able to set break points.
  if (shared->HasBreakInfo()) return {};

  // Do not use TurboFan if optimization is disabled or function doesn't pass
  // turbo_filter.
  if (!FLAG_opt || !shared->PassesFilter(FLAG_turbo_filter)) return {};

  // If code was pending optimization for testing, remove the entry from the
  // table that was preventing the bytecode from being flushed.
  if (V8_UNLIKELY(FLAG_testing_d8_test_runner)) {
    PendingOptimizationTable::FunctionWasOptimized(isolate, function);
  }

  // Check the optimized code cache (stored on the SharedFunctionInfo).
  if (CodeKindIsStoredInOptimizedCodeCache(code_kind)) {
    Handle<Code> cached_code;
    if (GetCodeFromOptimizedCodeCache(function, osr_offset, code_kind)
            .ToHandle(&cached_code)) {
      CompilerTracer::TraceOptimizedCodeCacheHit(isolate, function, osr_offset,
                                                 code_kind);
      return cached_code;
    }
  }

  // Reset profiler ticks, function is no longer considered hot.
  DCHECK(shared->is_compiled());
  function->feedback_vector().set_profiler_ticks(0);

  // Check the compilation cache (stored on the Isolate, shared between native
  // contexts).
  if (CodeKindIsNativeContextIndependentJSFunction(code_kind)) {
    DCHECK(osr_offset.IsNone());

    Handle<Code> cached_code;
    if (GetCodeFromCompilationCache(isolate, shared).ToHandle(&cached_code)) {
      CHECK_EQ(cached_code->kind(), CodeKind::NATIVE_CONTEXT_INDEPENDENT);
      if (FLAG_trace_turbo_nci) {
        CompilationCacheCode::TraceHit(shared, cached_code);
      }
      return cached_code;
    }
  }

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
      compiler::Pipeline::NewCompilationJob(isolate, function, code_kind,
                                            has_script, osr_offset, osr_frame));
  OptimizedCompilationInfo* compilation_info = job->compilation_info();

  // Prepare the job and launch concurrent compilation, or compile now.
  if (mode == ConcurrencyMode::kConcurrent) {
    if (GetOptimizedCodeLater(std::move(job), isolate, compilation_info,
                              code_kind, function)) {
      return ContinuationForConcurrentOptimization(isolate, function);
    }
  } else {
    DCHECK_EQ(mode, ConcurrencyMode::kNotConcurrent);
    if (GetOptimizedCodeNow(job.get(), isolate, compilation_info)) {
      InsertCodeIntoCompilationCache(isolate, compilation_info);
      return compilation_info->code();
    }
  }

  if (isolate->has_pending_exception()) isolate->clear_pending_exception();
  return {};
}

bool FailAndClearPendingException(Isolate* isolate) {
  isolate->clear_pending_exception();
  return false;
}

template <typename LocalIsolate>
bool PreparePendingException(LocalIsolate* isolate, ParseInfo* parse_info) {
  if (parse_info->pending_error_handler()->has_pending_error()) {
    parse_info->pending_error_handler()->PrepareErrors(
        isolate, parse_info->ast_value_factory());
  }
  return false;
}

bool FailWithPreparedPendingException(
    Isolate* isolate, Handle<Script> script,
    const PendingCompilationErrorHandler* pending_error_handler) {
  if (!isolate->has_pending_exception()) {
    if (pending_error_handler->has_pending_error()) {
      pending_error_handler->ReportErrors(isolate, script);
    } else {
      isolate->StackOverflow();
    }
  }
  return false;
}

bool FailWithPendingException(Isolate* isolate, Handle<Script> script,
                              ParseInfo* parse_info,
                              Compiler::ClearExceptionFlag flag) {
  if (flag == Compiler::CLEAR_EXCEPTION) {
    return FailAndClearPendingException(isolate);
  }

  PreparePendingException(isolate, parse_info);
  return FailWithPreparedPendingException(isolate, script,
                                          parse_info->pending_error_handler());
}

void FinalizeUnoptimizedCompilation(
    Isolate* isolate, Handle<Script> script,
    const UnoptimizedCompileFlags& flags,
    const UnoptimizedCompileState* compile_state,
    const FinalizeUnoptimizedCompilationDataList&
        finalize_unoptimized_compilation_data_list) {
  if (compile_state->pending_error_handler()->has_pending_warnings()) {
    compile_state->pending_error_handler()->ReportWarnings(isolate, script);
  }

  bool need_source_positions = FLAG_stress_lazy_source_positions ||
                               (!flags.collect_source_positions() &&
                                isolate->NeedsSourcePositionsForProfiling());

  for (const auto& finalize_data : finalize_unoptimized_compilation_data_list) {
    Handle<SharedFunctionInfo> shared_info = finalize_data.function_handle();
    // It's unlikely, but possible, that the bytecode was flushed between being
    // allocated and now, so guard against that case, and against it being
    // flushed in the middle of this loop.
    IsCompiledScope is_compiled_scope(*shared_info, isolate);
    if (!is_compiled_scope.is_compiled()) continue;

    if (need_source_positions) {
      SharedFunctionInfo::EnsureSourcePositionsAvailable(isolate, shared_info);
    }
    CodeEventListener::LogEventsAndTags log_tag;
    if (shared_info->is_toplevel()) {
      log_tag = flags.is_eval() ? CodeEventListener::EVAL_TAG
                                : CodeEventListener::SCRIPT_TAG;
    } else {
      log_tag = flags.is_lazy_compile() ? CodeEventListener::LAZY_COMPILE_TAG
                                        : CodeEventListener::FUNCTION_TAG;
    }
    log_tag = Logger::ToNativeByScript(log_tag, *script);
    if (FLAG_interpreted_frames_native_stack) {
      InstallInterpreterTrampolineCopy(isolate, shared_info, log_tag);
    }
    Handle<CoverageInfo> coverage_info;
    if (finalize_data.coverage_info().ToHandle(&coverage_info)) {
      isolate->debug()->InstallCoverageInfo(shared_info, coverage_info);
    }

    LogUnoptimizedCompilation(isolate, shared_info, log_tag,
                              finalize_data.time_taken_to_execute(),
                              finalize_data.time_taken_to_finalize());
  }
}

void FinalizeUnoptimizedScriptCompilation(
    Isolate* isolate, Handle<Script> script,
    const UnoptimizedCompileFlags& flags,
    const UnoptimizedCompileState* compile_state,
    const FinalizeUnoptimizedCompilationDataList&
        finalize_unoptimized_compilation_data_list) {
  FinalizeUnoptimizedCompilation(isolate, script, flags, compile_state,
                                 finalize_unoptimized_compilation_data_list);

  script->set_compilation_state(Script::COMPILATION_STATE_COMPILED);

  UnoptimizedCompileState::ParallelTasks* parallel_tasks =
      compile_state->parallel_tasks();
  if (parallel_tasks) {
    CompilerDispatcher* dispatcher = parallel_tasks->dispatcher();
    for (auto& it : *parallel_tasks) {
      FunctionLiteral* literal = it.first;
      CompilerDispatcher::JobId job_id = it.second;
      MaybeHandle<SharedFunctionInfo> maybe_shared_for_task =
          script->FindSharedFunctionInfo(isolate,
                                         literal->function_literal_id());
      Handle<SharedFunctionInfo> shared_for_task;
      if (maybe_shared_for_task.ToHandle(&shared_for_task)) {
        dispatcher->RegisterSharedFunctionInfo(job_id, *shared_for_task);
      } else {
        dispatcher->AbortJob(job_id);
      }
    }
  }

  if (isolate->NeedsSourcePositionsForProfiling()) {
    Script::InitLineEnds(isolate, script);
  }
}

void CompileAllWithBaseline(Isolate* isolate,
                            const FinalizeUnoptimizedCompilationDataList&
                                finalize_unoptimized_compilation_data_list) {
  for (const auto& finalize_data : finalize_unoptimized_compilation_data_list) {
    Handle<SharedFunctionInfo> shared_info = finalize_data.function_handle();
    IsCompiledScope is_compiled_scope(*shared_info, isolate);
    if (!is_compiled_scope.is_compiled()) continue;
    if (!CanCompileWithBaseline(isolate, shared_info)) continue;
    CompileSharedWithBaseline(isolate, shared_info, Compiler::CLEAR_EXCEPTION,
                              &is_compiled_scope);
  }
}

// Create shared function info for top level and shared function infos array for
// inner functions.
template <typename LocalIsolate>
Handle<SharedFunctionInfo> CreateTopLevelSharedFunctionInfo(
    ParseInfo* parse_info, Handle<Script> script, LocalIsolate* isolate) {
  EnsureSharedFunctionInfosArrayOnScript(script, parse_info, isolate);
  DCHECK_EQ(kNoSourcePosition,
            parse_info->literal()->function_token_position());
  return isolate->factory()->NewSharedFunctionInfoForLiteral(
      parse_info->literal(), script, true);
}

MaybeHandle<SharedFunctionInfo> CompileToplevel(
    ParseInfo* parse_info, Handle<Script> script,
    MaybeHandle<ScopeInfo> maybe_outer_scope_info, Isolate* isolate,
    IsCompiledScope* is_compiled_scope) {
  TimerEventScope<TimerEventCompileCode> top_level_timer(isolate);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.CompileCode");
  DCHECK_EQ(ThreadId::Current(), isolate->thread_id());

  PostponeInterruptsScope postpone(isolate);
  DCHECK(!isolate->native_context().is_null());
  RuntimeCallTimerScope runtimeTimer(
      isolate, parse_info->flags().is_eval()
                   ? RuntimeCallCounterId::kCompileEval
                   : RuntimeCallCounterId::kCompileScript);
  VMState<BYTECODE_COMPILER> state(isolate);
  if (parse_info->literal() == nullptr &&
      !parsing::ParseProgram(parse_info, script, maybe_outer_scope_info,
                             isolate, parsing::ReportStatisticsMode::kYes)) {
    FailWithPendingException(isolate, script, parse_info,
                             Compiler::ClearExceptionFlag::KEEP_EXCEPTION);
    return MaybeHandle<SharedFunctionInfo>();
  }
  // Measure how long it takes to do the compilation; only take the
  // rest of the function into account to avoid overlap with the
  // parsing statistics.
  HistogramTimer* rate = parse_info->flags().is_eval()
                             ? isolate->counters()->compile_eval()
                             : isolate->counters()->compile();
  HistogramTimerScope timer(rate);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               parse_info->flags().is_eval() ? "V8.CompileEval" : "V8.Compile");

  // Prepare and execute compilation of the outer-most function.

  // Create the SharedFunctionInfo and add it to the script's list.
  Handle<SharedFunctionInfo> shared_info =
      CreateTopLevelSharedFunctionInfo(parse_info, script, isolate);

  FinalizeUnoptimizedCompilationDataList
      finalize_unoptimized_compilation_data_list;

  if (!IterativelyExecuteAndFinalizeUnoptimizedCompilationJobs(
          isolate, shared_info, script, parse_info, isolate->allocator(),
          is_compiled_scope, &finalize_unoptimized_compilation_data_list,
          nullptr)) {
    FailWithPendingException(isolate, script, parse_info,
                             Compiler::ClearExceptionFlag::KEEP_EXCEPTION);
    return MaybeHandle<SharedFunctionInfo>();
  }

  // Character stream shouldn't be used again.
  parse_info->ResetCharacterStream();

  FinalizeUnoptimizedScriptCompilation(
      isolate, script, parse_info->flags(), parse_info->state(),
      finalize_unoptimized_compilation_data_list);

  if (FLAG_always_sparkplug) {
    CompileAllWithBaseline(isolate, finalize_unoptimized_compilation_data_list);
  }

  return shared_info;
}

RuntimeCallCounterId RuntimeCallCounterIdForCompileBackground(
    ParseInfo* parse_info) {
  if (parse_info->flags().is_toplevel()) {
    if (parse_info->flags().is_eval()) {
      return RuntimeCallCounterId::kCompileBackgroundEval;
    }
    return RuntimeCallCounterId::kCompileBackgroundScript;
  }
  return RuntimeCallCounterId::kCompileBackgroundFunction;
}

MaybeHandle<SharedFunctionInfo> CompileAndFinalizeOnBackgroundThread(
    ParseInfo* parse_info, AccountingAllocator* allocator,
    Handle<Script> script, LocalIsolate* isolate,
    FinalizeUnoptimizedCompilationDataList*
        finalize_unoptimized_compilation_data_list,
    DeferredFinalizationJobDataList* jobs_to_retry_finalization_on_main_thread,
    IsCompiledScope* is_compiled_scope) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.CompileCodeBackground");
  RuntimeCallTimerScope runtimeTimer(
      parse_info->runtime_call_stats(),
      RuntimeCallCounterIdForCompileBackground(parse_info));

  Handle<SharedFunctionInfo> shared_info =
      CreateTopLevelSharedFunctionInfo(parse_info, script, isolate);

  if (!IterativelyExecuteAndFinalizeUnoptimizedCompilationJobs(
          isolate, shared_info, script, parse_info, allocator,
          is_compiled_scope, finalize_unoptimized_compilation_data_list,
          jobs_to_retry_finalization_on_main_thread)) {
    return kNullMaybeHandle;
  }

  // Character stream shouldn't be used again.
  parse_info->ResetCharacterStream();

  return shared_info;
}

// TODO(leszeks): Remove this once off-thread finalization is always on.
void CompileOnBackgroundThread(ParseInfo* parse_info,
                               AccountingAllocator* allocator,
                               UnoptimizedCompilationJobList* jobs) {
  DisallowHeapAccess no_heap_access;
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.CompileCodeBackground");
  RuntimeCallTimerScope runtimeTimer(
      parse_info->runtime_call_stats(),
      RuntimeCallCounterIdForCompileBackground(parse_info));

  // Generate the unoptimized bytecode or asm-js data.
  DCHECK(jobs->empty());

  bool success = RecursivelyExecuteUnoptimizedCompilationJobs(
      parse_info, parse_info->literal(), allocator, jobs);

  USE(success);
  DCHECK_EQ(success, !jobs->empty());

  // Character stream shouldn't be used again.
  parse_info->ResetCharacterStream();
}
}  // namespace

CompilationHandleScope::~CompilationHandleScope() {
  info_->set_persistent_handles(persistent_.Detach());
}

FinalizeUnoptimizedCompilationData::FinalizeUnoptimizedCompilationData(
    LocalIsolate* isolate, Handle<SharedFunctionInfo> function_handle,
    MaybeHandle<CoverageInfo> coverage_info,
    base::TimeDelta time_taken_to_execute,
    base::TimeDelta time_taken_to_finalize)
    : time_taken_to_execute_(time_taken_to_execute),
      time_taken_to_finalize_(time_taken_to_finalize),
      function_handle_(isolate->heap()->NewPersistentHandle(function_handle)),
      coverage_info_(isolate->heap()->NewPersistentMaybeHandle(coverage_info)) {
}

DeferredFinalizationJobData::DeferredFinalizationJobData(
    LocalIsolate* isolate, Handle<SharedFunctionInfo> function_handle,
    std::unique_ptr<UnoptimizedCompilationJob> job)
    : function_handle_(isolate->heap()->NewPersistentHandle(function_handle)),
      job_(std::move(job)) {}

BackgroundCompileTask::BackgroundCompileTask(ScriptStreamingData* streamed_data,
                                             Isolate* isolate, ScriptType type)
    : flags_(UnoptimizedCompileFlags::ForToplevelCompile(
          isolate, true, construct_language_mode(FLAG_use_strict),
          REPLMode::kNo, type)),
      compile_state_(isolate),
      info_(std::make_unique<ParseInfo>(isolate, flags_, &compile_state_)),
      isolate_for_local_isolate_(isolate),
      start_position_(0),
      end_position_(0),
      function_literal_id_(kFunctionLiteralIdTopLevel),
      stack_size_(i::FLAG_stack_size),
      worker_thread_runtime_call_stats_(
          isolate->counters()->worker_thread_runtime_call_stats()),
      timer_(isolate->counters()->compile_script_on_background()),
      language_mode_(info_->language_mode()) {
  VMState<PARSER> state(isolate);

  // Prepare the data for the internalization phase and compilation phase, which
  // will happen in the main thread after parsing.

  LOG(isolate, ScriptEvent(Logger::ScriptEventType::kStreamingCompile,
                           info_->flags().script_id()));

  std::unique_ptr<Utf16CharacterStream> stream(ScannerStream::For(
      streamed_data->source_stream.get(), streamed_data->encoding));
  info_->set_character_stream(std::move(stream));
}

BackgroundCompileTask::BackgroundCompileTask(
    const ParseInfo* outer_parse_info, const AstRawString* function_name,
    const FunctionLiteral* function_literal,
    WorkerThreadRuntimeCallStats* worker_thread_runtime_stats,
    TimedHistogram* timer, int max_stack_size)
    : flags_(UnoptimizedCompileFlags::ForToplevelFunction(
          outer_parse_info->flags(), function_literal)),
      compile_state_(*outer_parse_info->state()),
      info_(ParseInfo::ForToplevelFunction(flags_, &compile_state_,
                                           function_literal, function_name)),
      isolate_for_local_isolate_(nullptr),
      start_position_(function_literal->start_position()),
      end_position_(function_literal->end_position()),
      function_literal_id_(function_literal->function_literal_id()),
      stack_size_(max_stack_size),
      worker_thread_runtime_call_stats_(worker_thread_runtime_stats),
      timer_(timer),
      language_mode_(info_->language_mode()) {
  DCHECK_EQ(outer_parse_info->parameters_end_pos(), kNoSourcePosition);
  DCHECK_NULL(outer_parse_info->extension());

  DCHECK(!function_literal->is_toplevel());

  // Clone the character stream so both can be accessed independently.
  std::unique_ptr<Utf16CharacterStream> character_stream =
      outer_parse_info->character_stream()->Clone();
  character_stream->Seek(start_position_);
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

// A scope object that ensures a parse info's runtime call stats and stack limit
// are set correctly during worker-thread compile, and restores it after going
// out of scope.
class V8_NODISCARD OffThreadParseInfoScope {
 public:
  OffThreadParseInfoScope(
      ParseInfo* parse_info,
      WorkerThreadRuntimeCallStats* worker_thread_runtime_stats, int stack_size)
      : parse_info_(parse_info),
        original_runtime_call_stats_(parse_info_->runtime_call_stats()),
        original_stack_limit_(parse_info_->stack_limit()),
        worker_thread_scope_(worker_thread_runtime_stats) {
    parse_info_->SetPerThreadState(GetCurrentStackPosition() - stack_size * KB,
                                   worker_thread_scope_.Get());
  }

  OffThreadParseInfoScope(const OffThreadParseInfoScope&) = delete;
  OffThreadParseInfoScope& operator=(const OffThreadParseInfoScope&) = delete;

  ~OffThreadParseInfoScope() {
    DCHECK_NOT_NULL(parse_info_);
    parse_info_->SetPerThreadState(original_stack_limit_,
                                   original_runtime_call_stats_);
  }

 private:
  ParseInfo* parse_info_;
  RuntimeCallStats* original_runtime_call_stats_;
  uintptr_t original_stack_limit_;
  WorkerThreadRuntimeCallStatsScope worker_thread_scope_;
};

}  // namespace

void BackgroundCompileTask::Run() {
  TimedHistogramScope timer(timer_);
  base::Optional<OffThreadParseInfoScope> off_thread_scope(
      base::in_place, info_.get(), worker_thread_runtime_call_stats_,
      stack_size_);
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

  parser_->ParseOnBackground(info_.get(), start_position_, end_position_,
                             function_literal_id_);

  // Save the language mode.
  language_mode_ = info_->language_mode();

  if (!FLAG_finalize_streaming_on_background || info_->flags().is_module()) {
    if (info_->literal() != nullptr) {
      CompileOnBackgroundThread(info_.get(), compile_state_.allocator(),
                                &compilation_jobs_);
    }
  } else {
    DCHECK(info_->flags().is_toplevel());

    {
      LocalIsolate isolate(isolate_for_local_isolate_, ThreadKind::kBackground);
      UnparkedScope unparked_scope(&isolate);
      LocalHandleScope handle_scope(&isolate);

      info_->ast_value_factory()->Internalize(&isolate);

      // We don't have the script source, origin, or details yet, so use default
      // values for them. These will be fixed up during the main-thread merge.
      Handle<Script> script = info_->CreateScript(
          &isolate, isolate.factory()->empty_string(), kNullMaybeHandle,
          ScriptOriginOptions(false, false, false, info_->flags().is_module()));

      parser_->HandleSourceURLComments(&isolate, script);

      MaybeHandle<SharedFunctionInfo> maybe_result;
      if (info_->literal() != nullptr) {
        maybe_result = CompileAndFinalizeOnBackgroundThread(
            info_.get(), compile_state_.allocator(), script, &isolate,
            &finalize_unoptimized_compilation_data_,
            &jobs_to_retry_finalization_on_main_thread_, &is_compiled_scope_);
      } else {
        DCHECK(compile_state_.pending_error_handler()->has_pending_error());
        PreparePendingException(&isolate, info_.get());
      }

      outer_function_sfi_ =
          isolate.heap()->NewPersistentMaybeHandle(maybe_result);
      script_ = isolate.heap()->NewPersistentHandle(script);

      persistent_handles_ = isolate.heap()->DetachPersistentHandles();
    }

    {
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                   "V8.FinalizeCodeBackground.ReleaseParser");
      DCHECK_EQ(language_mode_, info_->language_mode());
      off_thread_scope.reset();
      parser_.reset();
      info_.reset();
    }
  }
}

MaybeHandle<SharedFunctionInfo> BackgroundCompileTask::GetOuterFunctionSfi(
    Isolate* isolate) {
  // outer_function_sfi_ is a persistent Handle, tied to the lifetime of the
  // persistent_handles_ member, so create a new Handle to let it outlive
  // the BackgroundCompileTask.
  Handle<SharedFunctionInfo> result;
  if (outer_function_sfi_.ToHandle(&result)) {
    return handle(*result, isolate);
  }
  return kNullMaybeHandle;
}

Handle<Script> BackgroundCompileTask::GetScript(Isolate* isolate) {
  // script_ is a persistent Handle, tied to the lifetime of the
  // persistent_handles_ member, so create a new Handle to let it outlive
  // the BackgroundCompileTask.
  return handle(*script_, isolate);
}

// ----------------------------------------------------------------------------
// Implementation of Compiler

// static
bool Compiler::CollectSourcePositions(Isolate* isolate,
                                      Handle<SharedFunctionInfo> shared_info) {
  DCHECK(shared_info->is_compiled());
  DCHECK(shared_info->HasBytecodeArray());
  DCHECK(!shared_info->GetBytecodeArray(isolate).HasSourcePositionTable());

  // Source position collection should be context independent.
  NullContextScope null_context_scope(isolate);

  // Collecting source positions requires allocating a new source position
  // table.
  DCHECK(AllowHeapAllocation::IsAllowed());

  Handle<BytecodeArray> bytecode =
      handle(shared_info->GetBytecodeArray(isolate), isolate);

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
  UnoptimizedCompileFlags flags =
      UnoptimizedCompileFlags::ForFunctionCompile(isolate, *shared_info);
  flags.set_is_lazy_compile(true);
  flags.set_collect_source_positions(true);
  flags.set_allow_natives_syntax(FLAG_allow_natives_syntax);

  UnoptimizedCompileState compile_state(isolate);
  ParseInfo parse_info(isolate, flags, &compile_state);

  // Parse and update ParseInfo with the results. Don't update parsing
  // statistics since we've already parsed the code before.
  if (!parsing::ParseAny(&parse_info, shared_info, isolate,
                         parsing::ReportStatisticsMode::kNo)) {
    // Parsing failed probably as a result of stack exhaustion.
    bytecode->SetSourcePositionsFailedToCollect();
    return FailAndClearPendingException(isolate);
  }

  // Character stream shouldn't be used again.
  parse_info.ResetCharacterStream();

  // Generate the unoptimized bytecode.
  // TODO(v8:8510): Consider forcing preparsing of inner functions to avoid
  // wasting time fully parsing them when they won't ever be used.
  std::unique_ptr<UnoptimizedCompilationJob> job;
  {
    job = interpreter::Interpreter::NewSourcePositionCollectionJob(
        &parse_info, parse_info.literal(), bytecode, isolate->allocator(),
        isolate->main_thread_local_isolate());

    if (!job || job->ExecuteJob() != CompilationJob::SUCCEEDED ||
        job->FinalizeJob(shared_info, isolate) != CompilationJob::SUCCEEDED) {
      // Recompiling failed probably as a result of stack exhaustion.
      bytecode->SetSourcePositionsFailedToCollect();
      return FailAndClearPendingException(isolate);
    }
  }

  DCHECK(job->compilation_info()->flags().collect_source_positions());

  // If debugging, make sure that instrumented bytecode has the source position
  // table set on it as well.
  if (shared_info->HasDebugInfo() &&
      shared_info->GetDebugInfo().HasInstrumentedBytecodeArray()) {
    ByteArray source_position_table =
        job->compilation_info()->bytecode_array()->SourcePositionTable();
    shared_info->GetActiveBytecodeArray().set_source_position_table(
        source_position_table, kReleaseStore);
  }

  DCHECK(!isolate->has_pending_exception());
  DCHECK(shared_info->is_compiled_scope(isolate).is_compiled());
  return true;
}

// static
bool Compiler::Compile(Isolate* isolate, Handle<SharedFunctionInfo> shared_info,
                       ClearExceptionFlag flag,
                       IsCompiledScope* is_compiled_scope) {
  // We should never reach here if the function is already compiled.
  DCHECK(!shared_info->is_compiled());
  DCHECK(!is_compiled_scope->is_compiled());
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

  Handle<Script> script(Script::cast(shared_info->script()), isolate);

  // Set up parse info.
  UnoptimizedCompileFlags flags =
      UnoptimizedCompileFlags::ForFunctionCompile(isolate, *shared_info);
  flags.set_is_lazy_compile(true);

  UnoptimizedCompileState compile_state(isolate);
  ParseInfo parse_info(isolate, flags, &compile_state);

  // Check if the compiler dispatcher has shared_info enqueued for compile.
  CompilerDispatcher* dispatcher = isolate->compiler_dispatcher();
  if (dispatcher->IsEnqueued(shared_info)) {
    if (!dispatcher->FinishNow(shared_info)) {
      return FailWithPendingException(isolate, script, &parse_info, flag);
    }
    *is_compiled_scope = shared_info->is_compiled_scope(isolate);
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
  if (!parsing::ParseAny(&parse_info, shared_info, isolate,
                         parsing::ReportStatisticsMode::kYes)) {
    return FailWithPendingException(isolate, script, &parse_info, flag);
  }

  // Generate the unoptimized bytecode or asm-js data.
  FinalizeUnoptimizedCompilationDataList
      finalize_unoptimized_compilation_data_list;

  if (!IterativelyExecuteAndFinalizeUnoptimizedCompilationJobs(
          isolate, shared_info, script, &parse_info, isolate->allocator(),
          is_compiled_scope, &finalize_unoptimized_compilation_data_list,
          nullptr)) {
    return FailWithPendingException(isolate, script, &parse_info, flag);
  }

  FinalizeUnoptimizedCompilation(isolate, script, flags, &compile_state,
                                 finalize_unoptimized_compilation_data_list);

  if (FLAG_always_sparkplug) {
    CompileAllWithBaseline(isolate, finalize_unoptimized_compilation_data_list);
  }

  DCHECK(!isolate->has_pending_exception());
  DCHECK(is_compiled_scope->is_compiled());
  return true;
}

// static
bool Compiler::Compile(Isolate* isolate, Handle<JSFunction> function,
                       ClearExceptionFlag flag,
                       IsCompiledScope* is_compiled_scope) {
  // We should never reach here if the function is already compiled or
  // optimized.
  DCHECK(!function->is_compiled());
  DCHECK(!function->HasOptimizationMarker());
  DCHECK(!function->HasAvailableOptimizedCode());

  // Reset the JSFunction if we are recompiling due to the bytecode having been
  // flushed.
  function->ResetIfBytecodeFlushed();

  Handle<SharedFunctionInfo> shared_info = handle(function->shared(), isolate);

  // Ensure shared function info is compiled.
  *is_compiled_scope = shared_info->is_compiled_scope(isolate);
  if (!is_compiled_scope->is_compiled() &&
      !Compile(isolate, shared_info, flag, is_compiled_scope)) {
    return false;
  }

  DCHECK(is_compiled_scope->is_compiled());
  Handle<Code> code = handle(shared_info->GetCode(), isolate);

  // Initialize the feedback cell for this JSFunction and reset the interrupt
  // budget for feedback vector allocation even if there is a closure feedback
  // cell array. We are re-compiling when we have a closure feedback cell array
  // which means we are compiling after a bytecode flush.
  // TODO(verwaest/mythria): Investigate if allocating feedback vector
  // immediately after a flush would be better.
  JSFunction::InitializeFeedbackCell(function, is_compiled_scope, true);

  // Optimize now if --always-opt is enabled.
#if V8_ENABLE_WEBASSEMBLY
  if (FLAG_always_opt && !function->shared().HasAsmWasmData()) {
#else
  if (FLAG_always_opt) {
#endif  // V8_ENABLE_WEBASSEMBLY
    CompilerTracer::TraceOptimizeForAlwaysOpt(isolate, function,
                                              CodeKindForTopTier());

    Handle<Code> maybe_code;
    if (GetOptimizedCode(function, ConcurrencyMode::kNotConcurrent,
                         CodeKindForTopTier())
            .ToHandle(&maybe_code)) {
      code = maybe_code;
    }
  }

  // Install code on closure.
  function->set_code(*code, kReleaseStore);

  // Install a feedback vector if necessary.
  if (code->kind() == CodeKind::BASELINE) {
    JSFunction::EnsureFeedbackVector(function, is_compiled_scope);
  }

  // Check postconditions on success.
  DCHECK(!isolate->has_pending_exception());
  DCHECK(function->shared().is_compiled());
  DCHECK(function->is_compiled());
  return true;
}

// static
bool Compiler::CompileBaseline(Isolate* isolate, Handle<JSFunction> function,
                               ClearExceptionFlag flag,
                               IsCompiledScope* is_compiled_scope) {
  Handle<SharedFunctionInfo> shared(function->shared(isolate), isolate);
  if (!CompileSharedWithBaseline(isolate, shared, flag, is_compiled_scope)) {
    return false;
  }

  // Baseline code needs a feedback vector.
  JSFunction::EnsureFeedbackVector(function, is_compiled_scope);

  Code baseline_code = shared->baseline_data().baseline_code(isolate);
  DCHECK_EQ(baseline_code.kind(), CodeKind::BASELINE);
  function->set_code(baseline_code);

  return true;
}

// static
MaybeHandle<SharedFunctionInfo> Compiler::CompileToplevel(
    ParseInfo* parse_info, Handle<Script> script, Isolate* isolate,
    IsCompiledScope* is_compiled_scope) {
  return v8::internal::CompileToplevel(parse_info, script, kNullMaybeHandle,
                                       isolate, is_compiled_scope);
}

// static
bool Compiler::FinalizeBackgroundCompileTask(
    BackgroundCompileTask* task, Handle<SharedFunctionInfo> shared_info,
    Isolate* isolate, ClearExceptionFlag flag) {
  DCHECK(!FLAG_finalize_streaming_on_background);

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.FinalizeBackgroundCompileTask");
  RuntimeCallTimerScope runtimeTimer(
      isolate, RuntimeCallCounterId::kCompileFinalizeBackgroundCompileTask);
  HandleScope scope(isolate);
  ParseInfo* parse_info = task->info();
  DCHECK(!parse_info->flags().is_toplevel());
  DCHECK(!shared_info->is_compiled());

  Handle<Script> script(Script::cast(shared_info->script()), isolate);
  parse_info->CheckFlagsForFunctionFromScript(*script);

  task->parser()->UpdateStatistics(isolate, script);
  task->parser()->HandleSourceURLComments(isolate, script);

  if (task->compilation_jobs()->empty()) {
    // Parsing or compile failed on background thread - report error messages.
    return FailWithPendingException(isolate, script, parse_info, flag);
  }

  // Parsing has succeeded - finalize compilation.
  parse_info->ast_value_factory()->Internalize(isolate);
  if (!FinalizeAllUnoptimizedCompilationJobs(
          parse_info, isolate, script, task->compilation_jobs(),
          task->finalize_unoptimized_compilation_data())) {
    // Finalization failed - throw an exception.
    return FailWithPendingException(isolate, script, parse_info, flag);
  }
  FinalizeUnoptimizedCompilation(
      isolate, script, parse_info->flags(), parse_info->state(),
      *task->finalize_unoptimized_compilation_data());

  DCHECK(!isolate->has_pending_exception());
  DCHECK(shared_info->is_compiled());
  return true;
}

// static
bool Compiler::CompileOptimized(Isolate* isolate, Handle<JSFunction> function,
                                ConcurrencyMode mode, CodeKind code_kind) {
  DCHECK(CodeKindIsOptimizedJSFunction(code_kind));
  DCHECK(AllowCompilation::IsAllowed(isolate));

  Handle<Code> code;
  if (!GetOptimizedCode(function, mode, code_kind).ToHandle(&code)) {
    // Optimization failed, get the existing code. We could have optimized code
    // from a lower tier here. Unoptimized code must exist already if we are
    // optimizing.
    DCHECK(!isolate->has_pending_exception());
    DCHECK(function->shared().is_compiled());
    DCHECK(function->shared().IsInterpreted());
    code = ContinuationForConcurrentOptimization(isolate, function);
  }

  if (!CodeKindIsNativeContextIndependentJSFunction(code_kind)) {
    function->set_code(*code, kReleaseStore);
  }

  // Check postconditions on success.
  DCHECK(!isolate->has_pending_exception());
  DCHECK(function->shared().is_compiled());
  DCHECK(CodeKindIsNativeContextIndependentJSFunction(code_kind) ||
         function->is_compiled());
  if (!CodeKindIsNativeContextIndependentJSFunction(code_kind)) {
    DCHECK_IMPLIES(function->HasOptimizationMarker(),
                   function->IsInOptimizationQueue());
    DCHECK_IMPLIES(function->HasOptimizationMarker(),
                   function->ChecksOptimizationMarker());
    DCHECK_IMPLIES(function->IsInOptimizationQueue(),
                   mode == ConcurrencyMode::kConcurrent);
  }
  return true;
}

// static
MaybeHandle<SharedFunctionInfo> Compiler::CompileForLiveEdit(
    ParseInfo* parse_info, Handle<Script> script, Isolate* isolate) {
  IsCompiledScope is_compiled_scope;
  return Compiler::CompileToplevel(parse_info, script, isolate,
                                   &is_compiled_scope);
}

// static
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
    is_compiled_scope = shared_info->is_compiled_scope(isolate);
    allow_eval_cache = true;
  } else {
    UnoptimizedCompileFlags flags = UnoptimizedCompileFlags::ForToplevelCompile(
        isolate, true, language_mode, REPLMode::kNo);
    flags.set_is_eval(true);
    DCHECK(!flags.is_module());
    flags.set_parse_restriction(restriction);

    UnoptimizedCompileState compile_state(isolate);
    ParseInfo parse_info(isolate, flags, &compile_state);
    parse_info.set_parameters_end_pos(parameters_end_pos);

    MaybeHandle<ScopeInfo> maybe_outer_scope_info;
    if (!context->IsNativeContext()) {
      maybe_outer_scope_info = handle(context->scope_info(), isolate);
    }
    script =
        parse_info.CreateScript(isolate, source, kNullMaybeHandle,
                                OriginOptionsForEval(outer_info->script()));
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

    if (!v8::internal::CompileToplevel(&parse_info, script,
                                       maybe_outer_scope_info, isolate,
                                       &is_compiled_scope)
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
      result = Factory::JSFunctionBuilder{isolate, shared_info, context}
                   .set_feedback_cell(feedback_cell)
                   .set_allocation_type(AllocationType::kYoung)
                   .Build();
    } else {
      result = Factory::JSFunctionBuilder{isolate, shared_info, context}
                   .set_allocation_type(AllocationType::kYoung)
                   .Build();
      // TODO(mythria): I don't think we need this here. PostInstantiation
      // already initializes feedback cell.
      JSFunction::InitializeFeedbackCell(result, &is_compiled_scope, true);
      if (allow_eval_cache) {
        // Make sure to cache this result.
        Handle<FeedbackCell> new_feedback_cell(result->raw_feedback_cell(),
                                               isolate);
        compilation_cache->PutEval(source, outer_info, context, shared_info,
                                   new_feedback_cell, eval_scope_position);
      }
    }
  } else {
    result = Factory::JSFunctionBuilder{isolate, shared_info, context}
                 .set_allocation_type(AllocationType::kYoung)
                 .Build();
    // TODO(mythria): I don't think we need this here. PostInstantiation
    // already initializes feedback cell.
    JSFunction::InitializeFeedbackCell(result, &is_compiled_scope, true);
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
// (via v8::Isolate::SetModifyCodeGenerationFromStringsCallback
//  or v8::Isolate::SetModifyCodeGenerationFromStringsCallback2)
bool ModifyCodeGenerationFromStrings(Isolate* isolate, Handle<Context> context,
                                     Handle<i::Object>* source,
                                     bool is_code_like) {
  DCHECK(isolate->modify_code_gen_callback() ||
         isolate->modify_code_gen_callback2());
  DCHECK(source);

  // Callback set. Run it, and use the return value as source, or block
  // execution if it's not set.
  VMState<EXTERNAL> state(isolate);
  RuntimeCallTimerScope timer(
      isolate, RuntimeCallCounterId::kCodeGenerationFromStringsCallbacks);
  ModifyCodeGenerationFromStringsResult result =
      isolate->modify_code_gen_callback()
          ? isolate->modify_code_gen_callback()(v8::Utils::ToLocal(context),
                                                v8::Utils::ToLocal(*source))
          : isolate->modify_code_gen_callback2()(v8::Utils::ToLocal(context),
                                                 v8::Utils::ToLocal(*source),
                                                 is_code_like);
  if (result.codegen_allowed && !result.modified_source.IsEmpty()) {
    // Use the new source (which might be the same as the old source).
    *source =
        Utils::OpenHandle(*result.modified_source.ToLocalChecked(), false);
  }
  return result.codegen_allowed;
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

// static
std::pair<MaybeHandle<String>, bool> Compiler::ValidateDynamicCompilationSource(
    Isolate* isolate, Handle<Context> context,
    Handle<i::Object> original_source, bool is_code_like) {
  // Check if the context unconditionally allows code gen from strings.
  // allow_code_gen_from_strings can be many things, so we'll always check
  // against the 'false' literal, so that e.g. undefined and 'true' are treated
  // the same.
  if (!context->allow_code_gen_from_strings().IsFalse(isolate) &&
      original_source->IsString()) {
    return {Handle<String>::cast(original_source), false};
  }

  // Check if the context allows code generation for this string.
  // allow_code_gen_callback only allows proper strings.
  // (I.e., let allow_code_gen_callback decide, if it has been set.)
  if (isolate->allow_code_gen_callback()) {
    // If we run into this condition, the embedder has marked some object
    // templates as "code like", but has given us a callback that only accepts
    // strings. That makes no sense.
    DCHECK(!original_source->IsCodeLike(isolate));

    if (!original_source->IsString()) {
      return {MaybeHandle<String>(), true};
    }
    Handle<String> string_source = Handle<String>::cast(original_source);
    if (!CodeGenerationFromStringsAllowed(isolate, context, string_source)) {
      return {MaybeHandle<String>(), false};
    }
    return {string_source, false};
  }

  // Check if the context wants to block or modify this source object.
  // Double-check that we really have a string now.
  // (Let modify_code_gen_callback decide, if it's been set.)
  if (isolate->modify_code_gen_callback() ||
      isolate->modify_code_gen_callback2()) {
    Handle<i::Object> modified_source = original_source;
    if (!ModifyCodeGenerationFromStrings(isolate, context, &modified_source,
                                         is_code_like)) {
      return {MaybeHandle<String>(), false};
    }
    if (!modified_source->IsString()) {
      return {MaybeHandle<String>(), true};
    }
    return {Handle<String>::cast(modified_source), false};
  }

  if (!context->allow_code_gen_from_strings().IsFalse(isolate) &&
      original_source->IsCodeLike(isolate)) {
    // Codegen is unconditionally allowed, and we're been given a CodeLike
    // object. Stringify.
    MaybeHandle<String> stringified_source =
        Object::ToString(isolate, original_source);
    return {stringified_source, stringified_source.is_null()};
  }

  // If unconditional codegen was disabled, and no callback defined, we block
  // strings and allow all other objects.
  return {MaybeHandle<String>(), !original_source->IsString()};
}

// static
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

// static
MaybeHandle<JSFunction> Compiler::GetFunctionFromString(
    Handle<Context> context, Handle<Object> source,
    ParseRestriction restriction, int parameters_end_pos, bool is_code_like) {
  Isolate* const isolate = context->GetIsolate();
  MaybeHandle<String> validated_source =
      ValidateDynamicCompilationSource(isolate, context, source, is_code_like)
          .first;
  return GetFunctionFromValidatedString(context, validated_source, restriction,
                                        parameters_end_pos);
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

      // Note that this only counts the finalization part of streaming, the
      // actual streaming compile is counted by BackgroundCompileTask into
      // "compile_script_on_background".
      case CacheBehaviour::kNoCacheBecauseStreamingSource:
        return isolate_->counters()->compile_script_streaming_finalization();

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

void SetScriptFieldsFromDetails(Isolate* isolate, Script script,
                                Compiler::ScriptDetails script_details,
                                DisallowGarbageCollection* no_gc) {
  Handle<Object> script_name;
  if (script_details.name_obj.ToHandle(&script_name)) {
    script.set_name(*script_name);
    script.set_line_offset(script_details.line_offset);
    script.set_column_offset(script_details.column_offset);
  }
  // The API can provide a source map URL, but a source map URL could also have
  // been inferred by the parser from a magic comment. The latter takes
  // preference over the former, so we don't want to override the source mapping
  // URL if it already exists.
  Handle<Object> source_map_url;
  if (script_details.source_map_url.ToHandle(&source_map_url) &&
      script.source_mapping_url(isolate).IsUndefined(isolate)) {
    script.set_source_mapping_url(*source_map_url);
  }
  Handle<FixedArray> host_defined_options;
  if (script_details.host_defined_options.ToHandle(&host_defined_options)) {
    script.set_host_defined_options(*host_defined_options);
  }
}

Handle<Script> NewScript(
    Isolate* isolate, ParseInfo* parse_info, Handle<String> source,
    Compiler::ScriptDetails script_details, ScriptOriginOptions origin_options,
    NativesFlag natives,
    MaybeHandle<FixedArray> maybe_wrapped_arguments = kNullMaybeHandle) {
  // Create a script object describing the script to be compiled.
  Handle<Script> script = parse_info->CreateScript(
      isolate, source, maybe_wrapped_arguments, origin_options, natives);
  DisallowGarbageCollection no_gc;
  SetScriptFieldsFromDetails(isolate, *script, script_details, &no_gc);
  LOG(isolate, ScriptDetails(*script));
  return script;
}

MaybeHandle<SharedFunctionInfo> CompileScriptOnMainThread(
    const UnoptimizedCompileFlags flags, Handle<String> source,
    const Compiler::ScriptDetails& script_details,
    ScriptOriginOptions origin_options, NativesFlag natives,
    v8::Extension* extension, Isolate* isolate,
    IsCompiledScope* is_compiled_scope) {
  UnoptimizedCompileState compile_state(isolate);
  ParseInfo parse_info(isolate, flags, &compile_state);
  parse_info.set_extension(extension);

  Handle<Script> script = NewScript(isolate, &parse_info, source,
                                    script_details, origin_options, natives);
  DCHECK_IMPLIES(parse_info.flags().collect_type_profile(),
                 script->IsUserJavaScript());
  DCHECK_EQ(parse_info.flags().is_repl_mode(), script->is_repl_mode());

  return Compiler::CompileToplevel(&parse_info, script, isolate,
                                   is_compiled_scope);
}

class StressBackgroundCompileThread : public base::Thread {
 public:
  StressBackgroundCompileThread(Isolate* isolate, Handle<String> source,
                                ScriptType type)
      : base::Thread(
            base::Thread::Options("StressBackgroundCompileThread", 2 * i::MB)),
        source_(source),
        streamed_source_(std::make_unique<SourceStream>(source, isolate),
                         v8::ScriptCompiler::StreamedSource::UTF8) {
    data()->task =
        std::make_unique<i::BackgroundCompileTask>(data(), isolate, type);
  }

  void Run() override { data()->task->Run(); }

  ScriptStreamingData* data() { return streamed_source_.impl(); }

 private:
  // Dummy external source stream which returns the whole source in one go.
  // TODO(leszeks): Also test chunking the data.
  class SourceStream : public v8::ScriptCompiler::ExternalSourceStream {
   public:
    SourceStream(Handle<String> source, Isolate* isolate) : done_(false) {
      source_buffer_ = source->ToCString(ALLOW_NULLS, FAST_STRING_TRAVERSAL,
                                         &source_length_);
    }

    size_t GetMoreData(const uint8_t** src) override {
      if (done_) {
        return 0;
      }
      *src = reinterpret_cast<uint8_t*>(source_buffer_.release());
      done_ = true;

      return source_length_;
    }

   private:
    int source_length_;
    std::unique_ptr<char[]> source_buffer_;
    bool done_;
  };

  Handle<String> source_;
  v8::ScriptCompiler::StreamedSource streamed_source_;
};

bool CanBackgroundCompile(const Compiler::ScriptDetails& script_details,
                          ScriptOriginOptions origin_options,
                          v8::Extension* extension,
                          ScriptCompiler::CompileOptions compile_options,
                          NativesFlag natives) {
  // TODO(leszeks): Remove the module check once background compilation of
  // modules is supported.
  return !origin_options.IsModule() && !extension &&
         script_details.repl_mode == REPLMode::kNo &&
         compile_options == ScriptCompiler::kNoCompileOptions &&
         natives == NOT_NATIVES_CODE;
}

bool CompilationExceptionIsRangeError(Isolate* isolate, Handle<Object> obj) {
  if (!obj->IsJSError(isolate)) return false;
  Handle<JSReceiver> js_obj = Handle<JSReceiver>::cast(obj);
  Handle<JSReceiver> constructor;
  if (!JSReceiver::GetConstructor(js_obj).ToHandle(&constructor)) {
    return false;
  }
  return *constructor == *isolate->range_error_function();
}

MaybeHandle<SharedFunctionInfo> CompileScriptOnBothBackgroundAndMainThread(
    Handle<String> source, const Compiler::ScriptDetails& script_details,
    ScriptOriginOptions origin_options, Isolate* isolate,
    IsCompiledScope* is_compiled_scope) {
  // Start a background thread compiling the script.
  StressBackgroundCompileThread background_compile_thread(
      isolate, source,
      origin_options.IsModule() ? ScriptType::kModule : ScriptType::kClassic);

  UnoptimizedCompileFlags flags_copy =
      background_compile_thread.data()->task->flags();

  CHECK(background_compile_thread.Start());
  MaybeHandle<SharedFunctionInfo> main_thread_maybe_result;
  bool main_thread_had_stack_overflow = false;
  // In parallel, compile on the main thread to flush out any data races.
  {
    IsCompiledScope inner_is_compiled_scope;
    // The background thread should also create any relevant exceptions, so we
    // can ignore the main-thread created ones.
    // TODO(leszeks): Maybe verify that any thrown (or unthrown) exceptions are
    // equivalent.
    TryCatch ignore_try_catch(reinterpret_cast<v8::Isolate*>(isolate));
    flags_copy.set_script_id(Script::kTemporaryScriptId);
    main_thread_maybe_result = CompileScriptOnMainThread(
        flags_copy, source, script_details, origin_options, NOT_NATIVES_CODE,
        nullptr, isolate, &inner_is_compiled_scope);
    if (main_thread_maybe_result.is_null()) {
      // Assume all range errors are stack overflows.
      main_thread_had_stack_overflow = CompilationExceptionIsRangeError(
          isolate, handle(isolate->pending_exception(), isolate));
      isolate->clear_pending_exception();
    }
  }

  // Join with background thread and finalize compilation.
  {
    ParkedScope scope(isolate->main_thread_local_isolate());
    background_compile_thread.Join();
  }

  MaybeHandle<SharedFunctionInfo> maybe_result =
      Compiler::GetSharedFunctionInfoForStreamedScript(
          isolate, source, script_details, origin_options,
          background_compile_thread.data());

  // Either both compiles should succeed, or both should fail. The one exception
  // to this is that the main-thread compilation might stack overflow while the
  // background compilation doesn't, so relax the check to include this case.
  // TODO(leszeks): Compare the contents of the results of the two compiles.
  if (main_thread_had_stack_overflow) {
    CHECK(main_thread_maybe_result.is_null());
  } else {
    CHECK_EQ(maybe_result.is_null(), main_thread_maybe_result.is_null());
  }

  Handle<SharedFunctionInfo> result;
  if (maybe_result.ToHandle(&result)) {
    // The BackgroundCompileTask's IsCompiledScope will keep the result alive
    // until it dies at the end of this function, after which this new
    // IsCompiledScope can take over.
    *is_compiled_scope = result->is_compiled_scope(isolate);
  }

  return maybe_result;
}

}  // namespace

// static
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

  // For extensions or REPL mode scripts neither do a compilation cache lookup,
  // nor put the compilation result back into the cache.
  const bool use_compilation_cache =
      extension == nullptr && script_details.repl_mode == REPLMode::kNo;
  MaybeHandle<SharedFunctionInfo> maybe_result;
  IsCompiledScope is_compiled_scope;
  if (use_compilation_cache) {
    bool can_consume_code_cache =
        compile_options == ScriptCompiler::kConsumeCodeCache;
    if (can_consume_code_cache) {
      compile_timer.set_consuming_code_cache();
    }

    // First check per-isolate compilation cache.
    maybe_result = compilation_cache->LookupScript(
        source, script_details.name_obj, script_details.line_offset,
        script_details.column_offset, origin_options, language_mode);
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
        is_compiled_scope = inner_result->is_compiled_scope(isolate);
        DCHECK(is_compiled_scope.is_compiled());
        compilation_cache->PutScript(source, language_mode, inner_result);
        Handle<Script> script(Script::cast(inner_result->script()), isolate);
        maybe_result = inner_result;
      } else {
        // Deserializer failed. Fall through to compile.
        compile_timer.set_consuming_code_cache_failed();
      }
    }
  }

  if (maybe_result.is_null()) {
    // No cache entry found compile the script.
    if (FLAG_stress_background_compile &&
        CanBackgroundCompile(script_details, origin_options, extension,
                             compile_options, natives)) {
      // If the --stress-background-compile flag is set, do the actual
      // compilation on a background thread, and wait for its result.
      maybe_result = CompileScriptOnBothBackgroundAndMainThread(
          source, script_details, origin_options, isolate, &is_compiled_scope);
    } else {
      UnoptimizedCompileFlags flags =
          UnoptimizedCompileFlags::ForToplevelCompile(
              isolate, natives == NOT_NATIVES_CODE, language_mode,
              script_details.repl_mode,
              origin_options.IsModule() ? ScriptType::kModule
                                        : ScriptType::kClassic);

      flags.set_is_eager(compile_options == ScriptCompiler::kEagerCompile);

      maybe_result = CompileScriptOnMainThread(
          flags, source, script_details, origin_options, natives, extension,
          isolate, &is_compiled_scope);
    }

    // Add the result to the isolate cache.
    Handle<SharedFunctionInfo> result;
    if (use_compilation_cache && maybe_result.ToHandle(&result)) {
      DCHECK(is_compiled_scope.is_compiled());
      compilation_cache->PutScript(source, language_mode, result);
    } else if (maybe_result.is_null() && natives != EXTENSION_CODE) {
      isolate->ReportPendingMessages();
    }
  }

  return maybe_result;
}

// static
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
    UnoptimizedCompileFlags flags = UnoptimizedCompileFlags::ForToplevelCompile(
        isolate, true, language_mode, script_details.repl_mode);
    flags.set_is_eval(true);  // Use an eval scope as declaration scope.
    flags.set_function_syntax_kind(FunctionSyntaxKind::kWrapped);
    // TODO(delphick): Remove this and instead make the wrapped and wrapper
    // functions fully non-lazy instead thus preventing source positions from
    // being omitted.
    flags.set_collect_source_positions(true);
    // flags.set_eager(compile_options == ScriptCompiler::kEagerCompile);

    UnoptimizedCompileState compile_state(isolate);
    ParseInfo parse_info(isolate, flags, &compile_state);

    MaybeHandle<ScopeInfo> maybe_outer_scope_info;
    if (!context->IsNativeContext()) {
      maybe_outer_scope_info = handle(context->scope_info(), isolate);
    }

    script = NewScript(isolate, &parse_info, source, script_details,
                       origin_options, NOT_NATIVES_CODE, arguments);

    Handle<SharedFunctionInfo> top_level;
    maybe_result = v8::internal::CompileToplevel(&parse_info, script,
                                                 maybe_outer_scope_info,
                                                 isolate, &is_compiled_scope);
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
    is_compiled_scope = wrapped->is_compiled_scope(isolate);
    script = Handle<Script>(Script::cast(wrapped->script()), isolate);
  }
  DCHECK(is_compiled_scope.is_compiled());

  return Factory::JSFunctionBuilder{isolate, wrapped, context}
      .set_allocation_type(AllocationType::kYoung)
      .Build();
}

// static
MaybeHandle<SharedFunctionInfo>
Compiler::GetSharedFunctionInfoForStreamedScript(
    Isolate* isolate, Handle<String> source,
    const ScriptDetails& script_details, ScriptOriginOptions origin_options,
    ScriptStreamingData* streaming_data) {
  DCHECK(!origin_options.IsWasm());

  ScriptCompileTimerScope compile_timer(
      isolate, ScriptCompiler::kNoCacheBecauseStreamingSource);
  PostponeInterruptsScope postpone(isolate);

  int source_length = source->length();
  isolate->counters()->total_load_size()->Increment(source_length);
  isolate->counters()->total_compile_size()->Increment(source_length);

  BackgroundCompileTask* task = streaming_data->task.get();

  MaybeHandle<SharedFunctionInfo> maybe_result;
  // Check if compile cache already holds the SFI, if so no need to finalize
  // the code compiled on the background thread.
  CompilationCache* compilation_cache = isolate->compilation_cache();
  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                 "V8.StreamingFinalization.CheckCache");
    maybe_result = compilation_cache->LookupScript(
        source, script_details.name_obj, script_details.line_offset,
        script_details.column_offset, origin_options, task->language_mode());
    if (!maybe_result.is_null()) {
      compile_timer.set_hit_isolate_cache();
    }
  }

  if (maybe_result.is_null()) {
    // No cache entry found, finalize compilation of the script and add it to
    // the isolate cache.
    DCHECK_EQ(task->flags().is_module(), origin_options.IsModule());

    Handle<Script> script;
    if (FLAG_finalize_streaming_on_background && !origin_options.IsModule()) {
      RuntimeCallTimerScope runtimeTimerScope(
          isolate, RuntimeCallCounterId::kCompilePublishBackgroundFinalization);
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                   "V8.OffThreadFinalization.Publish");

      script = task->GetScript(isolate);

      // We might not have been able to finalize all jobs on the background
      // thread (e.g. asm.js jobs), so finalize those deferred jobs now.
      if (FinalizeDeferredUnoptimizedCompilationJobs(
              isolate, script,
              task->jobs_to_retry_finalization_on_main_thread(),
              task->compile_state()->pending_error_handler(),
              task->finalize_unoptimized_compilation_data())) {
        maybe_result = task->GetOuterFunctionSfi(isolate);
      }

      script->set_source(*source);
      script->set_origin_options(origin_options);

      // The one post-hoc fix-up: Add the script to the script list.
      Handle<WeakArrayList> scripts = isolate->factory()->script_list();
      scripts = WeakArrayList::Append(isolate, scripts,
                                      MaybeObjectHandle::Weak(script));
      isolate->heap()->SetRootScriptList(*scripts);
    } else {
      ParseInfo* parse_info = task->info();
      DCHECK_EQ(parse_info->flags().is_module(), origin_options.IsModule());
      DCHECK(parse_info->flags().is_toplevel());

      script = parse_info->CreateScript(isolate, source, kNullMaybeHandle,
                                        origin_options);

      task->parser()->UpdateStatistics(isolate, script);
      task->parser()->HandleSourceURLComments(isolate, script);

      if (!task->compilation_jobs()->empty()) {
        // Off-thread parse & compile has succeeded - finalize compilation.
        DCHECK_NOT_NULL(parse_info->literal());

        parse_info->ast_value_factory()->Internalize(isolate);

        Handle<SharedFunctionInfo> shared_info =
            CreateTopLevelSharedFunctionInfo(parse_info, script, isolate);
        if (FinalizeAllUnoptimizedCompilationJobs(
                parse_info, isolate, script, task->compilation_jobs(),
                task->finalize_unoptimized_compilation_data())) {
          maybe_result = shared_info;
        }
      }

      if (maybe_result.is_null()) {
        // Compilation failed - prepare to throw an exception after script
        // fields have been set.
        PreparePendingException(isolate, parse_info);
      }
    }

    // Set the script fields after finalization, to keep this path the same
    // between main-thread and off-thread finalization.
    {
      DisallowGarbageCollection no_gc;
      SetScriptFieldsFromDetails(isolate, *script, script_details, &no_gc);
      LOG(isolate, ScriptDetails(*script));
    }

    Handle<SharedFunctionInfo> result;
    if (!maybe_result.ToHandle(&result)) {
      FailWithPreparedPendingException(
          isolate, script, task->compile_state()->pending_error_handler());
    } else {
      FinalizeUnoptimizedScriptCompilation(
          isolate, script, task->flags(), task->compile_state(),
          *task->finalize_unoptimized_compilation_data());

      // Add compiled code to the isolate cache.
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                   "V8.StreamingFinalization.AddToCache");
      compilation_cache->PutScript(source, task->language_mode(), result);
    }
  }

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.StreamingFinalization.Release");
  streaming_data->Release();
  return maybe_result;
}

// static
template <typename LocalIsolate>
Handle<SharedFunctionInfo> Compiler::GetSharedFunctionInfo(
    FunctionLiteral* literal, Handle<Script> script, LocalIsolate* isolate) {
  // Precondition: code has been parsed and scopes have been analyzed.
  MaybeHandle<SharedFunctionInfo> maybe_existing;

  // Find any previously allocated shared function info for the given literal.
  maybe_existing =
      script->FindSharedFunctionInfo(isolate, literal->function_literal_id());

  // If we found an existing shared function info, return it.
  Handle<SharedFunctionInfo> existing;
  if (maybe_existing.ToHandle(&existing)) {
    // If the function has been uncompiled (bytecode flushed) it will have lost
    // any preparsed data. If we produced preparsed data during this compile for
    // this function, replace the uncompiled data with one that includes it.
    if (literal->produced_preparse_data() != nullptr &&
        existing->HasUncompiledDataWithoutPreparseData()) {
      Handle<UncompiledData> existing_uncompiled_data =
          handle(existing->uncompiled_data(), isolate);
      DCHECK_EQ(literal->start_position(),
                existing_uncompiled_data->start_position());
      DCHECK_EQ(literal->end_position(),
                existing_uncompiled_data->end_position());
      // Use existing uncompiled data's inferred name as it may be more
      // accurate than the literal we preparsed.
      Handle<String> inferred_name =
          handle(existing_uncompiled_data->inferred_name(), isolate);
      Handle<PreparseData> preparse_data =
          literal->produced_preparse_data()->Serialize(isolate);
      Handle<UncompiledData> new_uncompiled_data =
          isolate->factory()->NewUncompiledDataWithPreparseData(
              inferred_name, existing_uncompiled_data->start_position(),
              existing_uncompiled_data->end_position(), preparse_data);
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

template Handle<SharedFunctionInfo> Compiler::GetSharedFunctionInfo(
    FunctionLiteral* literal, Handle<Script> script, Isolate* isolate);
template Handle<SharedFunctionInfo> Compiler::GetSharedFunctionInfo(
    FunctionLiteral* literal, Handle<Script> script, LocalIsolate* isolate);

// static
MaybeHandle<Code> Compiler::GetOptimizedCodeForOSR(Handle<JSFunction> function,
                                                   BytecodeOffset osr_offset,
                                                   JavaScriptFrame* osr_frame) {
  DCHECK(!osr_offset.IsNone());
  DCHECK_NOT_NULL(osr_frame);
  return GetOptimizedCode(function, ConcurrencyMode::kNotConcurrent,
                          CodeKindForOSR(), osr_offset, osr_frame);
}

// static
bool Compiler::FinalizeOptimizedCompilationJob(OptimizedCompilationJob* job,
                                               Isolate* isolate) {
  VMState<COMPILER> state(isolate);
  // Take ownership of the job. Deleting the job also tears down the zone.
  std::unique_ptr<OptimizedCompilationJob> job_scope(job);
  OptimizedCompilationInfo* compilation_info = job->compilation_info();

  TimerEventScope<TimerEventRecompileSynchronous> timer(isolate);
  RuntimeCallTimerScope runtimeTimer(
      isolate, RuntimeCallCounterId::kOptimizeConcurrentFinalize);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.OptimizeConcurrentFinalize");

  Handle<SharedFunctionInfo> shared = compilation_info->shared_info();

  CodeKind code_kind = compilation_info->code_kind();
  const bool should_install_code_on_function =
      !CodeKindIsNativeContextIndependentJSFunction(code_kind);
  if (should_install_code_on_function) {
    // Reset profiler ticks, function is no longer considered hot.
    compilation_info->closure()->feedback_vector().set_profiler_ticks(0);
  }

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
      InsertCodeIntoCompilationCache(isolate, compilation_info);
      CompilerTracer::TraceCompletedJob(isolate, compilation_info);
      if (should_install_code_on_function) {
        compilation_info->closure()->set_code(*compilation_info->code(),
                                              kReleaseStore);
      }
      return CompilationJob::SUCCEEDED;
    }
  }

  DCHECK_EQ(job->state(), CompilationJob::State::kFailed);
  CompilerTracer::TraceAbortedJob(isolate, compilation_info);
  compilation_info->closure()->set_code(shared->GetCode(), kReleaseStore);
  // Clear the InOptimizationQueue marker, if it exists.
  if (!CodeKindIsNativeContextIndependentJSFunction(code_kind) &&
      compilation_info->closure()->IsInOptimizationQueue()) {
    compilation_info->closure()->ClearOptimizationMarker();
  }
  return CompilationJob::FAILED;
}

// static
void Compiler::PostInstantiation(Handle<JSFunction> function) {
  Isolate* isolate = function->GetIsolate();
  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  IsCompiledScope is_compiled_scope(shared->is_compiled_scope(isolate));

  // If code is compiled to bytecode (i.e., isn't asm.js), then allocate a
  // feedback and check for optimized code.
  if (is_compiled_scope.is_compiled() && shared->HasBytecodeArray()) {
    // Don't reset budget if there is a closure feedback cell array already. We
    // are just creating a new closure that shares the same feedback cell.
    JSFunction::InitializeFeedbackCell(function, &is_compiled_scope, false);

    if (function->has_feedback_vector()) {
      // Evict any deoptimized code on feedback vector. We need to do this after
      // creating the closure, since any heap allocations could trigger a GC and
      // deoptimized the code on the feedback vector. So check for any
      // deoptimized code just before installing it on the funciton.
      function->feedback_vector().EvictOptimizedCodeMarkedForDeoptimization(
          function->raw_feedback_cell(), *shared,
          "new function from shared function info");
      Code code = function->feedback_vector().optimized_code();
      if (!code.is_null()) {
        // Caching of optimized code enabled and optimized code found.
        DCHECK(!code.marked_for_deoptimization());
        DCHECK(function->shared().is_compiled());

        // We don't need a release store because the optimized code was
        // stored with release semantics into the vector
        STATIC_ASSERT(
            FeedbackVector::kFeedbackVectorMaybeOptimizedCodeIsStoreRelease);
        function->set_code(code);
      }
    }

    if (FLAG_always_opt && shared->allows_lazy_compilation() &&
        !shared->optimization_disabled() &&
        !function->HasAvailableOptimizedCode()) {
      CompilerTracer::TraceMarkForAlwaysOpt(isolate, function);
      JSFunction::EnsureFeedbackVector(function, &is_compiled_scope);
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
