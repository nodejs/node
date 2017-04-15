// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler.h"

#include <algorithm>
#include <memory>

#include "src/asmjs/asm-js.h"
#include "src/asmjs/asm-typer.h"
#include "src/ast/ast-numbering.h"
#include "src/ast/prettyprinter.h"
#include "src/ast/scopes.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/compilation-cache.h"
#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"
#include "src/compiler/pipeline.h"
#include "src/crankshaft/hydrogen.h"
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
#include "src/parsing/parsing.h"
#include "src/parsing/rewriter.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/runtime-profiler.h"
#include "src/snapshot/code-serializer.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

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
  std::unique_ptr<DisallowHeapAllocation> no_allocation;
  std::unique_ptr<DisallowHandleAllocation> no_handles;
  std::unique_ptr<DisallowHandleDereference> no_deref;
  std::unique_ptr<DisallowCodeDependencyChange> no_dependency_change;
  if (can_execute_on_background_thread()) {
    no_allocation.reset(new DisallowHeapAllocation());
    no_handles.reset(new DisallowHandleAllocation());
    no_deref.reset(new DisallowHandleDereference());
    no_dependency_change.reset(new DisallowCodeDependencyChange());
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
  if (FLAG_hydrogen_stats) {
    isolate()->GetHStatistics()->IncrementSubtotals(time_taken_to_prepare_,
                                                    time_taken_to_execute_,
                                                    time_taken_to_finalize_);
  }
}

Isolate* CompilationJob::isolate() const { return info()->isolate(); }

namespace {

void AddWeakObjectToCodeDependency(Isolate* isolate, Handle<HeapObject> object,
                                   Handle<Code> code) {
  Handle<WeakCell> cell = Code::WeakCellFor(code);
  Heap* heap = isolate->heap();
  if (heap->InNewSpace(*object)) {
    heap->AddWeakNewSpaceObjectToCodeDependency(object, cell);
  } else {
    Handle<DependentCode> dep(heap->LookupWeakObjectToCodeDependency(object));
    dep =
        DependentCode::InsertWeakCode(dep, DependentCode::kWeakCodeGroup, cell);
    heap->AddWeakObjectToCodeDependency(object, dep);
  }
}

}  // namespace

void CompilationJob::RegisterWeakObjectsInOptimizedCode(Handle<Code> code) {
  // TODO(turbofan): Move this to pipeline.cc once Crankshaft dies.
  Isolate* const isolate = code->GetIsolate();
  DCHECK(code->is_optimized_code());
  std::vector<Handle<Map>> maps;
  std::vector<Handle<HeapObject>> objects;
  {
    DisallowHeapAllocation no_gc;
    int const mode_mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
                          RelocInfo::ModeMask(RelocInfo::CELL);
    for (RelocIterator it(*code, mode_mask); !it.done(); it.next()) {
      RelocInfo::Mode mode = it.rinfo()->rmode();
      if (mode == RelocInfo::CELL &&
          code->IsWeakObjectInOptimizedCode(it.rinfo()->target_cell())) {
        objects.push_back(handle(it.rinfo()->target_cell(), isolate));
      } else if (mode == RelocInfo::EMBEDDED_OBJECT &&
                 code->IsWeakObjectInOptimizedCode(
                     it.rinfo()->target_object())) {
        Handle<HeapObject> object(HeapObject::cast(it.rinfo()->target_object()),
                                  isolate);
        if (object->IsMap()) {
          maps.push_back(Handle<Map>::cast(object));
        } else {
          objects.push_back(object);
        }
      }
    }
  }
  for (Handle<Map> map : maps) {
    if (map->dependent_code()->IsEmpty(DependentCode::kWeakCodeGroup)) {
      isolate->heap()->AddRetainedMap(map);
    }
    Map::AddDependentCode(map, DependentCode::kWeakCodeGroup, code);
  }
  for (Handle<HeapObject> object : objects) {
    AddWeakObjectToCodeDependency(isolate, object, code);
  }
  code->set_can_have_weak_objects(true);
}

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

bool UseTurboFan(Handle<SharedFunctionInfo> shared) {
  bool optimization_disabled = shared->optimization_disabled();
  bool must_use_ignition_turbo = shared->must_use_ignition_turbo();

  // Check the enabling conditions for Turbofan.
  // 1. "use asm" code.
  bool is_turbofanable_asm =
      FLAG_turbo_asm && shared->asm_function() && !optimization_disabled;

  // 2. Fallback for features unsupported by Crankshaft.
  bool is_unsupported_by_crankshaft_but_turbofanable =
      must_use_ignition_turbo && strcmp(FLAG_turbo_filter, "~~") == 0 &&
      !optimization_disabled;

  // 3. Explicitly enabled by the command-line filter.
  bool passes_turbo_filter = shared->PassesFilter(FLAG_turbo_filter);

  return is_turbofanable_asm || is_unsupported_by_crankshaft_but_turbofanable ||
         passes_turbo_filter;
}

bool ShouldUseIgnition(CompilationInfo* info) {
  DCHECK(info->has_shared_info());
  Handle<SharedFunctionInfo> shared = info->shared_info();

  // Code which can't be supported by the old pipeline should use Ignition.
  if (shared->must_use_ignition_turbo()) return true;

  // Resumable functions are not supported by {FullCodeGenerator}, suspended
  // activations stored as {JSGeneratorObject} on the heap always assume the
  // underlying code to be based on the bytecode array.
  DCHECK(!IsResumableFunction(shared->kind()));

  // Skip Ignition for asm.js functions.
  if (shared->asm_function()) return false;

  // Skip Ignition for asm wasm code.
  if (FLAG_validate_asm && shared->HasAsmWasmData()) {
    return false;
  }

  // When requesting debug code as a replacement for existing code, we provide
  // the same kind as the existing code (to prevent implicit tier-change).
  if (info->is_debug() && shared->is_compiled()) {
    return !shared->HasBaselineCode();
  }

  // Code destined for TurboFan should be compiled with Ignition first.
  if (UseTurboFan(shared)) return true;

  // Only use Ignition for any other function if FLAG_ignition is true.
  if (!FLAG_ignition) return false;

  // Checks whether top level functions should be passed by the filter.
  if (shared->is_toplevel()) {
    Vector<const char> filter = CStrVector(FLAG_ignition_filter);
    return (filter.length() == 0) || (filter.length() == 1 && filter[0] == '*');
  }

  // Finally respect the filter.
  return shared->PassesFilter(FLAG_ignition_filter);
}

CompilationJob* GetUnoptimizedCompilationJob(CompilationInfo* info) {
  // Function should have been parsed and analyzed before creating a compilation
  // job.
  DCHECK_NOT_NULL(info->literal());
  DCHECK_NOT_NULL(info->scope());

  if (ShouldUseIgnition(info)) {
    return interpreter::Interpreter::NewCompilationJob(info);
  } else {
    return FullCodeGenerator::NewCompilationJob(info);
  }
}

void InstallSharedScopeInfo(CompilationInfo* info,
                            Handle<SharedFunctionInfo> shared) {
  Handle<ScopeInfo> scope_info = info->scope()->scope_info();
  shared->set_scope_info(*scope_info);
  Scope* outer_scope = info->scope()->GetOuterScopeWithContext();
  if (outer_scope) {
    shared->set_outer_scope_info(*outer_scope->scope_info());
  }
}

void InstallSharedCompilationResult(CompilationInfo* info,
                                    Handle<SharedFunctionInfo> shared) {
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
}

void InstallUnoptimizedCode(CompilationInfo* info) {
  Handle<SharedFunctionInfo> shared = info->shared_info();

  // Update the shared function info with the scope info.
  InstallSharedScopeInfo(info, shared);

  // Install compilation result on the shared function info
  InstallSharedCompilationResult(info, shared);
}

CompilationJob::Status FinalizeUnoptimizedCompilationJob(CompilationJob* job) {
  CompilationJob::Status status = job->FinalizeJob();
  if (status == CompilationJob::SUCCEEDED) {
    EnsureFeedbackMetadata(job->info());
    InstallUnoptimizedCode(job->info());
    job->RecordUnoptimizedCompilationStats();
  }
  return status;
}

bool Renumber(ParseInfo* parse_info,
              Compiler::EagerInnerFunctionLiterals* eager_literals) {
  RuntimeCallTimerScope runtimeTimer(parse_info->isolate(),
                                     &RuntimeCallStats::CompileRenumber);
  if (!AstNumbering::Renumber(
          parse_info->isolate()->stack_guard()->real_climit(),
          parse_info->zone(), parse_info->literal(), eager_literals)) {
    return false;
  }
  Handle<SharedFunctionInfo> shared_info = parse_info->shared_info();
  if (!shared_info.is_null()) {
    FunctionLiteral* lit = parse_info->literal();
    shared_info->set_ast_node_count(lit->ast_node_count());
    if (lit->dont_optimize_reason() != kNoReason) {
      shared_info->DisableOptimization(lit->dont_optimize_reason());
    }
    if (lit->flags() & AstProperties::kMustUseIgnitionTurbo) {
      shared_info->set_must_use_ignition_turbo(true);
    }
  }
  return true;
}

bool GenerateUnoptimizedCode(CompilationInfo* info) {
  if (FLAG_validate_asm && info->scope()->asm_module() &&
      !info->shared_info()->is_asm_wasm_broken() && !info->is_debug()) {
    EnsureFeedbackMetadata(info);
    MaybeHandle<FixedArray> wasm_data;
    wasm_data = AsmJs::CompileAsmViaWasm(info);
    if (!wasm_data.is_null()) {
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

bool CompileUnoptimizedInnerFunctionsRecursively(
    ThreadedList<ThreadedListZoneEntry<FunctionLiteral*>>* literals,
    CompilationInfo* outer_info) {
  Isolate* isolate = outer_info->isolate();
  Handle<Script> script = outer_info->script();
  RuntimeCallTimerScope runtimeTimer(isolate,
                                     &RuntimeCallStats::CompileInnerFunction);

  for (auto it : *literals) {
    FunctionLiteral* literal = it->value();

    // Find any previously allocated shared function info for the given literal.
    Handle<SharedFunctionInfo> shared;
    MaybeHandle<SharedFunctionInfo> maybe_existing =
        script->FindSharedFunctionInfo(isolate, literal);
    if (maybe_existing.ToHandle(&shared)) {
      DCHECK(!shared->is_toplevel());
      // If we found an existing shared function info with compiled code,
      // we are done.
      if (shared->is_compiled()) continue;
    } else {
      shared =
          isolate->factory()->NewSharedFunctionInfoForLiteral(literal, script);
      shared->set_is_toplevel(false);
    }

    Zone zone(isolate->allocator(), ZONE_NAME);
    ParseInfo parse_info(&zone, script);
    parse_info.set_literal(literal);
    parse_info.set_shared_info(shared);
    parse_info.set_function_literal_id(shared->function_literal_id());
    parse_info.set_language_mode(literal->scope()->language_mode());
    parse_info.set_ast_value_factory(
        outer_info->parse_info()->ast_value_factory());
    parse_info.set_ast_value_factory_owned(false);

    CompilationInfo info(&parse_info, Handle<JSFunction>::null());
    if (outer_info->will_serialize()) info.PrepareForSerializing();
    if (outer_info->is_debug()) info.MarkAsDebug();

    Compiler::EagerInnerFunctionLiterals inner_literals;
    if (!Renumber(&parse_info, &inner_literals) ||
        !CompileUnoptimizedInnerFunctionsRecursively(&inner_literals,
                                                     outer_info) ||
        !GenerateUnoptimizedCode(&info)) {
      if (!isolate->has_pending_exception()) isolate->StackOverflow();
      return false;
    }

    DCHECK(!info.code().is_null());
    RecordFunctionCompilation(CodeEventListener::FUNCTION_TAG, &info);
    if (literal->should_be_used_once_hint()) {
      info.code()->MarkToBeExecutedOnce(isolate);
    }
  }
  return true;
}

bool CompileUnoptimizedCode(CompilationInfo* info) {
  Isolate* isolate = info->isolate();
  DCHECK(AllowCompilation::IsAllowed(isolate));

  Compiler::EagerInnerFunctionLiterals inner_literals;
  if (!Compiler::Analyze(info->parse_info(), &inner_literals) ||
      !CompileUnoptimizedInnerFunctionsRecursively(&inner_literals, info) ||
      !GenerateUnoptimizedCode(info)) {
    if (!isolate->has_pending_exception()) isolate->StackOverflow();
    return false;
  }

  return true;
}

void EnsureSharedFunctionInfosArrayOnScript(ParseInfo* info) {
  DCHECK(info->is_toplevel());
  DCHECK(!info->script().is_null());
  if (info->script()->shared_function_infos()->length() > 0) {
    DCHECK_EQ(info->script()->shared_function_infos()->length(),
              info->max_function_literal_id() + 1);
    return;
  }
  Isolate* isolate = info->isolate();
  Handle<FixedArray> infos(
      isolate->factory()->NewFixedArray(info->max_function_literal_id() + 1));
  info->script()->set_shared_function_infos(*infos);
}

MUST_USE_RESULT MaybeHandle<Code> GetUnoptimizedCode(CompilationInfo* info) {
  RuntimeCallTimerScope runtimeTimer(
      info->isolate(), &RuntimeCallStats::CompileGetUnoptimizedCode);
  VMState<COMPILER> state(info->isolate());
  PostponeInterruptsScope postpone(info->isolate());

  // Parse and update CompilationInfo with the results.
  if (!parsing::ParseAny(info->parse_info())) return MaybeHandle<Code>();
  if (info->parse_info()->is_toplevel()) {
    EnsureSharedFunctionInfosArrayOnScript(info->parse_info());
  }
  DCHECK_EQ(info->shared_info()->language_mode(),
            info->literal()->language_mode());

  // Compile either unoptimized code or bytecode for the interpreter.
  if (!CompileUnoptimizedCode(info)) return MaybeHandle<Code>();

  // Record the function compilation event.
  RecordFunctionCompilation(CodeEventListener::LAZY_COMPILE_TAG, info);

  return info->code();
}

MUST_USE_RESULT MaybeHandle<Code> GetCodeFromOptimizedCodeMap(
    Handle<JSFunction> function, BailoutId osr_ast_id) {
  RuntimeCallTimerScope runtimeTimer(
      function->GetIsolate(),
      &RuntimeCallStats::CompileGetFromOptimizedCodeMap);
  Handle<SharedFunctionInfo> shared(function->shared());
  DisallowHeapAllocation no_gc;
  CodeAndLiterals cached = shared->SearchOptimizedCodeMap(
      function->context()->native_context(), osr_ast_id);
  if (cached.code != nullptr) {
    // Caching of optimized code enabled and optimized code found.
    if (cached.literals != nullptr) function->set_literals(cached.literals);
    DCHECK(!cached.code->marked_for_deoptimization());
    DCHECK(function->shared()->is_compiled());
    return Handle<Code>(cached.code);
  }
  return MaybeHandle<Code>();
}

void InsertCodeIntoOptimizedCodeMap(CompilationInfo* info) {
  Handle<Code> code = info->code();
  if (code->kind() != Code::OPTIMIZED_FUNCTION) return;  // Nothing to do.

  // Function context specialization folds-in the function context,
  // so no sharing can occur.
  if (info->is_function_context_specializing()) return;
  // Frame specialization implies function context specialization.
  DCHECK(!info->is_frame_specializing());

  // TODO(4764): When compiling for OSR from bytecode, BailoutId might derive
  // from bytecode offset and overlap with actual BailoutId. No caching!
  if (info->is_osr() && info->is_optimizing_from_bytecode()) return;

  // Cache optimized context-specific code.
  Handle<JSFunction> function = info->closure();
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<LiteralsArray> literals(function->literals());
  Handle<Context> native_context(function->context()->native_context());
  SharedFunctionInfo::AddToOptimizedCodeMap(shared, native_context, code,
                                            literals, info->osr_ast_id());
}

bool GetOptimizedCodeNow(CompilationJob* job) {
  CompilationInfo* info = job->info();
  Isolate* isolate = info->isolate();

  // Parsing is not required when optimizing from existing bytecode.
  if (!info->is_optimizing_from_bytecode()) {
    if (!Compiler::ParseAndAnalyze(info->parse_info())) return false;
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
  InsertCodeIntoOptimizedCodeMap(info);
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
    if (!Compiler::ParseAndAnalyze(info->parse_info())) return false;
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
                                   Compiler::ConcurrencyMode mode,
                                   BailoutId osr_ast_id = BailoutId::None(),
                                   JavaScriptFrame* osr_frame = nullptr) {
  Isolate* isolate = function->GetIsolate();
  Handle<SharedFunctionInfo> shared(function->shared(), isolate);

  bool ignition_osr = osr_frame && osr_frame->is_interpreted();
  DCHECK_IMPLIES(ignition_osr, !osr_ast_id.IsNone());
  DCHECK_IMPLIES(ignition_osr, FLAG_ignition_osr);

  // Shared function no longer needs to be tiered up
  shared->set_marked_for_tier_up(false);

  Handle<Code> cached_code;
  // TODO(4764): When compiling for OSR from bytecode, BailoutId might derive
  // from bytecode offset and overlap with actual BailoutId. No lookup!
  if (!ignition_osr &&
      GetCodeFromOptimizedCodeMap(function, osr_ast_id)
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
  if (shared->HasBaselineCode()) {
    shared->code()->set_profiler_ticks(0);
  } else if (shared->HasBytecodeArray()) {
    shared->set_profiler_ticks(0);
  }

  VMState<COMPILER> state(isolate);
  DCHECK(!isolate->has_pending_exception());
  PostponeInterruptsScope postpone(isolate);
  bool use_turbofan = UseTurboFan(shared) || ignition_osr;
  std::unique_ptr<CompilationJob> job(
      use_turbofan ? compiler::Pipeline::NewCompilationJob(function)
                   : new HCompilationJob(function));
  CompilationInfo* info = job->info();
  ParseInfo* parse_info = info->parse_info();

  info->SetOptimizingForOsr(osr_ast_id, osr_frame);

  // Do not use Crankshaft/TurboFan if we need to be able to set break points.
  if (info->shared_info()->HasDebugInfo()) {
    info->AbortOptimization(kFunctionBeingDebugged);
    return MaybeHandle<Code>();
  }

  // Limit the number of times we try to optimize functions.
  const int kMaxOptCount =
      FLAG_deopt_every_n_times == 0 ? FLAG_max_opt_count : 1000;
  if (info->shared_info()->opt_count() > kMaxOptCount) {
    info->AbortOptimization(kDeoptimizedTooManyTimes);
    return MaybeHandle<Code>();
  }

  TimerEventScope<TimerEventOptimizeCode> optimize_code_timer(isolate);
  RuntimeCallTimerScope runtimeTimer(isolate, &RuntimeCallStats::OptimizeCode);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.OptimizeCode");

  // TurboFan can optimize directly from existing bytecode.
  if (use_turbofan && ShouldUseIgnition(info)) {
    if (info->is_osr() && !ignition_osr) return MaybeHandle<Code>();
    DCHECK(shared->HasBytecodeArray());
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
  std::unique_ptr<CompilationHandleScope> compilation;
  if (mode == Compiler::CONCURRENT) {
    compilation.reset(new CompilationHandleScope(info));
  }

  // In case of TurboFan, all handles below will be canonicalized.
  std::unique_ptr<CanonicalHandleScope> canonical;
  if (use_turbofan) canonical.reset(new CanonicalHandleScope(info->isolate()));

  // Reopen handles in the new CompilationHandleScope.
  info->ReopenHandlesInNewHandleScope();
  parse_info->ReopenHandlesInNewHandleScope();

  if (mode == Compiler::CONCURRENT) {
    if (GetOptimizedCodeLater(job.get())) {
      job.release();  // The background recompile job owns this now.
      return isolate->builtins()->InOptimizationQueue();
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
  if (shared->HasBaselineCode()) {
    shared->code()->set_profiler_ticks(0);
  } else if (shared->HasBytecodeArray()) {
    shared->set_profiler_ticks(0);
  }

  DCHECK(!shared->HasDebugInfo());

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
      if (shared
              ->SearchOptimizedCodeMap(info->context()->native_context(),
                                       info->osr_ast_id())
              .code == nullptr) {
        InsertCodeIntoOptimizedCodeMap(info);
      }
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
  return CompilationJob::FAILED;
}

MaybeHandle<Code> GetBaselineCode(Handle<JSFunction> function) {
  Isolate* isolate = function->GetIsolate();
  VMState<COMPILER> state(isolate);
  PostponeInterruptsScope postpone(isolate);
  Zone zone(isolate->allocator(), ZONE_NAME);
  ParseInfo parse_info(&zone, handle(function->shared()));
  CompilationInfo info(&parse_info, function);

  DCHECK(function->shared()->is_compiled());

  // Function no longer needs to be tiered up
  function->shared()->set_marked_for_tier_up(false);

  // Reset profiler ticks, function is no longer considered hot.
  if (function->shared()->HasBytecodeArray()) {
    function->shared()->set_profiler_ticks(0);
  }

  // Nothing left to do if the function already has baseline code.
  if (function->shared()->code()->kind() == Code::FUNCTION) {
    return Handle<Code>(function->shared()->code());
  }

  // We do not switch to baseline code when the debugger might have created a
  // copy of the bytecode with break slots to be able to set break points.
  if (function->shared()->HasDebugInfo()) {
    return MaybeHandle<Code>();
  }

  // Don't generate full-codegen code for functions it can't support.
  if (function->shared()->must_use_ignition_turbo()) {
    return MaybeHandle<Code>();
  }
  DCHECK(!IsResumableFunction(function->shared()->kind()));

  if (FLAG_trace_opt) {
    OFStream os(stdout);
    os << "[switching method " << Brief(*function) << " to baseline code]"
       << std::endl;
  }

  // Parse and update CompilationInfo with the results.
  if (!parsing::ParseFunction(info.parse_info())) return MaybeHandle<Code>();
  Handle<SharedFunctionInfo> shared = info.shared_info();
  DCHECK_EQ(shared->language_mode(), info.literal()->language_mode());

  // Compile baseline code using the full code generator.
  if (!Compiler::Analyze(info.parse_info()) ||
      !FullCodeGenerator::MakeCode(&info)) {
    if (!isolate->has_pending_exception()) isolate->StackOverflow();
    return MaybeHandle<Code>();
  }

  // Update the shared function info with the scope info.
  InstallSharedScopeInfo(&info, shared);

  // Install compilation result on the shared function info
  InstallSharedCompilationResult(&info, shared);

  // Record the function compilation event.
  RecordFunctionCompilation(CodeEventListener::LAZY_COMPILE_TAG, &info);

  return info.code();
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

  Handle<Code> cached_code;
  if (GetCodeFromOptimizedCodeMap(function, BailoutId::None())
          .ToHandle(&cached_code)) {
    if (FLAG_trace_opt) {
      PrintF("[found optimized code for ");
      function->ShortPrint();
      PrintF(" during unoptimized compile]\n");
    }
    DCHECK(function->shared()->is_compiled());
    return cached_code;
  }

  if (function->shared()->is_compiled() &&
      function->shared()->marked_for_tier_up()) {
    DCHECK(FLAG_mark_shared_functions_for_tier_up);

    function->shared()->set_marked_for_tier_up(false);

    switch (Compiler::NextCompilationTier(*function)) {
      case Compiler::BASELINE: {
        if (FLAG_trace_opt) {
          PrintF("[recompiling function ");
          function->ShortPrint();
          PrintF(
              " to baseline eagerly (shared function marked for tier up)]\n");
        }

        Handle<Code> code;
        if (GetBaselineCode(function).ToHandle(&code)) {
          return code;
        }
        break;
      }
      case Compiler::OPTIMIZED: {
        if (FLAG_trace_opt) {
          PrintF("[optimizing method ");
          function->ShortPrint();
          PrintF(" eagerly (shared function marked for tier up)]\n");
        }

        Handle<Code> code;
        // TODO(leszeks): Look into performing this compilation concurrently.
        if (GetOptimizedCode(function, Compiler::NOT_CONCURRENT)
                .ToHandle(&code)) {
          return code;
        }
        break;
      }
      default:
        UNREACHABLE();
    }
  }

  if (function->shared()->is_compiled()) {
    return Handle<Code>(function->shared()->code());
  }

  if (function->shared()->HasBytecodeArray()) {
    Handle<Code> entry = isolate->builtins()->InterpreterEntryTrampoline();
    function->shared()->ReplaceCode(*entry);
    return entry;
  }

  Zone zone(isolate->allocator(), ZONE_NAME);
  ParseInfo parse_info(&zone, handle(function->shared()));
  CompilationInfo info(&parse_info, function);
  Handle<Code> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result, GetUnoptimizedCode(&info), Code);

  if (FLAG_always_opt && !info.shared_info()->HasAsmWasmData()) {
    Handle<Code> opt_code;
    if (GetOptimizedCode(function, Compiler::NOT_CONCURRENT)
            .ToHandle(&opt_code)) {
      result = opt_code;
    }
  }

  return result;
}


Handle<SharedFunctionInfo> CompileToplevel(CompilationInfo* info) {
  Isolate* isolate = info->isolate();
  TimerEventScope<TimerEventCompileCode> timer(isolate);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.CompileCode");
  PostponeInterruptsScope postpone(isolate);
  DCHECK(!isolate->native_context().is_null());
  ParseInfo* parse_info = info->parse_info();

  RuntimeCallTimerScope runtimeTimer(
      isolate, parse_info->is_eval() ? &RuntimeCallStats::CompileEval
                                     : &RuntimeCallStats::CompileScript);

  Handle<Script> script = parse_info->script();

  // TODO(svenpanne) Obscure place for this, perhaps move to OnBeforeCompile?
  FixedArray* array = isolate->native_context()->embedder_data();
  script->set_context_data(array->get(v8::Context::kDebugIdIndex));

  Handle<SharedFunctionInfo> result;

  { VMState<COMPILER> state(info->isolate());
    if (parse_info->literal() == nullptr &&
        !parsing::ParseProgram(parse_info)) {
      return Handle<SharedFunctionInfo>::null();
    }

    EnsureSharedFunctionInfosArrayOnScript(parse_info);

    // Measure how long it takes to do the compilation; only take the
    // rest of the function into account to avoid overlap with the
    // parsing statistics.
    HistogramTimer* rate = parse_info->is_eval()
                               ? info->isolate()->counters()->compile_eval()
                               : info->isolate()->counters()->compile();
    HistogramTimerScope timer(rate);
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                 parse_info->is_eval() ? "V8.CompileEval" : "V8.Compile");

    // Allocate a shared function info object.
    FunctionLiteral* lit = parse_info->literal();
    DCHECK_EQ(kNoSourcePosition, lit->function_token_position());
    result = isolate->factory()->NewSharedFunctionInfoForLiteral(lit, script);
    result->set_is_toplevel(true);
    parse_info->set_shared_info(result);
    parse_info->set_function_literal_id(result->function_literal_id());

    // Compile the code.
    if (!CompileUnoptimizedCode(info)) {
      return Handle<SharedFunctionInfo>::null();
    }

    Handle<String> script_name =
        script->name()->IsString()
            ? Handle<String>(String::cast(script->name()))
            : isolate->factory()->empty_string();
    CodeEventListener::LogEventsAndTags log_tag =
        parse_info->is_eval()
            ? CodeEventListener::EVAL_TAG
            : Logger::ToNativeByScript(CodeEventListener::SCRIPT_TAG, *script);

    PROFILE(isolate, CodeCreateEvent(log_tag, result->abstract_code(), *result,
                                     *script_name));

    if (!script.is_null())
      script->set_compilation_state(Script::COMPILATION_STATE_COMPILED);
  }

  return result;
}

}  // namespace

// ----------------------------------------------------------------------------
// Implementation of Compiler

bool Compiler::Analyze(ParseInfo* info,
                       EagerInnerFunctionLiterals* eager_literals) {
  DCHECK_NOT_NULL(info->literal());
  RuntimeCallTimerScope runtimeTimer(info->isolate(),
                                     &RuntimeCallStats::CompileAnalyse);
  if (!Rewriter::Rewrite(info)) return false;
  DeclarationScope::Analyze(info, AnalyzeMode::kRegular);
  if (!Renumber(info, eager_literals)) return false;
  DCHECK_NOT_NULL(info->scope());
  return true;
}

bool Compiler::ParseAndAnalyze(ParseInfo* info) {
  if (!parsing::ParseAny(info)) return false;
  if (info->is_toplevel()) EnsureSharedFunctionInfosArrayOnScript(info);
  if (!Compiler::Analyze(info)) return false;
  DCHECK_NOT_NULL(info->literal());
  DCHECK_NOT_NULL(info->scope());
  return true;
}

bool Compiler::Compile(Handle<JSFunction> function, ClearExceptionFlag flag) {
  if (function->is_compiled()) return true;
  Isolate* isolate = function->GetIsolate();
  DCHECK(AllowCompilation::IsAllowed(isolate));

  // Start a compilation.
  Handle<Code> code;
  if (!GetLazyCode(function).ToHandle(&code)) {
    if (flag == CLEAR_EXCEPTION) {
      isolate->clear_pending_exception();
    }
    return false;
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

bool Compiler::CompileBaseline(Handle<JSFunction> function) {
  Isolate* isolate = function->GetIsolate();
  DCHECK(AllowCompilation::IsAllowed(isolate));

  // Start a compilation.
  Handle<Code> code;
  if (!GetBaselineCode(function).ToHandle(&code)) {
    // Baseline generation failed, get unoptimized code.
    DCHECK(function->shared()->is_compiled());
    code = handle(function->shared()->code());
    isolate->clear_pending_exception();
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
    // Optimization failed, get unoptimized code.
    DCHECK(!isolate->has_pending_exception());
    if (function->shared()->is_compiled()) {
      code = handle(function->shared()->code(), isolate);
    } else if (function->shared()->HasBytecodeArray()) {
      code = isolate->builtins()->InterpreterEntryTrampoline();
      function->shared()->ReplaceCode(*code);
    } else {
      Zone zone(isolate->allocator(), ZONE_NAME);
      ParseInfo parse_info(&zone, handle(function->shared()));
      CompilationInfo info(&parse_info, function);
      if (!GetUnoptimizedCode(&info).ToHandle(&code)) {
        return false;
      }
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

bool Compiler::CompileDebugCode(Handle<SharedFunctionInfo> shared) {
  Isolate* isolate = shared->GetIsolate();
  DCHECK(AllowCompilation::IsAllowed(isolate));

  // Start a compilation.
  Zone zone(isolate->allocator(), ZONE_NAME);
  ParseInfo parse_info(&zone, shared);
  CompilationInfo info(&parse_info, Handle<JSFunction>::null());
  info.MarkAsDebug();
  if (GetUnoptimizedCode(&info).is_null()) {
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
  Zone zone(isolate->allocator(), ZONE_NAME);
  ParseInfo parse_info(&zone, script);
  CompilationInfo info(&parse_info, Handle<JSFunction>::null());
  info.MarkAsDebug();

  // TODO(635): support extensions.
  const bool compilation_succeeded = !CompileToplevel(&info).is_null();
  Handle<JSArray> infos;
  if (compilation_succeeded) {
    // Check postconditions on success.
    DCHECK(!isolate->has_pending_exception());
    infos = LiveEditFunctionTracker::Collect(parse_info.literal(), script,
                                             &zone, isolate);
  }

  // Restore the original function info list in order to remain side-effect
  // free as much as possible, since some code expects the old shared function
  // infos to stick around.
  script->set_shared_function_infos(*old_function_infos);

  return infos;
}

bool Compiler::EnsureBytecode(CompilationInfo* info) {
  if (!info->shared_info()->is_compiled()) {
    if (GetUnoptimizedCode(info).is_null()) return false;
  }
  DCHECK(info->shared_info()->is_compiled());

  if (info->shared_info()->HasAsmWasmData()) return false;

  DCHECK_EQ(ShouldUseIgnition(info), info->shared_info()->HasBytecodeArray());
  return info->shared_info()->HasBytecodeArray();
}

// TODO(turbofan): In the future, unoptimized code with deopt support could
// be generated lazily once deopt is triggered.
bool Compiler::EnsureDeoptimizationSupport(CompilationInfo* info) {
  DCHECK_NOT_NULL(info->literal());
  DCHECK_NOT_NULL(info->scope());
  Handle<SharedFunctionInfo> shared = info->shared_info();
  if (!shared->has_deoptimization_support()) {
    Zone zone(info->isolate()->allocator(), ZONE_NAME);
    CompilationInfo unoptimized(info->parse_info(), info->closure());
    unoptimized.EnableDeoptimizationSupport();

    // Don't generate full-codegen code for functions it can't support.
    if (shared->must_use_ignition_turbo()) return false;
    DCHECK(!IsResumableFunction(shared->kind()));

    // When we call PrepareForSerializing below, we will change the shared
    // ParseInfo. Make sure to reset it.
    bool old_will_serialize_value = info->parse_info()->will_serialize();

    // If the current code has reloc info for serialization, also include
    // reloc info for serialization for the new code, so that deopt support
    // can be added without losing IC state.
    if (shared->code()->kind() == Code::FUNCTION &&
        shared->code()->has_reloc_info_for_serialization()) {
      unoptimized.PrepareForSerializing();
    }
    EnsureFeedbackMetadata(&unoptimized);

    // Ensure we generate and install bytecode first if the function should use
    // Ignition to avoid implicit tier-down.
    if (!shared->is_compiled() && ShouldUseIgnition(info) &&
        !GenerateUnoptimizedCode(info)) {
      return false;
    }

    if (!FullCodeGenerator::MakeCode(&unoptimized)) return false;

    info->parse_info()->set_will_serialize(old_will_serialize_value);

    // The scope info might not have been set if a lazily compiled
    // function is inlined before being called for the first time.
    if (shared->scope_info() == ScopeInfo::Empty(info->isolate())) {
      InstallSharedScopeInfo(info, shared);
    }

    // Install compilation result on the shared function info
    shared->EnableDeoptimizationSupport(*unoptimized.code());

    // The existing unoptimized code was replaced with the new one.
    RecordFunctionCompilation(CodeEventListener::LAZY_COMPILE_TAG,
                              &unoptimized);
  }
  return true;
}

// static
Compiler::CompilationTier Compiler::NextCompilationTier(JSFunction* function) {
  Handle<SharedFunctionInfo> shared(function->shared(), function->GetIsolate());
  if (shared->IsInterpreted()) {
    if (UseTurboFan(shared)) {
      return OPTIMIZED;
    } else {
      return BASELINE;
    }
  } else {
    return OPTIMIZED;
  }
}

MaybeHandle<JSFunction> Compiler::GetFunctionFromEval(
    Handle<String> source, Handle<SharedFunctionInfo> outer_info,
    Handle<Context> context, LanguageMode language_mode,
    ParseRestriction restriction, int eval_scope_position, int eval_position,
    int line_offset, int column_offset, Handle<Object> script_name,
    ScriptOriginOptions options) {
  Isolate* isolate = source->GetIsolate();
  int source_length = source->length();
  isolate->counters()->total_eval_size()->Increment(source_length);
  isolate->counters()->total_compile_size()->Increment(source_length);

  CompilationCache* compilation_cache = isolate->compilation_cache();
  MaybeHandle<SharedFunctionInfo> maybe_shared_info =
      compilation_cache->LookupEval(source, outer_info, context, language_mode,
                                    eval_scope_position);
  Handle<SharedFunctionInfo> shared_info;

  Handle<Script> script;
  if (!maybe_shared_info.ToHandle(&shared_info)) {
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

    Zone zone(isolate->allocator(), ZONE_NAME);
    ParseInfo parse_info(&zone, script);
    CompilationInfo info(&parse_info, Handle<JSFunction>::null());
    parse_info.set_eval();
    parse_info.set_language_mode(language_mode);
    parse_info.set_parse_restriction(restriction);
    if (!context->IsNativeContext()) {
      parse_info.set_outer_scope_info(handle(context->scope_info()));
    }

    shared_info = CompileToplevel(&info);

    if (shared_info.is_null()) {
      return MaybeHandle<JSFunction>();
    } else {
      // If caller is strict mode, the result must be in strict mode as well.
      DCHECK(is_sloppy(language_mode) ||
             is_strict(shared_info->language_mode()));
      compilation_cache->PutEval(source, outer_info, context, shared_info,
                                 eval_scope_position);
    }
  }

  Handle<JSFunction> result =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          shared_info, context, NOT_TENURED);

  // OnAfterCompile has to be called after we create the JSFunction, which we
  // may require to recompile the eval for debugging, if we find a function
  // that contains break points in the eval script.
  isolate->debug()->OnAfterCompile(script);

  return result;
}

namespace {

bool CodeGenerationFromStringsAllowed(Isolate* isolate,
                                      Handle<Context> context) {
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
    return callback(v8::Utils::ToLocal(context));
  }
}

bool ContainsAsmModule(Handle<Script> script) {
  DisallowHeapAllocation no_gc;
  SharedFunctionInfo::ScriptIterator iter(script);
  while (SharedFunctionInfo* info = iter.Next()) {
    if (info->HasAsmWasmData()) return true;
  }
  return false;
}

}  // namespace

MaybeHandle<JSFunction> Compiler::GetFunctionFromString(
    Handle<Context> context, Handle<String> source,
    ParseRestriction restriction) {
  Isolate* const isolate = context->GetIsolate();
  Handle<Context> native_context(context->native_context(), isolate);

  // Check if native context allows code generation from
  // strings. Throw an exception if it doesn't.
  if (native_context->allow_code_gen_from_strings()->IsFalse(isolate) &&
      !CodeGenerationFromStringsAllowed(isolate, native_context)) {
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
                                       SLOPPY, restriction, eval_scope_position,
                                       eval_position);
}

Handle<SharedFunctionInfo> Compiler::GetSharedFunctionInfoForScript(
    Handle<String> source, Handle<Object> script_name, int line_offset,
    int column_offset, ScriptOriginOptions resource_options,
    Handle<Object> source_map_url, Handle<Context> context,
    v8::Extension* extension, ScriptData** cached_data,
    ScriptCompiler::CompileOptions compile_options, NativesFlag natives,
    bool is_module) {
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
  MaybeHandle<SharedFunctionInfo> maybe_result;
  Handle<SharedFunctionInfo> result;
  if (extension == NULL) {
    // First check per-isolate compilation cache.
    maybe_result = compilation_cache->LookupScript(
        source, script_name, line_offset, column_offset, resource_options,
        context, language_mode);
    if (maybe_result.is_null() && FLAG_serialize_toplevel &&
        compile_options == ScriptCompiler::kConsumeCodeCache &&
        !isolate->debug()->is_loaded()) {
      // Then check cached code provided by embedder.
      HistogramTimerScope timer(isolate->counters()->compile_deserialize());
      RuntimeCallTimerScope runtimeTimer(isolate,
                                         &RuntimeCallStats::CompileDeserialize);
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                   "V8.CompileDeserialize");
      Handle<SharedFunctionInfo> result;
      if (CodeSerializer::Deserialize(isolate, *cached_data, source)
              .ToHandle(&result)) {
        // Promote to per-isolate compilation cache.
        compilation_cache->PutScript(source, context, language_mode, result);
        return result;
      }
      // Deserializer failed. Fall through to compile.
    }
  }

  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization && FLAG_serialize_toplevel &&
      compile_options == ScriptCompiler::kProduceCodeCache) {
    timer.Start();
  }

  if (!maybe_result.ToHandle(&result) ||
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
    Zone zone(isolate->allocator(), ZONE_NAME);
    ParseInfo parse_info(&zone, script);
    CompilationInfo info(&parse_info, Handle<JSFunction>::null());
    if (is_module) parse_info.set_module();
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
      compilation_cache->PutScript(source, context, language_mode, result);
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
      if (natives != EXTENSION_CODE) isolate->ReportPendingMessages();
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

  CompilationInfo compile_info(parse_info, Handle<JSFunction>::null());

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
      name, fun->shared()->num_literals(), FunctionKind::kNormalFunction, code,
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
  return GetOptimizedCode(function, NOT_CONCURRENT, osr_ast_id, osr_frame);
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
    if (FinalizeUnoptimizedCompilationJob(job.get()) ==
        CompilationJob::SUCCEEDED) {
      RecordFunctionCompilation(CodeEventListener::LAZY_COMPILE_TAG,
                                job->info());
      return true;
    }
    return false;
  }
}

void Compiler::PostInstantiation(Handle<JSFunction> function,
                                 PretenureFlag pretenure) {
  Handle<SharedFunctionInfo> shared(function->shared());

  if (FLAG_always_opt && shared->allows_lazy_compilation() &&
      !function->shared()->HasAsmWasmData() &&
      function->shared()->is_compiled()) {
    function->MarkForOptimization();
  }

  CodeAndLiterals cached = shared->SearchOptimizedCodeMap(
      function->context()->native_context(), BailoutId::None());
  if (cached.code != nullptr) {
    // Caching of optimized code enabled and optimized code found.
    DCHECK(!cached.code->marked_for_deoptimization());
    DCHECK(function->shared()->is_compiled());
    function->ReplaceCode(cached.code);
  }

  if (cached.literals != nullptr) {
    DCHECK(shared->is_compiled());
    function->set_literals(cached.literals);
  } else if (shared->is_compiled()) {
    // TODO(mvstanton): pass pretenure flag to EnsureLiterals.
    JSFunction::EnsureLiterals(function);
  }
}

}  // namespace internal
}  // namespace v8
