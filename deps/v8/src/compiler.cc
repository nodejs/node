// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler.h"

#include <algorithm>

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
#include "src/lithium.h"
#include "src/liveedit.h"
#include "src/messages.h"
#include "src/parser.h"
#include "src/prettyprinter.h"
#include "src/rewriter.h"
#include "src/runtime-profiler.h"
#include "src/scanner-character-streams.h"
#include "src/scopeinfo.h"
#include "src/scopes.h"
#include "src/snapshot/serialize.h"
#include "src/typing.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

std::ostream& operator<<(std::ostream& os, const SourcePosition& p) {
  if (p.IsUnknown()) {
    return os << "<?>";
  } else if (FLAG_hydrogen_track_positions) {
    return os << "<" << p.inlining_id() << ":" << p.position() << ">";
  } else {
    return os << "<0:" << p.raw() << ">";
  }
}


#define PARSE_INFO_GETTER(type, name)  \
  type CompilationInfo::name() const { \
    CHECK(parse_info());               \
    return parse_info()->name();       \
  }


#define PARSE_INFO_GETTER_WITH_DEFAULT(type, name, def) \
  type CompilationInfo::name() const {                  \
    return parse_info() ? parse_info()->name() : def;   \
  }


PARSE_INFO_GETTER(Handle<Script>, script)
PARSE_INFO_GETTER(bool, is_eval)
PARSE_INFO_GETTER(bool, is_native)
PARSE_INFO_GETTER(bool, is_module)
PARSE_INFO_GETTER_WITH_DEFAULT(LanguageMode, language_mode, STRICT)
PARSE_INFO_GETTER_WITH_DEFAULT(Handle<JSFunction>, closure,
                               Handle<JSFunction>::null())
PARSE_INFO_GETTER(FunctionLiteral*, function)
PARSE_INFO_GETTER_WITH_DEFAULT(Scope*, scope, nullptr)
PARSE_INFO_GETTER(Handle<Context>, context)
PARSE_INFO_GETTER(Handle<SharedFunctionInfo>, shared_info)

#undef PARSE_INFO_GETTER
#undef PARSE_INFO_GETTER_WITH_DEFAULT


// Exactly like a CompilationInfo, except being allocated via {new} and it also
// creates and enters a Zone on construction and deallocates it on destruction.
class CompilationInfoWithZone : public CompilationInfo {
 public:
  explicit CompilationInfoWithZone(Handle<JSFunction> function)
      : CompilationInfo(new ParseInfo(&zone_, function)) {}

  // Virtual destructor because a CompilationInfoWithZone has to exit the
  // zone scope and get rid of dependent maps even when the destructor is
  // called when cast as a CompilationInfo.
  virtual ~CompilationInfoWithZone() {
    DisableFutureOptimization();
    dependencies()->Rollback();
    delete parse_info_;
    parse_info_ = nullptr;
  }

 private:
  Zone zone_;
};


bool CompilationInfo::has_shared_info() const {
  return parse_info_ && !parse_info_->shared_info().is_null();
}


CompilationInfo::CompilationInfo(ParseInfo* parse_info)
    : CompilationInfo(parse_info, nullptr, BASE, parse_info->isolate(),
                      parse_info->zone()) {
  // Compiling for the snapshot typically results in different code than
  // compiling later on. This means that code recompiled with deoptimization
  // support won't be "equivalent" (as defined by SharedFunctionInfo::
  // EnableDeoptimizationSupport), so it will replace the old code and all
  // its type feedback. To avoid this, always compile functions in the snapshot
  // with deoptimization support.
  if (isolate_->serializer_enabled()) EnableDeoptimizationSupport();

  if (isolate_->debug()->is_active()) MarkAsDebug();
  if (FLAG_context_specialization) MarkAsContextSpecializing();
  if (FLAG_turbo_builtin_inlining) MarkAsBuiltinInliningEnabled();
  if (FLAG_turbo_deoptimization) MarkAsDeoptimizationEnabled();
  if (FLAG_turbo_inlining) MarkAsInliningEnabled();
  if (FLAG_turbo_source_positions) MarkAsSourcePositionsEnabled();
  if (FLAG_turbo_splitting) MarkAsSplittingEnabled();
  if (FLAG_turbo_types) MarkAsTypingEnabled();

  if (has_shared_info() && shared_info()->is_compiled()) {
    // We should initialize the CompilationInfo feedback vector from the
    // passed in shared info, rather than creating a new one.
    feedback_vector_ = Handle<TypeFeedbackVector>(
        shared_info()->feedback_vector(), parse_info->isolate());
  }
}


CompilationInfo::CompilationInfo(CodeStub* stub, Isolate* isolate, Zone* zone)
    : CompilationInfo(nullptr, stub, STUB, isolate, zone) {}


CompilationInfo::CompilationInfo(ParseInfo* parse_info, CodeStub* code_stub,
                                 Mode mode, Isolate* isolate, Zone* zone)
    : parse_info_(parse_info),
      isolate_(isolate),
      flags_(0),
      code_stub_(code_stub),
      mode_(mode),
      osr_ast_id_(BailoutId::None()),
      zone_(zone),
      deferred_handles_(nullptr),
      dependencies_(isolate, zone),
      bailout_reason_(kNoReason),
      prologue_offset_(Code::kPrologueOffsetNotSet),
      no_frame_ranges_(isolate->cpu_profiler()->is_profiling()
                           ? new List<OffsetRange>(2)
                           : nullptr),
      track_positions_(FLAG_hydrogen_track_positions ||
                       isolate->cpu_profiler()->is_profiling()),
      opt_count_(has_shared_info() ? shared_info()->opt_count() : 0),
      parameter_count_(0),
      optimization_id_(-1),
      osr_expr_stack_height_(0) {}


CompilationInfo::~CompilationInfo() {
  DisableFutureOptimization();
  delete deferred_handles_;
  delete no_frame_ranges_;
#ifdef DEBUG
  // Check that no dependent maps have been added or added dependent maps have
  // been rolled back or committed.
  DCHECK(dependencies()->IsEmpty());
#endif  // DEBUG
}


int CompilationInfo::num_parameters() const {
  return has_scope() ? scope()->num_parameters() : parameter_count_;
}


int CompilationInfo::num_heap_slots() const {
  return has_scope() ? scope()->num_heap_slots() : 0;
}


Code::Flags CompilationInfo::flags() const {
  return code_stub() != nullptr
             ? Code::ComputeFlags(
                   code_stub()->GetCodeKind(), code_stub()->GetICState(),
                   code_stub()->GetExtraICState(), code_stub()->GetStubType())
             : Code::ComputeFlags(Code::OPTIMIZED_FUNCTION);
}


// Primitive functions are unlikely to be picked up by the stack-walking
// profiler, so they trigger their own optimization when they're called
// for the SharedFunctionInfo::kCallsUntilPrimitiveOptimization-th time.
bool CompilationInfo::ShouldSelfOptimize() {
  return FLAG_crankshaft && !function()->flags()->Contains(kDontSelfOptimize) &&
         !function()->dont_optimize() &&
         function()->scope()->AllowsLazyCompilation() &&
         (!has_shared_info() || !shared_info()->optimization_disabled());
}


void CompilationInfo::EnsureFeedbackVector() {
  if (feedback_vector_.is_null() ||
      feedback_vector_->SpecDiffersFrom(function()->feedback_vector_spec())) {
    feedback_vector_ = isolate()->factory()->NewTypeFeedbackVector(
        function()->feedback_vector_spec());
  }
}


bool CompilationInfo::is_simple_parameter_list() {
  return scope()->is_simple_parameter_list();
}


bool CompilationInfo::MayUseThis() const {
  return scope()->uses_this() || scope()->inner_uses_this() ||
         scope()->calls_sloppy_eval();
}


int CompilationInfo::TraceInlinedFunction(Handle<SharedFunctionInfo> shared,
                                          SourcePosition position,
                                          int parent_id) {
  DCHECK(track_positions_);

  int inline_id = static_cast<int>(inlined_function_infos_.size());
  InlinedFunctionInfo info(parent_id, position, UnboundScript::kNoScriptId,
      shared->start_position());
  if (!shared->script()->IsUndefined()) {
    Handle<Script> script(Script::cast(shared->script()));
    info.script_id = script->id()->value();

    if (FLAG_hydrogen_track_positions && !script->source()->IsUndefined()) {
      CodeTracer::Scope tracing_scope(isolate()->GetCodeTracer());
      OFStream os(tracing_scope.file());
      os << "--- FUNCTION SOURCE (" << shared->DebugName()->ToCString().get()
         << ") id{" << optimization_id() << "," << inline_id << "} ---\n";
      {
        DisallowHeapAllocation no_allocation;
        int start = shared->start_position();
        int len = shared->end_position() - start;
        String::SubStringRange source(String::cast(script->source()), start,
                                      len);
        for (const auto& c : source) {
          os << AsReversiblyEscapedUC16(c);
        }
      }

      os << "\n--- END ---\n";
    }
  }

  inlined_function_infos_.push_back(info);

  if (FLAG_hydrogen_track_positions && inline_id != 0) {
    CodeTracer::Scope tracing_scope(isolate()->GetCodeTracer());
    OFStream os(tracing_scope.file());
    os << "INLINE (" << shared->DebugName()->ToCString().get() << ") id{"
       << optimization_id() << "," << inline_id << "} AS " << inline_id
       << " AT " << position << std::endl;
  }

  return inline_id;
}


void CompilationInfo::LogDeoptCallPosition(int pc_offset, int inlining_id) {
  if (!track_positions_ || IsStub()) return;
  DCHECK_LT(static_cast<size_t>(inlining_id), inlined_function_infos_.size());
  inlined_function_infos_.at(inlining_id).deopt_pc_offsets.push_back(pc_offset);
}


class HOptimizedGraphBuilderWithPositions: public HOptimizedGraphBuilder {
 public:
  explicit HOptimizedGraphBuilderWithPositions(CompilationInfo* info)
      : HOptimizedGraphBuilder(info) {
  }

#define DEF_VISIT(type)                                      \
  void Visit##type(type* node) override {                    \
    SourcePosition old_position = SourcePosition::Unknown(); \
    if (node->position() != RelocInfo::kNoPosition) {        \
      old_position = source_position();                      \
      SetSourcePosition(node->position());                   \
    }                                                        \
    HOptimizedGraphBuilder::Visit##type(node);               \
    if (!old_position.IsUnknown()) {                         \
      set_source_position(old_position);                     \
    }                                                        \
  }
  EXPRESSION_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

#define DEF_VISIT(type)                                      \
  void Visit##type(type* node) override {                    \
    SourcePosition old_position = SourcePosition::Unknown(); \
    if (node->position() != RelocInfo::kNoPosition) {        \
      old_position = source_position();                      \
      SetSourcePosition(node->position());                   \
    }                                                        \
    HOptimizedGraphBuilder::Visit##type(node);               \
    if (!old_position.IsUnknown()) {                         \
      set_source_position(old_position);                     \
    }                                                        \
  }
  STATEMENT_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

#define DEF_VISIT(type)                        \
  void Visit##type(type* node) override {      \
    HOptimizedGraphBuilder::Visit##type(node); \
  }
  DECLARATION_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT
};


OptimizedCompileJob::Status OptimizedCompileJob::CreateGraph() {
  DCHECK(info()->IsOptimizing());
  DCHECK(!info()->IsCompilingForDebugging());

  // Do not use Crankshaft/TurboFan if we need to be able to set break points.
  if (isolate()->debug()->has_break_points()) {
    return RetryOptimization(kDebuggerHasBreakPoints);
  }

  // Limit the number of times we try to optimize functions.
  const int kMaxOptCount =
      FLAG_deopt_every_n_times == 0 ? FLAG_max_opt_count : 1000;
  if (info()->opt_count() > kMaxOptCount) {
    return AbortOptimization(kOptimizedTooManyTimes);
  }

  // Check the whitelist for Crankshaft.
  if (!info()->closure()->PassesFilter(FLAG_hydrogen_filter)) {
    return AbortOptimization(kHydrogenFilter);
  }

  // Optimization requires a version of fullcode with deoptimization support.
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

  // Check the enabling conditions for TurboFan.
  if (((FLAG_turbo_asm && info()->shared_info()->asm_function()) ||
       info()->closure()->PassesFilter(FLAG_turbo_filter)) &&
      (FLAG_turbo_osr || !info()->is_osr())) {
    // Use TurboFan for the compilation.
    if (FLAG_trace_opt) {
      OFStream os(stdout);
      os << "[compiling method " << Brief(*info()->closure())
         << " using TurboFan";
      if (info()->is_osr()) os << " OSR";
      os << "]" << std::endl;
    }

    if (info()->shared_info()->asm_function()) {
      info()->MarkAsContextSpecializing();
    } else if (FLAG_turbo_type_feedback) {
      info()->MarkAsTypeFeedbackEnabled();
      info()->EnsureFeedbackVector();
    }

    Timer t(this, &time_taken_to_create_graph_);
    compiler::Pipeline pipeline(info());
    pipeline.GenerateCode();
    if (!info()->code().is_null()) {
      return SetLastStatus(SUCCEEDED);
    }
  }

  if (!isolate()->use_crankshaft()) {
    // Crankshaft is entirely disabled.
    return SetLastStatus(FAILED);
  }

  Scope* scope = info()->scope();
  if (LUnallocated::TooManyParameters(scope->num_parameters())) {
    // Crankshaft would require too many Lithium operands.
    return AbortOptimization(kTooManyParameters);
  }

  if (info()->is_osr() &&
      LUnallocated::TooManyParametersOrStackSlots(scope->num_parameters(),
                                                  scope->num_stack_slots())) {
    // Crankshaft would require too many Lithium operands.
    return AbortOptimization(kTooManyParametersLocals);
  }

  if (scope->HasIllegalRedeclaration()) {
    // Crankshaft cannot handle illegal redeclarations.
    return AbortOptimization(kFunctionWithIllegalRedeclaration);
  }

  if (FLAG_trace_opt) {
    OFStream os(stdout);
    os << "[compiling method " << Brief(*info()->closure())
       << " using Crankshaft";
    if (info()->is_osr()) os << " OSR";
    os << "]" << std::endl;
  }

  if (FLAG_trace_hydrogen) {
    isolate()->GetHTracer()->TraceCompilation(info());
  }

  // Type-check the function.
  AstTyper::Run(info());

  // Optimization could have been disabled by the parser. Note that this check
  // is only needed because the Hydrogen graph builder is missing some bailouts.
  if (info()->shared_info()->optimization_disabled()) {
    return AbortOptimization(
        info()->shared_info()->disable_optimization_reason());
  }

  graph_builder_ = (info()->is_tracking_positions() || FLAG_trace_ic)
                       ? new (info()->zone())
                             HOptimizedGraphBuilderWithPositions(info())
                       : new (info()->zone()) HOptimizedGraphBuilder(info());

  Timer t(this, &time_taken_to_create_graph_);
  graph_ = graph_builder_->CreateGraph();

  if (isolate()->has_pending_exception()) {
    return SetLastStatus(FAILED);
  }

  if (graph_ == NULL) return SetLastStatus(BAILED_OUT);

  if (info()->dependencies()->HasAborted()) {
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
    info()->dependencies()->Commit(info()->code());
    if (FLAG_turbo_deoptimization) {
      info()->parse_info()->context()->native_context()->AddOptimizedCode(
          *info()->code());
    }
    RecordOptimizationStats();
    return last_status();
  }

  DCHECK(!info()->dependencies()->HasAborted());
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
  } else {
    // Inobject slack tracking will reclaim redundant inobject space later,
    // so we can afford to adjust the estimate generously.
    estimate += 8;
  }

  shared->set_expected_nof_properties(estimate);
}


static void MaybeDisableOptimization(Handle<SharedFunctionInfo> shared_info,
                                     BailoutReason bailout_reason) {
  if (bailout_reason != kNoReason) {
    shared_info->DisableOptimization(bailout_reason);
  }
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
    Handle<Script> script = info->parse_info()->script();
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
}


static bool CompileUnoptimizedCode(CompilationInfo* info) {
  DCHECK(AllowCompilation::IsAllowed(info->isolate()));
  if (!Compiler::Analyze(info->parse_info()) ||
      !FullCodeGenerator::MakeCode(info)) {
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
  if (!Parser::ParseStatic(info->parse_info())) return MaybeHandle<Code>();
  Handle<SharedFunctionInfo> shared = info->shared_info();
  FunctionLiteral* lit = info->function();
  shared->set_language_mode(lit->language_mode());
  SetExpectedNofPropertiesFromEstimate(shared, lit->expected_property_count());
  MaybeDisableOptimization(shared, lit->dont_optimize_reason());

  // Compile unoptimized code.
  if (!CompileUnoptimizedCode(info)) return MaybeHandle<Code>();

  CHECK_EQ(Code::FUNCTION, info->code()->kind());
  RecordFunctionCompilation(Logger::LAZY_COMPILE_TAG, info, shared);

  // Update the shared function info with the scope info. Allocating the
  // ScopeInfo object may cause a GC.
  Handle<ScopeInfo> scope_info =
      ScopeInfo::Create(info->isolate(), info->zone(), info->scope());
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


static bool Renumber(ParseInfo* parse_info) {
  if (!AstNumbering::Renumber(parse_info->isolate(), parse_info->zone(),
                              parse_info->function())) {
    return false;
  }
  Handle<SharedFunctionInfo> shared_info = parse_info->shared_info();
  if (!shared_info.is_null()) {
    FunctionLiteral* lit = parse_info->function();
    shared_info->set_ast_node_count(lit->ast_node_count());
    MaybeDisableOptimization(shared_info, lit->dont_optimize_reason());
    shared_info->set_dont_cache(lit->flags()->Contains(kDontCache));
  }
  return true;
}


bool Compiler::Analyze(ParseInfo* info) {
  DCHECK(info->function() != NULL);
  if (!Rewriter::Rewrite(info)) return false;
  if (!Scope::Analyze(info)) return false;
  if (!Renumber(info)) return false;
  DCHECK(info->scope() != NULL);
  return true;
}


bool Compiler::ParseAndAnalyze(ParseInfo* info) {
  if (!Parser::ParseStatic(info)) return false;
  return Compiler::Analyze(info);
}


static bool GetOptimizedCodeNow(CompilationInfo* info) {
  if (!Compiler::ParseAndAnalyze(info->parse_info())) return false;

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
  return true;
}


static bool GetOptimizedCodeLater(CompilationInfo* info) {
  Isolate* isolate = info->isolate();
  if (!isolate->optimizing_compile_dispatcher()->IsQueueAvailable()) {
    if (FLAG_trace_concurrent_recompilation) {
      PrintF("  ** Compilation queue full, will retry optimizing ");
      info->closure()->ShortPrint();
      PrintF(" later.\n");
    }
    return false;
  }

  CompilationHandleScope handle_scope(info);
  if (!Compiler::ParseAndAnalyze(info->parse_info())) return false;

  // Reopen handles in the new CompilationHandleScope.
  info->ReopenHandlesInNewHandleScope();
  info->parse_info()->ReopenHandlesInNewHandleScope();

  TimerEventScope<TimerEventRecompileSynchronous> timer(info->isolate());

  OptimizedCompileJob* job = new (info->zone()) OptimizedCompileJob(info);
  OptimizedCompileJob::Status status = job->CreateGraph();
  if (status != OptimizedCompileJob::SUCCEEDED) return false;
  isolate->optimizing_compile_dispatcher()->QueueForOptimization(job);

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
  AggregatedHistogramTimerScope timer(isolate->counters()->compile_lazy());
  // If the debugger is active, do not compile with turbofan unless we can
  // deopt from turbofan code.
  if (FLAG_turbo_asm && function->shared()->asm_function() &&
      (FLAG_turbo_deoptimization || !isolate->debug()->is_active()) &&
      !FLAG_turbo_osr) {
    CompilationInfoWithZone info(function);

    VMState<COMPILER> state(isolate);
    PostponeInterruptsScope postpone(isolate);

    info.SetOptimizing(BailoutId::None(), handle(function->shared()->code()));

    if (GetOptimizedCodeNow(&info)) {
      DCHECK(function->shared()->is_compiled());
      return info.code();
    }
    // We have failed compilation. If there was an exception clear it so that
    // we can compile unoptimized code.
    if (isolate->has_pending_exception()) isolate->clear_pending_exception();
  }

  if (function->shared()->is_compiled()) {
    return Handle<Code>(function->shared()->code());
  }

  CompilationInfoWithZone info(function);
  Handle<Code> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result, GetUnoptimizedCodeCommon(&info),
                             Code);

  if (FLAG_always_opt) {
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

  Zone zone;
  ParseInfo parse_info(&zone, shared);
  CompilationInfo info(&parse_info);
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
  Handle<SharedFunctionInfo> shared = info->shared_info();
  if (!shared->has_deoptimization_support()) {
    // TODO(titzer): just reuse the ParseInfo for the unoptimized compile.
    CompilationInfoWithZone unoptimized(info->closure());
    // Note that we use the same AST that we will use for generating the
    // optimized code.
    ParseInfo* parse_info = unoptimized.parse_info();
    parse_info->set_literal(info->function());
    parse_info->set_scope(info->scope());
    parse_info->set_context(info->context());
    unoptimized.EnableDeoptimizationSupport();
    // If the current code has reloc info for serialization, also include
    // reloc info for serialization for the new code, so that deopt support
    // can be added without losing IC state.
    if (shared->code()->kind() == Code::FUNCTION &&
        shared->code()->has_reloc_info_for_serialization()) {
      unoptimized.PrepareForSerializing();
    }
    if (!FullCodeGenerator::MakeCode(&unoptimized)) return false;

    shared->EnableDeoptimizationSupport(*unoptimized.code());
    shared->set_feedback_vector(*unoptimized.feedback_vector());

    // The scope info might not have been set if a lazily compiled
    // function is inlined before being called for the first time.
    if (shared->scope_info() == ScopeInfo::Empty(info->isolate())) {
      Handle<ScopeInfo> target_scope_info =
          ScopeInfo::Create(info->isolate(), info->zone(), info->scope());
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
  Zone zone;
  ParseInfo parse_info(&zone, script);
  CompilationInfo info(&parse_info);
  PostponeInterruptsScope postpone(info.isolate());
  VMState<COMPILER> state(info.isolate());

  info.parse_info()->set_global();
  if (!Parser::ParseStatic(info.parse_info())) return;

  LiveEditFunctionTracker tracker(info.isolate(), info.function());
  if (!CompileUnoptimizedCode(&info)) return;
  if (info.has_shared_info()) {
    Handle<ScopeInfo> scope_info =
        ScopeInfo::Create(info.isolate(), info.zone(), info.scope());
    info.shared_info()->set_scope_info(*scope_info);
  }
  tracker.RecordRootFunctionInfo(info.code());
}


static Handle<SharedFunctionInfo> CompileToplevel(CompilationInfo* info) {
  Isolate* isolate = info->isolate();
  PostponeInterruptsScope postpone(isolate);
  DCHECK(!isolate->native_context().is_null());
  ParseInfo* parse_info = info->parse_info();
  Handle<Script> script = parse_info->script();

  // TODO(svenpanne) Obscure place for this, perhaps move to OnBeforeCompile?
  FixedArray* array = isolate->native_context()->embedder_data();
  script->set_context_data(array->get(v8::Context::kDebugIdIndex));

  isolate->debug()->OnBeforeCompile(script);

  DCHECK(parse_info->is_eval() || parse_info->is_global() ||
         parse_info->is_module());

  parse_info->set_toplevel();

  Handle<SharedFunctionInfo> result;

  { VMState<COMPILER> state(info->isolate());
    if (parse_info->literal() == NULL) {
      // Parse the script if needed (if it's already parsed, function() is
      // non-NULL).
      ScriptCompiler::CompileOptions options = parse_info->compile_options();
      bool parse_allow_lazy = (options == ScriptCompiler::kConsumeParserCache ||
                               String::cast(script->source())->length() >
                                   FLAG_min_preparse_length) &&
                              !Compiler::DebuggerWantsEagerCompilation(isolate);

      parse_info->set_allow_lazy_parsing(parse_allow_lazy);
      if (!parse_allow_lazy &&
          (options == ScriptCompiler::kProduceParserCache ||
           options == ScriptCompiler::kConsumeParserCache)) {
        // We are going to parse eagerly, but we either 1) have cached data
        // produced by lazy parsing or 2) are asked to generate cached data.
        // Eager parsing cannot benefit from cached data, and producing cached
        // data while parsing eagerly is not implemented.
        parse_info->set_cached_data(nullptr);
        parse_info->set_compile_options(ScriptCompiler::kNoCompileOptions);
      }
      if (!Parser::ParseStatic(parse_info)) {
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
        info->code(),
        ScopeInfo::Create(info->isolate(), info->zone(), info->scope()),
        info->feedback_vector());

    DCHECK_EQ(RelocInfo::kNoPosition, lit->function_token_position());
    SharedFunctionInfo::InitFromFunctionLiteral(result, lit);
    result->set_script(*script);
    result->set_is_toplevel(true);

    Handle<String> script_name = script->name()->IsString()
        ? Handle<String>(String::cast(script->name()))
        : isolate->factory()->empty_string();
    Logger::LogEventsAndTags log_tag = info->is_eval()
        ? Logger::EVAL_TAG
        : Logger::ToNativeByScript(Logger::SCRIPT_TAG, *script);

    PROFILE(isolate, CodeCreateEvent(
                log_tag, *info->code(), *result, info, *script_name));

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
    Handle<Context> context, LanguageMode language_mode,
    ParseRestriction restriction, int scope_position) {
  Isolate* isolate = source->GetIsolate();
  int source_length = source->length();
  isolate->counters()->total_eval_size()->Increment(source_length);
  isolate->counters()->total_compile_size()->Increment(source_length);

  CompilationCache* compilation_cache = isolate->compilation_cache();
  MaybeHandle<SharedFunctionInfo> maybe_shared_info =
      compilation_cache->LookupEval(source, outer_info, context, language_mode,
                                    scope_position);
  Handle<SharedFunctionInfo> shared_info;

  if (!maybe_shared_info.ToHandle(&shared_info)) {
    Handle<Script> script = isolate->factory()->NewScript(source);
    Zone zone;
    ParseInfo parse_info(&zone, script);
    CompilationInfo info(&parse_info);
    parse_info.set_eval();
    if (context->IsNativeContext()) parse_info.set_global();
    parse_info.set_language_mode(language_mode);
    parse_info.set_parse_restriction(restriction);
    parse_info.set_context(context);

    Debug::RecordEvalCaller(script);

    shared_info = CompileToplevel(&info);

    if (shared_info.is_null()) {
      return MaybeHandle<JSFunction>();
    } else {
      // Explicitly disable optimization for eval code. We're not yet prepared
      // to handle eval-code in the optimizing compiler.
      if (restriction != ONLY_SINGLE_FUNCTION_LITERAL) {
        shared_info->DisableOptimization(kEval);
      }

      // If caller is strict mode, the result must be in strict mode as well.
      DCHECK(is_sloppy(language_mode) ||
             is_strict(shared_info->language_mode()));
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
    int column_offset, bool is_embedder_debug_script,
    bool is_shared_cross_origin, Handle<Object> source_map_url,
    Handle<Context> context, v8::Extension* extension, ScriptData** cached_data,
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

  // TODO(rossberg): The natives do not yet obey strong mode rules
  // (for example, some macros use '==').
  bool use_strong = FLAG_use_strong && !isolate->bootstrapper()->IsActive();
  LanguageMode language_mode =
      construct_language_mode(FLAG_use_strict, use_strong);

  CompilationCache* compilation_cache = isolate->compilation_cache();

  // Do a lookup in the compilation cache but not for extensions.
  MaybeHandle<SharedFunctionInfo> maybe_result;
  Handle<SharedFunctionInfo> result;
  if (extension == NULL) {
    // First check per-isolate compilation cache.
    maybe_result = compilation_cache->LookupScript(
        source, script_name, line_offset, column_offset,
        is_embedder_debug_script, is_shared_cross_origin, context,
        language_mode);
    if (maybe_result.is_null() && FLAG_serialize_toplevel &&
        compile_options == ScriptCompiler::kConsumeCodeCache &&
        !isolate->debug()->is_loaded()) {
      // Then check cached code provided by embedder.
      HistogramTimerScope timer(isolate->counters()->compile_deserialize());
      Handle<SharedFunctionInfo> result;
      if (CodeSerializer::Deserialize(isolate, *cached_data, source)
              .ToHandle(&result)) {
        // Promote to per-isolate compilation cache.
        DCHECK(!result->dont_cache());
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
    script->set_is_embedder_debug_script(is_embedder_debug_script);
    if (!source_map_url.is_null()) {
      script->set_source_mapping_url(*source_map_url);
    }

    // Compile the function and add it to the cache.
    Zone zone;
    ParseInfo parse_info(&zone, script);
    CompilationInfo info(&parse_info);
    if (FLAG_harmony_modules && is_module) {
      parse_info.set_module();
    } else {
      parse_info.set_global();
    }
    if (compile_options != ScriptCompiler::kNoCompileOptions) {
      parse_info.set_cached_data(cached_data);
    }
    parse_info.set_compile_options(compile_options);
    parse_info.set_extension(extension);
    parse_info.set_context(context);
    if (FLAG_serialize_toplevel &&
        compile_options == ScriptCompiler::kProduceCodeCache) {
      info.PrepareForSerializing();
    }

    parse_info.set_language_mode(
        static_cast<LanguageMode>(info.language_mode() | language_mode));
    result = CompileToplevel(&info);
    if (extension == NULL && !result.is_null() && !result->dont_cache()) {
      compilation_cache->PutScript(source, context, language_mode, result);
      if (FLAG_serialize_toplevel &&
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
    Handle<Script> script, ParseInfo* parse_info, int source_length) {
  Isolate* isolate = script->GetIsolate();
  // TODO(titzer): increment the counters in caller.
  isolate->counters()->total_load_size()->Increment(source_length);
  isolate->counters()->total_compile_size()->Increment(source_length);

  LanguageMode language_mode =
      construct_language_mode(FLAG_use_strict, FLAG_use_strong);
  parse_info->set_language_mode(
      static_cast<LanguageMode>(parse_info->language_mode() | language_mode));

  CompilationInfo compile_info(parse_info);
  // TODO(marja): FLAG_serialize_toplevel is not honoured and won't be; when the
  // real code caching lands, streaming needs to be adapted to use it.
  return CompileToplevel(&compile_info);
}


Handle<SharedFunctionInfo> Compiler::BuildFunctionInfo(
    FunctionLiteral* literal, Handle<Script> script,
    CompilationInfo* outer_info) {
  // Precondition: code has been parsed and scopes have been analyzed.
  Zone zone;
  ParseInfo parse_info(&zone, script);
  CompilationInfo info(&parse_info);
  parse_info.set_literal(literal);
  parse_info.set_scope(literal->scope());
  parse_info.set_language_mode(literal->scope()->language_mode());
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
  bool allow_lazy =
      literal->AllowsLazyCompilation() &&
      !DebuggerWantsEagerCompilation(isolate, allow_lazy_without_ctx);

  if (outer_info->parse_info()->is_toplevel() && outer_info->will_serialize()) {
    // Make sure that if the toplevel code (possibly to be serialized),
    // the inner function must be allowed to be compiled lazily.
    // This is necessary to serialize toplevel code without inner functions.
    DCHECK(allow_lazy);
  }

  // Generate code
  Handle<ScopeInfo> scope_info;
  if (FLAG_lazy && allow_lazy && !literal->should_eager_compile()) {
    Handle<Code> code = isolate->builtins()->CompileLazy();
    info.SetCode(code);
    // There's no need in theory for a lazy-compiled function to have a type
    // feedback vector, but some parts of the system expect all
    // SharedFunctionInfo instances to have one.  The size of the vector depends
    // on how many feedback-needing nodes are in the tree, and when lazily
    // parsing we might not know that, if this function was never parsed before.
    // In that case the vector will be replaced the next time MakeCode is
    // called.
    info.EnsureFeedbackVector();
    scope_info = Handle<ScopeInfo>(ScopeInfo::Empty(isolate));
  } else if (Renumber(info.parse_info()) &&
             FullCodeGenerator::MakeCode(&info)) {
    // MakeCode will ensure that the feedback vector is present and
    // appropriately sized.
    DCHECK(!info.code().is_null());
    scope_info = ScopeInfo::Create(info.isolate(), info.zone(), info.scope());
    if (literal->should_eager_compile() &&
        literal->should_be_used_once_hint()) {
      info.code()->MarkToBeExecutedOnce(isolate);
    }
  } else {
    return Handle<SharedFunctionInfo>::null();
  }

  // Create a shared function info object.
  Handle<SharedFunctionInfo> result = factory->NewSharedFunctionInfo(
      literal->name(), literal->materialized_literal_count(), literal->kind(),
      info.code(), scope_info, info.feedback_vector());

  SharedFunctionInfo::InitFromFunctionLiteral(result, literal);
  result->set_script(*script);
  result->set_is_toplevel(false);

  RecordFunctionCompilation(Logger::FUNCTION_TAG, &info, result);
  result->set_allows_lazy_compilation(literal->AllowsLazyCompilation());
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

  // TODO(mstarzinger): We cannot properly deserialize a scope chain containing
  // an eval scope and hence would fail at parsing the eval source again.
  if (shared->disable_optimization_reason() == kEval) {
    return MaybeHandle<Code>();
  }

  // TODO(mstarzinger): We cannot properly deserialize a scope chain for the
  // builtin context, hence Genesis::InstallExperimentalNatives would fail.
  if (shared->is_toplevel() && isolate->bootstrapper()->IsActive()) {
    return MaybeHandle<Code>();
  }

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
    } else if (info->dependencies()->HasAborted()) {
      job->RetryOptimization(kBailedOutDueToDependencyChange);
    } else if (isolate->debug()->has_break_points()) {
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


bool Compiler::DebuggerWantsEagerCompilation(Isolate* isolate,
                                             bool allow_lazy_without_ctx) {
  if (LiveEditFunctionTracker::IsActive(isolate)) return true;
  Debug* debug = isolate->debug();
  bool debugging = debug->is_active() || debug->has_break_points();
  return debugging && !allow_lazy_without_ctx;
}


CompilationPhase::CompilationPhase(const char* name, CompilationInfo* info)
    : name_(name), info_(info) {
  if (FLAG_hydrogen_stats) {
    info_zone_start_allocation_size_ = info->zone()->allocation_size();
    timer_.Start();
  }
}


CompilationPhase::~CompilationPhase() {
  if (FLAG_hydrogen_stats) {
    size_t size = zone()->allocation_size();
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


#if DEBUG
void CompilationInfo::PrintAstForTesting() {
  PrintF("--- Source from AST ---\n%s\n",
         PrettyPrinter(isolate(), zone()).PrintProgram(function()));
}
#endif
} }  // namespace v8::internal
