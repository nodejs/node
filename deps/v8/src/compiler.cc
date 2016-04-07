// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler.h"

#include <algorithm>

#include "src/ast/ast-numbering.h"
#include "src/ast/prettyprinter.h"
#include "src/ast/scopeinfo.h"
#include "src/ast/scopes.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/compilation-cache.h"
#include "src/compiler/pipeline.h"
#include "src/crankshaft/hydrogen.h"
#include "src/crankshaft/lithium.h"
#include "src/crankshaft/typing.h"
#include "src/debug/debug.h"
#include "src/debug/liveedit.h"
#include "src/deoptimizer.h"
#include "src/full-codegen/full-codegen.h"
#include "src/gdb-jit.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate-inl.h"
#include "src/log-inl.h"
#include "src/messages.h"
#include "src/parsing/parser.h"
#include "src/parsing/rewriter.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/profiler/cpu-profiler.h"
#include "src/runtime-profiler.h"
#include "src/snapshot/serialize.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {


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
PARSE_INFO_GETTER(FunctionLiteral*, literal)
PARSE_INFO_GETTER_WITH_DEFAULT(LanguageMode, language_mode, STRICT)
PARSE_INFO_GETTER_WITH_DEFAULT(Handle<JSFunction>, closure,
                               Handle<JSFunction>::null())
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


bool CompilationInfo::has_context() const {
  return parse_info_ && !parse_info_->context().is_null();
}


bool CompilationInfo::has_literal() const {
  return parse_info_ && parse_info_->literal() != nullptr;
}


bool CompilationInfo::has_scope() const {
  return parse_info_ && parse_info_->scope() != nullptr;
}


CompilationInfo::CompilationInfo(ParseInfo* parse_info)
    : CompilationInfo(parse_info, nullptr, Code::ComputeFlags(Code::FUNCTION),
                      BASE, parse_info->isolate(), parse_info->zone()) {
  // Compiling for the snapshot typically results in different code than
  // compiling later on. This means that code recompiled with deoptimization
  // support won't be "equivalent" (as defined by SharedFunctionInfo::
  // EnableDeoptimizationSupport), so it will replace the old code and all
  // its type feedback. To avoid this, always compile functions in the snapshot
  // with deoptimization support.
  if (isolate_->serializer_enabled()) EnableDeoptimizationSupport();

  if (FLAG_function_context_specialization) MarkAsFunctionContextSpecializing();
  if (FLAG_turbo_inlining) MarkAsInliningEnabled();
  if (FLAG_turbo_source_positions) MarkAsSourcePositionsEnabled();
  if (FLAG_turbo_splitting) MarkAsSplittingEnabled();
  if (FLAG_turbo_types) MarkAsTypingEnabled();

  if (has_shared_info()) {
    if (shared_info()->is_compiled()) {
      // We should initialize the CompilationInfo feedback vector from the
      // passed in shared info, rather than creating a new one.
      feedback_vector_ = Handle<TypeFeedbackVector>(
          shared_info()->feedback_vector(), parse_info->isolate());
    }
    if (shared_info()->never_compiled()) MarkAsFirstCompile();
  }
}


CompilationInfo::CompilationInfo(const char* debug_name, Isolate* isolate,
                                 Zone* zone, Code::Flags code_flags)
    : CompilationInfo(nullptr, debug_name, code_flags, STUB, isolate, zone) {}

CompilationInfo::CompilationInfo(ParseInfo* parse_info, const char* debug_name,
                                 Code::Flags code_flags, Mode mode,
                                 Isolate* isolate, Zone* zone)
    : parse_info_(parse_info),
      isolate_(isolate),
      flags_(0),
      code_flags_(code_flags),
      mode_(mode),
      osr_ast_id_(BailoutId::None()),
      zone_(zone),
      deferred_handles_(nullptr),
      dependencies_(isolate, zone),
      bailout_reason_(kNoReason),
      prologue_offset_(Code::kPrologueOffsetNotSet),
      track_positions_(FLAG_hydrogen_track_positions ||
                       isolate->cpu_profiler()->is_profiling()),
      opt_count_(has_shared_info() ? shared_info()->opt_count() : 0),
      parameter_count_(0),
      optimization_id_(-1),
      osr_expr_stack_height_(0),
      debug_name_(debug_name) {}


CompilationInfo::~CompilationInfo() {
  DisableFutureOptimization();
  delete deferred_handles_;
#ifdef DEBUG
  // Check that no dependent maps have been added or added dependent maps have
  // been rolled back or committed.
  DCHECK(dependencies()->IsEmpty());
#endif  // DEBUG
}


int CompilationInfo::num_parameters() const {
  return has_scope() ? scope()->num_parameters() : parameter_count_;
}


int CompilationInfo::num_parameters_including_this() const {
  return num_parameters() + (is_this_defined() ? 1 : 0);
}


bool CompilationInfo::is_this_defined() const { return !IsStub(); }


int CompilationInfo::num_heap_slots() const {
  return has_scope() ? scope()->num_heap_slots() : 0;
}


// Primitive functions are unlikely to be picked up by the stack-walking
// profiler, so they trigger their own optimization when they're called
// for the SharedFunctionInfo::kCallsUntilPrimitiveOptimization-th time.
bool CompilationInfo::ShouldSelfOptimize() {
  return FLAG_crankshaft &&
         !(literal()->flags() & AstProperties::kDontSelfOptimize) &&
         !literal()->dont_optimize() &&
         literal()->scope()->AllowsLazyCompilation() &&
         (!has_shared_info() || !shared_info()->optimization_disabled());
}


void CompilationInfo::EnsureFeedbackVector() {
  if (feedback_vector_.is_null()) {
    Handle<TypeFeedbackMetadata> feedback_metadata =
        TypeFeedbackMetadata::New(isolate(), literal()->feedback_vector_spec());
    feedback_vector_ = TypeFeedbackVector::New(isolate(), feedback_metadata);
  }

  // It's very important that recompiles do not alter the structure of the
  // type feedback vector.
  CHECK(!feedback_vector_->metadata()->SpecDiffersFrom(
      literal()->feedback_vector_spec()));
}


bool CompilationInfo::has_simple_parameters() {
  return scope()->has_simple_parameters();
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
    info.script_id = script->id();

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


base::SmartArrayPointer<char> CompilationInfo::GetDebugName() const {
  if (parse_info() && parse_info()->literal()) {
    AllowHandleDereference allow_deref;
    return parse_info()->literal()->debug_name()->ToCString();
  }
  if (parse_info() && !parse_info()->shared_info().is_null()) {
    return parse_info()->shared_info()->DebugName()->ToCString();
  }
  const char* str = debug_name_ ? debug_name_ : "unknown";
  size_t len = strlen(str) + 1;
  base::SmartArrayPointer<char> name(new char[len]);
  memcpy(name.get(), str, len);
  return name;
}


bool CompilationInfo::ExpectsJSReceiverAsReceiver() {
  return is_sloppy(language_mode()) && !is_native();
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

  // Do not use Crankshaft/TurboFan if we need to be able to set break points.
  if (info()->shared_info()->HasDebugInfo()) {
    return AbortOptimization(kFunctionBeingDebugged);
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
  DCHECK(!info()->is_first_compile());

  bool optimization_disabled = info()->shared_info()->optimization_disabled();
  bool dont_crankshaft = info()->shared_info()->dont_crankshaft();

  // Check the enabling conditions for Turbofan.
  // 1. "use asm" code.
  bool is_turbofanable_asm = FLAG_turbo_asm &&
                             info()->shared_info()->asm_function() &&
                             !optimization_disabled;

  // 2. Fallback for features unsupported by Crankshaft.
  bool is_unsupported_by_crankshaft_but_turbofanable =
      dont_crankshaft && strcmp(FLAG_turbo_filter, "~~") == 0 &&
      !optimization_disabled;

  // 3. Explicitly enabled by the command-line filter.
  bool passes_turbo_filter = info()->closure()->PassesFilter(FLAG_turbo_filter);

  // If this is OSR request, OSR must be enabled by Turbofan.
  bool passes_osr_test = FLAG_turbo_osr || !info()->is_osr();

  if ((is_turbofanable_asm || is_unsupported_by_crankshaft_but_turbofanable ||
       passes_turbo_filter) &&
      passes_osr_test) {
    // Use TurboFan for the compilation.
    if (FLAG_trace_opt) {
      OFStream os(stdout);
      os << "[compiling method " << Brief(*info()->closure())
         << " using TurboFan";
      if (info()->is_osr()) os << " OSR";
      os << "]" << std::endl;
    }

    if (info()->shared_info()->asm_function()) {
      if (info()->osr_frame()) info()->MarkAsFrameSpecializing();
      info()->MarkAsFunctionContextSpecializing();
    } else {
      if (!FLAG_always_opt) {
        info()->MarkAsBailoutOnUninitialized();
      }
      if (FLAG_native_context_specialization) {
        info()->MarkAsNativeContextSpecializing();
        info()->MarkAsTypingEnabled();
      }
    }
    if (!info()->shared_info()->asm_function() ||
        FLAG_turbo_asm_deoptimization) {
      info()->MarkAsDeoptimizationEnabled();
    }

    Timer t(this, &time_taken_to_create_graph_);
    compiler::Pipeline pipeline(info());
    pipeline.GenerateCode();
    if (!info()->code().is_null()) {
      return SetLastStatus(SUCCEEDED);
    }
  }

  if (!isolate()->use_crankshaft() || dont_crankshaft) {
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
  AstTyper(info()->isolate(), info()->zone(), info()->closure(),
           info()->scope(), info()->osr_ast_id(), info()->literal())
      .Run();

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


namespace {

void AddWeakObjectToCodeDependency(Isolate* isolate, Handle<HeapObject> object,
                                   Handle<Code> code) {
  Handle<WeakCell> cell = Code::WeakCellFor(code);
  Heap* heap = isolate->heap();
  Handle<DependentCode> dep(heap->LookupWeakObjectToCodeDependency(object));
  dep = DependentCode::InsertWeakCode(dep, DependentCode::kWeakCodeGroup, cell);
  heap->AddWeakObjectToCodeDependency(object, dep);
}


void RegisterWeakObjectsInOptimizedCode(Handle<Code> code) {
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

}  // namespace


OptimizedCompileJob::Status OptimizedCompileJob::GenerateCode() {
  DCHECK(last_status() == SUCCEEDED);
  // TODO(turbofan): Currently everything is done in the first phase.
  if (!info()->code().is_null()) {
    info()->dependencies()->Commit(info()->code());
    if (info()->is_deoptimization_enabled()) {
      info()->parse_info()->context()->native_context()->AddOptimizedCode(
          *info()->code());
      RegisterWeakObjectsInOptimizedCode(info()->code());
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
    RegisterWeakObjectsInOptimizedCode(optimized_code);
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


static bool UseIgnition(CompilationInfo* info) {
  // Cannot use Ignition when the {function_data} is already used.
  if (info->has_shared_info() && info->shared_info()->HasBuiltinFunctionId()) {
    return false;
  }

  // Checks whether top level functions should be passed by the filter.
  if (info->closure().is_null()) {
    Vector<const char> filter = CStrVector(FLAG_ignition_filter);
    return (filter.length() == 0) || (filter.length() == 1 && filter[0] == '*');
  }

  // Finally respect the filter.
  return info->closure()->PassesFilter(FLAG_ignition_filter);
}

static int CodeAndMetadataSize(CompilationInfo* info) {
  int size = 0;
  if (info->has_bytecode_array()) {
    Handle<BytecodeArray> bytecode_array = info->bytecode_array();
    size += bytecode_array->BytecodeArraySize();
    size += bytecode_array->constant_pool()->Size();
    size += bytecode_array->handler_table()->Size();
    size += bytecode_array->source_position_table()->Size();
  } else {
    Handle<Code> code = info->code();
    size += code->CodeSize();
    size += code->relocation_info()->Size();
    size += code->deoptimization_data()->Size();
    size += code->handler_table()->Size();
  }
  return size;
}


static bool GenerateBaselineCode(CompilationInfo* info) {
  bool success;
  if (FLAG_ignition && UseIgnition(info)) {
    success = interpreter::Interpreter::MakeBytecode(info);
  } else {
    success = FullCodeGenerator::MakeCode(info);
  }
  if (success) {
    Isolate* isolate = info->isolate();
    Counters* counters = isolate->counters();
    counters->total_baseline_code_size()->Increment(CodeAndMetadataSize(info));
    counters->total_baseline_compile_count()->Increment(1);
  }
  return success;
}


static bool CompileBaselineCode(CompilationInfo* info) {
  DCHECK(AllowCompilation::IsAllowed(info->isolate()));
  if (!Compiler::Analyze(info->parse_info()) || !GenerateBaselineCode(info)) {
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
  FunctionLiteral* lit = info->literal();
  DCHECK_EQ(shared->language_mode(), lit->language_mode());
  SetExpectedNofPropertiesFromEstimate(shared, lit->expected_property_count());
  MaybeDisableOptimization(shared, lit->dont_optimize_reason());

  // Compile either unoptimized code or bytecode for the interpreter.
  if (!CompileBaselineCode(info)) return MaybeHandle<Code>();
  if (info->code()->kind() == Code::FUNCTION) {  // Only for full code.
    RecordFunctionCompilation(Logger::LAZY_COMPILE_TAG, info, shared);
  }

  // Update the shared function info with the scope info. Allocating the
  // ScopeInfo object may cause a GC.
  Handle<ScopeInfo> scope_info =
      ScopeInfo::Create(info->isolate(), info->zone(), info->scope());
  shared->set_scope_info(*scope_info);

  // Update the code and feedback vector for the shared function info.
  shared->ReplaceCode(*info->code());
  shared->set_feedback_vector(*info->feedback_vector());
  if (info->has_bytecode_array()) {
    DCHECK(shared->function_data()->IsUndefined());
    shared->set_function_data(*info->bytecode_array());
  }

  return info->code();
}


MUST_USE_RESULT static MaybeHandle<Code> GetCodeFromOptimizedCodeMap(
    Handle<JSFunction> function, BailoutId osr_ast_id) {
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


static void InsertCodeIntoOptimizedCodeMap(CompilationInfo* info) {
  Handle<Code> code = info->code();
  if (code->kind() != Code::OPTIMIZED_FUNCTION) return;  // Nothing to do.

  // Function context specialization folds-in the function context,
  // so no sharing can occur.
  if (info->is_function_context_specializing()) return;
  // Frame specialization implies function context specialization.
  DCHECK(!info->is_frame_specializing());

  // Cache optimized context-specific code.
  Handle<JSFunction> function = info->closure();
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<LiteralsArray> literals(function->literals());
  Handle<Context> native_context(function->context()->native_context());
  SharedFunctionInfo::AddToOptimizedCodeMap(shared, native_context, code,
                                            literals, info->osr_ast_id());

  // Do not cache (native) context-independent code compiled for OSR.
  if (code->is_turbofanned() && info->is_osr()) return;

  // Cache optimized (native) context-independent code.
  if (FLAG_turbo_cache_shared_code && code->is_turbofanned() &&
      !info->is_native_context_specializing()) {
    DCHECK(!info->is_function_context_specializing());
    DCHECK(info->osr_ast_id().IsNone());
    Handle<SharedFunctionInfo> shared(function->shared());
    SharedFunctionInfo::AddSharedCodeToOptimizedCodeMap(shared, code);
  }
}


static bool Renumber(ParseInfo* parse_info) {
  if (!AstNumbering::Renumber(parse_info->isolate(), parse_info->zone(),
                              parse_info->literal())) {
    return false;
  }
  Handle<SharedFunctionInfo> shared_info = parse_info->shared_info();
  if (!shared_info.is_null()) {
    FunctionLiteral* lit = parse_info->literal();
    shared_info->set_ast_node_count(lit->ast_node_count());
    MaybeDisableOptimization(shared_info, lit->dont_optimize_reason());
    shared_info->set_dont_crankshaft(lit->flags() &
                                     AstProperties::kDontCrankshaft);
  }
  return true;
}


bool Compiler::Analyze(ParseInfo* info) {
  DCHECK_NOT_NULL(info->literal());
  if (!Rewriter::Rewrite(info)) return false;
  if (!Scope::Analyze(info)) return false;
  if (!Renumber(info)) return false;
  DCHECK_NOT_NULL(info->scope());
  return true;
}


bool Compiler::ParseAndAnalyze(ParseInfo* info) {
  if (!Parser::ParseStatic(info)) return false;
  return Compiler::Analyze(info);
}


static bool GetOptimizedCodeNow(CompilationInfo* info) {
  Isolate* isolate = info->isolate();
  CanonicalHandleScope canonical(isolate);
  TimerEventScope<TimerEventOptimizeCode> optimize_code_timer(isolate);
  TRACE_EVENT0("v8", "V8.OptimizeCode");

  if (!Compiler::ParseAndAnalyze(info->parse_info())) return false;

  TimerEventScope<TimerEventRecompileSynchronous> timer(isolate);
  TRACE_EVENT0("v8", "V8.RecompileSynchronous");

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
  DCHECK(!isolate->has_pending_exception());
  InsertCodeIntoOptimizedCodeMap(info);
  RecordFunctionCompilation(Logger::LAZY_COMPILE_TAG, info,
                            info->shared_info());
  return true;
}


static bool GetOptimizedCodeLater(CompilationInfo* info) {
  Isolate* isolate = info->isolate();
  CanonicalHandleScope canonical(isolate);
  TimerEventScope<TimerEventOptimizeCode> optimize_code_timer(isolate);
  TRACE_EVENT0("v8", "V8.OptimizeCode");

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
  TRACE_EVENT0("v8", "V8.RecompileSynchronous");

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
  TimerEventScope<TimerEventCompileCode> compile_timer(isolate);
  TRACE_EVENT0("v8", "V8.CompileCode");
  AggregatedHistogramTimerScope timer(isolate->counters()->compile_lazy());
  // If the debugger is active, do not compile with turbofan unless we can
  // deopt from turbofan code.
  if (FLAG_turbo_asm && function->shared()->asm_function() &&
      (FLAG_turbo_asm_deoptimization || !isolate->debug()->is_active()) &&
      !FLAG_turbo_osr) {
    CompilationInfoWithZone info(function);

    VMState<COMPILER> state(isolate);
    PostponeInterruptsScope postpone(isolate);

    info.SetOptimizing();

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
    if (Compiler::GetOptimizedCode(function, Compiler::NOT_CONCURRENT)
            .ToHandle(&opt_code)) {
      result = opt_code;
    }
  }

  return result;
}


bool Compiler::Compile(Handle<JSFunction> function, ClearExceptionFlag flag) {
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
  DCHECK_NOT_NULL(info->literal());
  DCHECK(info->has_scope());
  Handle<SharedFunctionInfo> shared = info->shared_info();
  if (!shared->has_deoptimization_support()) {
    // TODO(titzer): just reuse the ParseInfo for the unoptimized compile.
    CompilationInfoWithZone unoptimized(info->closure());
    // Note that we use the same AST that we will use for generating the
    // optimized code.
    ParseInfo* parse_info = unoptimized.parse_info();
    parse_info->set_literal(info->literal());
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

    info->MarkAsCompiled();

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


bool CompileEvalForDebugging(Handle<JSFunction> function,
                             Handle<SharedFunctionInfo> shared) {
  Handle<Script> script(Script::cast(shared->script()));
  Handle<Context> context(function->context());

  Zone zone;
  ParseInfo parse_info(&zone, script);
  CompilationInfo info(&parse_info);
  Isolate* isolate = info.isolate();

  parse_info.set_eval();
  parse_info.set_context(context);
  if (context->IsNativeContext()) parse_info.set_global();
  parse_info.set_toplevel();
  parse_info.set_allow_lazy_parsing(false);
  parse_info.set_language_mode(shared->language_mode());
  parse_info.set_parse_restriction(NO_PARSE_RESTRICTION);
  info.MarkAsDebug();

  VMState<COMPILER> state(info.isolate());

  if (!Parser::ParseStatic(&parse_info)) {
    isolate->clear_pending_exception();
    return false;
  }

  FunctionLiteral* lit = parse_info.literal();
  LiveEditFunctionTracker live_edit_tracker(isolate, lit);

  if (!CompileUnoptimizedCode(&info)) {
    isolate->clear_pending_exception();
    return false;
  }
  shared->ReplaceCode(*info.code());
  return true;
}


bool CompileForDebugging(CompilationInfo* info) {
  info->MarkAsDebug();
  if (GetUnoptimizedCodeCommon(info).is_null()) {
    info->isolate()->clear_pending_exception();
    return false;
  }
  return true;
}


static inline bool IsEvalToplevel(Handle<SharedFunctionInfo> shared) {
  return shared->is_toplevel() && shared->script()->IsScript() &&
         Script::cast(shared->script())->compilation_type() ==
             Script::COMPILATION_TYPE_EVAL;
}


bool Compiler::CompileDebugCode(Handle<JSFunction> function) {
  Handle<SharedFunctionInfo> shared(function->shared());
  if (IsEvalToplevel(shared)) {
    return CompileEvalForDebugging(function, shared);
  } else {
    CompilationInfoWithZone info(function);
    return CompileForDebugging(&info);
  }
}


bool Compiler::CompileDebugCode(Handle<SharedFunctionInfo> shared) {
  DCHECK(shared->allows_lazy_compilation_without_context());
  DCHECK(!IsEvalToplevel(shared));
  Zone zone;
  ParseInfo parse_info(&zone, shared);
  CompilationInfo info(&parse_info);
  return CompileForDebugging(&info);
}


void Compiler::CompileForLiveEdit(Handle<Script> script) {
  // TODO(635): support extensions.
  Zone zone;
  ParseInfo parse_info(&zone, script);
  CompilationInfo info(&parse_info);
  PostponeInterruptsScope postpone(info.isolate());
  VMState<COMPILER> state(info.isolate());

  // Get rid of old list of shared function infos.
  info.MarkAsFirstCompile();
  info.MarkAsDebug();
  info.parse_info()->set_global();
  if (!Parser::ParseStatic(info.parse_info())) return;

  LiveEditFunctionTracker tracker(info.isolate(), parse_info.literal());
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
  TimerEventScope<TimerEventCompileCode> timer(isolate);
  TRACE_EVENT0("v8", "V8.CompileCode");
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
      // Parse the script if needed (if it's already parsed, literal() is
      // non-NULL). If compiling for debugging, we may eagerly compile inner
      // functions, so do not parse lazily in that case.
      ScriptCompiler::CompileOptions options = parse_info->compile_options();
      bool parse_allow_lazy = (options == ScriptCompiler::kConsumeParserCache ||
                               String::cast(script->source())->length() >
                                   FLAG_min_preparse_length) &&
                              !info->is_debug();

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

    DCHECK(!info->is_debug() || !parse_info->allow_lazy_parsing());

    info->MarkAsFirstCompile();

    FunctionLiteral* lit = parse_info->literal();
    LiveEditFunctionTracker live_edit_tracker(isolate, lit);

    // Measure how long it takes to do the compilation; only take the
    // rest of the function into account to avoid overlap with the
    // parsing statistics.
    HistogramTimer* rate = info->is_eval()
          ? info->isolate()->counters()->compile_eval()
          : info->isolate()->counters()->compile();
    HistogramTimerScope timer(rate);
    TRACE_EVENT0("v8", info->is_eval() ? "V8.CompileEval" : "V8.Compile");

    // Compile the code.
    if (!CompileBaselineCode(info)) {
      return Handle<SharedFunctionInfo>::null();
    }

    // Allocate function.
    DCHECK(!info->code().is_null());
    result = isolate->factory()->NewSharedFunctionInfo(
        lit->name(), lit->materialized_literal_count(), lit->kind(),
        info->code(),
        ScopeInfo::Create(info->isolate(), info->zone(), info->scope()),
        info->feedback_vector());
    if (info->has_bytecode_array()) {
      DCHECK(result->function_data()->IsUndefined());
      result->set_function_data(*info->bytecode_array());
    }

    DCHECK_EQ(RelocInfo::kNoPosition, lit->function_token_position());
    SharedFunctionInfo::InitFromFunctionLiteral(result, lit);
    SharedFunctionInfo::SetScript(result, script);
    result->set_is_toplevel(true);
    if (info->is_eval()) {
      // Eval scripts cannot be (re-)compiled without context.
      result->set_allows_lazy_compilation_without_context(false);
    }

    Handle<String> script_name =
        script->name()->IsString()
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

  return result;
}


MaybeHandle<JSFunction> Compiler::GetFunctionFromEval(
    Handle<String> source, Handle<SharedFunctionInfo> outer_info,
    Handle<Context> context, LanguageMode language_mode,
    ParseRestriction restriction, int line_offset, int column_offset,
    Handle<Object> script_name, ScriptOriginOptions options) {
  Isolate* isolate = source->GetIsolate();
  int source_length = source->length();
  isolate->counters()->total_eval_size()->Increment(source_length);
  isolate->counters()->total_compile_size()->Increment(source_length);

  CompilationCache* compilation_cache = isolate->compilation_cache();
  MaybeHandle<SharedFunctionInfo> maybe_shared_info =
      compilation_cache->LookupEval(source, outer_info, context, language_mode,
                                    line_offset);
  Handle<SharedFunctionInfo> shared_info;

  Handle<Script> script;
  if (!maybe_shared_info.ToHandle(&shared_info)) {
    script = isolate->factory()->NewScript(source);
    if (!script_name.is_null()) {
      script->set_name(*script_name);
      script->set_line_offset(line_offset);
      script->set_column_offset(column_offset);
    }
    script->set_origin_options(options);
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
      compilation_cache->PutEval(source, outer_info, context, shared_info,
                                 line_offset);
    }
  } else if (shared_info->ic_age() != isolate->heap()->global_ic_age()) {
    shared_info->ResetForNewContext(isolate->heap()->global_ic_age());
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


Handle<SharedFunctionInfo> Compiler::CompileScript(
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
        source, script_name, line_offset, column_offset, resource_options,
        context, language_mode);
    if (maybe_result.is_null() && FLAG_serialize_toplevel &&
        compile_options == ScriptCompiler::kConsumeCodeCache &&
        !isolate->debug()->is_loaded()) {
      // Then check cached code provided by embedder.
      HistogramTimerScope timer(isolate->counters()->compile_deserialize());
      TRACE_EVENT0("v8", "V8.CompileDeserialize");
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

  if (!maybe_result.ToHandle(&result)) {
    // No cache entry found. Compile the script.

    // Create a script object describing the script to be compiled.
    Handle<Script> script = isolate->factory()->NewScript(source);
    if (natives == NATIVES_CODE) {
      script->set_type(Script::TYPE_NATIVE);
      script->set_hide_source(true);
    } else if (natives == EXTENSION_CODE) {
      script->set_type(Script::TYPE_EXTENSION);
      script->set_hide_source(true);
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
    if (extension == NULL && !result.is_null()) {
      compilation_cache->PutScript(source, context, language_mode, result);
      if (FLAG_serialize_toplevel &&
          compile_options == ScriptCompiler::kProduceCodeCache) {
        HistogramTimerScope histogram_timer(
            isolate->counters()->compile_serialize());
        TRACE_EVENT0("v8", "V8.CompileSerialize");
        *cached_data = CodeSerializer::Serialize(isolate, result, source);
        if (FLAG_profile_deserialization) {
          PrintF("[Compiling and serializing took %0.3f ms]\n",
                 timer.Elapsed().InMillisecondsF());
        }
      }
    }

    if (result.is_null()) {
      isolate->ReportPendingMessages();
    } else {
      isolate->debug()->OnAfterCompile(script);
    }
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
  if (outer_info->is_first_compile()) {
    // On the first compile, there are no existing shared function info for
    // inner functions yet, so do not try to find them. All bets are off for
    // live edit though.
    DCHECK(script->FindSharedFunctionInfo(literal).is_null() ||
           isolate->debug()->live_edit_enabled());
  } else {
    maybe_existing = script->FindSharedFunctionInfo(literal);
  }
  // We found an existing shared function info. If it's already compiled,
  // don't worry about compiling it, and simply return it. If it's not yet
  // compiled, continue to decide whether to eagerly compile.
  // Carry on if we are compiling eager to obtain code for debugging,
  // unless we already have code with debut break slots.
  Handle<SharedFunctionInfo> existing;
  if (maybe_existing.ToHandle(&existing) && existing->is_compiled()) {
    if (!outer_info->is_debug() || existing->HasDebugCode()) {
      return existing;
    }
  }

  Zone zone;
  ParseInfo parse_info(&zone, script);
  CompilationInfo info(&parse_info);
  parse_info.set_literal(literal);
  parse_info.set_scope(literal->scope());
  parse_info.set_language_mode(literal->scope()->language_mode());
  if (outer_info->will_serialize()) info.PrepareForSerializing();
  if (outer_info->is_first_compile()) info.MarkAsFirstCompile();
  if (outer_info->is_debug()) info.MarkAsDebug();

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
  // Compile eagerly for live edit. When compiling debug code, eagerly compile
  // unless we can lazily compile without the context.
  bool allow_lazy = literal->AllowsLazyCompilation() &&
                    !LiveEditFunctionTracker::IsActive(isolate) &&
                    (!info.is_debug() || allow_lazy_without_ctx);

  bool lazy = FLAG_lazy && allow_lazy && !literal->should_eager_compile();

  // Generate code
  TimerEventScope<TimerEventCompileCode> timer(isolate);
  TRACE_EVENT0("v8", "V8.CompileCode");
  Handle<ScopeInfo> scope_info;
  if (lazy) {
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
  } else if (Renumber(info.parse_info()) && GenerateBaselineCode(&info)) {
    // Code generation will ensure that the feedback vector is present and
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

  if (maybe_existing.is_null()) {
    // Create a shared function info object.
    Handle<SharedFunctionInfo> result =
        isolate->factory()->NewSharedFunctionInfo(
            literal->name(), literal->materialized_literal_count(),
            literal->kind(), info.code(), scope_info, info.feedback_vector());
    if (info.has_bytecode_array()) {
      DCHECK(result->function_data()->IsUndefined());
      result->set_function_data(*info.bytecode_array());
    }

    SharedFunctionInfo::InitFromFunctionLiteral(result, literal);
    SharedFunctionInfo::SetScript(result, script);
    result->set_is_toplevel(false);
    // If the outer function has been compiled before, we cannot be sure that
    // shared function info for this function literal has been created for the
    // first time. It may have already been compiled previously.
    result->set_never_compiled(outer_info->is_first_compile() && lazy);

    RecordFunctionCompilation(Logger::FUNCTION_TAG, &info, result);
    result->set_allows_lazy_compilation(literal->AllowsLazyCompilation());
    result->set_allows_lazy_compilation_without_context(allow_lazy_without_ctx);

    // Set the expected number of properties for instances and return
    // the resulting function.
    SetExpectedNofPropertiesFromEstimate(result,
                                         literal->expected_property_count());
    live_edit_tracker.RecordFunctionInfo(result, literal, info.zone());
    return result;
  } else if (!lazy) {
    // Assert that we are not overwriting (possibly patched) debug code.
    DCHECK(!existing->HasDebugCode());
    existing->ReplaceCode(*info.code());
    existing->set_scope_info(*scope_info);
    existing->set_feedback_vector(*info.feedback_vector());
  }
  return existing;
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
  const int literals = fun->NumberOfLiterals();
  Handle<Code> code = Handle<Code>(fun->shared()->code());
  Handle<Code> construct_stub = Handle<Code>(fun->shared()->construct_stub());
  Handle<SharedFunctionInfo> shared = isolate->factory()->NewSharedFunctionInfo(
      name, literals, FunctionKind::kNormalFunction, code,
      Handle<ScopeInfo>(fun->shared()->scope_info()),
      Handle<TypeFeedbackVector>(fun->shared()->feedback_vector()));
  shared->set_construct_stub(*construct_stub);

  // Copy the function data to the shared function info.
  shared->set_function_data(fun->shared()->function_data());
  int parameters = fun->shared()->internal_formal_parameter_count();
  shared->set_internal_formal_parameter_count(parameters);

  return shared;
}

MaybeHandle<Code> Compiler::GetOptimizedCode(Handle<JSFunction> function,
                                             ConcurrencyMode mode,
                                             BailoutId osr_ast_id,
                                             JavaScriptFrame* osr_frame) {
  Isolate* isolate = function->GetIsolate();
  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  if (shared->HasDebugInfo()) return MaybeHandle<Code>();

  Handle<Code> cached_code;
  if (GetCodeFromOptimizedCodeMap(
          function, osr_ast_id).ToHandle(&cached_code)) {
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

  DCHECK(AllowCompilation::IsAllowed(isolate));

  Handle<Code> current_code(shared->code());
  if (!shared->is_compiled() ||
      shared->scope_info() == ScopeInfo::Empty(isolate)) {
    // The function was never compiled. Compile it unoptimized first.
    // TODO(titzer): reuse the AST and scope info from this compile.
    CompilationInfoWithZone unoptimized(function);
    unoptimized.EnableDeoptimizationSupport();
    if (!GetUnoptimizedCodeCommon(&unoptimized).ToHandle(&current_code)) {
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

  base::SmartPointer<CompilationInfo> info(
      new CompilationInfoWithZone(function));
  VMState<COMPILER> state(isolate);
  DCHECK(!isolate->has_pending_exception());
  PostponeInterruptsScope postpone(isolate);

  info->SetOptimizingForOsr(osr_ast_id, current_code);

  if (mode == CONCURRENT) {
    if (GetOptimizedCodeLater(info.get())) {
      info.Detach();  // The background recompile job owns this now.
      return isolate->builtins()->InOptimizationQueue();
    }
  } else {
    info->set_osr_frame(osr_frame);
    if (GetOptimizedCodeNow(info.get())) return info->code();
  }

  if (isolate->has_pending_exception()) isolate->clear_pending_exception();
  return MaybeHandle<Code>();
}

MaybeHandle<Code> Compiler::GetConcurrentlyOptimizedCode(
    OptimizedCompileJob* job) {
  // Take ownership of compilation info.  Deleting compilation info
  // also tears down the zone and the recompile job.
  base::SmartPointer<CompilationInfo> info(job->info());
  Isolate* isolate = info->isolate();

  VMState<COMPILER> state(isolate);
  TimerEventScope<TimerEventRecompileSynchronous> timer(info->isolate());
  TRACE_EVENT0("v8", "V8.RecompileSynchronous");

  Handle<SharedFunctionInfo> shared = info->shared_info();
  shared->code()->set_profiler_ticks(0);

  DCHECK(!shared->HasDebugInfo());

  // 1) Optimization on the concurrent thread may have failed.
  // 2) The function may have already been optimized by OSR.  Simply continue.
  //    Except when OSR already disabled optimization for some reason.
  // 3) The code may have already been invalidated due to dependency change.
  // 4) Code generation may have failed.
  if (job->last_status() == OptimizedCompileJob::SUCCEEDED) {
    if (shared->optimization_disabled()) {
      job->RetryOptimization(kOptimizationDisabled);
    } else if (info->dependencies()->HasAborted()) {
      job->RetryOptimization(kBailedOutDueToDependencyChange);
    } else if (job->GenerateCode() == OptimizedCompileJob::SUCCEEDED) {
      RecordFunctionCompilation(Logger::LAZY_COMPILE_TAG, info.get(), shared);
      if (shared->SearchOptimizedCodeMap(info->context()->native_context(),
                                         info->osr_ast_id()).code == nullptr) {
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
  return MaybeHandle<Code>();
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
         PrettyPrinter(isolate()).PrintProgram(literal()));
}
#endif
}  // namespace internal
}  // namespace v8
