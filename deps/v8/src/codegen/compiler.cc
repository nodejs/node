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
#include "src/codegen/script-details.h"
#include "src/codegen/unoptimized-compilation-info.h"
#include "src/common/assert-scope.h"
#include "src/common/globals.h"
#include "src/common/message-template.h"
#include "src/compiler-dispatcher/lazy-compile-dispatcher.h"
#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"
#include "src/compiler/pipeline.h"
#include "src/debug/debug.h"
#include "src/debug/liveedit.h"
#include "src/diagnostics/code-tracer.h"
#include "src/execution/frames-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/isolate.h"
#include "src/execution/local-isolate.h"
#include "src/execution/vm-state-inl.h"
#include "src/handles/handles.h"
#include "src/handles/maybe-handles.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/heap-inl.h"
#include "src/heap/local-factory-inl.h"
#include "src/heap/local-heap-inl.h"
#include "src/heap/local-heap.h"
#include "src/heap/parked-scope.h"
#include "src/init/bootstrapper.h"
#include "src/interpreter/interpreter.h"
#include "src/logging/counters-scopes.h"
#include "src/logging/log-inl.h"
#include "src/logging/runtime-call-stats-scope.h"
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
#include "src/web-snapshot/web-snapshot.h"
#include "src/zone/zone-list-inl.h"  // crbug.com/v8/8816

#ifdef V8_ENABLE_MAGLEV
#include "src/maglev/maglev-concurrent-dispatcher.h"
#include "src/maglev/maglev.h"
#endif  // V8_ENABLE_MAGLEV

namespace v8 {
namespace internal {

namespace {

constexpr bool IsOSR(BytecodeOffset osr_offset) { return !osr_offset.IsNone(); }

void SetTieringState(JSFunction function, BytecodeOffset osr_offset,
                     TieringState value) {
  if (IsOSR(osr_offset)) {
    function.set_osr_tiering_state(value);
  } else {
    function.set_tiering_state(value);
  }
}

void ResetTieringState(JSFunction function, BytecodeOffset osr_offset) {
  if (function.has_feedback_vector()) {
    SetTieringState(function, osr_offset, TieringState::kNone);
  }
}

void ResetProfilerTicks(JSFunction function, BytecodeOffset osr_offset) {
  if (!IsOSR(osr_offset)) {
    // Reset profiler ticks, the function is no longer considered hot.
    // TODO(v8:7700): Update for Maglev tiering.
    function.feedback_vector().set_profiler_ticks(0);
  }
}

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

  static void TraceOptimizeOSR(Isolate* isolate, Handle<JSFunction> function,
                               BytecodeOffset osr_offset,
                               ConcurrencyMode mode) {
    if (!FLAG_trace_osr) return;
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintF(scope.file(),
           "[OSR - started. function: %s, osr offset: %d, mode: %s]\n",
           function->DebugNameCStr().get(), osr_offset.ToInt(), ToString(mode));
  }

  static void TraceOptimizeOSRUnavailable(Isolate* isolate,
                                          Handle<JSFunction> function,
                                          BytecodeOffset osr_offset,
                                          ConcurrencyMode mode) {
    if (!FLAG_trace_osr) return;
    CodeTracer::Scope scope(isolate->GetCodeTracer());
    PrintF(scope.file(),
           "[OSR - unavailable (failed or in progress). function: %s, osr "
           "offset: %d, mode: %s]\n",
           function->DebugNameCStr().get(), osr_offset.ToInt(), ToString(mode));
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
    if (IsOSR(osr_offset)) {
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

void LogFunctionCompilation(Isolate* isolate,
                            CodeEventListener::LogEventsAndTags tag,
                            Handle<Script> script,
                            Handle<SharedFunctionInfo> shared,
                            Handle<FeedbackVector> vector,
                            Handle<AbstractCode> abstract_code, CodeKind kind,
                            double time_taken_ms) {
  DCHECK(!abstract_code.is_null());
  if (V8_EXTERNAL_CODE_SPACE_BOOL) {
    DCHECK_NE(*abstract_code, FromCodeT(*BUILTIN_CODE(isolate, CompileLazy)));
  } else {
    DCHECK(!abstract_code.is_identical_to(BUILTIN_CODE(isolate, CompileLazy)));
  }

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
  if (!vector.is_null()) {
    LOG(isolate, FeedbackVectorEvent(*vector, *abstract_code));
  }
  if (!FLAG_log_function_events) return;

  std::string name;
  switch (kind) {
    case CodeKind::INTERPRETED_FUNCTION:
      name = "interpreter";
      break;
    case CodeKind::BASELINE:
      name = "baseline";
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

namespace {

ScriptOriginOptions OriginOptionsForEval(
    Object script, ParsingWhileDebugging parsing_while_debugging) {
  bool is_shared_cross_origin =
      parsing_while_debugging == ParsingWhileDebugging::kYes;
  bool is_opaque = false;
  if (script.IsScript()) {
    auto script_origin_options = Script::cast(script).origin_options();
    if (script_origin_options.IsSharedCrossOrigin()) {
      is_shared_cross_origin = true;
    }
    if (script_origin_options.IsOpaque()) {
      is_opaque = true;
    }
  }
  return ScriptOriginOptions(is_shared_cross_origin, is_opaque);
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
        ToAbstractCode(BUILTIN_CODE(isolate, InstantiateAsmJs), isolate);
#else
    UNREACHABLE();
#endif  // V8_ENABLE_WEBASSEMBLY
  }

  double time_taken_ms = time_taken_to_execute.InMillisecondsF() +
                         time_taken_to_finalize.InMillisecondsF();

  Handle<Script> script(Script::cast(shared->script()), isolate);
  LogFunctionCompilation(isolate, tag, script, shared, Handle<FeedbackVector>(),
                         abstract_code, CodeKind::INTERPRETED_FUNCTION,
                         time_taken_ms);
}

}  // namespace

// ----------------------------------------------------------------------------
// Implementation of OptimizedCompilationJob

CompilationJob::Status OptimizedCompilationJob::PrepareJob(Isolate* isolate) {
  DCHECK_EQ(ThreadId::Current(), isolate->thread_id());
  DisallowJavascriptExecution no_js(isolate);

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

CompilationJob::Status TurbofanCompilationJob::RetryOptimization(
    BailoutReason reason) {
  DCHECK(compilation_info_->IsOptimizing());
  compilation_info_->RetryOptimization(reason);
  return UpdateState(FAILED, State::kFailed);
}

CompilationJob::Status TurbofanCompilationJob::AbortOptimization(
    BailoutReason reason) {
  DCHECK(compilation_info_->IsOptimizing());
  compilation_info_->AbortOptimization(reason);
  return UpdateState(FAILED, State::kFailed);
}

void TurbofanCompilationJob::RecordCompilationStats(ConcurrencyMode mode,
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
        case ConcurrencyMode::kConcurrent:
          time_background += time_taken_to_execute_;
          counters->turbofan_optimize_concurrent_total_time()->AddSample(
              static_cast<int>(ElapsedTime().InMicroseconds()));
          break;
        case ConcurrencyMode::kSynchronous:
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

void TurbofanCompilationJob::RecordFunctionCompilation(
    CodeEventListener::LogEventsAndTags tag, Isolate* isolate) const {
  Handle<AbstractCode> abstract_code =
      Handle<AbstractCode>::cast(compilation_info()->code());

  double time_taken_ms = time_taken_to_prepare_.InMillisecondsF() +
                         time_taken_to_execute_.InMillisecondsF() +
                         time_taken_to_finalize_.InMillisecondsF();

  Handle<Script> script(
      Script::cast(compilation_info()->shared_info()->script()), isolate);
  Handle<FeedbackVector> feedback_vector(
      compilation_info()->closure()->feedback_vector(), isolate);
  LogFunctionCompilation(
      isolate, tag, script, compilation_info()->shared_info(), feedback_vector,
      abstract_code, compilation_info()->code_kind(), time_taken_ms);
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
  interpreter_data->set_interpreter_trampoline(ToCodeT(*code));

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

template <typename IsolateT>
void InstallUnoptimizedCode(UnoptimizedCompilationInfo* compilation_info,
                            Handle<SharedFunctionInfo> shared_info,
                            IsolateT* isolate) {
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
    shared_info->set_feedback_metadata(*feedback_metadata, kReleaseStore);
  } else {
#if V8_ENABLE_WEBASSEMBLY
    DCHECK(compilation_info->has_asm_wasm_data());
    // We should only have asm/wasm data when finalizing on the main thread.
    DCHECK((std::is_same<IsolateT, Isolate>::value));
    shared_info->set_asm_wasm_data(*compilation_info->asm_wasm_data());
    shared_info->set_feedback_metadata(
        ReadOnlyRoots(isolate).empty_feedback_metadata(), kReleaseStore);
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

template <typename IsolateT>
void EnsureSharedFunctionInfosArrayOnScript(Handle<Script> script,
                                            ParseInfo* parse_info,
                                            IsolateT* isolate) {
  DCHECK(parse_info->flags().is_toplevel());
  if (script->shared_function_info_count() > 0) {
    DCHECK_LE(script->shared_function_info_count(),
              script->shared_function_infos().length());
    DCHECK_EQ(script->shared_function_info_count(),
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

  // These fields are all initialised in ParseInfo from the SharedFunctionInfo,
  // and then set back on the literal after parse. Hence, they should already
  // match.
  DCHECK_EQ(shared_info.requires_instance_members_initializer(),
            literal->requires_instance_members_initializer());
  DCHECK_EQ(shared_info.class_scope_has_private_brand(),
            literal->class_scope_has_private_brand());
  DCHECK_EQ(shared_info.has_static_private_methods_or_accessors(),
            literal->has_static_private_methods_or_accessors());

  shared_info.set_has_duplicate_parameters(literal->has_duplicate_parameters());
  shared_info.UpdateAndFinalizeExpectedNofPropertiesFromEstimate(literal);

  shared_info.SetScopeInfo(*literal->scope()->scope_info());
}

// Finalize a single compilation job. This function can return
// RETRY_ON_MAIN_THREAD if the job cannot be finalized off-thread, in which case
// it should be safe to call it again on the main thread with the same job.
template <typename IsolateT>
CompilationJob::Status FinalizeSingleUnoptimizedCompilationJob(
    UnoptimizedCompilationJob* job, Handle<SharedFunctionInfo> shared_info,
    IsolateT* isolate,
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
  DCHECK_IMPLIES(status == CompilationJob::RETRY_ON_MAIN_THREAD,
                 (std::is_same<IsolateT, LocalIsolate>::value));
  return status;
}

std::unique_ptr<UnoptimizedCompilationJob>
ExecuteSingleUnoptimizedCompilationJob(
    ParseInfo* parse_info, FunctionLiteral* literal, Handle<Script> script,
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
          parse_info, literal, script, allocator, eager_inner_literals,
          local_isolate));

  if (job->ExecuteJob() != CompilationJob::SUCCEEDED) {
    // Compilation failed, return null.
    return std::unique_ptr<UnoptimizedCompilationJob>();
  }

  return job;
}

template <typename IsolateT>
bool IterativelyExecuteAndFinalizeUnoptimizedCompilationJobs(
    IsolateT* isolate, Handle<SharedFunctionInfo> outer_shared_info,
    Handle<Script> script, ParseInfo* parse_info,
    AccountingAllocator* allocator, IsCompiledScope* is_compiled_scope,
    FinalizeUnoptimizedCompilationDataList*
        finalize_unoptimized_compilation_data_list,
    DeferredFinalizationJobDataList*
        jobs_to_retry_finalization_on_main_thread) {
  DeclarationScope::AllocateScopeInfos(parse_info, isolate);

  std::vector<FunctionLiteral*> functions_to_compile;
  functions_to_compile.push_back(parse_info->literal());

  bool is_first = true;
  while (!functions_to_compile.empty()) {
    FunctionLiteral* literal = functions_to_compile.back();
    functions_to_compile.pop_back();
    Handle<SharedFunctionInfo> shared_info;
    if (is_first) {
      // We get the first SharedFunctionInfo directly as outer_shared_info
      // rather than with Compiler::GetSharedFunctionInfo, to support
      // placeholder SharedFunctionInfos that aren't on the script's SFI list.
      DCHECK_EQ(literal->function_literal_id(),
                outer_shared_info->function_literal_id());
      shared_info = outer_shared_info;
      is_first = false;
    } else {
      shared_info = Compiler::GetSharedFunctionInfo(literal, script, isolate);
    }

    if (shared_info->is_compiled()) continue;

    std::unique_ptr<UnoptimizedCompilationJob> job =
        ExecuteSingleUnoptimizedCompilationJob(parse_info, literal, script,
                                               allocator, &functions_to_compile,
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
        DCHECK((!std::is_same<IsolateT, Isolate>::value));
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

// A wrapper to access either the OSR optimized code cache (one per native
// context), or the optimized code cache slot on the feedback vector.
class OptimizedCodeCache : public AllStatic {
 public:
  static V8_WARN_UNUSED_RESULT MaybeHandle<CodeT> Get(
      Isolate* isolate, Handle<JSFunction> function, BytecodeOffset osr_offset,
      CodeKind code_kind) {
    if (!CodeKindIsStoredInOptimizedCodeCache(code_kind)) return {};

    DisallowGarbageCollection no_gc;
    SharedFunctionInfo shared = function->shared();
    RCS_SCOPE(isolate, RuntimeCallCounterId::kCompileGetFromOptimizedCodeMap);

    CodeT code;
    if (IsOSR(osr_offset)) {
      // For OSR, check the OSR optimized code cache.
      code = function->native_context().osr_code_cache().TryGet(
          shared, osr_offset, isolate);
    } else {
      // Non-OSR code may be cached on the feedback vector.
      if (function->has_feedback_vector()) {
        FeedbackVector feedback_vector = function->feedback_vector();
        feedback_vector.EvictOptimizedCodeMarkedForDeoptimization(
            shared, "OptimizedCodeCache::Get");
        code = feedback_vector.optimized_code();
      }
    }

    DCHECK_IMPLIES(!code.is_null(), code.kind() <= code_kind);
    if (code.is_null() || code.kind() != code_kind) return {};

    DCHECK(!code.marked_for_deoptimization());
    DCHECK(shared.is_compiled());
    DCHECK(CodeKindIsStoredInOptimizedCodeCache(code.kind()));
    DCHECK_IMPLIES(IsOSR(osr_offset), CodeKindCanOSR(code.kind()));

    CompilerTracer::TraceOptimizedCodeCacheHit(isolate, function, osr_offset,
                                               code_kind);
    return handle(code, isolate);
  }

  static void Insert(OptimizedCompilationInfo* compilation_info) {
    const CodeKind kind = compilation_info->code_kind();
    if (!CodeKindIsStoredInOptimizedCodeCache(kind)) return;

    // Cache optimized code.
    Handle<JSFunction> function = compilation_info->closure();
    Isolate* isolate = function->GetIsolate();
    Handle<CodeT> code = ToCodeT(compilation_info->code(), isolate);
    const BytecodeOffset osr_offset = compilation_info->osr_offset();

    if (IsOSR(osr_offset)) {
      DCHECK(CodeKindCanOSR(kind));
      DCHECK(!compilation_info->function_context_specializing());
      Handle<SharedFunctionInfo> shared(function->shared(), isolate);
      Handle<NativeContext> native_context(function->native_context(), isolate);
      OSROptimizedCodeCache::Insert(isolate, native_context, shared, code,
                                    osr_offset);
      return;
    }

    DCHECK(!IsOSR(osr_offset));

    if (compilation_info->function_context_specializing()) {
      // Function context specialization folds-in the function context, so no
      // sharing can occur. Make sure the optimized code cache is cleared.
      if (function->feedback_vector().has_optimized_code()) {
        function->feedback_vector().ClearOptimizedCode();
      }
      return;
    }

    function->feedback_vector().SetOptimizedCode(code);
  }
};

// Runs PrepareJob in the proper compilation & canonical scopes. Handles will be
// allocated in a persistent handle scope that is detached and handed off to the
// {compilation_info} after PrepareJob.
bool PrepareJobWithHandleScope(OptimizedCompilationJob* job, Isolate* isolate,
                               OptimizedCompilationInfo* compilation_info) {
  CompilationHandleScope compilation(isolate, compilation_info);
  CanonicalHandleScopeForTurbofan canonical(isolate, compilation_info);
  CompilerTracer::TracePrepareJob(isolate, compilation_info,
                                  job->compiler_name());
  compilation_info->ReopenHandlesInNewHandleScope(isolate);
  return job->PrepareJob(isolate) == CompilationJob::SUCCEEDED;
}

bool CompileTurbofan_NotConcurrent(Isolate* isolate,
                                   TurbofanCompilationJob* job) {
  OptimizedCompilationInfo* const compilation_info = job->compilation_info();
  DCHECK_EQ(compilation_info->code_kind(), CodeKind::TURBOFAN);

  TimerEventScope<TimerEventRecompileSynchronous> timer(isolate);
  RCS_SCOPE(isolate, RuntimeCallCounterId::kOptimizeNonConcurrent);
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
  job->RecordCompilationStats(ConcurrencyMode::kSynchronous, isolate);
  DCHECK(!isolate->has_pending_exception());
  OptimizedCodeCache::Insert(compilation_info);
  job->RecordFunctionCompilation(CodeEventListener::LAZY_COMPILE_TAG, isolate);
  return true;
}

bool CompileTurbofan_Concurrent(Isolate* isolate,
                                std::unique_ptr<TurbofanCompilationJob> job) {
  OptimizedCompilationInfo* const compilation_info = job->compilation_info();
  DCHECK_EQ(compilation_info->code_kind(), CodeKind::TURBOFAN);
  Handle<JSFunction> function = compilation_info->closure();

  if (!isolate->optimizing_compile_dispatcher()->IsQueueAvailable()) {
    if (FLAG_trace_concurrent_recompilation) {
      PrintF("  ** Compilation queue full, will retry optimizing ");
      function->ShortPrint();
      PrintF(" later.\n");
    }
    return false;
  }

  if (isolate->heap()->HighMemoryPressure()) {
    if (FLAG_trace_concurrent_recompilation) {
      PrintF("  ** High memory pressure, will retry optimizing ");
      function->ShortPrint();
      PrintF(" later.\n");
    }
    return false;
  }

  TimerEventScope<TimerEventRecompileSynchronous> timer(isolate);
  RCS_SCOPE(isolate, RuntimeCallCounterId::kOptimizeConcurrentPrepare);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.OptimizeConcurrentPrepare");

  if (!PrepareJobWithHandleScope(job.get(), isolate, compilation_info)) {
    return false;
  }

  // The background recompile will own this job.
  isolate->optimizing_compile_dispatcher()->QueueForOptimization(job.release());

  if (FLAG_trace_concurrent_recompilation) {
    PrintF("  ** Queued ");
    function->ShortPrint();
    PrintF(" for concurrent optimization.\n");
  }

  SetTieringState(*function, compilation_info->osr_offset(),
                  TieringState::kInProgress);

  DCHECK(compilation_info->shared_info()->HasBytecodeArray());
  return true;
}

enum class CompileResultBehavior {
  // Default behavior, i.e. install the result, insert into caches, etc.
  kDefault,
  // Used only for stress testing. The compilation result should be discarded.
  kDiscardForTesting,
};

bool ShouldOptimize(CodeKind code_kind, Handle<SharedFunctionInfo> shared) {
  DCHECK(CodeKindIsOptimizedJSFunction(code_kind));
  switch (code_kind) {
    case CodeKind::TURBOFAN:
      return FLAG_opt && shared->PassesFilter(FLAG_turbo_filter);
    case CodeKind::MAGLEV:
      // TODO(v8:7700): FLAG_maglev_filter.
      return FLAG_maglev;
    default:
      UNREACHABLE();
  }
}

MaybeHandle<CodeT> CompileTurbofan(Isolate* isolate,
                                   Handle<JSFunction> function,
                                   Handle<SharedFunctionInfo> shared,
                                   ConcurrencyMode mode,
                                   BytecodeOffset osr_offset,
                                   JavaScriptFrame* osr_frame,
                                   CompileResultBehavior result_behavior) {
  VMState<COMPILER> state(isolate);
  TimerEventScope<TimerEventOptimizeCode> optimize_code_timer(isolate);
  RCS_SCOPE(isolate, RuntimeCallCounterId::kOptimizeCode);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.OptimizeCode");

  DCHECK(!isolate->has_pending_exception());
  PostponeInterruptsScope postpone(isolate);
  bool has_script = shared->script().IsScript();
  // BUG(5946): This DCHECK is necessary to make certain that we won't
  // tolerate the lack of a script without bytecode.
  DCHECK_IMPLIES(!has_script, shared->HasBytecodeArray());
  std::unique_ptr<TurbofanCompilationJob> job(
      compiler::Pipeline::NewCompilationJob(isolate, function,
                                            CodeKind::TURBOFAN, has_script,
                                            osr_offset, osr_frame));

  if (result_behavior == CompileResultBehavior::kDiscardForTesting) {
    job->compilation_info()->set_discard_result_for_testing();
  }

  // Prepare the job and launch concurrent compilation, or compile now.
  if (IsConcurrent(mode)) {
    if (CompileTurbofan_Concurrent(isolate, std::move(job))) return {};
  } else {
    DCHECK(IsSynchronous(mode));
    if (CompileTurbofan_NotConcurrent(isolate, job.get())) {
      return ToCodeT(job->compilation_info()->code(), isolate);
    }
  }

  if (isolate->has_pending_exception()) isolate->clear_pending_exception();
  return {};
}

#ifdef V8_ENABLE_MAGLEV
// TODO(v8:7700): Record maglev compilations better.
void RecordMaglevFunctionCompilation(Isolate* isolate,
                                     Handle<JSFunction> function) {
  Handle<AbstractCode> abstract_code(
      AbstractCode::cast(FromCodeT(function->code())), isolate);
  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  Handle<Script> script(Script::cast(shared->script()), isolate);
  Handle<FeedbackVector> feedback_vector(function->feedback_vector(), isolate);

  // Optimistic estimate.
  double time_taken_ms = 0;

  LogFunctionCompilation(isolate, CodeEventListener::FUNCTION_TAG, script,
                         shared, feedback_vector, abstract_code,
                         abstract_code->kind(), time_taken_ms);
}
#endif  // V8_ENABLE_MAGLEV

MaybeHandle<CodeT> CompileMaglev(Isolate* isolate, Handle<JSFunction> function,
                                 ConcurrencyMode mode,
                                 BytecodeOffset osr_offset,
                                 JavaScriptFrame* osr_frame,
                                 CompileResultBehavior result_behavior) {
#ifdef V8_ENABLE_MAGLEV
  DCHECK(FLAG_maglev);
  // TODO(v8:7700): Add missing support.
  CHECK(!IsOSR(osr_offset));
  CHECK(osr_frame == nullptr);
  CHECK(result_behavior == CompileResultBehavior::kDefault);

  // TODO(v8:7700): Tracing, see CompileTurbofan.

  DCHECK(!isolate->has_pending_exception());
  PostponeInterruptsScope postpone(isolate);

  // TODO(v8:7700): See everything in CompileTurbofan_Concurrent.
  // - Tracing,
  // - timers,
  // - aborts on memory pressure,
  // ...

  // Prepare the job.
  auto job = maglev::MaglevCompilationJob::New(isolate, function);
  CompilationJob::Status status = job->PrepareJob(isolate);
  CHECK_EQ(status, CompilationJob::SUCCEEDED);  // TODO(v8:7700): Use status.

  if (IsSynchronous(mode)) {
    function->reset_tiering_state();
    {
      // Park the main thread Isolate here, to be in the same state as
      // background threads.
      ParkedScope parked_scope(isolate->main_thread_local_isolate());
      if (job->ExecuteJob(isolate->counters()->runtime_call_stats(),
                          isolate->main_thread_local_isolate()) !=
          CompilationJob::SUCCEEDED) {
        return {};
      }
    }

    if (job->FinalizeJob(isolate) != CompilationJob::SUCCEEDED) {
      return {};
    }

    RecordMaglevFunctionCompilation(isolate, function);
    return handle(function->code(), isolate);
  }

  DCHECK(IsConcurrent(mode));

  // Enqueue it.
  isolate->maglev_concurrent_dispatcher()->EnqueueJob(std::move(job));

  // Remember that the function is currently being processed.
  SetTieringState(*function, osr_offset, TieringState::kInProgress);

  return {};
#else   // V8_ENABLE_MAGLEV
  UNREACHABLE();
#endif  // V8_ENABLE_MAGLEV
}

MaybeHandle<CodeT> GetOrCompileOptimized(
    Isolate* isolate, Handle<JSFunction> function, ConcurrencyMode mode,
    CodeKind code_kind, BytecodeOffset osr_offset = BytecodeOffset::None(),
    JavaScriptFrame* osr_frame = nullptr,
    CompileResultBehavior result_behavior = CompileResultBehavior::kDefault) {
  DCHECK(CodeKindIsOptimizedJSFunction(code_kind));

  Handle<SharedFunctionInfo> shared(function->shared(), isolate);

  // Clear the optimization marker on the function so that we don't try to
  // re-optimize.
  if (!IsOSR(osr_offset)) {
    ResetTieringState(*function, osr_offset);
  }

  // TODO(v8:7700): Distinguish between Maglev and Turbofan.
  if (shared->optimization_disabled() &&
      shared->disabled_optimization_reason() == BailoutReason::kNeverOptimize) {
    return {};
  }

  // Do not optimize when debugger needs to hook into every call.
  if (isolate->debug()->needs_check_on_function_call()) return {};

  // Do not optimize if we need to be able to set break points.
  if (shared->HasBreakInfo()) return {};

  // Do not optimize if optimization is disabled or function doesn't pass
  // turbo_filter.
  if (!ShouldOptimize(code_kind, shared)) return {};

  // If code was pending optimization for testing, remove the entry from the
  // table that was preventing the bytecode from being flushed.
  if (V8_UNLIKELY(FLAG_testing_d8_test_runner)) {
    PendingOptimizationTable::FunctionWasOptimized(isolate, function);
  }

  Handle<CodeT> cached_code;
  if (OptimizedCodeCache::Get(isolate, function, osr_offset, code_kind)
          .ToHandle(&cached_code)) {
    return cached_code;
  }

  DCHECK(shared->is_compiled());

  ResetProfilerTicks(*function, osr_offset);

  if (code_kind == CodeKind::TURBOFAN) {
    return CompileTurbofan(isolate, function, shared, mode, osr_offset,
                           osr_frame, result_behavior);
  } else {
    DCHECK_EQ(code_kind, CodeKind::MAGLEV);
    return CompileMaglev(isolate, function, mode, osr_offset, osr_frame,
                         result_behavior);
  }
}

// When --stress-concurrent-inlining is enabled, spawn concurrent jobs in
// addition to non-concurrent compiles to increase coverage in mjsunit tests
// (where most interesting compiles are non-concurrent). The result of the
// compilation is thrown out.
void SpawnDuplicateConcurrentJobForStressTesting(Isolate* isolate,
                                                 Handle<JSFunction> function,
                                                 ConcurrencyMode mode,
                                                 CodeKind code_kind) {
  // TODO(v8:7700): Support Maglev.
  if (code_kind == CodeKind::MAGLEV) return;

  DCHECK(FLAG_stress_concurrent_inlining &&
         isolate->concurrent_recompilation_enabled() && IsSynchronous(mode) &&
         isolate->node_observer() == nullptr);
  CompileResultBehavior result_behavior =
      FLAG_stress_concurrent_inlining_attach_code
          ? CompileResultBehavior::kDefault
          : CompileResultBehavior::kDiscardForTesting;
  USE(GetOrCompileOptimized(isolate, function, ConcurrencyMode::kConcurrent,
                            code_kind, BytecodeOffset::None(), nullptr,
                            result_behavior));
}

bool FailAndClearPendingException(Isolate* isolate) {
  isolate->clear_pending_exception();
  return false;
}

template <typename IsolateT>
bool PreparePendingException(IsolateT* isolate, ParseInfo* parse_info) {
  if (parse_info->pending_error_handler()->has_pending_error()) {
    parse_info->pending_error_handler()->PrepareErrors(
        isolate, parse_info->ast_value_factory());
  }
  return false;
}

bool FailWithPreparedPendingException(
    Isolate* isolate, Handle<Script> script,
    const PendingCompilationErrorHandler* pending_error_handler,
    Compiler::ClearExceptionFlag flag = Compiler::KEEP_EXCEPTION) {
  if (flag == Compiler::CLEAR_EXCEPTION) {
    return FailAndClearPendingException(isolate);
  }

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
  PreparePendingException(isolate, parse_info);
  return FailWithPreparedPendingException(
      isolate, script, parse_info->pending_error_handler(), flag);
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

  if (isolate->NeedsSourcePositionsForProfiling()) {
    Script::InitLineEnds(isolate, script);
  }
}

void CompileAllWithBaseline(Isolate* isolate,
                            const FinalizeUnoptimizedCompilationDataList&
                                finalize_unoptimized_compilation_data_list) {
  CodePageCollectionMemoryModificationScope code_allocation(isolate->heap());
  for (const auto& finalize_data : finalize_unoptimized_compilation_data_list) {
    Handle<SharedFunctionInfo> shared_info = finalize_data.function_handle();
    IsCompiledScope is_compiled_scope(*shared_info, isolate);
    if (!is_compiled_scope.is_compiled()) continue;
    if (!CanCompileWithBaseline(isolate, *shared_info)) continue;
    Compiler::CompileSharedWithBaseline(
        isolate, shared_info, Compiler::CLEAR_EXCEPTION, &is_compiled_scope);
  }
}

// Create shared function info for top level and shared function infos array for
// inner functions.
template <typename IsolateT>
Handle<SharedFunctionInfo> CreateTopLevelSharedFunctionInfo(
    ParseInfo* parse_info, Handle<Script> script, IsolateT* isolate) {
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
  RCS_SCOPE(isolate, parse_info->flags().is_eval()
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
  NestedTimedHistogram* rate = parse_info->flags().is_eval()
                                   ? isolate->counters()->compile_eval()
                                   : isolate->counters()->compile();
  NestedTimedHistogramScope timer(rate);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               parse_info->flags().is_eval() ? "V8.CompileEval" : "V8.Compile");

  // Create the SharedFunctionInfo and add it to the script's list.
  Handle<SharedFunctionInfo> shared_info =
      CreateTopLevelSharedFunctionInfo(parse_info, script, isolate);

  FinalizeUnoptimizedCompilationDataList
      finalize_unoptimized_compilation_data_list;

  // Prepare and execute compilation of the outer-most function.
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

#ifdef V8_RUNTIME_CALL_STATS
RuntimeCallCounterId RuntimeCallCounterIdForCompile(ParseInfo* parse_info) {
  if (parse_info->flags().is_toplevel()) {
    if (parse_info->flags().is_eval()) {
      return RuntimeCallCounterId::kCompileEval;
    }
    return RuntimeCallCounterId::kCompileScript;
  }
  return RuntimeCallCounterId::kCompileFunction;
}
#endif  // V8_RUNTIME_CALL_STATS

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
    : isolate_for_local_isolate_(isolate),
      flags_(UnoptimizedCompileFlags::ForToplevelCompile(
          isolate, true, construct_language_mode(FLAG_use_strict),
          REPLMode::kNo, type, FLAG_lazy_streaming)),
      character_stream_(ScannerStream::For(streamed_data->source_stream.get(),
                                           streamed_data->encoding)),
      stack_size_(i::FLAG_stack_size),
      worker_thread_runtime_call_stats_(
          isolate->counters()->worker_thread_runtime_call_stats()),
      timer_(isolate->counters()->compile_script_on_background()),
      start_position_(0),
      end_position_(0),
      function_literal_id_(kFunctionLiteralIdTopLevel) {
  VMState<PARSER> state(isolate);

  LOG(isolate, ScriptEvent(Logger::ScriptEventType::kStreamingCompile,
                           flags_.script_id()));
}

BackgroundCompileTask::BackgroundCompileTask(
    Isolate* isolate, Handle<SharedFunctionInfo> shared_info,
    std::unique_ptr<Utf16CharacterStream> character_stream,
    WorkerThreadRuntimeCallStats* worker_thread_runtime_stats,
    TimedHistogram* timer, int max_stack_size)
    : isolate_for_local_isolate_(isolate),
      // TODO(leszeks): Create this from parent compile flags, to avoid
      // accessing the Isolate.
      flags_(
          UnoptimizedCompileFlags::ForFunctionCompile(isolate, *shared_info)),
      character_stream_(std::move(character_stream)),
      stack_size_(max_stack_size),
      worker_thread_runtime_call_stats_(worker_thread_runtime_stats),
      timer_(timer),
      input_shared_info_(shared_info),
      start_position_(shared_info->StartPosition()),
      end_position_(shared_info->EndPosition()),
      function_literal_id_(shared_info->function_literal_id()) {
  DCHECK(!shared_info->is_toplevel());

  character_stream_->Seek(start_position_);

  // Get the script out of the outer ParseInfo and turn it into a persistent
  // handle we can transfer to the background thread.
  persistent_handles_ = std::make_unique<PersistentHandles>(isolate);
  input_shared_info_ = persistent_handles_->NewHandle(shared_info);
}

BackgroundCompileTask::~BackgroundCompileTask() = default;

namespace {

void SetScriptFieldsFromDetails(Isolate* isolate, Script script,
                                ScriptDetails script_details,
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
  Handle<Object> host_defined_options;
  if (script_details.host_defined_options.ToHandle(&host_defined_options)) {
    // TODO(cbruni, chromium:1244145): Remove once migrated to the context.
    if (host_defined_options->IsFixedArray()) {
      script.set_host_defined_options(FixedArray::cast(*host_defined_options));
    }
  }
}

}  // namespace

void BackgroundCompileTask::Run() {
  DCHECK_NE(ThreadId::Current(), isolate_for_local_isolate_->thread_id());
  LocalIsolate isolate(isolate_for_local_isolate_, ThreadKind::kBackground);
  UnparkedScope unparked_scope(&isolate);
  LocalHandleScope handle_scope(&isolate);

  ReusableUnoptimizedCompileState reusable_state(&isolate);

  Run(&isolate, &reusable_state);
}

void BackgroundCompileTask::RunOnMainThread(Isolate* isolate) {
  LocalHandleScope handle_scope(isolate->main_thread_local_isolate());
  ReusableUnoptimizedCompileState reusable_state(isolate);
  Run(isolate->main_thread_local_isolate(), &reusable_state);
}

void BackgroundCompileTask::Run(
    LocalIsolate* isolate, ReusableUnoptimizedCompileState* reusable_state) {
  TimedHistogramScope timer(timer_);

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "BackgroundCompileTask::Run");
  RCS_SCOPE(isolate, RuntimeCallCounterId::kCompileCompileTask,
            RuntimeCallStats::CounterMode::kThreadSpecific);

  bool toplevel_script_compilation = flags_.is_toplevel();

  ParseInfo info(isolate, flags_, &compile_state_, reusable_state,
                 GetCurrentStackPosition() - stack_size_ * KB);
  info.set_character_stream(std::move(character_stream_));

  if (toplevel_script_compilation) {
    DCHECK_NULL(persistent_handles_);
    DCHECK(input_shared_info_.is_null());

    // We don't have the script source, origin, or details yet, so use default
    // values for them. These will be fixed up during the main-thread merge.
    Handle<Script> script = info.CreateScript(
        isolate, isolate->factory()->empty_string(), kNullMaybeHandle,
        ScriptOriginOptions(false, false, false, info.flags().is_module()));
    script_ = isolate->heap()->NewPersistentHandle(script);
  } else {
    DCHECK_NOT_NULL(persistent_handles_);
    isolate->heap()->AttachPersistentHandles(std::move(persistent_handles_));
    Handle<SharedFunctionInfo> shared_info =
        input_shared_info_.ToHandleChecked();
    script_ = isolate->heap()->NewPersistentHandle(
        Script::cast(shared_info->script()));
    info.CheckFlagsForFunctionFromScript(*script_);

    {
      SharedStringAccessGuardIfNeeded access_guard(isolate);
      info.set_function_name(info.ast_value_factory()->GetString(
          shared_info->Name(), access_guard));
    }

    // Get preparsed scope data from the function literal.
    if (shared_info->HasUncompiledDataWithPreparseData()) {
      info.set_consumed_preparse_data(ConsumedPreparseData::For(
          isolate, handle(shared_info->uncompiled_data_with_preparse_data()
                              .preparse_data(isolate),
                          isolate)));
    }
  }

  // Update the character stream's runtime call stats.
  info.character_stream()->set_runtime_call_stats(info.runtime_call_stats());

  // Parser needs to stay alive for finalizing the parsing on the main
  // thread.
  Parser parser(isolate, &info, script_);
  if (flags().is_toplevel()) {
    parser.InitializeEmptyScopeChain(&info);
  } else {
    // TODO(leszeks): Consider keeping Scope zones alive between compile tasks
    // and passing the Scope for the FunctionLiteral through here directly
    // without copying/deserializing.
    Handle<SharedFunctionInfo> shared_info =
        input_shared_info_.ToHandleChecked();
    MaybeHandle<ScopeInfo> maybe_outer_scope_info;
    if (shared_info->HasOuterScopeInfo()) {
      maybe_outer_scope_info =
          handle(shared_info->GetOuterScopeInfo(), isolate);
    }
    parser.DeserializeScopeChain(
        isolate, &info, maybe_outer_scope_info,
        Scope::DeserializationMode::kIncludingVariables);
  }

  parser.ParseOnBackground(isolate, &info, start_position_, end_position_,
                           function_literal_id_);
  parser.UpdateStatistics(script_, &use_counts_, &total_preparse_skipped_);

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.CompileCodeBackground");
  RCS_SCOPE(isolate, RuntimeCallCounterIdForCompile(&info),
            RuntimeCallStats::CounterMode::kThreadSpecific);

  MaybeHandle<SharedFunctionInfo> maybe_result;
  if (info.literal() != nullptr) {
    Handle<SharedFunctionInfo> shared_info;
    if (toplevel_script_compilation) {
      shared_info = CreateTopLevelSharedFunctionInfo(&info, script_, isolate);
    } else {
      // Clone into a placeholder SFI for storing the results.
      shared_info = isolate->factory()->CloneSharedFunctionInfo(
          input_shared_info_.ToHandleChecked());
    }

    if (IterativelyExecuteAndFinalizeUnoptimizedCompilationJobs(
            isolate, shared_info, script_, &info, reusable_state->allocator(),
            &is_compiled_scope_, &finalize_unoptimized_compilation_data_,
            &jobs_to_retry_finalization_on_main_thread_)) {
      maybe_result = shared_info;
    }
  }

  if (maybe_result.is_null()) {
    PreparePendingException(isolate, &info);
  }

  outer_function_sfi_ = isolate->heap()->NewPersistentMaybeHandle(maybe_result);
  DCHECK(isolate->heap()->ContainsPersistentHandle(script_.location()));
  persistent_handles_ = isolate->heap()->DetachPersistentHandles();
}

MaybeHandle<SharedFunctionInfo> BackgroundCompileTask::FinalizeScript(
    Isolate* isolate, Handle<String> source,
    const ScriptDetails& script_details) {
  ScriptOriginOptions origin_options = script_details.origin_options;

  DCHECK(flags_.is_toplevel());
  DCHECK_EQ(flags_.is_module(), origin_options.IsModule());

  MaybeHandle<SharedFunctionInfo> maybe_result;

  // We might not have been able to finalize all jobs on the background
  // thread (e.g. asm.js jobs), so finalize those deferred jobs now.
  if (FinalizeDeferredUnoptimizedCompilationJobs(
          isolate, script_, &jobs_to_retry_finalization_on_main_thread_,
          compile_state_.pending_error_handler(),
          &finalize_unoptimized_compilation_data_)) {
    maybe_result = outer_function_sfi_;
  }

  script_->set_source(*source);
  script_->set_origin_options(origin_options);

  // The one post-hoc fix-up: Add the script to the script list.
  Handle<WeakArrayList> scripts = isolate->factory()->script_list();
  scripts =
      WeakArrayList::Append(isolate, scripts, MaybeObjectHandle::Weak(script_));
  isolate->heap()->SetRootScriptList(*scripts);

  // Set the script fields after finalization, to keep this path the same
  // between main-thread and off-thread finalization.
  {
    DisallowGarbageCollection no_gc;
    SetScriptFieldsFromDetails(isolate, *script_, script_details, &no_gc);
    LOG(isolate, ScriptDetails(*script_));
  }

  ReportStatistics(isolate);

  Handle<SharedFunctionInfo> result;
  if (!maybe_result.ToHandle(&result)) {
    FailWithPreparedPendingException(isolate, script_,
                                     compile_state_.pending_error_handler());
    return kNullMaybeHandle;
  }

  FinalizeUnoptimizedScriptCompilation(isolate, script_, flags_,
                                       &compile_state_,
                                       finalize_unoptimized_compilation_data_);

  return handle(*result, isolate);
}

bool BackgroundCompileTask::FinalizeFunction(
    Isolate* isolate, Compiler::ClearExceptionFlag flag) {
  DCHECK(!flags_.is_toplevel());

  MaybeHandle<SharedFunctionInfo> maybe_result;
  Handle<SharedFunctionInfo> input_shared_info =
      input_shared_info_.ToHandleChecked();

  // The UncompiledData on the input SharedFunctionInfo will have a pointer to
  // the LazyCompileDispatcher Job that launched this task, which will now be
  // considered complete, so clear that regardless of whether the finalize
  // succeeds or not.
  input_shared_info->ClearUncompiledDataJobPointer();

  // We might not have been able to finalize all jobs on the background
  // thread (e.g. asm.js jobs), so finalize those deferred jobs now.
  if (FinalizeDeferredUnoptimizedCompilationJobs(
          isolate, script_, &jobs_to_retry_finalization_on_main_thread_,
          compile_state_.pending_error_handler(),
          &finalize_unoptimized_compilation_data_)) {
    maybe_result = outer_function_sfi_;
  }

  ReportStatistics(isolate);

  Handle<SharedFunctionInfo> result;
  if (!maybe_result.ToHandle(&result)) {
    FailWithPreparedPendingException(
        isolate, script_, compile_state_.pending_error_handler(), flag);
    return false;
  }

  FinalizeUnoptimizedCompilation(isolate, script_, flags_, &compile_state_,
                                 finalize_unoptimized_compilation_data_);

  // Move the compiled data from the placeholder SFI back to the real SFI.
  input_shared_info->CopyFrom(*result);

  return true;
}

void BackgroundCompileTask::AbortFunction() {
  // The UncompiledData on the input SharedFunctionInfo will have a pointer to
  // the LazyCompileDispatcher Job that launched this task, which is about to be
  // deleted, so clear that to avoid the SharedFunctionInfo from pointing to
  // deallocated memory.
  input_shared_info_.ToHandleChecked()->ClearUncompiledDataJobPointer();
}

void BackgroundCompileTask::ReportStatistics(Isolate* isolate) {
  // Update use-counts.
  for (auto feature : use_counts_) {
    isolate->CountUsage(feature);
  }
  if (total_preparse_skipped_ > 0) {
    isolate->counters()->total_preparse_skipped()->Increment(
        total_preparse_skipped_);
  }
}

BackgroundDeserializeTask::BackgroundDeserializeTask(
    Isolate* isolate, std::unique_ptr<ScriptCompiler::CachedData> cached_data)
    : isolate_for_local_isolate_(isolate),
      cached_data_(cached_data->data, cached_data->length) {
  // If the passed in cached data has ownership of the buffer, move it to the
  // task.
  if (cached_data->buffer_policy == ScriptCompiler::CachedData::BufferOwned &&
      !cached_data_.HasDataOwnership()) {
    cached_data->buffer_policy = ScriptCompiler::CachedData::BufferNotOwned;
    cached_data_.AcquireDataOwnership();
  }
}

void BackgroundDeserializeTask::Run() {
  LocalIsolate isolate(isolate_for_local_isolate_, ThreadKind::kBackground);
  UnparkedScope unparked_scope(&isolate);
  LocalHandleScope handle_scope(&isolate);

  Handle<SharedFunctionInfo> inner_result;
  off_thread_data_ =
      CodeSerializer::StartDeserializeOffThread(&isolate, &cached_data_);
}

MaybeHandle<SharedFunctionInfo> BackgroundDeserializeTask::Finish(
    Isolate* isolate, Handle<String> source,
    ScriptOriginOptions origin_options) {
  return CodeSerializer::FinishOffThreadDeserialize(
      isolate, std::move(off_thread_data_), &cached_data_, source,
      origin_options);
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

  // Unfinalized scripts don't yet have the proper source string attached and
  // thus can't be reparsed.
  if (Script::cast(shared_info->script()).IsMaybeUnfinalized(isolate)) {
    bytecode->SetSourcePositionsFailedToCollect();
    return false;
  }

  DCHECK(AllowCompilation::IsAllowed(isolate));
  DCHECK_EQ(ThreadId::Current(), isolate->thread_id());
  DCHECK(!isolate->has_pending_exception());
  VMState<BYTECODE_COMPILER> state(isolate);
  PostponeInterruptsScope postpone(isolate);
  RCS_SCOPE(isolate, RuntimeCallCounterId::kCompileCollectSourcePositions);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.CollectSourcePositions");
  NestedTimedHistogramScope timer(
      isolate->counters()->collect_source_positions());

  // Set up parse info.
  UnoptimizedCompileFlags flags =
      UnoptimizedCompileFlags::ForFunctionCompile(isolate, *shared_info);
  flags.set_collect_source_positions(true);
  // Prevent parallel tasks from being spawned by this job.
  flags.set_post_parallel_compile_tasks_for_eager_toplevel(false);
  flags.set_post_parallel_compile_tasks_for_lazy(false);

  UnoptimizedCompileState compile_state;
  ReusableUnoptimizedCompileState reusable_state(isolate);
  ParseInfo parse_info(isolate, flags, &compile_state, &reusable_state);

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
                       IsCompiledScope* is_compiled_scope,
                       CreateSourcePositions create_source_positions_flag) {
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
  RCS_SCOPE(isolate, RuntimeCallCounterId::kCompileFunction);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.CompileCode");
  AggregatedHistogramTimerScope timer(isolate->counters()->compile_lazy());

  Handle<Script> script(Script::cast(shared_info->script()), isolate);

  // Set up parse info.
  UnoptimizedCompileFlags flags =
      UnoptimizedCompileFlags::ForFunctionCompile(isolate, *shared_info);
  if (create_source_positions_flag == CreateSourcePositions::kYes) {
    flags.set_collect_source_positions(true);
  }

  UnoptimizedCompileState compile_state;
  ReusableUnoptimizedCompileState reusable_state(isolate);
  ParseInfo parse_info(isolate, flags, &compile_state, &reusable_state);

  // Check if the compiler dispatcher has shared_info enqueued for compile.
  LazyCompileDispatcher* dispatcher = isolate->lazy_compile_dispatcher();
  if (dispatcher && dispatcher->IsEnqueued(shared_info)) {
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
  DCHECK(IsNone(function->tiering_state()));
  DCHECK(!function->HasAvailableOptimizedCode());

  // Reset the JSFunction if we are recompiling due to the bytecode having been
  // flushed.
  function->ResetIfCodeFlushed();

  Handle<SharedFunctionInfo> shared_info = handle(function->shared(), isolate);

  // Ensure shared function info is compiled.
  *is_compiled_scope = shared_info->is_compiled_scope(isolate);
  if (!is_compiled_scope->is_compiled() &&
      !Compile(isolate, shared_info, flag, is_compiled_scope)) {
    return false;
  }

  DCHECK(is_compiled_scope->is_compiled());
  Handle<CodeT> code = handle(shared_info->GetCode(), isolate);

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

    const CodeKind code_kind = CodeKindForTopTier();
    const ConcurrencyMode concurrency_mode = ConcurrencyMode::kSynchronous;

    if (FLAG_stress_concurrent_inlining &&
        isolate->concurrent_recompilation_enabled() &&
        isolate->node_observer() == nullptr) {
      SpawnDuplicateConcurrentJobForStressTesting(isolate, function,
                                                  concurrency_mode, code_kind);
    }

    Handle<CodeT> maybe_code;
    if (GetOrCompileOptimized(isolate, function, concurrency_mode, code_kind)
            .ToHandle(&maybe_code)) {
      code = maybe_code;
    }
  }

  // Install code on closure.
  function->set_code(*code, kReleaseStore);

  // Install a feedback vector if necessary.
  if (code->kind() == CodeKind::BASELINE) {
    JSFunction::EnsureFeedbackVector(isolate, function, is_compiled_scope);
  }

  // Check postconditions on success.
  DCHECK(!isolate->has_pending_exception());
  DCHECK(function->shared().is_compiled());
  DCHECK(function->is_compiled());
  return true;
}

// static
bool Compiler::CompileSharedWithBaseline(Isolate* isolate,
                                         Handle<SharedFunctionInfo> shared,
                                         Compiler::ClearExceptionFlag flag,
                                         IsCompiledScope* is_compiled_scope) {
  // We shouldn't be passing uncompiled functions into this function.
  DCHECK(is_compiled_scope->is_compiled());

  // Early return for already baseline-compiled functions.
  if (shared->HasBaselineCode()) return true;

  // Check if we actually can compile with baseline.
  if (!CanCompileWithBaseline(isolate, *shared)) return false;

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
    shared->set_baseline_code(ToCodeT(*code), kReleaseStore);

    if (V8_LIKELY(FLAG_use_osr)) {
      shared->GetBytecodeArray(isolate).RequestOsrAtNextOpportunity();
    }
  }
  double time_taken_ms = time_taken.InMillisecondsF();

  CompilerTracer::TraceFinishBaselineCompile(isolate, shared, time_taken_ms);

  if (shared->script().IsScript()) {
    LogFunctionCompilation(isolate, CodeEventListener::FUNCTION_TAG,
                           handle(Script::cast(shared->script()), isolate),
                           shared, Handle<FeedbackVector>(),
                           Handle<AbstractCode>::cast(code), CodeKind::BASELINE,
                           time_taken_ms);
  }
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
  JSFunction::EnsureFeedbackVector(isolate, function, is_compiled_scope);

  CodeT baseline_code = shared->baseline_code(kAcquireLoad);
  DCHECK_EQ(baseline_code.kind(), CodeKind::BASELINE);
  function->set_code(baseline_code);

  return true;
}

// static
bool Compiler::CompileMaglev(Isolate* isolate, Handle<JSFunction> function,
                             ConcurrencyMode mode,
                             IsCompiledScope* is_compiled_scope) {
#ifdef V8_ENABLE_MAGLEV
  // Bytecode must be available for maglev compilation.
  DCHECK(is_compiled_scope->is_compiled());
  // TODO(v8:7700): Support concurrent compilation.
  DCHECK(IsSynchronous(mode));

  // Maglev code needs a feedback vector.
  JSFunction::EnsureFeedbackVector(isolate, function, is_compiled_scope);

  MaybeHandle<CodeT> maybe_code = Maglev::Compile(isolate, function);
  Handle<CodeT> code;
  if (!maybe_code.ToHandle(&code)) return false;

  DCHECK_EQ(code->kind(), CodeKind::MAGLEV);
  function->set_code(*code);

  return true;
#else
  return false;
#endif  // V8_ENABLE_MAGLEV
}

// static
MaybeHandle<SharedFunctionInfo> Compiler::CompileToplevel(
    ParseInfo* parse_info, Handle<Script> script, Isolate* isolate,
    IsCompiledScope* is_compiled_scope) {
  return v8::internal::CompileToplevel(parse_info, script, kNullMaybeHandle,
                                       isolate, is_compiled_scope);
}

// static
bool Compiler::FinalizeBackgroundCompileTask(BackgroundCompileTask* task,
                                             Isolate* isolate,
                                             ClearExceptionFlag flag) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.FinalizeBackgroundCompileTask");
  RCS_SCOPE(isolate,
            RuntimeCallCounterId::kCompileFinalizeBackgroundCompileTask);

  HandleScope scope(isolate);

  if (!task->FinalizeFunction(isolate, flag)) return false;

  DCHECK(!isolate->has_pending_exception());
  return true;
}

// static
void Compiler::CompileOptimized(Isolate* isolate, Handle<JSFunction> function,
                                ConcurrencyMode mode, CodeKind code_kind) {
  DCHECK(CodeKindIsOptimizedJSFunction(code_kind));
  DCHECK(AllowCompilation::IsAllowed(isolate));

  if (FLAG_stress_concurrent_inlining &&
      isolate->concurrent_recompilation_enabled() && IsSynchronous(mode) &&
      isolate->node_observer() == nullptr) {
    SpawnDuplicateConcurrentJobForStressTesting(isolate, function, mode,
                                                code_kind);
  }

  Handle<CodeT> code;
  if (GetOrCompileOptimized(isolate, function, mode, code_kind)
          .ToHandle(&code)) {
    function->set_code(*code, kReleaseStore);
  }

#ifdef DEBUG
  DCHECK(!isolate->has_pending_exception());
  DCHECK(function->is_compiled());
  DCHECK(function->shared().HasBytecodeArray());
  const TieringState tiering_state = function->tiering_state();
  DCHECK(IsNone(tiering_state) || IsInProgress(tiering_state));
  DCHECK_IMPLIES(IsInProgress(tiering_state), function->ChecksTieringState());
  DCHECK_IMPLIES(IsInProgress(tiering_state), IsConcurrent(mode));
#endif  // DEBUG
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
    int eval_scope_position, int eval_position,
    ParsingWhileDebugging parsing_while_debugging) {
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
        isolate, true, language_mode, REPLMode::kNo, ScriptType::kClassic,
        FLAG_lazy_eval);
    flags.set_is_eval(true);
    flags.set_parsing_while_debugging(parsing_while_debugging);
    DCHECK(!flags.is_module());
    flags.set_parse_restriction(restriction);

    UnoptimizedCompileState compile_state;
    ReusableUnoptimizedCompileState reusable_state(isolate);
    ParseInfo parse_info(isolate, flags, &compile_state, &reusable_state);
    parse_info.set_parameters_end_pos(parameters_end_pos);

    MaybeHandle<ScopeInfo> maybe_outer_scope_info;
    if (!context->IsNativeContext()) {
      maybe_outer_scope_info = handle(context->scope_info(), isolate);
    }
    script = parse_info.CreateScript(
        isolate, source, kNullMaybeHandle,
        OriginOptionsForEval(outer_info->script(), parsing_while_debugging));
    script->set_eval_from_shared(*outer_info);
    if (eval_position == kNoSourcePosition) {
      // If the position is missing, attempt to get the code offset by
      // walking the stack. Do not translate the code offset into source
      // position, but store it as negative value for lazy translation.
      StackTraceFrameIterator it(isolate);
      if (!it.done() && it.is_javascript()) {
        FrameSummary summary = it.GetTopValidFrame();
        script->set_eval_from_shared(
            summary.AsJavaScript().function()->shared());
        script->set_origin_options(
            OriginOptionsForEval(*summary.script(), parsing_while_debugging));
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
  RCS_SCOPE(isolate, RuntimeCallCounterId::kCodeGenerationFromStringsCallbacks);
  DCHECK(context->allow_code_gen_from_strings().IsFalse(isolate));
  DCHECK(isolate->allow_code_gen_callback());
  AllowCodeGenerationFromStringsCallback callback =
      isolate->allow_code_gen_callback();
  ExternalCallbackScope external_callback(isolate,
                                          reinterpret_cast<Address>(callback));
  // Callback set. Let it decide if code generation is allowed.
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
  RCS_SCOPE(isolate, RuntimeCallCounterId::kCodeGenerationFromStringsCallbacks);
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
        all_scripts_histogram_scope_(isolate->counters()->compile_script()),
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
  NestedTimedHistogramScope all_scripts_histogram_scope_;
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

Handle<Script> NewScript(
    Isolate* isolate, ParseInfo* parse_info, Handle<String> source,
    ScriptDetails script_details, NativesFlag natives,
    MaybeHandle<FixedArray> maybe_wrapped_arguments = kNullMaybeHandle) {
  // Create a script object describing the script to be compiled.
  Handle<Script> script =
      parse_info->CreateScript(isolate, source, maybe_wrapped_arguments,
                               script_details.origin_options, natives);
  DisallowGarbageCollection no_gc;
  SetScriptFieldsFromDetails(isolate, *script, script_details, &no_gc);
  LOG(isolate, ScriptDetails(*script));
  return script;
}

MaybeHandle<SharedFunctionInfo> CompileScriptOnMainThread(
    const UnoptimizedCompileFlags flags, Handle<String> source,
    const ScriptDetails& script_details, NativesFlag natives,
    v8::Extension* extension, Isolate* isolate,
    IsCompiledScope* is_compiled_scope) {
  UnoptimizedCompileState compile_state;
  ReusableUnoptimizedCompileState reusable_state(isolate);
  ParseInfo parse_info(isolate, flags, &compile_state, &reusable_state);
  parse_info.set_extension(extension);

  Handle<Script> script =
      NewScript(isolate, &parse_info, source, script_details, natives);
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

bool CanBackgroundCompile(const ScriptDetails& script_details,
                          v8::Extension* extension,
                          ScriptCompiler::CompileOptions compile_options,
                          NativesFlag natives) {
  // TODO(leszeks): Remove the module check once background compilation of
  // modules is supported.
  return !script_details.origin_options.IsModule() && !extension &&
         script_details.repl_mode == REPLMode::kNo &&
         compile_options == ScriptCompiler::kNoCompileOptions &&
         natives == NOT_NATIVES_CODE;
}

bool CompilationExceptionIsRangeError(Isolate* isolate, Handle<Object> obj) {
  if (!obj->IsJSError(isolate)) return false;
  Handle<JSReceiver> js_obj = Handle<JSReceiver>::cast(obj);
  Handle<JSReceiver> constructor;
  if (!JSReceiver::GetConstructor(isolate, js_obj).ToHandle(&constructor)) {
    return false;
  }
  return *constructor == *isolate->range_error_function();
}

MaybeHandle<SharedFunctionInfo> CompileScriptOnBothBackgroundAndMainThread(
    Handle<String> source, const ScriptDetails& script_details,
    Isolate* isolate, IsCompiledScope* is_compiled_scope) {
  // Start a background thread compiling the script.
  StressBackgroundCompileThread background_compile_thread(
      isolate, source,
      script_details.origin_options.IsModule() ? ScriptType::kModule
                                               : ScriptType::kClassic);

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
        flags_copy, source, script_details, NOT_NATIVES_CODE, nullptr, isolate,
        &inner_is_compiled_scope);
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
          isolate, source, script_details, background_compile_thread.data());

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

MaybeHandle<SharedFunctionInfo> GetSharedFunctionInfoForScriptImpl(
    Isolate* isolate, Handle<String> source,
    const ScriptDetails& script_details, v8::Extension* extension,
    AlignedCachedData* cached_data, BackgroundDeserializeTask* deserialize_task,
    ScriptCompiler::CompileOptions compile_options,
    ScriptCompiler::NoCacheReason no_cache_reason, NativesFlag natives) {
  ScriptCompileTimerScope compile_timer(isolate, no_cache_reason);

  if (compile_options == ScriptCompiler::kNoCompileOptions ||
      compile_options == ScriptCompiler::kEagerCompile) {
    DCHECK_NULL(cached_data);
    DCHECK_NULL(deserialize_task);
  } else {
    DCHECK_EQ(compile_options, ScriptCompiler::kConsumeCodeCache);
    // Have to have exactly one of cached_data or deserialize_task.
    DCHECK(cached_data || deserialize_task);
    DCHECK(!(cached_data && deserialize_task));
    DCHECK_NULL(extension);
  }
  int source_length = source->length();
  isolate->counters()->total_load_size()->Increment(source_length);
  isolate->counters()->total_compile_size()->Increment(source_length);

  if (V8_UNLIKELY(
          i::FLAG_experimental_web_snapshots &&
          (source->IsExternalOneByteString() || source->IsSeqOneByteString() ||
           source->IsExternalTwoByteString() || source->IsSeqTwoByteString()) &&
          source_length > 4)) {
    // Experimental: Treat the script as a web snapshot if it starts with the
    // magic byte sequence. TODO(v8:11525): Remove this once proper embedder
    // integration is done.
    bool magic_matches = true;
    for (size_t i = 0;
         i < sizeof(WebSnapshotSerializerDeserializer::kMagicNumber); ++i) {
      if (source->Get(static_cast<int>(i)) !=
          WebSnapshotSerializerDeserializer::kMagicNumber[i]) {
        magic_matches = false;
        break;
      }
    }
    if (magic_matches) {
      return Compiler::GetSharedFunctionInfoForWebSnapshot(
          isolate, source, script_details.name_obj);
    }
  }

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
    maybe_result =
        compilation_cache->LookupScript(source, script_details, language_mode);
    if (!maybe_result.is_null()) {
      compile_timer.set_hit_isolate_cache();
    } else if (can_consume_code_cache) {
      compile_timer.set_consuming_code_cache();
      // Then check cached code provided by embedder.
      NestedTimedHistogramScope timer(
          isolate->counters()->compile_deserialize());
      RCS_SCOPE(isolate, RuntimeCallCounterId::kCompileDeserialize);
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                   "V8.CompileDeserialize");
      if (deserialize_task) {
        // If there's a cache consume task, finish it.
        maybe_result = deserialize_task->Finish(isolate, source,
                                                script_details.origin_options);
      } else {
        maybe_result = CodeSerializer::Deserialize(
            isolate, cached_data, source, script_details.origin_options);
      }

      bool consuming_code_cache_succeeded = false;
      Handle<SharedFunctionInfo> result;
      if (maybe_result.ToHandle(&result)) {
        is_compiled_scope = result->is_compiled_scope(isolate);
        if (is_compiled_scope.is_compiled()) {
          consuming_code_cache_succeeded = true;
          // Promote to per-isolate compilation cache.
          compilation_cache->PutScript(source, language_mode, result);
        }
      }
      if (!consuming_code_cache_succeeded) {
        // Deserializer failed. Fall through to compile.
        compile_timer.set_consuming_code_cache_failed();
      }
    }
  }

  if (maybe_result.is_null()) {
    // No cache entry found compile the script.
    if (FLAG_stress_background_compile &&
        CanBackgroundCompile(script_details, extension, compile_options,
                             natives)) {
      // If the --stress-background-compile flag is set, do the actual
      // compilation on a background thread, and wait for its result.
      maybe_result = CompileScriptOnBothBackgroundAndMainThread(
          source, script_details, isolate, &is_compiled_scope);
    } else {
      UnoptimizedCompileFlags flags =
          UnoptimizedCompileFlags::ForToplevelCompile(
              isolate, natives == NOT_NATIVES_CODE, language_mode,
              script_details.repl_mode,
              script_details.origin_options.IsModule() ? ScriptType::kModule
                                                       : ScriptType::kClassic,
              FLAG_lazy);

      flags.set_is_eager(compile_options == ScriptCompiler::kEagerCompile);

      maybe_result =
          CompileScriptOnMainThread(flags, source, script_details, natives,
                                    extension, isolate, &is_compiled_scope);
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

}  // namespace

MaybeHandle<SharedFunctionInfo> Compiler::GetSharedFunctionInfoForScript(
    Isolate* isolate, Handle<String> source,
    const ScriptDetails& script_details,
    ScriptCompiler::CompileOptions compile_options,
    ScriptCompiler::NoCacheReason no_cache_reason, NativesFlag natives) {
  return GetSharedFunctionInfoForScriptImpl(
      isolate, source, script_details, nullptr, nullptr, nullptr,
      compile_options, no_cache_reason, natives);
}

MaybeHandle<SharedFunctionInfo>
Compiler::GetSharedFunctionInfoForScriptWithExtension(
    Isolate* isolate, Handle<String> source,
    const ScriptDetails& script_details, v8::Extension* extension,
    ScriptCompiler::CompileOptions compile_options, NativesFlag natives) {
  return GetSharedFunctionInfoForScriptImpl(
      isolate, source, script_details, extension, nullptr, nullptr,
      compile_options, ScriptCompiler::kNoCacheBecauseV8Extension, natives);
}

MaybeHandle<SharedFunctionInfo>
Compiler::GetSharedFunctionInfoForScriptWithCachedData(
    Isolate* isolate, Handle<String> source,
    const ScriptDetails& script_details, AlignedCachedData* cached_data,
    ScriptCompiler::CompileOptions compile_options,
    ScriptCompiler::NoCacheReason no_cache_reason, NativesFlag natives) {
  return GetSharedFunctionInfoForScriptImpl(
      isolate, source, script_details, nullptr, cached_data, nullptr,
      compile_options, no_cache_reason, natives);
}

MaybeHandle<SharedFunctionInfo>
Compiler::GetSharedFunctionInfoForScriptWithDeserializeTask(
    Isolate* isolate, Handle<String> source,
    const ScriptDetails& script_details,
    BackgroundDeserializeTask* deserialize_task,
    ScriptCompiler::CompileOptions compile_options,
    ScriptCompiler::NoCacheReason no_cache_reason, NativesFlag natives) {
  return GetSharedFunctionInfoForScriptImpl(
      isolate, source, script_details, nullptr, nullptr, deserialize_task,
      compile_options, no_cache_reason, natives);
}

// static
MaybeHandle<JSFunction> Compiler::GetWrappedFunction(
    Handle<String> source, Handle<FixedArray> arguments,
    Handle<Context> context, const ScriptDetails& script_details,
    AlignedCachedData* cached_data,
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
    NestedTimedHistogramScope timer(isolate->counters()->compile_deserialize());
    RCS_SCOPE(isolate, RuntimeCallCounterId::kCompileDeserialize);
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                 "V8.CompileDeserialize");
    maybe_result = CodeSerializer::Deserialize(isolate, cached_data, source,
                                               script_details.origin_options);
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
        isolate, true, language_mode, script_details.repl_mode,
        ScriptType::kClassic, FLAG_lazy);
    flags.set_is_eval(true);  // Use an eval scope as declaration scope.
    flags.set_function_syntax_kind(FunctionSyntaxKind::kWrapped);
    // TODO(delphick): Remove this and instead make the wrapped and wrapper
    // functions fully non-lazy instead thus preventing source positions from
    // being omitted.
    flags.set_collect_source_positions(true);
    // flags.set_eager(compile_options == ScriptCompiler::kEagerCompile);

    UnoptimizedCompileState compile_state;
    ReusableUnoptimizedCompileState reusable_state(isolate);
    ParseInfo parse_info(isolate, flags, &compile_state, &reusable_state);

    MaybeHandle<ScopeInfo> maybe_outer_scope_info;
    if (!context->IsNativeContext()) {
      maybe_outer_scope_info = handle(context->scope_info(), isolate);
    }

    script = NewScript(isolate, &parse_info, source, script_details,
                       NOT_NATIVES_CODE, arguments);

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
    const ScriptDetails& script_details, ScriptStreamingData* streaming_data) {
  DCHECK(!script_details.origin_options.IsWasm());

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
        source, script_details, task->flags().outer_language_mode());
    if (!maybe_result.is_null()) {
      compile_timer.set_hit_isolate_cache();
    }
  }

  if (maybe_result.is_null()) {
    // No cache entry found, finalize compilation of the script and add it to
    // the isolate cache.
    RCS_SCOPE(isolate,
              RuntimeCallCounterId::kCompilePublishBackgroundFinalization);
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                 "V8.OffThreadFinalization.Publish");

    maybe_result = task->FinalizeScript(isolate, source, script_details);

    Handle<SharedFunctionInfo> result;
    if (maybe_result.ToHandle(&result)) {
      // Add compiled code to the isolate cache.
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                   "V8.StreamingFinalization.AddToCache");
      compilation_cache->PutScript(source, task->flags().outer_language_mode(),
                                   result);
    }
  }

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.StreamingFinalization.Release");
  streaming_data->Release();
  return maybe_result;
}  // namespace internal

// static
Handle<SharedFunctionInfo> Compiler::GetSharedFunctionInfoForWebSnapshot(
    Isolate* isolate, Handle<String> source,
    MaybeHandle<Object> maybe_script_name) {
  // This script won't hold the functions created from the web snapshot;
  // reserving space only for the top-level SharedFunctionInfo is enough.
  Handle<WeakFixedArray> shared_function_infos =
      isolate->factory()->NewWeakFixedArray(1, AllocationType::kOld);
  Handle<Script> script = isolate->factory()->NewScript(source);
  script->set_type(Script::TYPE_WEB_SNAPSHOT);
  script->set_shared_function_infos(*shared_function_infos);
  Handle<Object> script_name;
  if (maybe_script_name.ToHandle(&script_name) && script_name->IsString()) {
    script->set_name(String::cast(*script_name));
  } else {
    script->set_name(*isolate->factory()->empty_string());
  }

  Handle<SharedFunctionInfo> shared =
      isolate->factory()->NewSharedFunctionInfoForWebSnapshot();
  shared->SetScript(isolate->factory()->read_only_roots(), *script, 0, false);
  return shared;
}

// static
template <typename IsolateT>
Handle<SharedFunctionInfo> Compiler::GetSharedFunctionInfo(
    FunctionLiteral* literal, Handle<Script> script, IsolateT* isolate) {
  // Precondition: code has been parsed and scopes have been analyzed.
  MaybeHandle<SharedFunctionInfo> maybe_existing;

  // Find any previously allocated shared function info for the given literal.
  maybe_existing = Script::FindSharedFunctionInfo(script, isolate, literal);

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
MaybeHandle<CodeT> Compiler::CompileOptimizedOSR(Isolate* isolate,
                                                 Handle<JSFunction> function,
                                                 BytecodeOffset osr_offset,
                                                 UnoptimizedFrame* frame,
                                                 ConcurrencyMode mode) {
  DCHECK(IsOSR(osr_offset));
  DCHECK_NOT_NULL(frame);

  if (V8_UNLIKELY(isolate->serializer_enabled())) return {};
  if (V8_UNLIKELY(function->shared().optimization_disabled())) return {};

  // TODO(chromium:1031479): Currently, OSR triggering mechanism is tied to the
  // bytecode array. So, it might be possible to mark closure in one native
  // context and optimize a closure from a different native context. So check if
  // there is a feedback vector before OSRing. We don't expect this to happen
  // often.
  if (V8_UNLIKELY(!function->has_feedback_vector())) return {};

  // One OSR job per function at a time.
  if (IsInProgress(function->osr_tiering_state())) {
    return {};
  }

  // -- Alright, decided to proceed. --

  // Disarm all back edges, i.e. reset the OSR urgency and install target.
  //
  // Note that the bytecode array active on the stack might be different from
  // the one installed on the function (e.g. patched by debugger). This however
  // is fine because we guarantee the layout to be in sync, hence any
  // BytecodeOffset representing the entry point will be valid for any copy of
  // the bytecode.
  Handle<BytecodeArray> bytecode(frame->GetBytecodeArray(), isolate);
  bytecode->reset_osr_urgency_and_install_target();

  CompilerTracer::TraceOptimizeOSR(isolate, function, osr_offset, mode);
  MaybeHandle<CodeT> result = GetOrCompileOptimized(
      isolate, function, mode, CodeKind::TURBOFAN, osr_offset, frame);

  if (result.is_null()) {
    CompilerTracer::TraceOptimizeOSRUnavailable(isolate, function, osr_offset,
                                                mode);
  }

  return result;
}

// static
void Compiler::DisposeTurbofanCompilationJob(TurbofanCompilationJob* job,
                                             bool restore_function_code) {
  Handle<JSFunction> function = job->compilation_info()->closure();
  ResetTieringState(*function, job->compilation_info()->osr_offset());
  if (restore_function_code) {
    function->set_code(function->shared().GetCode(), kReleaseStore);
  }
}

// static
bool Compiler::FinalizeTurbofanCompilationJob(TurbofanCompilationJob* job,
                                              Isolate* isolate) {
  VMState<COMPILER> state(isolate);
  OptimizedCompilationInfo* compilation_info = job->compilation_info();

  TimerEventScope<TimerEventRecompileSynchronous> timer(isolate);
  RCS_SCOPE(isolate, RuntimeCallCounterId::kOptimizeConcurrentFinalize);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
               "V8.OptimizeConcurrentFinalize");

  Handle<JSFunction> function = compilation_info->closure();
  Handle<SharedFunctionInfo> shared = compilation_info->shared_info();

  const bool use_result = !compilation_info->discard_result_for_testing();
  const BytecodeOffset osr_offset = compilation_info->osr_offset();

  if (V8_LIKELY(use_result)) {
    ResetProfilerTicks(*function, osr_offset);
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
      job->RecordCompilationStats(ConcurrencyMode::kConcurrent, isolate);
      job->RecordFunctionCompilation(CodeEventListener::LAZY_COMPILE_TAG,
                                     isolate);
      if (V8_LIKELY(use_result)) {
        ResetTieringState(*function, osr_offset);
        OptimizedCodeCache::Insert(compilation_info);
        CompilerTracer::TraceCompletedJob(isolate, compilation_info);
        if (IsOSR(osr_offset)) {
          if (FLAG_trace_osr) {
            PrintF(CodeTracer::Scope{isolate->GetCodeTracer()}.file(),
                   "[OSR - requesting install. function: %s, osr offset: %d]\n",
                   function->DebugNameCStr().get(), osr_offset.ToInt());
          }
          shared->GetBytecodeArray(isolate).set_osr_install_target(osr_offset);
        } else {
          function->set_code(*compilation_info->code(), kReleaseStore);
        }
      }
      return CompilationJob::SUCCEEDED;
    }
  }

  DCHECK_EQ(job->state(), CompilationJob::State::kFailed);
  CompilerTracer::TraceAbortedJob(isolate, compilation_info);
  if (V8_LIKELY(use_result)) {
    ResetTieringState(*function, osr_offset);
    if (!IsOSR(osr_offset)) {
      function->set_code(shared->GetCode(), kReleaseStore);
    }
  }
  return CompilationJob::FAILED;
}

// static
bool Compiler::FinalizeMaglevCompilationJob(maglev::MaglevCompilationJob* job,
                                            Isolate* isolate) {
#ifdef V8_ENABLE_MAGLEV
  VMState<COMPILER> state(isolate);
  RecordMaglevFunctionCompilation(isolate, job->function());
#endif
  return CompilationJob::SUCCEEDED;
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
          *shared, "new function from shared function info");
      CodeT code = function->feedback_vector().optimized_code();
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
      JSFunction::EnsureFeedbackVector(isolate, function, &is_compiled_scope);
      function->MarkForOptimization(isolate, CodeKind::TURBOFAN,
                                    ConcurrencyMode::kSynchronous);
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
