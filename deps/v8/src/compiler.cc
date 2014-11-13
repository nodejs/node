// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/compiler.h"

#include "src/ast-numbering.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/compilation-cache.h"
#include "src/compiler/pipeline.h"
#include "src/cpu-profiler.h"
#include "src/debug.h"
#include "src/deoptimizer.h"
#include "src/full-codegen.h"
#include "src/gdb-jit.h"
#include "src/hydrogen.h"
#include "src/isolate-inl.h"
#include "src/lithium.h"
#include "src/liveedit.h"
#include "src/parser.h"
#include "src/rewriter.h"
#include "src/runtime-profiler.h"
#include "src/scanner-character-streams.h"
#include "src/scopeinfo.h"
#include "src/scopes.h"
#include "src/typing.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {


ScriptData::ScriptData(const byte* data, int length)
    : owns_data_(false), data_(data), length_(length) {
  if (!IsAligned(reinterpret_cast<intptr_t>(data), kPointerAlignment)) {
    byte* copy = NewArray<byte>(length);
    DCHECK(IsAligned(reinterpret_cast<intptr_t>(copy), kPointerAlignment));
    CopyBytes(copy, data, length);
    data_ = copy;
    AcquireDataOwnership();
  }
}


CompilationInfo::CompilationInfo(Handle<Script> script, Zone* zone)
    : flags_(kThisHasUses),
      script_(script),
      source_stream_(NULL),
      osr_ast_id_(BailoutId::None()),
      parameter_count_(0),
      optimization_id_(-1),
      ast_value_factory_(NULL),
      ast_value_factory_owned_(false),
      aborted_due_to_dependency_change_(false) {
  Initialize(script->GetIsolate(), BASE, zone);
}


CompilationInfo::CompilationInfo(Isolate* isolate, Zone* zone)
    : flags_(kThisHasUses),
      script_(Handle<Script>::null()),
      source_stream_(NULL),
      osr_ast_id_(BailoutId::None()),
      parameter_count_(0),
      optimization_id_(-1),
      ast_value_factory_(NULL),
      ast_value_factory_owned_(false),
      aborted_due_to_dependency_change_(false) {
  Initialize(isolate, STUB, zone);
}


CompilationInfo::CompilationInfo(Handle<SharedFunctionInfo> shared_info,
                                 Zone* zone)
    : flags_(kLazy | kThisHasUses),
      shared_info_(shared_info),
      script_(Handle<Script>(Script::cast(shared_info->script()))),
      source_stream_(NULL),
      osr_ast_id_(BailoutId::None()),
      parameter_count_(0),
      optimization_id_(-1),
      ast_value_factory_(NULL),
      ast_value_factory_owned_(false),
      aborted_due_to_dependency_change_(false) {
  Initialize(script_->GetIsolate(), BASE, zone);
}


CompilationInfo::CompilationInfo(Handle<JSFunction> closure, Zone* zone)
    : flags_(kLazy | kThisHasUses),
      closure_(closure),
      shared_info_(Handle<SharedFunctionInfo>(closure->shared())),
      script_(Handle<Script>(Script::cast(shared_info_->script()))),
      source_stream_(NULL),
      context_(closure->context()),
      osr_ast_id_(BailoutId::None()),
      parameter_count_(0),
      optimization_id_(-1),
      ast_value_factory_(NULL),
      ast_value_factory_owned_(false),
      aborted_due_to_dependency_change_(false) {
  Initialize(script_->GetIsolate(), BASE, zone);
}


CompilationInfo::CompilationInfo(HydrogenCodeStub* stub, Isolate* isolate,
                                 Zone* zone)
    : flags_(kLazy | kThisHasUses),
      source_stream_(NULL),
      osr_ast_id_(BailoutId::None()),
      parameter_count_(0),
      optimization_id_(-1),
      ast_value_factory_(NULL),
      ast_value_factory_owned_(false),
      aborted_due_to_dependency_change_(false) {
  Initialize(isolate, STUB, zone);
  code_stub_ = stub;
}


CompilationInfo::CompilationInfo(
    ScriptCompiler::ExternalSourceStream* stream,
    ScriptCompiler::StreamedSource::Encoding encoding, Isolate* isolate,
    Zone* zone)
    : flags_(kThisHasUses),
      source_stream_(stream),
      source_stream_encoding_(encoding),
      osr_ast_id_(BailoutId::None()),
      parameter_count_(0),
      optimization_id_(-1),
      ast_value_factory_(NULL),
      ast_value_factory_owned_(false),
      aborted_due_to_dependency_change_(false) {
  Initialize(isolate, BASE, zone);
}


void CompilationInfo::Initialize(Isolate* isolate,
                                 Mode mode,
                                 Zone* zone) {
  isolate_ = isolate;
  function_ = NULL;
  scope_ = NULL;
  global_scope_ = NULL;
  extension_ = NULL;
  cached_data_ = NULL;
  compile_options_ = ScriptCompiler::kNoCompileOptions;
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
  if (!script_.is_null() && script_->type()->value() == Script::TYPE_NATIVE) {
    MarkAsNative();
  }
  // Compiling for the snapshot typically results in different code than
  // compiling later on. This means that code recompiled with deoptimization
  // support won't be "equivalent" (as defined by SharedFunctionInfo::
  // EnableDeoptimizationSupport), so it will replace the old code and all
  // its type feedback. To avoid this, always compile functions in the snapshot
  // with deoptimization support.
  if (isolate_->serializer_enabled()) EnableDeoptimizationSupport();

  if (isolate_->debug()->is_active()) MarkAsDebug();
  if (FLAG_context_specialization) MarkAsContextSpecializing();
  if (FLAG_turbo_inlining) MarkAsInliningEnabled();
  if (FLAG_turbo_types) MarkAsTypingEnabled();

  if (!shared_info_.is_null()) {
    DCHECK(strict_mode() == SLOPPY);
    SetStrictMode(shared_info_->strict_mode());
  }
  bailout_reason_ = kUnknown;

  if (!shared_info().is_null() && shared_info()->is_compiled()) {
    // We should initialize the CompilationInfo feedback vector from the
    // passed in shared info, rather than creating a new one.
    feedback_vector_ =
        Handle<TypeFeedbackVector>(shared_info()->feedback_vector(), isolate);
  }
}


CompilationInfo::~CompilationInfo() {
  if (GetFlag(kDisableFutureOptimization)) {
    shared_info()->DisableOptimization(bailout_reason());
  }
  delete deferred_handles_;
  delete no_frame_ranges_;
  if (ast_value_factory_owned_) delete ast_value_factory_;
#ifdef DEBUG
  // Check that no dependent maps have been added or added dependent maps have
  // been rolled back or committed.
  for (int i = 0; i < DependentCode::kGroupCount; i++) {
    DCHECK_EQ(NULL, dependencies_[i]);
  }
#endif  // DEBUG
}


void CompilationInfo::CommitDependencies(Handle<Code> code) {
  for (int i = 0; i < DependentCode::kGroupCount; i++) {
    ZoneList<Handle<HeapObject> >* group_objects = dependencies_[i];
    if (group_objects == NULL) continue;
    DCHECK(!object_wrapper_.is_null());
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
    DCHECK(parameter_count_ > 0);
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
                              code_stub()->GetStubType());
  } else {
    return Code::ComputeFlags(Code::OPTIMIZED_FUNCTION);
  }
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


void CompilationInfo::PrepareForCompilation(Scope* scope) {
  DCHECK(scope_ == NULL);
  scope_ = scope;

  if (feedback_vector_.is_null()) {
    // Allocate the feedback vector too.
    feedback_vector_ = isolate()->factory()->NewTypeFeedbackVector(
        function()->slot_count(), function()->ic_slot_count());
  }
  DCHECK(feedback_vector_->Slots() == function()->slot_count() &&
         feedback_vector_->ICSlots() == function()->ic_slot_count());
}


class HOptimizedGraphBuilderWithPositions: public HOptimizedGraphBuilder {
 public:
  explicit HOptimizedGraphBuilderWithPositions(CompilationInfo* info)
      : HOptimizedGraphBuilder(info) {
  }

#define DEF_VISIT(type)                                 \
  virtual void Visit##type(type* node) OVERRIDE {    \
    if (node->position() != RelocInfo::kNoPosition) {   \
      SetSourcePosition(node->position());              \
    }                                                   \
    HOptimizedGraphBuilder::Visit##type(node);          \
  }
  EXPRESSION_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

#define DEF_VISIT(type)                                          \
  virtual void Visit##type(type* node) OVERRIDE {             \
    if (node->position() != RelocInfo::kNoPosition) {            \
      SetSourcePosition(node->position());                       \
    }                                                            \
    HOptimizedGraphBuilder::Visit##type(node);                   \
  }
  STATEMENT_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

#define DEF_VISIT(type)                                            \
  virtual void Visit##type(type* node) OVERRIDE {               \
    HOptimizedGraphBuilder::Visit##type(node);                     \
  }
  MODULE_NODE_LIST(DEF_VISIT)
  DECLARATION_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT
};


OptimizedCompileJob::Status OptimizedCompileJob::CreateGraph() {
  DCHECK(info()->IsOptimizing());
  DCHECK(!info()->IsCompilingForDebugging());

  // We should never arrive here if optimization has been disabled on the
  // shared function info.
  DCHECK(!info()->shared_info()->optimization_disabled());

  // Do not use crankshaft if we need to be able to set break points.
  if (isolate()->DebuggerHasBreakPoints()) {
    return RetryOptimization(kDebuggerHasBreakPoints);
  }

  // Limit the number of times we re-compile a functions with
  // the optimizing compiler.
  const int kMaxOptCount =
      FLAG_deopt_every_n_times == 0 ? FLAG_max_opt_count : 1000;
  if (info()->opt_count() > kMaxOptCount) {
    return AbortOptimization(kOptimizedTooManyTimes);
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
    return AbortOptimization(kTooManyParameters);
  }

  const int locals_limit = LUnallocated::kMaxFixedSlotIndex;
  if (info()->is_osr() &&
      scope->num_parameters() + 1 + scope->num_stack_slots() > locals_limit) {
    return AbortOptimization(kTooManyParametersLocals);
  }

  if (scope->HasIllegalRedeclaration()) {
    return AbortOptimization(kFunctionWithIllegalRedeclaration);
  }

  // Check the whitelist for Crankshaft.
  if (!info()->closure()->PassesFilter(FLAG_hydrogen_filter)) {
    return AbortOptimization(kHydrogenFilter);
  }

  // Crankshaft requires a version of fullcode with deoptimization support.
  // Recompile the unoptimized version of the code if the current version
  // doesn't have deoptimization support already.
  // Otherwise, if we are gathering compilation time and space statistics
  // for hydrogen, gather baseline statistics for a fullcode compilation.
  bool should_recompile = !info()->shared_info()->has_deoptimization_support();
  if (should_recompile || FLAG_hydrogen_stats) {
    base::ElapsedTimer timer;
    if (FLAG_hydrogen_stats) {
      timer.Start();
    }
    if (!Compiler::EnsureDeoptimizationSupport(info())) {
      return SetLastStatus(FAILED);
    }
    if (FLAG_hydrogen_stats) {
      isolate()->GetHStatistics()->IncrementFullCodeGen(timer.Elapsed());
    }
  }

  DCHECK(info()->shared_info()->has_deoptimization_support());

  // Check the whitelist for TurboFan.
  if ((FLAG_turbo_asm && info()->shared_info()->asm_function()) ||
      info()->closure()->PassesFilter(FLAG_turbo_filter)) {
    compiler::Pipeline pipeline(info());
    pipeline.GenerateCode();
    if (!info()->code().is_null()) {
      return SetLastStatus(SUCCEEDED);
    }
  }

  if (FLAG_trace_hydrogen) {
    Handle<String> name = info()->function()->debug_name();
    PrintF("-----------------------------------------------------------\n");
    PrintF("Compiling method %s using hydrogen\n", name->ToCString().get());
    isolate()->GetHTracer()->TraceCompilation(info());
  }

  // Type-check the function.
  AstTyper::Run(info());

  graph_builder_ = (FLAG_hydrogen_track_positions || FLAG_trace_ic)
      ? new(info()->zone()) HOptimizedGraphBuilderWithPositions(info())
      : new(info()->zone()) HOptimizedGraphBuilder(info());

  Timer t(this, &time_taken_to_create_graph_);
  info()->set_this_has_uses(false);
  graph_ = graph_builder_->CreateGraph();

  if (isolate()->has_pending_exception()) {
    return SetLastStatus(FAILED);
  }

  if (graph_ == NULL) return SetLastStatus(BAILED_OUT);

  if (info()->HasAbortedDueToDependencyChange()) {
    // Dependency has changed during graph creation. Let's try again later.
    return RetryOptimization(kBailedOutDueToDependencyChange);
  }

  return SetLastStatus(SUCCEEDED);
}


OptimizedCompileJob::Status OptimizedCompileJob::OptimizeGraph() {
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;
  DisallowCodeDependencyChange no_dependency_change;

  DCHECK(last_status() == SUCCEEDED);
  // TODO(turbofan): Currently everything is done in the first phase.
  if (!info()->code().is_null()) {
    return last_status();
  }

  Timer t(this, &time_taken_to_optimize_);
  DCHECK(graph_ != NULL);
  BailoutReason bailout_reason = kNoReason;

  if (graph_->Optimize(&bailout_reason)) {
    chunk_ = LChunk::NewChunk(graph_);
    if (chunk_ != NULL) return SetLastStatus(SUCCEEDED);
  } else if (bailout_reason != kNoReason) {
    graph_builder_->Bailout(bailout_reason);
  }

  return SetLastStatus(BAILED_OUT);
}


OptimizedCompileJob::Status OptimizedCompileJob::GenerateCode() {
  DCHECK(last_status() == SUCCEEDED);
  // TODO(turbofan): Currently everything is done in the first phase.
  if (!info()->code().is_null()) {
    if (FLAG_turbo_deoptimization) {
      info()->context()->native_context()->AddOptimizedCode(*info()->code());
    }
    RecordOptimizationStats();
    return last_status();
  }

  DCHECK(!info()->HasAbortedDueToDependencyChange());
  DisallowCodeDependencyChange no_dependency_change;
  DisallowJavascriptExecution no_js(isolate());
  {  // Scope for timer.
    Timer timer(this, &time_taken_to_codegen_);
    DCHECK(chunk_ != NULL);
    DCHECK(graph_ != NULL);
    // Deferred handles reference objects that were accessible during
    // graph creation.  To make sure that we don't encounter inconsistencies
    // between graph creation and code generation, we disallow accessing
    // objects through deferred handles during the latter, with exceptions.
    DisallowDeferredHandleDereference no_deferred_handle_deref;
    Handle<Code> optimized_code = chunk_->Codegen();
    if (optimized_code.is_null()) {
      if (info()->bailout_reason() == kNoReason) {
        return AbortOptimization(kCodeGenerationFailed);
      }
      return SetLastStatus(BAILED_OUT);
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
  // If no properties are added in the constructor, they are more likely
  // to be added later.
  if (estimate == 0) estimate = 2;

  // TODO(yangguo): check whether those heuristics are still up-to-date.
  // We do not shrink objects that go into a snapshot (yet), so we adjust
  // the estimate conservatively.
  if (shared->GetIsolate()->serializer_enabled()) {
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
  function_info->set_strict_mode(lit->strict_mode());
  function_info->set_uses_arguments(lit->scope()->arguments() != NULL);
  function_info->set_has_duplicate_parameters(lit->has_duplicate_parameters());
  function_info->set_ast_node_count(lit->ast_node_count());
  function_info->set_is_function(lit->is_function());
  function_info->set_bailout_reason(lit->dont_optimize_reason());
  function_info->set_dont_cache(lit->flags()->Contains(kDontCache));
  function_info->set_kind(lit->kind());
  function_info->set_asm_function(lit->scope()->asm_function());
}


static void RecordFunctionCompilation(Logger::LogEventsAndTags tag,
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
    if (code.is_identical_to(info->isolate()->builtins()->CompileLazy())) {
      return;
    }
    int line_num = Script::GetLineNumber(script, shared->start_position()) + 1;
    int column_num =
        Script::GetColumnNumber(script, shared->start_position()) + 1;
    String* script_name = script->name()->IsString()
                              ? String::cast(script->name())
                              : info->isolate()->heap()->empty_string();
    Logger::LogEventsAndTags log_tag = Logger::ToNativeByScript(tag, *script);
    PROFILE(info->isolate(),
            CodeCreateEvent(log_tag, *code, *shared, info, script_name,
                            line_num, column_num));
  }

  GDBJIT(AddCode(Handle<String>(shared->DebugName()),
                 Handle<Script>(info->script()), Handle<Code>(info->code()),
                 info));
}


static bool CompileUnoptimizedCode(CompilationInfo* info) {
  DCHECK(AllowCompilation::IsAllowed(info->isolate()));
  if (!Compiler::Analyze(info) || !FullCodeGenerator::MakeCode(info)) {
    Isolate* isolate = info->isolate();
    if (!isolate->has_pending_exception()) isolate->StackOverflow();
    return false;
  }
  return true;
}


MUST_USE_RESULT static MaybeHandle<Code> GetUnoptimizedCodeCommon(
    CompilationInfo* info) {
  VMState<COMPILER> state(info->isolate());
  PostponeInterruptsScope postpone(info->isolate());

  // Parse and update CompilationInfo with the results.
  if (!Parser::Parse(info)) return MaybeHandle<Code>();
  Handle<SharedFunctionInfo> shared = info->shared_info();
  FunctionLiteral* lit = info->function();
  shared->set_strict_mode(lit->strict_mode());
  SetExpectedNofPropertiesFromEstimate(shared, lit->expected_property_count());
  shared->set_bailout_reason(lit->dont_optimize_reason());

  // Compile unoptimized code.
  if (!CompileUnoptimizedCode(info)) return MaybeHandle<Code>();

  CHECK_EQ(Code::FUNCTION, info->code()->kind());
  RecordFunctionCompilation(Logger::LAZY_COMPILE_TAG, info, shared);

  // Update the shared function info with the scope info. Allocating the
  // ScopeInfo object may cause a GC.
  Handle<ScopeInfo> scope_info = ScopeInfo::Create(info->scope(), info->zone());
  shared->set_scope_info(*scope_info);

  // Update the code and feedback vector for the shared function info.
  shared->ReplaceCode(*info->code());
  if (shared->optimization_disabled()) info->code()->set_optimizable(false);
  shared->set_feedback_vector(*info->feedback_vector());

  return info->code();
}


MUST_USE_RESULT static MaybeHandle<Code> GetCodeFromOptimizedCodeMap(
    Handle<JSFunction> function, BailoutId osr_ast_id) {
  if (FLAG_cache_optimized_code) {
    Handle<SharedFunctionInfo> shared(function->shared());
    // Bound functions are not cached.
    if (shared->bound()) return MaybeHandle<Code>();
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
  return MaybeHandle<Code>();
}


static void InsertCodeIntoOptimizedCodeMap(CompilationInfo* info) {
  Handle<Code> code = info->code();
  if (code->kind() != Code::OPTIMIZED_FUNCTION) return;  // Nothing to do.

  // Context specialization folds-in the context, so no sharing can occur.
  if (code->is_turbofanned() && info->is_context_specializing()) return;

  // Cache optimized code.
  if (FLAG_cache_optimized_code) {
    Handle<JSFunction> function = info->closure();
    Handle<SharedFunctionInfo> shared(function->shared());
    // Do not cache bound functions.
    if (shared->bound()) return;
    Handle<FixedArray> literals(function->literals());
    Handle<Context> native_context(function->context()->native_context());
    SharedFunctionInfo::AddToOptimizedCodeMap(shared, native_context, code,
                                              literals, info->osr_ast_id());
  }
}


static bool Renumber(CompilationInfo* info) {
  if (!AstNumbering::Renumber(info->function(), info->zone())) return false;
  if (!info->shared_info().is_null()) {
    info->shared_info()->set_ast_node_count(info->function()->ast_node_count());
  }
  return true;
}


bool Compiler::Analyze(CompilationInfo* info) {
  DCHECK(info->function() != NULL);
  if (!Rewriter::Rewrite(info)) return false;
  if (!Scope::Analyze(info)) return false;
  if (!Renumber(info)) return false;
  DCHECK(info->scope() != NULL);
  return true;
}


bool Compiler::ParseAndAnalyze(CompilationInfo* info) {
  if (!Parser::Parse(info)) return false;
  return Compiler::Analyze(info);
}


static bool GetOptimizedCodeNow(CompilationInfo* info) {
  if (!Compiler::ParseAndAnalyze(info)) return false;

  TimerEventScope<TimerEventRecompileSynchronous> timer(info->isolate());

  OptimizedCompileJob job(info);
  if (job.CreateGraph() != OptimizedCompileJob::SUCCEEDED ||
      job.OptimizeGraph() != OptimizedCompileJob::SUCCEEDED ||
      job.GenerateCode() != OptimizedCompileJob::SUCCEEDED) {
    if (FLAG_trace_opt) {
      PrintF("[aborted optimizing ");
      info->closure()->ShortPrint();
      PrintF(" because: %s]\n", GetBailoutReason(info->bailout_reason()));
    }
    return false;
  }

  // Success!
  DCHECK(!info->isolate()->has_pending_exception());
  InsertCodeIntoOptimizedCodeMap(info);
  RecordFunctionCompilation(Logger::LAZY_COMPILE_TAG, info,
                            info->shared_info());
  if (FLAG_trace_opt) {
    PrintF("[completed optimizing ");
    info->closure()->ShortPrint();
    PrintF("]\n");
  }
  return true;
}


static bool GetOptimizedCodeLater(CompilationInfo* info) {
  Isolate* isolate = info->isolate();
  if (!isolate->optimizing_compiler_thread()->IsQueueAvailable()) {
    if (FLAG_trace_concurrent_recompilation) {
      PrintF("  ** Compilation queue full, will retry optimizing ");
      info->closure()->ShortPrint();
      PrintF(" later.\n");
    }
    return false;
  }

  CompilationHandleScope handle_scope(info);
  if (!Compiler::ParseAndAnalyze(info)) return false;
  info->SaveHandles();  // Copy handles to the compilation handle scope.

  TimerEventScope<TimerEventRecompileSynchronous> timer(info->isolate());

  OptimizedCompileJob* job = new (info->zone()) OptimizedCompileJob(info);
  OptimizedCompileJob::Status status = job->CreateGraph();
  if (status != OptimizedCompileJob::SUCCEEDED) return false;
  isolate->optimizing_compiler_thread()->QueueForOptimization(job);

  if (FLAG_trace_concurrent_recompilation) {
    PrintF("  ** Queued ");
    info->closure()->ShortPrint();
    if (info->is_osr()) {
      PrintF(" for concurrent OSR at %d.\n", info->osr_ast_id().ToInt());
    } else {
      PrintF(" for concurrent optimization.\n");
    }
  }
  return true;
}


MaybeHandle<Code> Compiler::GetUnoptimizedCode(Handle<JSFunction> function) {
  DCHECK(!function->GetIsolate()->has_pending_exception());
  DCHECK(!function->is_compiled());
  if (function->shared()->is_compiled()) {
    return Handle<Code>(function->shared()->code());
  }

  CompilationInfoWithZone info(function);
  Handle<Code> result;
  ASSIGN_RETURN_ON_EXCEPTION(info.isolate(), result,
                             GetUnoptimizedCodeCommon(&info),
                             Code);
  return result;
}


MaybeHandle<Code> Compiler::GetLazyCode(Handle<JSFunction> function) {
  Isolate* isolate = function->GetIsolate();
  DCHECK(!isolate->has_pending_exception());
  DCHECK(!function->is_compiled());
  // If the debugger is active, do not compile with turbofan unless we can
  // deopt from turbofan code.
  if (FLAG_turbo_asm && function->shared()->asm_function() &&
      (FLAG_turbo_deoptimization || !isolate->debug()->is_active())) {
    CompilationInfoWithZone info(function);

    VMState<COMPILER> state(isolate);
    PostponeInterruptsScope postpone(isolate);

    info.SetOptimizing(BailoutId::None(),
                       Handle<Code>(function->shared()->code()));

    info.MarkAsContextSpecializing();
    info.MarkAsTypingEnabled();
    info.MarkAsInliningDisabled();

    if (GetOptimizedCodeNow(&info)) {
      DCHECK(function->shared()->is_compiled());
      return info.code();
    }
  }

  if (function->shared()->is_compiled()) {
    return Handle<Code>(function->shared()->code());
  }

  CompilationInfoWithZone info(function);
  Handle<Code> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result, GetUnoptimizedCodeCommon(&info),
                             Code);

  if (FLAG_always_opt && isolate->use_crankshaft() &&
      !info.shared_info()->optimization_disabled() &&
      !isolate->DebuggerHasBreakPoints()) {
    Handle<Code> opt_code;
    if (Compiler::GetOptimizedCode(
            function, result,
            Compiler::NOT_CONCURRENT).ToHandle(&opt_code)) {
      result = opt_code;
    }
  }

  return result;
}


MaybeHandle<Code> Compiler::GetUnoptimizedCode(
    Handle<SharedFunctionInfo> shared) {
  DCHECK(!shared->GetIsolate()->has_pending_exception());
  DCHECK(!shared->is_compiled());

  CompilationInfoWithZone info(shared);
  return GetUnoptimizedCodeCommon(&info);
}


bool Compiler::EnsureCompiled(Handle<JSFunction> function,
                              ClearExceptionFlag flag) {
  if (function->is_compiled()) return true;
  MaybeHandle<Code> maybe_code = Compiler::GetLazyCode(function);
  Handle<Code> code;
  if (!maybe_code.ToHandle(&code)) {
    if (flag == CLEAR_EXCEPTION) {
      function->GetIsolate()->clear_pending_exception();
    }
    return false;
  }
  function->ReplaceCode(*code);
  DCHECK(function->is_compiled());
  return true;
}


// TODO(turbofan): In the future, unoptimized code with deopt support could
// be generated lazily once deopt is triggered.
bool Compiler::EnsureDeoptimizationSupport(CompilationInfo* info) {
  DCHECK(info->function() != NULL);
  DCHECK(info->scope() != NULL);
  if (!info->shared_info()->has_deoptimization_support()) {
    CompilationInfoWithZone unoptimized(info->shared_info());
    // Note that we use the same AST that we will use for generating the
    // optimized code.
    unoptimized.SetFunction(info->function());
    unoptimized.PrepareForCompilation(info->scope());
    unoptimized.SetContext(info->context());
    unoptimized.EnableDeoptimizationSupport();
    if (!FullCodeGenerator::MakeCode(&unoptimized)) return false;

    Handle<SharedFunctionInfo> shared = info->shared_info();
    shared->EnableDeoptimizationSupport(*unoptimized.code());
    shared->set_feedback_vector(*unoptimized.feedback_vector());

    // The scope info might not have been set if a lazily compiled
    // function is inlined before being called for the first time.
    if (shared->scope_info() == ScopeInfo::Empty(info->isolate())) {
      Handle<ScopeInfo> target_scope_info =
          ScopeInfo::Create(info->scope(), info->zone());
      shared->set_scope_info(*target_scope_info);
    }

    // The existing unoptimized code was replaced with the new one.
    RecordFunctionCompilation(Logger::LAZY_COMPILE_TAG, &unoptimized, shared);
  }
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
MaybeHandle<Code> Compiler::GetDebugCode(Handle<JSFunction> function) {
  CompilationInfoWithZone info(function);
  Isolate* isolate = info.isolate();
  VMState<COMPILER> state(isolate);

  info.MarkAsDebug();

  DCHECK(!isolate->has_pending_exception());
  Handle<Code> old_code(function->shared()->code());
  DCHECK(old_code->kind() == Code::FUNCTION);
  DCHECK(!old_code->has_debug_break_slots());

  info.MarkCompilingForDebugging();
  if (old_code->is_compiled_optimizable()) {
    info.EnableDeoptimizationSupport();
  } else {
    info.MarkNonOptimizable();
  }
  MaybeHandle<Code> maybe_new_code = GetUnoptimizedCodeCommon(&info);
  Handle<Code> new_code;
  if (!maybe_new_code.ToHandle(&new_code)) {
    isolate->clear_pending_exception();
  } else {
    DCHECK_EQ(old_code->is_compiled_optimizable(),
              new_code->is_compiled_optimizable());
  }
  return maybe_new_code;
}


void Compiler::CompileForLiveEdit(Handle<Script> script) {
  // TODO(635): support extensions.
  CompilationInfoWithZone info(script);
  PostponeInterruptsScope postpone(info.isolate());
  VMState<COMPILER> state(info.isolate());

  info.MarkAsGlobal();
  if (!Parser::Parse(&info)) return;

  LiveEditFunctionTracker tracker(info.isolate(), info.function());
  if (!CompileUnoptimizedCode(&info)) return;
  if (!info.shared_info().is_null()) {
    Handle<ScopeInfo> scope_info = ScopeInfo::Create(info.scope(),
                                                     info.zone());
    info.shared_info()->set_scope_info(*scope_info);
  }
  tracker.RecordRootFunctionInfo(info.code());
}


static Handle<SharedFunctionInfo> CompileToplevel(CompilationInfo* info) {
  Isolate* isolate = info->isolate();
  PostponeInterruptsScope postpone(isolate);
  DCHECK(!isolate->native_context().is_null());
  Handle<Script> script = info->script();

  // TODO(svenpanne) Obscure place for this, perhaps move to OnBeforeCompile?
  FixedArray* array = isolate->native_context()->embedder_data();
  script->set_context_data(array->get(0));

  isolate->debug()->OnBeforeCompile(script);

  DCHECK(info->is_eval() || info->is_global());

  info->MarkAsToplevel();

  Handle<SharedFunctionInfo> result;

  { VMState<COMPILER> state(info->isolate());
    if (info->function() == NULL) {
      // Parse the script if needed (if it's already parsed, function() is
      // non-NULL).
      bool parse_allow_lazy =
          (info->compile_options() == ScriptCompiler::kConsumeParserCache ||
           String::cast(script->source())->length() >
               FLAG_min_preparse_length) &&
          !Compiler::DebuggerWantsEagerCompilation(info);

      if (!parse_allow_lazy &&
          (info->compile_options() == ScriptCompiler::kProduceParserCache ||
           info->compile_options() == ScriptCompiler::kConsumeParserCache)) {
        // We are going to parse eagerly, but we either 1) have cached data
        // produced by lazy parsing or 2) are asked to generate cached data.
        // Eager parsing cannot benefit from cached data, and producing cached
        // data while parsing eagerly is not implemented.
        info->SetCachedData(NULL, ScriptCompiler::kNoCompileOptions);
      }
      if (!Parser::Parse(info, parse_allow_lazy)) {
        return Handle<SharedFunctionInfo>::null();
      }
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
    DCHECK(!info->code().is_null());
    result = isolate->factory()->NewSharedFunctionInfo(
        lit->name(), lit->materialized_literal_count(), lit->kind(),
        info->code(), ScopeInfo::Create(info->scope(), info->zone()),
        info->feedback_vector());

    DCHECK_EQ(RelocInfo::kNoPosition, lit->function_token_position());
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

    if (!script.is_null())
      script->set_compilation_state(Script::COMPILATION_STATE_COMPILED);

    live_edit_tracker.RecordFunctionInfo(result, lit, info->zone());
  }

  isolate->debug()->OnAfterCompile(script);

  return result;
}


MaybeHandle<JSFunction> Compiler::GetFunctionFromEval(
    Handle<String> source, Handle<SharedFunctionInfo> outer_info,
    Handle<Context> context, StrictMode strict_mode,
    ParseRestriction restriction, int scope_position) {
  Isolate* isolate = source->GetIsolate();
  int source_length = source->length();
  isolate->counters()->total_eval_size()->Increment(source_length);
  isolate->counters()->total_compile_size()->Increment(source_length);

  CompilationCache* compilation_cache = isolate->compilation_cache();
  MaybeHandle<SharedFunctionInfo> maybe_shared_info =
      compilation_cache->LookupEval(source, outer_info, context, strict_mode,
                                    scope_position);
  Handle<SharedFunctionInfo> shared_info;

  if (!maybe_shared_info.ToHandle(&shared_info)) {
    Handle<Script> script = isolate->factory()->NewScript(source);
    CompilationInfoWithZone info(script);
    info.MarkAsEval();
    if (context->IsNativeContext()) info.MarkAsGlobal();
    info.SetStrictMode(strict_mode);
    info.SetParseRestriction(restriction);
    info.SetContext(context);

    Debug::RecordEvalCaller(script);

    shared_info = CompileToplevel(&info);

    if (shared_info.is_null()) {
      return MaybeHandle<JSFunction>();
    } else {
      // Explicitly disable optimization for eval code. We're not yet prepared
      // to handle eval-code in the optimizing compiler.
      shared_info->DisableOptimization(kEval);

      // If caller is strict mode, the result must be in strict mode as well.
      DCHECK(strict_mode == SLOPPY || shared_info->strict_mode() == STRICT);
      if (!shared_info->dont_cache()) {
        compilation_cache->PutEval(source, outer_info, context, shared_info,
                                   scope_position);
      }
    }
  } else if (shared_info->ic_age() != isolate->heap()->global_ic_age()) {
    shared_info->ResetForNewContext(isolate->heap()->global_ic_age());
  }

  return isolate->factory()->NewFunctionFromSharedFunctionInfo(
      shared_info, context, NOT_TENURED);
}


Handle<SharedFunctionInfo> Compiler::CompileScript(
    Handle<String> source, Handle<Object> script_name, int line_offset,
    int column_offset, bool is_shared_cross_origin, Handle<Context> context,
    v8::Extension* extension, ScriptData** cached_data,
    ScriptCompiler::CompileOptions compile_options, NativesFlag natives) {
  if (compile_options == ScriptCompiler::kNoCompileOptions) {
    cached_data = NULL;
  } else if (compile_options == ScriptCompiler::kProduceParserCache ||
             compile_options == ScriptCompiler::kProduceCodeCache) {
    DCHECK(cached_data && !*cached_data);
    DCHECK(extension == NULL);
  } else {
    DCHECK(compile_options == ScriptCompiler::kConsumeParserCache ||
           compile_options == ScriptCompiler::kConsumeCodeCache);
    DCHECK(cached_data && *cached_data);
    DCHECK(extension == NULL);
  }
  Isolate* isolate = source->GetIsolate();
  int source_length = source->length();
  isolate->counters()->total_load_size()->Increment(source_length);
  isolate->counters()->total_compile_size()->Increment(source_length);

  CompilationCache* compilation_cache = isolate->compilation_cache();

  // Do a lookup in the compilation cache but not for extensions.
  MaybeHandle<SharedFunctionInfo> maybe_result;
  Handle<SharedFunctionInfo> result;
  if (extension == NULL) {
    if (FLAG_serialize_toplevel &&
        compile_options == ScriptCompiler::kConsumeCodeCache &&
        !isolate->debug()->is_loaded()) {
      HistogramTimerScope timer(isolate->counters()->compile_deserialize());
      Handle<SharedFunctionInfo> result;
      if (CodeSerializer::Deserialize(isolate, *cached_data, source)
              .ToHandle(&result)) {
        return result;
      }
      // Deserializer failed. Fall through to compile.
    } else {
      maybe_result = compilation_cache->LookupScript(
          source, script_name, line_offset, column_offset,
          is_shared_cross_origin, context);
    }
  }

  base::ElapsedTimer timer;
  if (FLAG_profile_deserialization && FLAG_serialize_toplevel &&
      compile_options == ScriptCompiler::kProduceCodeCache) {
    timer.Start();
  }

  if (!maybe_result.ToHandle(&result)) {
    // No cache entry found. Compile the script.

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

    // Compile the function and add it to the cache.
    CompilationInfoWithZone info(script);
    info.MarkAsGlobal();
    info.SetCachedData(cached_data, compile_options);
    info.SetExtension(extension);
    info.SetContext(context);
    if (FLAG_serialize_toplevel &&
        compile_options == ScriptCompiler::kProduceCodeCache) {
      info.PrepareForSerializing();
    }
    if (FLAG_use_strict) info.SetStrictMode(STRICT);

    result = CompileToplevel(&info);
    if (extension == NULL && !result.is_null() && !result->dont_cache()) {
      compilation_cache->PutScript(source, context, result);
      // TODO(yangguo): Issue 3628
      // With block scoping, top-level variables may resolve to a global,
      // context, which makes the code context-dependent.
      if (FLAG_serialize_toplevel && !FLAG_harmony_scoping &&
          compile_options == ScriptCompiler::kProduceCodeCache) {
        HistogramTimerScope histogram_timer(
            isolate->counters()->compile_serialize());
        *cached_data = CodeSerializer::Serialize(isolate, result, source);
        if (FLAG_profile_deserialization) {
          PrintF("[Compiling and serializing took %0.3f ms]\n",
                 timer.Elapsed().InMillisecondsF());
        }
      }
    }

    if (result.is_null()) isolate->ReportPendingMessages();
  } else if (result->ic_age() != isolate->heap()->global_ic_age()) {
    result->ResetForNewContext(isolate->heap()->global_ic_age());
  }
  return result;
}


Handle<SharedFunctionInfo> Compiler::CompileStreamedScript(
    CompilationInfo* info, int source_length) {
  Isolate* isolate = info->isolate();
  isolate->counters()->total_load_size()->Increment(source_length);
  isolate->counters()->total_compile_size()->Increment(source_length);

  if (FLAG_use_strict) info->SetStrictMode(STRICT);
  // TODO(marja): FLAG_serialize_toplevel is not honoured and won't be; when the
  // real code caching lands, streaming needs to be adapted to use it.
  return CompileToplevel(info);
}


Handle<SharedFunctionInfo> Compiler::BuildFunctionInfo(
    FunctionLiteral* literal, Handle<Script> script,
    CompilationInfo* outer_info) {
  // Precondition: code has been parsed and scopes have been analyzed.
  CompilationInfoWithZone info(script);
  info.SetFunction(literal);
  info.PrepareForCompilation(literal->scope());
  info.SetStrictMode(literal->scope()->strict_mode());
  if (outer_info->will_serialize()) info.PrepareForSerializing();

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


  if (outer_info->is_toplevel() && outer_info->will_serialize()) {
    // Make sure that if the toplevel code (possibly to be serialized),
    // the inner function must be allowed to be compiled lazily.
    DCHECK(allow_lazy);
  }

  // Generate code
  Handle<ScopeInfo> scope_info;
  if (FLAG_lazy && allow_lazy && !literal->is_parenthesized()) {
    Handle<Code> code = isolate->builtins()->CompileLazy();
    info.SetCode(code);
    scope_info = Handle<ScopeInfo>(ScopeInfo::Empty(isolate));
  } else if (Renumber(&info) && FullCodeGenerator::MakeCode(&info)) {
    DCHECK(!info.code().is_null());
    scope_info = ScopeInfo::Create(info.scope(), info.zone());
  } else {
    return Handle<SharedFunctionInfo>::null();
  }

  // Create a shared function info object.
  Handle<SharedFunctionInfo> result = factory->NewSharedFunctionInfo(
      literal->name(), literal->materialized_literal_count(), literal->kind(),
      info.code(), scope_info, info.feedback_vector());
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


MaybeHandle<Code> Compiler::GetOptimizedCode(Handle<JSFunction> function,
                                             Handle<Code> current_code,
                                             ConcurrencyMode mode,
                                             BailoutId osr_ast_id) {
  Handle<Code> cached_code;
  if (GetCodeFromOptimizedCodeMap(
          function, osr_ast_id).ToHandle(&cached_code)) {
    return cached_code;
  }

  SmartPointer<CompilationInfo> info(new CompilationInfoWithZone(function));
  Isolate* isolate = info->isolate();
  DCHECK(AllowCompilation::IsAllowed(isolate));
  VMState<COMPILER> state(isolate);
  DCHECK(!isolate->has_pending_exception());
  PostponeInterruptsScope postpone(isolate);

  Handle<SharedFunctionInfo> shared = info->shared_info();
  if (shared->code()->kind() != Code::FUNCTION ||
      ScopeInfo::Empty(isolate) == shared->scope_info()) {
    // The function was never compiled. Compile it unoptimized first.
    // TODO(titzer): reuse the AST and scope info from this compile.
    CompilationInfoWithZone nested(function);
    nested.EnableDeoptimizationSupport();
    if (!GetUnoptimizedCodeCommon(&nested).ToHandle(&current_code)) {
      return MaybeHandle<Code>();
    }
    shared->ReplaceCode(*current_code);
  }
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

  if (isolate->has_pending_exception()) isolate->clear_pending_exception();
  return MaybeHandle<Code>();
}


Handle<Code> Compiler::GetConcurrentlyOptimizedCode(OptimizedCompileJob* job) {
  // Take ownership of compilation info.  Deleting compilation info
  // also tears down the zone and the recompile job.
  SmartPointer<CompilationInfo> info(job->info());
  Isolate* isolate = info->isolate();

  VMState<COMPILER> state(isolate);
  TimerEventScope<TimerEventRecompileSynchronous> timer(info->isolate());

  Handle<SharedFunctionInfo> shared = info->shared_info();
  shared->code()->set_profiler_ticks(0);

  // 1) Optimization on the concurrent thread may have failed.
  // 2) The function may have already been optimized by OSR.  Simply continue.
  //    Except when OSR already disabled optimization for some reason.
  // 3) The code may have already been invalidated due to dependency change.
  // 4) Debugger may have been activated.
  // 5) Code generation may have failed.
  if (job->last_status() == OptimizedCompileJob::SUCCEEDED) {
    if (shared->optimization_disabled()) {
      job->RetryOptimization(kOptimizationDisabled);
    } else if (info->HasAbortedDueToDependencyChange()) {
      job->RetryOptimization(kBailedOutDueToDependencyChange);
    } else if (isolate->DebuggerHasBreakPoints()) {
      job->RetryOptimization(kDebuggerHasBreakPoints);
    } else if (job->GenerateCode() == OptimizedCompileJob::SUCCEEDED) {
      RecordFunctionCompilation(Logger::LAZY_COMPILE_TAG, info.get(), shared);
      if (info->shared_info()->SearchOptimizedCodeMap(
              info->context()->native_context(), info->osr_ast_id()) == -1) {
        InsertCodeIntoOptimizedCodeMap(info.get());
      }
      if (FLAG_trace_opt) {
        PrintF("[completed optimizing ");
        info->closure()->ShortPrint();
        PrintF("]\n");
      }
      return Handle<Code>(*info->code());
    }
  }

  DCHECK(job->last_status() != OptimizedCompileJob::SUCCEEDED);
  if (FLAG_trace_opt) {
    PrintF("[aborted optimizing ");
    info->closure()->ShortPrint();
    PrintF(" because: %s]\n", GetBailoutReason(info->bailout_reason()));
  }
  return Handle<Code>::null();
}


bool Compiler::DebuggerWantsEagerCompilation(CompilationInfo* info,
                                             bool allow_lazy_without_ctx) {
  if (LiveEditFunctionTracker::IsActive(info->isolate())) return true;
  Debug* debug = info->isolate()->debug();
  bool debugging = debug->is_active() || debug->has_break_points();
  return debugging && !allow_lazy_without_ctx;
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
      base::OS::StrChr(const_cast<char*>(FLAG_trace_phase), name_[0]) != NULL);
}

} }  // namespace v8::internal
