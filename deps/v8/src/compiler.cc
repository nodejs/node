// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "compiler.h"

#include "bootstrapper.h"
#include "codegen.h"
#include "compilation-cache.h"
#include "cpu-profiler.h"
#include "debug.h"
#include "deoptimizer.h"
#include "full-codegen.h"
#include "gdb-jit.h"
#include "typing.h"
#include "hydrogen.h"
#include "isolate-inl.h"
#include "lithium.h"
#include "liveedit.h"
#include "parser.h"
#include "rewriter.h"
#include "runtime-profiler.h"
#include "scanner-character-streams.h"
#include "scopeinfo.h"
#include "scopes.h"
#include "vm-state-inl.h"

namespace v8 {
namespace internal {


CompilationInfo::CompilationInfo(Handle<Script> script,
                                 Zone* zone)
    : flags_(LanguageModeField::encode(CLASSIC_MODE)),
      script_(script),
      osr_ast_id_(BailoutId::None()),
      parameter_count_(0),
      this_has_uses_(true) {
  Initialize(script->GetIsolate(), BASE, zone);
}


CompilationInfo::CompilationInfo(Handle<SharedFunctionInfo> shared_info,
                                 Zone* zone)
    : flags_(LanguageModeField::encode(CLASSIC_MODE) | IsLazy::encode(true)),
      shared_info_(shared_info),
      script_(Handle<Script>(Script::cast(shared_info->script()))),
      osr_ast_id_(BailoutId::None()),
      parameter_count_(0),
      this_has_uses_(true) {
  Initialize(script_->GetIsolate(), BASE, zone);
}


CompilationInfo::CompilationInfo(Handle<JSFunction> closure,
                                 Zone* zone)
    : flags_(LanguageModeField::encode(CLASSIC_MODE) | IsLazy::encode(true)),
      closure_(closure),
      shared_info_(Handle<SharedFunctionInfo>(closure->shared())),
      script_(Handle<Script>(Script::cast(shared_info_->script()))),
      context_(closure->context()),
      osr_ast_id_(BailoutId::None()),
      parameter_count_(0),
      this_has_uses_(true) {
  Initialize(script_->GetIsolate(), BASE, zone);
}


CompilationInfo::CompilationInfo(HydrogenCodeStub* stub,
                                 Isolate* isolate,
                                 Zone* zone)
    : flags_(LanguageModeField::encode(CLASSIC_MODE) |
             IsLazy::encode(true)),
      osr_ast_id_(BailoutId::None()),
      parameter_count_(0),
      this_has_uses_(true) {
  Initialize(isolate, STUB, zone);
  code_stub_ = stub;
}


void CompilationInfo::Initialize(Isolate* isolate,
                                 Mode mode,
                                 Zone* zone) {
  isolate_ = isolate;
  function_ = NULL;
  scope_ = NULL;
  global_scope_ = NULL;
  extension_ = NULL;
  pre_parse_data_ = NULL;
  zone_ = zone;
  deferred_handles_ = NULL;
  code_stub_ = NULL;
  prologue_offset_ = Code::kPrologueOffsetNotSet;
  opt_count_ = shared_info().is_null() ? 0 : shared_info()->opt_count();
  no_frame_ranges_ = isolate->cpu_profiler()->is_profiling()
                   ? new List<OffsetRange>(2) : NULL;
  for (int i = 0; i < DependentCode::kGroupCount; i++) {
    dependencies_[i] = NULL;
  }
  if (mode == STUB) {
    mode_ = STUB;
    return;
  }
  mode_ = mode;
  abort_due_to_dependency_ = false;
  if (script_->type()->value() == Script::TYPE_NATIVE) {
    MarkAsNative();
  }
  if (!shared_info_.is_null()) {
    ASSERT(language_mode() == CLASSIC_MODE);
    SetLanguageMode(shared_info_->language_mode());
  }
  set_bailout_reason(kUnknown);
}


CompilationInfo::~CompilationInfo() {
  delete deferred_handles_;
  delete no_frame_ranges_;
#ifdef DEBUG
  // Check that no dependent maps have been added or added dependent maps have
  // been rolled back or committed.
  for (int i = 0; i < DependentCode::kGroupCount; i++) {
    ASSERT_EQ(NULL, dependencies_[i]);
  }
#endif  // DEBUG
}


void CompilationInfo::CommitDependencies(Handle<Code> code) {
  for (int i = 0; i < DependentCode::kGroupCount; i++) {
    ZoneList<Handle<HeapObject> >* group_objects = dependencies_[i];
    if (group_objects == NULL) continue;
    ASSERT(!object_wrapper_.is_null());
    for (int j = 0; j < group_objects->length(); j++) {
      DependentCode::DependencyGroup group =
          static_cast<DependentCode::DependencyGroup>(i);
      DependentCode* dependent_code =
          DependentCode::ForObject(group_objects->at(j), group);
      dependent_code->UpdateToFinishedCode(group, this, *code);
    }
    dependencies_[i] = NULL;  // Zone-allocated, no need to delete.
  }
}


void CompilationInfo::RollbackDependencies() {
  // Unregister from all dependent maps if not yet committed.
  for (int i = 0; i < DependentCode::kGroupCount; i++) {
    ZoneList<Handle<HeapObject> >* group_objects = dependencies_[i];
    if (group_objects == NULL) continue;
    for (int j = 0; j < group_objects->length(); j++) {
      DependentCode::DependencyGroup group =
          static_cast<DependentCode::DependencyGroup>(i);
      DependentCode* dependent_code =
          DependentCode::ForObject(group_objects->at(j), group);
      dependent_code->RemoveCompilationInfo(group, this);
    }
    dependencies_[i] = NULL;  // Zone-allocated, no need to delete.
  }
}


int CompilationInfo::num_parameters() const {
  if (IsStub()) {
    ASSERT(parameter_count_ > 0);
    return parameter_count_;
  } else {
    return scope()->num_parameters();
  }
}


int CompilationInfo::num_heap_slots() const {
  if (IsStub()) {
    return 0;
  } else {
    return scope()->num_heap_slots();
  }
}


Code::Flags CompilationInfo::flags() const {
  if (IsStub()) {
    return Code::ComputeFlags(code_stub()->GetCodeKind(),
                              code_stub()->GetICState(),
                              code_stub()->GetExtraICState(),
                              code_stub()->GetStubType(),
                              code_stub()->GetStubFlags());
  } else {
    return Code::ComputeFlags(Code::OPTIMIZED_FUNCTION);
  }
}


// Disable optimization for the rest of the compilation pipeline.
void CompilationInfo::DisableOptimization() {
  bool is_optimizable_closure =
    FLAG_optimize_closures &&
    closure_.is_null() &&
    !scope_->HasTrivialOuterContext() &&
    !scope_->outer_scope_calls_non_strict_eval() &&
    !scope_->inside_with();
  SetMode(is_optimizable_closure ? BASE : NONOPT);
}


// Primitive functions are unlikely to be picked up by the stack-walking
// profiler, so they trigger their own optimization when they're called
// for the SharedFunctionInfo::kCallsUntilPrimitiveOptimization-th time.
bool CompilationInfo::ShouldSelfOptimize() {
  return FLAG_crankshaft &&
      !function()->flags()->Contains(kDontSelfOptimize) &&
      !function()->dont_optimize() &&
      function()->scope()->AllowsLazyCompilation() &&
      (shared_info().is_null() || !shared_info()->optimization_disabled());
}


class HOptimizedGraphBuilderWithPositions: public HOptimizedGraphBuilder {
 public:
  explicit HOptimizedGraphBuilderWithPositions(CompilationInfo* info)
      : HOptimizedGraphBuilder(info) {
  }

#define DEF_VISIT(type)                                 \
  virtual void Visit##type(type* node) V8_OVERRIDE {    \
    if (node->position() != RelocInfo::kNoPosition) {   \
      SetSourcePosition(node->position());              \
    }                                                   \
    HOptimizedGraphBuilder::Visit##type(node);          \
  }
  EXPRESSION_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

#define DEF_VISIT(type)                                          \
  virtual void Visit##type(type* node) V8_OVERRIDE {             \
    if (node->position() != RelocInfo::kNoPosition) {            \
      SetSourcePosition(node->position());                       \
    }                                                            \
    HOptimizedGraphBuilder::Visit##type(node);                   \
  }
  STATEMENT_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

#define DEF_VISIT(type)                                            \
  virtual void Visit##type(type* node) V8_OVERRIDE {               \
    HOptimizedGraphBuilder::Visit##type(node);                     \
  }
  MODULE_NODE_LIST(DEF_VISIT)
  DECLARATION_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT
};


// Determine whether to use the full compiler for all code. If the flag
// --always-full-compiler is specified this is the case. For the virtual frame
// based compiler the full compiler is also used if a debugger is connected, as
// the code from the full compiler supports mode precise break points. For the
// crankshaft adaptive compiler debugging the optimized code is not possible at
// all. However crankshaft support recompilation of functions, so in this case
// the full compiler need not be be used if a debugger is attached, but only if
// break points has actually been set.
static bool IsDebuggerActive(Isolate* isolate) {
#ifdef ENABLE_DEBUGGER_SUPPORT
  return isolate->use_crankshaft() ?
    isolate->debug()->has_break_points() :
    isolate->debugger()->IsDebuggerActive();
#else
  return false;
#endif
}


OptimizedCompileJob::Status OptimizedCompileJob::CreateGraph() {
  ASSERT(isolate()->use_crankshaft());
  ASSERT(info()->IsOptimizing());
  ASSERT(!info()->IsCompilingForDebugging());

  // We should never arrive here if there is no code object on the
  // shared function object.
  ASSERT(info()->shared_info()->code()->kind() == Code::FUNCTION);

  // We should never arrive here if optimization has been disabled on the
  // shared function info.
  ASSERT(!info()->shared_info()->optimization_disabled());

  // Fall back to using the full code generator if it's not possible
  // to use the Hydrogen-based optimizing compiler. We already have
  // generated code for this from the shared function object.
  if (FLAG_always_full_compiler) return AbortOptimization();
  if (IsDebuggerActive(isolate())) return AbortOptimization(kDebuggerIsActive);

  // Limit the number of times we re-compile a functions with
  // the optimizing compiler.
  const int kMaxOptCount =
      FLAG_deopt_every_n_times == 0 ? FLAG_max_opt_count : 1000;
  if (info()->opt_count() > kMaxOptCount) {
    return AbortAndDisableOptimization(kOptimizedTooManyTimes);
  }

  // Due to an encoding limit on LUnallocated operands in the Lithium
  // language, we cannot optimize functions with too many formal parameters
  // or perform on-stack replacement for function with too many
  // stack-allocated local variables.
  //
  // The encoding is as a signed value, with parameters and receiver using
  // the negative indices and locals the non-negative ones.
  const int parameter_limit = -LUnallocated::kMinFixedSlotIndex;
  Scope* scope = info()->scope();
  if ((scope->num_parameters() + 1) > parameter_limit) {
    return AbortAndDisableOptimization(kTooManyParameters);
  }

  const int locals_limit = LUnallocated::kMaxFixedSlotIndex;
  if (info()->is_osr() &&
      scope->num_parameters() + 1 + scope->num_stack_slots() > locals_limit) {
    return AbortAndDisableOptimization(kTooManyParametersLocals);
  }

  // Take --hydrogen-filter into account.
  if (!info()->closure()->PassesFilter(FLAG_hydrogen_filter)) {
    return AbortOptimization(kHydrogenFilter);
  }

  // Recompile the unoptimized version of the code if the current version
  // doesn't have deoptimization support. Alternatively, we may decide to
  // run the full code generator to get a baseline for the compile-time
  // performance of the hydrogen-based compiler.
  bool should_recompile = !info()->shared_info()->has_deoptimization_support();
  if (should_recompile || FLAG_hydrogen_stats) {
    ElapsedTimer timer;
    if (FLAG_hydrogen_stats) {
      timer.Start();
    }
    CompilationInfoWithZone unoptimized(info()->shared_info());
    // Note that we use the same AST that we will use for generating the
    // optimized code.
    unoptimized.SetFunction(info()->function());
    unoptimized.SetScope(info()->scope());
    unoptimized.SetContext(info()->context());
    if (should_recompile) unoptimized.EnableDeoptimizationSupport();
    bool succeeded = FullCodeGenerator::MakeCode(&unoptimized);
    if (should_recompile) {
      if (!succeeded) return SetLastStatus(FAILED);
      Handle<SharedFunctionInfo> shared = info()->shared_info();
      shared->EnableDeoptimizationSupport(*unoptimized.code());
      // The existing unoptimized code was replaced with the new one.
      Compiler::RecordFunctionCompilation(
          Logger::LAZY_COMPILE_TAG, &unoptimized, shared);
    }
    if (FLAG_hydrogen_stats) {
      isolate()->GetHStatistics()->IncrementFullCodeGen(timer.Elapsed());
    }
  }

  // Check that the unoptimized, shared code is ready for
  // optimizations.  When using the always_opt flag we disregard the
  // optimizable marker in the code object and optimize anyway. This
  // is safe as long as the unoptimized code has deoptimization
  // support.
  ASSERT(FLAG_always_opt || info()->shared_info()->code()->optimizable());
  ASSERT(info()->shared_info()->has_deoptimization_support());

  if (FLAG_trace_hydrogen) {
    Handle<String> name = info()->function()->debug_name();
    PrintF("-----------------------------------------------------------\n");
    PrintF("Compiling method %s using hydrogen\n", name->ToCString().get());
    isolate()->GetHTracer()->TraceCompilation(info());
  }

  // Type-check the function.
  AstTyper::Run(info());

  graph_builder_ = FLAG_emit_opt_code_positions
      ? new(info()->zone()) HOptimizedGraphBuilderWithPositions(info())
      : new(info()->zone()) HOptimizedGraphBuilder(info());

  Timer t(this, &time_taken_to_create_graph_);
  info()->set_this_has_uses(false);
  graph_ = graph_builder_->CreateGraph();

  if (isolate()->has_pending_exception()) {
    return SetLastStatus(FAILED);
  }

  // The function being compiled may have bailed out due to an inline
  // candidate bailing out.  In such a case, we don't disable
  // optimization on the shared_info.
  ASSERT(!graph_builder_->inline_bailout() || graph_ == NULL);
  if (graph_ == NULL) {
    if (graph_builder_->inline_bailout()) {
      return AbortOptimization();
    } else {
      return AbortAndDisableOptimization();
    }
  }

  if (info()->HasAbortedDueToDependencyChange()) {
    return AbortOptimization(kBailedOutDueToDependencyChange);
  }

  return SetLastStatus(SUCCEEDED);
}


OptimizedCompileJob::Status OptimizedCompileJob::OptimizeGraph() {
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;
  DisallowCodeDependencyChange no_dependency_change;

  ASSERT(last_status() == SUCCEEDED);
  Timer t(this, &time_taken_to_optimize_);
  ASSERT(graph_ != NULL);
  BailoutReason bailout_reason = kNoReason;

  if (graph_->Optimize(&bailout_reason)) {
    chunk_ = LChunk::NewChunk(graph_);
    if (chunk_ != NULL) return SetLastStatus(SUCCEEDED);
  } else if (bailout_reason != kNoReason) {
    graph_builder_->Bailout(bailout_reason);
  }

  return AbortOptimization();
}


OptimizedCompileJob::Status OptimizedCompileJob::GenerateCode() {
  ASSERT(last_status() == SUCCEEDED);
  ASSERT(!info()->HasAbortedDueToDependencyChange());
  DisallowCodeDependencyChange no_dependency_change;
  {  // Scope for timer.
    Timer timer(this, &time_taken_to_codegen_);
    ASSERT(chunk_ != NULL);
    ASSERT(graph_ != NULL);
    // Deferred handles reference objects that were accessible during
    // graph creation.  To make sure that we don't encounter inconsistencies
    // between graph creation and code generation, we disallow accessing
    // objects through deferred handles during the latter, with exceptions.
    DisallowDeferredHandleDereference no_deferred_handle_deref;
    Handle<Code> optimized_code = chunk_->Codegen();
    if (optimized_code.is_null()) {
      if (info()->bailout_reason() == kNoReason) {
        info_->set_bailout_reason(kCodeGenerationFailed);
      }
      return AbortAndDisableOptimization();
    }
    info()->SetCode(optimized_code);
  }
  RecordOptimizationStats();
  // Add to the weak list of optimized code objects.
  info()->context()->native_context()->AddOptimizedCode(*info()->code());
  return SetLastStatus(SUCCEEDED);
}


void OptimizedCompileJob::RecordOptimizationStats() {
  Handle<JSFunction> function = info()->closure();
  if (!function->IsOptimized()) {
    // Concurrent recompilation and OSR may race.  Increment only once.
    int opt_count = function->shared()->opt_count();
    function->shared()->set_opt_count(opt_count + 1);
  }
  double ms_creategraph = time_taken_to_create_graph_.InMillisecondsF();
  double ms_optimize = time_taken_to_optimize_.InMillisecondsF();
  double ms_codegen = time_taken_to_codegen_.InMillisecondsF();
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
           compiled_functions,
           code_size,
           compilation_time);
  }
  if (FLAG_hydrogen_stats) {
    isolate()->GetHStatistics()->IncrementSubtotals(time_taken_to_create_graph_,
                                                    time_taken_to_optimize_,
                                                    time_taken_to_codegen_);
  }
}


// Sets the expected number of properties based on estimate from compiler.
void SetExpectedNofPropertiesFromEstimate(Handle<SharedFunctionInfo> shared,
                                          int estimate) {
  // See the comment in SetExpectedNofProperties.
  if (shared->live_objects_may_exist()) return;

  // If no properties are added in the constructor, they are more likely
  // to be added later.
  if (estimate == 0) estimate = 2;

  // TODO(yangguo): check whether those heuristics are still up-to-date.
  // We do not shrink objects that go into a snapshot (yet), so we adjust
  // the estimate conservatively.
  if (Serializer::enabled()) {
    estimate += 2;
  } else if (FLAG_clever_optimizations) {
    // Inobject slack tracking will reclaim redundant inobject space later,
    // so we can afford to adjust the estimate generously.
    estimate += 8;
  } else {
    estimate += 3;
  }

  shared->set_expected_nof_properties(estimate);
}


static void UpdateSharedFunctionInfo(CompilationInfo* info) {
  // Update the shared function info with the compiled code and the
  // scope info.  Please note, that the order of the shared function
  // info initialization is important since set_scope_info might
  // trigger a GC, causing the ASSERT below to be invalid if the code
  // was flushed. By setting the code object last we avoid this.
  Handle<SharedFunctionInfo> shared = info->shared_info();
  Handle<ScopeInfo> scope_info =
      ScopeInfo::Create(info->scope(), info->zone());
  shared->set_scope_info(*scope_info);

  Handle<Code> code = info->code();
  CHECK(code->kind() == Code::FUNCTION);
  shared->ReplaceCode(*code);
  if (shared->optimization_disabled()) code->set_optimizable(false);

  // Set the expected number of properties for instances.
  FunctionLiteral* lit = info->function();
  int expected = lit->expected_property_count();
  SetExpectedNofPropertiesFromEstimate(shared, expected);

  // Check the function has compiled code.
  ASSERT(shared->is_compiled());
  shared->set_dont_optimize_reason(lit->dont_optimize_reason());
  shared->set_dont_inline(lit->flags()->Contains(kDontInline));
  shared->set_ast_node_count(lit->ast_node_count());
  shared->set_language_mode(lit->language_mode());
}


// Sets the function info on a function.
// The start_position points to the first '(' character after the function name
// in the full script source. When counting characters in the script source the
// the first character is number 0 (not 1).
static void SetFunctionInfo(Handle<SharedFunctionInfo> function_info,
                            FunctionLiteral* lit,
                            bool is_toplevel,
                            Handle<Script> script) {
  function_info->set_length(lit->parameter_count());
  function_info->set_formal_parameter_count(lit->parameter_count());
  function_info->set_script(*script);
  function_info->set_function_token_position(lit->function_token_position());
  function_info->set_start_position(lit->start_position());
  function_info->set_end_position(lit->end_position());
  function_info->set_is_expression(lit->is_expression());
  function_info->set_is_anonymous(lit->is_anonymous());
  function_info->set_is_toplevel(is_toplevel);
  function_info->set_inferred_name(*lit->inferred_name());
  function_info->set_allows_lazy_compilation(lit->AllowsLazyCompilation());
  function_info->set_allows_lazy_compilation_without_context(
      lit->AllowsLazyCompilationWithoutContext());
  function_info->set_language_mode(lit->language_mode());
  function_info->set_uses_arguments(lit->scope()->arguments() != NULL);
  function_info->set_has_duplicate_parameters(lit->has_duplicate_parameters());
  function_info->set_ast_node_count(lit->ast_node_count());
  function_info->set_is_function(lit->is_function());
  function_info->set_dont_optimize_reason(lit->dont_optimize_reason());
  function_info->set_dont_inline(lit->flags()->Contains(kDontInline));
  function_info->set_dont_cache(lit->flags()->Contains(kDontCache));
  function_info->set_is_generator(lit->is_generator());
}


static bool CompileUnoptimizedCode(CompilationInfo* info) {
  ASSERT(info->function() != NULL);
  if (!Rewriter::Rewrite(info)) return false;
  if (!Scope::Analyze(info)) return false;
  ASSERT(info->scope() != NULL);

  if (!FullCodeGenerator::MakeCode(info)) {
    Isolate* isolate = info->isolate();
    if (!isolate->has_pending_exception()) isolate->StackOverflow();
    return false;
  }
  return true;
}


static Handle<Code> GetUnoptimizedCodeCommon(CompilationInfo* info) {
  VMState<COMPILER> state(info->isolate());
  PostponeInterruptsScope postpone(info->isolate());
  if (!Parser::Parse(info)) return Handle<Code>::null();
  LanguageMode language_mode = info->function()->language_mode();
  info->SetLanguageMode(language_mode);

  if (!CompileUnoptimizedCode(info)) return Handle<Code>::null();
  Compiler::RecordFunctionCompilation(
      Logger::LAZY_COMPILE_TAG, info, info->shared_info());
  UpdateSharedFunctionInfo(info);
  ASSERT_EQ(Code::FUNCTION, info->code()->kind());
  return info->code();
}


Handle<Code> Compiler::GetUnoptimizedCode(Handle<JSFunction> function) {
  ASSERT(!function->GetIsolate()->has_pending_exception());
  ASSERT(!function->is_compiled());
  if (function->shared()->is_compiled()) {
    return Handle<Code>(function->shared()->code());
  }

  CompilationInfoWithZone info(function);
  Handle<Code> result = GetUnoptimizedCodeCommon(&info);
  ASSERT_EQ(result.is_null(), info.isolate()->has_pending_exception());

  if (FLAG_always_opt &&
      !result.is_null() &&
      info.isolate()->use_crankshaft() &&
      !info.shared_info()->optimization_disabled() &&
      !info.isolate()->DebuggerHasBreakPoints()) {
    Handle<Code> opt_code = Compiler::GetOptimizedCode(
        function, result, Compiler::NOT_CONCURRENT);
    if (!opt_code.is_null()) result = opt_code;
  }

  return result;
}


Handle<Code> Compiler::GetUnoptimizedCode(Handle<SharedFunctionInfo> shared) {
  ASSERT(!shared->GetIsolate()->has_pending_exception());
  ASSERT(!shared->is_compiled());

  CompilationInfoWithZone info(shared);
  Handle<Code> result = GetUnoptimizedCodeCommon(&info);
  ASSERT_EQ(result.is_null(), info.isolate()->has_pending_exception());
  return result;
}


bool Compiler::EnsureCompiled(Handle<JSFunction> function,
                              ClearExceptionFlag flag) {
  if (function->is_compiled()) return true;
  Handle<Code> code = Compiler::GetUnoptimizedCode(function);
  if (code.is_null()) {
    if (flag == CLEAR_EXCEPTION) {
      function->GetIsolate()->clear_pending_exception();
    }
    return false;
  }
  function->ReplaceCode(*code);
  ASSERT(function->is_compiled());
  return true;
}


// Compile full code for debugging. This code will have debug break slots
// and deoptimization information. Deoptimization information is required
// in case that an optimized version of this function is still activated on
// the stack. It will also make sure that the full code is compiled with
// the same flags as the previous version, that is flags which can change
// the code generated. The current method of mapping from already compiled
// full code without debug break slots to full code with debug break slots
// depends on the generated code is otherwise exactly the same.
// If compilation fails, just keep the existing code.
Handle<Code> Compiler::GetCodeForDebugging(Handle<JSFunction> function) {
  CompilationInfoWithZone info(function);
  Isolate* isolate = info.isolate();
  VMState<COMPILER> state(isolate);

  ASSERT(!isolate->has_pending_exception());
  Handle<Code> old_code(function->shared()->code());
  ASSERT(old_code->kind() == Code::FUNCTION);
  ASSERT(!old_code->has_debug_break_slots());

  info.MarkCompilingForDebugging();
  if (old_code->is_compiled_optimizable()) {
    info.EnableDeoptimizationSupport();
  } else {
    info.MarkNonOptimizable();
  }
  Handle<Code> new_code = GetUnoptimizedCodeCommon(&info);
  if (new_code.is_null()) {
    isolate->clear_pending_exception();
  } else {
    ASSERT_EQ(old_code->is_compiled_optimizable(),
              new_code->is_compiled_optimizable());
  }
  return new_code;
}


#ifdef ENABLE_DEBUGGER_SUPPORT
void Compiler::CompileForLiveEdit(Handle<Script> script) {
  // TODO(635): support extensions.
  CompilationInfoWithZone info(script);
  PostponeInterruptsScope postpone(info.isolate());
  VMState<COMPILER> state(info.isolate());

  info.MarkAsGlobal();
  if (!Parser::Parse(&info)) return;
  LanguageMode language_mode = info.function()->language_mode();
  info.SetLanguageMode(language_mode);

  LiveEditFunctionTracker tracker(info.isolate(), info.function());
  if (!CompileUnoptimizedCode(&info)) return;
  if (!info.shared_info().is_null()) {
    Handle<ScopeInfo> scope_info = ScopeInfo::Create(info.scope(),
                                                     info.zone());
    info.shared_info()->set_scope_info(*scope_info);
  }
  tracker.RecordRootFunctionInfo(info.code());
}
#endif


static bool DebuggerWantsEagerCompilation(CompilationInfo* info,
                                          bool allow_lazy_without_ctx = false) {
  return LiveEditFunctionTracker::IsActive(info->isolate()) ||
         (info->isolate()->DebuggerHasBreakPoints() && !allow_lazy_without_ctx);
}


static Handle<SharedFunctionInfo> CompileToplevel(CompilationInfo* info) {
  Isolate* isolate = info->isolate();
  PostponeInterruptsScope postpone(isolate);
  ASSERT(!isolate->native_context().is_null());
  Handle<Script> script = info->script();

  // TODO(svenpanne) Obscure place for this, perhaps move to OnBeforeCompile?
  FixedArray* array = isolate->native_context()->embedder_data();
  script->set_context_data(array->get(0));

#ifdef ENABLE_DEBUGGER_SUPPORT
  isolate->debugger()->OnBeforeCompile(script);
#endif

  ASSERT(info->is_eval() || info->is_global());

  bool parse_allow_lazy =
      (info->pre_parse_data() != NULL ||
       String::cast(script->source())->length() > FLAG_min_preparse_length) &&
      !DebuggerWantsEagerCompilation(info);

  Handle<SharedFunctionInfo> result;

  { VMState<COMPILER> state(info->isolate());
    if (!Parser::Parse(info, parse_allow_lazy)) {
      return Handle<SharedFunctionInfo>::null();
    }

    FunctionLiteral* lit = info->function();
    LiveEditFunctionTracker live_edit_tracker(isolate, lit);

    // Measure how long it takes to do the compilation; only take the
    // rest of the function into account to avoid overlap with the
    // parsing statistics.
    HistogramTimer* rate = info->is_eval()
          ? info->isolate()->counters()->compile_eval()
          : info->isolate()->counters()->compile();
    HistogramTimerScope timer(rate);

    // Compile the code.
    if (!CompileUnoptimizedCode(info)) {
      return Handle<SharedFunctionInfo>::null();
    }

    // Allocate function.
    ASSERT(!info->code().is_null());
    result = isolate->factory()->NewSharedFunctionInfo(
        lit->name(),
        lit->materialized_literal_count(),
        lit->is_generator(),
        info->code(),
        ScopeInfo::Create(info->scope(), info->zone()));

    ASSERT_EQ(RelocInfo::kNoPosition, lit->function_token_position());
    SetFunctionInfo(result, lit, true, script);

    Handle<String> script_name = script->name()->IsString()
        ? Handle<String>(String::cast(script->name()))
        : isolate->factory()->empty_string();
    Logger::LogEventsAndTags log_tag = info->is_eval()
        ? Logger::EVAL_TAG
        : Logger::ToNativeByScript(Logger::SCRIPT_TAG, *script);

    PROFILE(isolate, CodeCreateEvent(
                log_tag, *info->code(), *result, info, *script_name));
    GDBJIT(AddCode(script_name, script, info->code(), info));

    // Hint to the runtime system used when allocating space for initial
    // property space by setting the expected number of properties for
    // the instances of the function.
    SetExpectedNofPropertiesFromEstimate(result,
                                         lit->expected_property_count());

    script->set_compilation_state(Script::COMPILATION_STATE_COMPILED);

    live_edit_tracker.RecordFunctionInfo(result, lit, info->zone());
  }

#ifdef ENABLE_DEBUGGER_SUPPORT
  isolate->debugger()->OnAfterCompile(script, Debugger::NO_AFTER_COMPILE_FLAGS);
#endif

  return result;
}


Handle<JSFunction> Compiler::GetFunctionFromEval(Handle<String> source,
                                                 Handle<Context> context,
                                                 LanguageMode language_mode,
                                                 ParseRestriction restriction,
                                                 int scope_position) {
  Isolate* isolate = source->GetIsolate();
  int source_length = source->length();
  isolate->counters()->total_eval_size()->Increment(source_length);
  isolate->counters()->total_compile_size()->Increment(source_length);

  CompilationCache* compilation_cache = isolate->compilation_cache();
  Handle<SharedFunctionInfo> shared_info = compilation_cache->LookupEval(
      source, context, language_mode, scope_position);

  if (shared_info.is_null()) {
    Handle<Script> script = isolate->factory()->NewScript(source);
    CompilationInfoWithZone info(script);
    info.MarkAsEval();
    if (context->IsNativeContext()) info.MarkAsGlobal();
    info.SetLanguageMode(language_mode);
    info.SetParseRestriction(restriction);
    info.SetContext(context);

#if ENABLE_DEBUGGER_SUPPORT
    Debug::RecordEvalCaller(script);
#endif  // ENABLE_DEBUGGER_SUPPORT

    shared_info = CompileToplevel(&info);

    if (shared_info.is_null()) {
      return Handle<JSFunction>::null();
    } else {
      // Explicitly disable optimization for eval code. We're not yet prepared
      // to handle eval-code in the optimizing compiler.
      shared_info->DisableOptimization(kEval);

      // If caller is strict mode, the result must be in strict mode or
      // extended mode as well, but not the other way around. Consider:
      // eval("'use strict'; ...");
      ASSERT(language_mode != STRICT_MODE || !shared_info->is_classic_mode());
      // If caller is in extended mode, the result must also be in
      // extended mode.
      ASSERT(language_mode != EXTENDED_MODE ||
             shared_info->is_extended_mode());
      if (!shared_info->dont_cache()) {
        compilation_cache->PutEval(
            source, context, shared_info, scope_position);
      }
    }
  } else if (shared_info->ic_age() != isolate->heap()->global_ic_age()) {
    shared_info->ResetForNewContext(isolate->heap()->global_ic_age());
  }

  return isolate->factory()->NewFunctionFromSharedFunctionInfo(
      shared_info, context, NOT_TENURED);
}


Handle<SharedFunctionInfo> Compiler::CompileScript(Handle<String> source,
                                                   Handle<Object> script_name,
                                                   int line_offset,
                                                   int column_offset,
                                                   bool is_shared_cross_origin,
                                                   Handle<Context> context,
                                                   v8::Extension* extension,
                                                   ScriptDataImpl* pre_data,
                                                   Handle<Object> script_data,
                                                   NativesFlag natives) {
  Isolate* isolate = source->GetIsolate();
  int source_length = source->length();
  isolate->counters()->total_load_size()->Increment(source_length);
  isolate->counters()->total_compile_size()->Increment(source_length);

  CompilationCache* compilation_cache = isolate->compilation_cache();

  // Do a lookup in the compilation cache but not for extensions.
  Handle<SharedFunctionInfo> result;
  if (extension == NULL) {
    result = compilation_cache->LookupScript(source,
                                             script_name,
                                             line_offset,
                                             column_offset,
                                             is_shared_cross_origin,
                                             context);
  }

  if (result.is_null()) {
    // No cache entry found. Do pre-parsing, if it makes sense, and compile
    // the script.
    // Building preparse data that is only used immediately after is only a
    // saving if we might skip building the AST for lazily compiled functions.
    // I.e., preparse data isn't relevant when the lazy flag is off, and
    // for small sources, odds are that there aren't many functions
    // that would be compiled lazily anyway, so we skip the preparse step
    // in that case too.

    // Create a script object describing the script to be compiled.
    Handle<Script> script = isolate->factory()->NewScript(source);
    if (natives == NATIVES_CODE) {
      script->set_type(Smi::FromInt(Script::TYPE_NATIVE));
    }
    if (!script_name.is_null()) {
      script->set_name(*script_name);
      script->set_line_offset(Smi::FromInt(line_offset));
      script->set_column_offset(Smi::FromInt(column_offset));
    }
    script->set_is_shared_cross_origin(is_shared_cross_origin);

    script->set_data(script_data.is_null() ? isolate->heap()->undefined_value()
                                           : *script_data);

    // Compile the function and add it to the cache.
    CompilationInfoWithZone info(script);
    info.MarkAsGlobal();
    info.SetExtension(extension);
    info.SetPreParseData(pre_data);
    info.SetContext(context);
    if (FLAG_use_strict) {
      info.SetLanguageMode(FLAG_harmony_scoping ? EXTENDED_MODE : STRICT_MODE);
    }
    result = CompileToplevel(&info);
    if (extension == NULL && !result.is_null() && !result->dont_cache()) {
      compilation_cache->PutScript(source, context, result);
    }
  } else if (result->ic_age() != isolate->heap()->global_ic_age()) {
      result->ResetForNewContext(isolate->heap()->global_ic_age());
  }

  if (result.is_null()) isolate->ReportPendingMessages();
  return result;
}


Handle<SharedFunctionInfo> Compiler::BuildFunctionInfo(FunctionLiteral* literal,
                                                       Handle<Script> script) {
  // Precondition: code has been parsed and scopes have been analyzed.
  CompilationInfoWithZone info(script);
  info.SetFunction(literal);
  info.SetScope(literal->scope());
  info.SetLanguageMode(literal->scope()->language_mode());

  Isolate* isolate = info.isolate();
  Factory* factory = isolate->factory();
  LiveEditFunctionTracker live_edit_tracker(isolate, literal);
  // Determine if the function can be lazily compiled. This is necessary to
  // allow some of our builtin JS files to be lazily compiled. These
  // builtins cannot be handled lazily by the parser, since we have to know
  // if a function uses the special natives syntax, which is something the
  // parser records.
  // If the debugger requests compilation for break points, we cannot be
  // aggressive about lazy compilation, because it might trigger compilation
  // of functions without an outer context when setting a breakpoint through
  // Debug::FindSharedFunctionInfoInScript.
  bool allow_lazy_without_ctx = literal->AllowsLazyCompilationWithoutContext();
  bool allow_lazy = literal->AllowsLazyCompilation() &&
      !DebuggerWantsEagerCompilation(&info, allow_lazy_without_ctx);

  // Generate code
  Handle<ScopeInfo> scope_info;
  if (FLAG_lazy && allow_lazy && !literal->is_parenthesized()) {
    Handle<Code> code = isolate->builtins()->CompileUnoptimized();
    info.SetCode(code);
    scope_info = Handle<ScopeInfo>(ScopeInfo::Empty(isolate));
  } else if (FullCodeGenerator::MakeCode(&info)) {
    ASSERT(!info.code().is_null());
    scope_info = ScopeInfo::Create(info.scope(), info.zone());
  } else {
    return Handle<SharedFunctionInfo>::null();
  }

  // Create a shared function info object.
  Handle<SharedFunctionInfo> result =
      factory->NewSharedFunctionInfo(literal->name(),
                                     literal->materialized_literal_count(),
                                     literal->is_generator(),
                                     info.code(),
                                     scope_info);
  SetFunctionInfo(result, literal, false, script);
  RecordFunctionCompilation(Logger::FUNCTION_TAG, &info, result);
  result->set_allows_lazy_compilation(allow_lazy);
  result->set_allows_lazy_compilation_without_context(allow_lazy_without_ctx);

  // Set the expected number of properties for instances and return
  // the resulting function.
  SetExpectedNofPropertiesFromEstimate(result,
                                       literal->expected_property_count());
  live_edit_tracker.RecordFunctionInfo(result, literal, info.zone());
  return result;
}


static Handle<Code> GetCodeFromOptimizedCodeMap(Handle<JSFunction> function,
                                                BailoutId osr_ast_id) {
  if (FLAG_cache_optimized_code) {
    Handle<SharedFunctionInfo> shared(function->shared());
    DisallowHeapAllocation no_gc;
    int index = shared->SearchOptimizedCodeMap(
        function->context()->native_context(), osr_ast_id);
    if (index > 0) {
      if (FLAG_trace_opt) {
        PrintF("[found optimized code for ");
        function->ShortPrint();
        if (!osr_ast_id.IsNone()) {
          PrintF(" at OSR AST id %d", osr_ast_id.ToInt());
        }
        PrintF("]\n");
      }
      FixedArray* literals = shared->GetLiteralsFromOptimizedCodeMap(index);
      if (literals != NULL) function->set_literals(literals);
      return Handle<Code>(shared->GetCodeFromOptimizedCodeMap(index));
    }
  }
  return Handle<Code>::null();
}


static void InsertCodeIntoOptimizedCodeMap(CompilationInfo* info) {
  Handle<Code> code = info->code();
  if (code->kind() != Code::OPTIMIZED_FUNCTION) return;  // Nothing to do.

  // Cache optimized code.
  if (FLAG_cache_optimized_code) {
    Handle<JSFunction> function = info->closure();
    Handle<SharedFunctionInfo> shared(function->shared());
    Handle<FixedArray> literals(function->literals());
    Handle<Context> native_context(function->context()->native_context());
    SharedFunctionInfo::AddToOptimizedCodeMap(
        shared, native_context, code, literals, info->osr_ast_id());
  }
}


static bool CompileOptimizedPrologue(CompilationInfo* info) {
  if (!Parser::Parse(info)) return false;
  LanguageMode language_mode = info->function()->language_mode();
  info->SetLanguageMode(language_mode);

  if (!Rewriter::Rewrite(info)) return false;
  if (!Scope::Analyze(info)) return false;
  ASSERT(info->scope() != NULL);
  return true;
}


static bool GetOptimizedCodeNow(CompilationInfo* info) {
  if (!CompileOptimizedPrologue(info)) return false;

  Logger::TimerEventScope timer(
      info->isolate(), Logger::TimerEventScope::v8_recompile_synchronous);

  OptimizedCompileJob job(info);
  if (job.CreateGraph() != OptimizedCompileJob::SUCCEEDED) return false;
  if (job.OptimizeGraph() != OptimizedCompileJob::SUCCEEDED) return false;
  if (job.GenerateCode() != OptimizedCompileJob::SUCCEEDED) return false;

  // Success!
  ASSERT(!info->isolate()->has_pending_exception());
  InsertCodeIntoOptimizedCodeMap(info);
  Compiler::RecordFunctionCompilation(
      Logger::LAZY_COMPILE_TAG, info, info->shared_info());
  return true;
}


static bool GetOptimizedCodeLater(CompilationInfo* info) {
  Isolate* isolate = info->isolate();
  if (!isolate->optimizing_compiler_thread()->IsQueueAvailable()) {
    if (FLAG_trace_concurrent_recompilation) {
      PrintF("  ** Compilation queue full, will retry optimizing ");
      info->closure()->PrintName();
      PrintF(" later.\n");
    }
    return false;
  }

  CompilationHandleScope handle_scope(info);
  if (!CompileOptimizedPrologue(info)) return false;
  info->SaveHandles();  // Copy handles to the compilation handle scope.

  Logger::TimerEventScope timer(
      isolate, Logger::TimerEventScope::v8_recompile_synchronous);

  OptimizedCompileJob* job = new(info->zone()) OptimizedCompileJob(info);
  OptimizedCompileJob::Status status = job->CreateGraph();
  if (status != OptimizedCompileJob::SUCCEEDED) return false;
  isolate->optimizing_compiler_thread()->QueueForOptimization(job);

  if (FLAG_trace_concurrent_recompilation) {
    PrintF("  ** Queued ");
     info->closure()->PrintName();
    if (info->is_osr()) {
      PrintF(" for concurrent OSR at %d.\n", info->osr_ast_id().ToInt());
    } else {
      PrintF(" for concurrent optimization.\n");
    }
  }
  return true;
}


Handle<Code> Compiler::GetOptimizedCode(Handle<JSFunction> function,
                                        Handle<Code> current_code,
                                        ConcurrencyMode mode,
                                        BailoutId osr_ast_id) {
  Handle<Code> cached_code = GetCodeFromOptimizedCodeMap(function, osr_ast_id);
  if (!cached_code.is_null()) return cached_code;

  SmartPointer<CompilationInfo> info(new CompilationInfoWithZone(function));
  Isolate* isolate = info->isolate();
  VMState<COMPILER> state(isolate);
  ASSERT(!isolate->has_pending_exception());
  PostponeInterruptsScope postpone(isolate);

  Handle<SharedFunctionInfo> shared = info->shared_info();
  ASSERT_NE(ScopeInfo::Empty(isolate), shared->scope_info());
  int compiled_size = shared->end_position() - shared->start_position();
  isolate->counters()->total_compile_size()->Increment(compiled_size);
  current_code->set_profiler_ticks(0);

  info->SetOptimizing(osr_ast_id, current_code);

  if (mode == CONCURRENT) {
    if (GetOptimizedCodeLater(info.get())) {
      info.Detach();  // The background recompile job owns this now.
      return isolate->builtins()->InOptimizationQueue();
    }
  } else {
    if (GetOptimizedCodeNow(info.get())) return info->code();
  }

  // Failed.
  if (FLAG_trace_opt) {
    PrintF("[failed to optimize ");
    function->PrintName();
    PrintF("]\n");
  }

  if (isolate->has_pending_exception()) isolate->clear_pending_exception();
  return Handle<Code>::null();
}


Handle<Code> Compiler::GetConcurrentlyOptimizedCode(OptimizedCompileJob* job) {
  // Take ownership of compilation info.  Deleting compilation info
  // also tears down the zone and the recompile job.
  SmartPointer<CompilationInfo> info(job->info());
  Isolate* isolate = info->isolate();

  VMState<COMPILER> state(isolate);
  Logger::TimerEventScope timer(
      isolate, Logger::TimerEventScope::v8_recompile_synchronous);

  Handle<SharedFunctionInfo> shared = info->shared_info();
  shared->code()->set_profiler_ticks(0);

  // 1) Optimization may have failed.
  // 2) The function may have already been optimized by OSR.  Simply continue.
  //    Except when OSR already disabled optimization for some reason.
  // 3) The code may have already been invalidated due to dependency change.
  // 4) Debugger may have been activated.

  if (job->last_status() != OptimizedCompileJob::SUCCEEDED ||
      shared->optimization_disabled() ||
      info->HasAbortedDueToDependencyChange() ||
      isolate->DebuggerHasBreakPoints()) {
    return Handle<Code>::null();
  }

  if (job->GenerateCode() != OptimizedCompileJob::SUCCEEDED) {
    return Handle<Code>::null();
  }

  Compiler::RecordFunctionCompilation(
      Logger::LAZY_COMPILE_TAG, info.get(), shared);
  if (info->shared_info()->SearchOptimizedCodeMap(
          info->context()->native_context(), info->osr_ast_id()) == -1) {
    InsertCodeIntoOptimizedCodeMap(info.get());
  }

  if (FLAG_trace_concurrent_recompilation) {
    PrintF("  ** Optimized code for ");
    info->closure()->PrintName();
    PrintF(" generated.\n");
  }

  return Handle<Code>(*info->code());
}


void Compiler::RecordFunctionCompilation(Logger::LogEventsAndTags tag,
                                         CompilationInfo* info,
                                         Handle<SharedFunctionInfo> shared) {
  // SharedFunctionInfo is passed separately, because if CompilationInfo
  // was created using Script object, it will not have it.

  // Log the code generation. If source information is available include
  // script name and line number. Check explicitly whether logging is
  // enabled as finding the line number is not free.
  if (info->isolate()->logger()->is_logging_code_events() ||
      info->isolate()->cpu_profiler()->is_profiling()) {
    Handle<Script> script = info->script();
    Handle<Code> code = info->code();
    if (code.is_identical_to(info->isolate()->builtins()->CompileUnoptimized()))
      return;
    int line_num = GetScriptLineNumber(script, shared->start_position()) + 1;
    int column_num =
        GetScriptColumnNumber(script, shared->start_position()) + 1;
    USE(line_num);
    String* script_name = script->name()->IsString()
        ? String::cast(script->name())
        : info->isolate()->heap()->empty_string();
    Logger::LogEventsAndTags log_tag = Logger::ToNativeByScript(tag, *script);
    PROFILE(info->isolate(), CodeCreateEvent(
        log_tag, *code, *shared, info, script_name, line_num, column_num));
  }

  GDBJIT(AddCode(Handle<String>(shared->DebugName()),
                 Handle<Script>(info->script()),
                 Handle<Code>(info->code()),
                 info));
}


CompilationPhase::CompilationPhase(const char* name, CompilationInfo* info)
    : name_(name), info_(info), zone_(info->isolate()) {
  if (FLAG_hydrogen_stats) {
    info_zone_start_allocation_size_ = info->zone()->allocation_size();
    timer_.Start();
  }
}


CompilationPhase::~CompilationPhase() {
  if (FLAG_hydrogen_stats) {
    unsigned size = zone()->allocation_size();
    size += info_->zone()->allocation_size() - info_zone_start_allocation_size_;
    isolate()->GetHStatistics()->SaveTiming(name_, timer_.Elapsed(), size);
  }
}


bool CompilationPhase::ShouldProduceTraceOutput() const {
  // Trace if the appropriate trace flag is set and the phase name's first
  // character is in the FLAG_trace_phase command line parameter.
  AllowHandleDereference allow_deref;
  bool tracing_on = info()->IsStub()
      ? FLAG_trace_hydrogen_stubs
      : (FLAG_trace_hydrogen &&
         info()->closure()->PassesFilter(FLAG_trace_hydrogen_filter));
  return (tracing_on &&
      OS::StrChr(const_cast<char*>(FLAG_trace_phase), name_[0]) != NULL);
}

} }  // namespace v8::internal
