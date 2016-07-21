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
#include "src/debug/debug.h"
#include "src/debug/liveedit.h"
#include "src/deoptimizer.h"
#include "src/frames-inl.h"
#include "src/full-codegen/full-codegen.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate-inl.h"
#include "src/log-inl.h"
#include "src/messages.h"
#include "src/parsing/parser.h"
#include "src/parsing/rewriter.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/profiler/cpu-profiler.h"
#include "src/runtime-profiler.h"
#include "src/snapshot/code-serializer.h"
#include "src/typing-asm.h"
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
PARSE_INFO_GETTER(FunctionLiteral*, literal)
PARSE_INFO_GETTER_WITH_DEFAULT(Scope*, scope, nullptr)
PARSE_INFO_GETTER_WITH_DEFAULT(Handle<Context>, context,
                               Handle<Context>::null())
PARSE_INFO_GETTER(Handle<SharedFunctionInfo>, shared_info)

#undef PARSE_INFO_GETTER
#undef PARSE_INFO_GETTER_WITH_DEFAULT

// A wrapper around a CompilationInfo that detaches the Handles from
// the underlying DeferredHandleScope and stores them in info_ on
// destruction.
class CompilationHandleScope BASE_EMBEDDED {
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
// Implementation of CompilationInfo

bool CompilationInfo::has_shared_info() const {
  return parse_info_ && !parse_info_->shared_info().is_null();
}

CompilationInfo::CompilationInfo(ParseInfo* parse_info,
                                 Handle<JSFunction> closure)
    : CompilationInfo(parse_info, {}, Code::ComputeFlags(Code::FUNCTION), BASE,
                      parse_info->isolate(), parse_info->zone()) {
  closure_ = closure;

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
}

CompilationInfo::CompilationInfo(Vector<const char> debug_name,
                                 Isolate* isolate, Zone* zone,
                                 Code::Flags code_flags)
    : CompilationInfo(nullptr, debug_name, code_flags, STUB, isolate, zone) {}

CompilationInfo::CompilationInfo(ParseInfo* parse_info,
                                 Vector<const char> debug_name,
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
      parameter_count_(0),
      optimization_id_(-1),
      osr_expr_stack_height_(0),
      debug_name_(debug_name) {}

CompilationInfo::~CompilationInfo() {
  DisableFutureOptimization();
  dependencies()->Rollback();
  delete deferred_handles_;
}


int CompilationInfo::num_parameters() const {
  return !IsStub() ? scope()->num_parameters() : parameter_count_;
}


int CompilationInfo::num_parameters_including_this() const {
  return num_parameters() + (is_this_defined() ? 1 : 0);
}


bool CompilationInfo::is_this_defined() const { return !IsStub(); }


// Primitive functions are unlikely to be picked up by the stack-walking
// profiler, so they trigger their own optimization when they're called
// for the SharedFunctionInfo::kCallsUntilPrimitiveOptimization-th time.
bool CompilationInfo::ShouldSelfOptimize() {
  return FLAG_crankshaft &&
         !(literal()->flags() & AstProperties::kDontSelfOptimize) &&
         !literal()->dont_optimize() &&
         literal()->scope()->AllowsLazyCompilation() &&
         !shared_info()->optimization_disabled();
}


bool CompilationInfo::has_simple_parameters() {
  return scope()->has_simple_parameters();
}


base::SmartArrayPointer<char> CompilationInfo::GetDebugName() const {
  if (parse_info() && parse_info()->literal()) {
    AllowHandleDereference allow_deref;
    return parse_info()->literal()->debug_name()->ToCString();
  }
  if (parse_info() && !parse_info()->shared_info().is_null()) {
    return parse_info()->shared_info()->DebugName()->ToCString();
  }
  Vector<const char> name_vec = debug_name_;
  if (name_vec.is_empty()) name_vec = ArrayVector("unknown");
  base::SmartArrayPointer<char> name(new char[name_vec.length() + 1]);
  memcpy(name.get(), name_vec.start(), name_vec.length());
  name[name_vec.length()] = '\0';
  return name;
}

StackFrame::Type CompilationInfo::GetOutputStackFrameType() const {
  switch (output_code_kind()) {
    case Code::STUB:
    case Code::BYTECODE_HANDLER:
    case Code::HANDLER:
    case Code::BUILTIN:
      return StackFrame::STUB;
    case Code::WASM_FUNCTION:
      return StackFrame::WASM;
    case Code::JS_TO_WASM_FUNCTION:
      return StackFrame::JS_TO_WASM;
    case Code::WASM_TO_JS_FUNCTION:
      return StackFrame::WASM_TO_JS;
    default:
      UNIMPLEMENTED();
      return StackFrame::NONE;
  }
}

int CompilationInfo::GetDeclareGlobalsFlags() const {
  DCHECK(DeclareGlobalsLanguageMode::is_valid(parse_info()->language_mode()));
  return DeclareGlobalsEvalFlag::encode(parse_info()->is_eval()) |
         DeclareGlobalsNativeFlag::encode(parse_info()->is_native()) |
         DeclareGlobalsLanguageMode::encode(parse_info()->language_mode());
}

bool CompilationInfo::ExpectsJSReceiverAsReceiver() {
  return is_sloppy(parse_info()->language_mode()) && !parse_info()->is_native();
}

#if DEBUG
void CompilationInfo::PrintAstForTesting() {
  PrintF("--- Source from AST ---\n%s\n",
         PrettyPrinter(isolate()).PrintProgram(literal()));
}
#endif

// ----------------------------------------------------------------------------
// Implementation of CompilationJob

CompilationJob::Status CompilationJob::CreateGraph() {
  DisallowJavascriptExecution no_js(isolate());
  DCHECK(info()->IsOptimizing());

  if (FLAG_trace_opt) {
    OFStream os(stdout);
    os << "[compiling method " << Brief(*info()->closure()) << " using "
       << compiler_name_;
    if (info()->is_osr()) os << " OSR";
    os << "]" << std::endl;
  }

  // Delegate to the underlying implementation.
  DCHECK_EQ(SUCCEEDED, last_status());
  ScopedTimer t(&time_taken_to_create_graph_);
  return SetLastStatus(CreateGraphImpl());
}

CompilationJob::Status CompilationJob::OptimizeGraph() {
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;
  DisallowCodeDependencyChange no_dependency_change;

  // Delegate to the underlying implementation.
  DCHECK_EQ(SUCCEEDED, last_status());
  ScopedTimer t(&time_taken_to_optimize_);
  return SetLastStatus(OptimizeGraphImpl());
}

CompilationJob::Status CompilationJob::GenerateCode() {
  DisallowCodeDependencyChange no_dependency_change;
  DisallowJavascriptExecution no_js(isolate());
  DCHECK(!info()->dependencies()->HasAborted());

  // Delegate to the underlying implementation.
  DCHECK_EQ(SUCCEEDED, last_status());
  ScopedTimer t(&time_taken_to_codegen_);
  return SetLastStatus(GenerateCodeImpl());
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

void CompilationJob::RecordOptimizationStats() {
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

// ----------------------------------------------------------------------------
// Local helper methods that make up the compilation pipeline.

namespace {

bool IsEvalToplevel(Handle<SharedFunctionInfo> shared) {
  return shared->is_toplevel() && shared->script()->IsScript() &&
         Script::cast(shared->script())->compilation_type() ==
             Script::COMPILATION_TYPE_EVAL;
}

void RecordFunctionCompilation(Logger::LogEventsAndTags tag,
                               CompilationInfo* info) {
  // Log the code generation. If source information is available include
  // script name and line number. Check explicitly whether logging is
  // enabled as finding the line number is not free.
  if (info->isolate()->logger()->is_logging_code_events() ||
      info->isolate()->cpu_profiler()->is_profiling()) {
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
    Logger::LogEventsAndTags log_tag = Logger::ToNativeByScript(tag, *script);
    PROFILE(info->isolate(),
            CodeCreateEvent(log_tag, *abstract_code, *shared, script_name,
                            line_num, column_num));
  }
}

void EnsureFeedbackVector(CompilationInfo* info) {
  DCHECK(info->has_shared_info());

  // If no type feedback vector exists, we create one now. At this point the
  // AstNumbering pass has already run. Note the snapshot can contain outdated
  // vectors for a different configuration, hence we also recreate a new vector
  // when the function is not compiled (i.e. no code was serialized).
  if (info->shared_info()->feedback_vector()->is_empty() ||
      !info->shared_info()->is_compiled()) {
    Handle<TypeFeedbackMetadata> feedback_metadata = TypeFeedbackMetadata::New(
        info->isolate(), info->literal()->feedback_vector_spec());
    Handle<TypeFeedbackVector> feedback_vector =
        TypeFeedbackVector::New(info->isolate(), feedback_metadata);
    info->shared_info()->set_feedback_vector(*feedback_vector);
  }

  // It's very important that recompiles do not alter the structure of the type
  // feedback vector. Verify that the structure fits the function literal.
  CHECK(!info->shared_info()->feedback_vector()->metadata()->SpecDiffersFrom(
      info->literal()->feedback_vector_spec()));
}

bool UseIgnition(CompilationInfo* info) {
  if (info->is_debug()) return false;
  if (info->shared_info()->is_resumable() && !FLAG_ignition_generators) {
    return false;
  }

  // Checks whether top level functions should be passed by the filter.
  if (info->shared_info()->is_toplevel()) {
    Vector<const char> filter = CStrVector(FLAG_ignition_filter);
    return (filter.length() == 0) || (filter.length() == 1 && filter[0] == '*');
  }

  // Finally respect the filter.
  return info->shared_info()->PassesFilter(FLAG_ignition_filter);
}

int CodeAndMetadataSize(CompilationInfo* info) {
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

bool GenerateUnoptimizedCode(CompilationInfo* info) {
  bool success;
  EnsureFeedbackVector(info);
  if (FLAG_validate_asm && info->scope()->asm_module()) {
    AsmTyper typer(info->isolate(), info->zone(), *(info->script()),
                   info->literal());
    if (FLAG_enable_simd_asmjs) {
      typer.set_allow_simd(true);
    }
    if (!typer.Validate()) {
      DCHECK(!info->isolate()->has_pending_exception());
      PrintF("Validation of asm.js module failed: %s", typer.error_message());
    }
  }
  if (FLAG_ignition && UseIgnition(info)) {
    success = interpreter::Interpreter::MakeBytecode(info);
  } else {
    success = FullCodeGenerator::MakeCode(info);
  }
  if (success) {
    Isolate* isolate = info->isolate();
    Counters* counters = isolate->counters();
    // TODO(4280): Rename counters from "baseline" to "unoptimized" eventually.
    counters->total_baseline_code_size()->Increment(CodeAndMetadataSize(info));
    counters->total_baseline_compile_count()->Increment(1);
  }
  return success;
}

bool CompileUnoptimizedCode(CompilationInfo* info) {
  DCHECK(AllowCompilation::IsAllowed(info->isolate()));
  if (!Compiler::Analyze(info->parse_info()) ||
      !GenerateUnoptimizedCode(info)) {
    Isolate* isolate = info->isolate();
    if (!isolate->has_pending_exception()) isolate->StackOverflow();
    return false;
  }
  return true;
}

void InstallSharedScopeInfo(CompilationInfo* info,
                            Handle<SharedFunctionInfo> shared) {
  Handle<ScopeInfo> scope_info =
      ScopeInfo::Create(info->isolate(), info->zone(), info->scope());
  shared->set_scope_info(*scope_info);
}

void InstallSharedCompilationResult(CompilationInfo* info,
                                    Handle<SharedFunctionInfo> shared) {
  // Assert that we are not overwriting (possibly patched) debug code.
  DCHECK(!shared->HasDebugInfo());
  DCHECK(!info->code().is_null());
  shared->ReplaceCode(*info->code());
  if (info->has_bytecode_array()) {
    DCHECK(!shared->HasBytecodeArray());  // Only compiled once.
    shared->set_bytecode_array(*info->bytecode_array());
  }
}

MUST_USE_RESULT MaybeHandle<Code> GetUnoptimizedCode(CompilationInfo* info) {
  VMState<COMPILER> state(info->isolate());
  PostponeInterruptsScope postpone(info->isolate());

  // Parse and update CompilationInfo with the results.
  if (!Parser::ParseStatic(info->parse_info())) return MaybeHandle<Code>();
  Handle<SharedFunctionInfo> shared = info->shared_info();
  DCHECK_EQ(shared->language_mode(), info->literal()->language_mode());

  // Compile either unoptimized code or bytecode for the interpreter.
  if (!CompileUnoptimizedCode(info)) return MaybeHandle<Code>();

  // Update the shared function info with the scope info.
  InstallSharedScopeInfo(info, shared);

  // Install compilation result on the shared function info
  InstallSharedCompilationResult(info, shared);

  // Record the function compilation event.
  RecordFunctionCompilation(Logger::LAZY_COMPILE_TAG, info);

  return info->code();
}

MUST_USE_RESULT MaybeHandle<Code> GetCodeFromOptimizedCodeMap(
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

void InsertCodeIntoOptimizedCodeMap(CompilationInfo* info) {
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

bool Renumber(ParseInfo* parse_info) {
  if (!AstNumbering::Renumber(parse_info->isolate(), parse_info->zone(),
                              parse_info->literal())) {
    return false;
  }
  Handle<SharedFunctionInfo> shared_info = parse_info->shared_info();
  if (!shared_info.is_null()) {
    FunctionLiteral* lit = parse_info->literal();
    shared_info->set_ast_node_count(lit->ast_node_count());
    if (lit->dont_optimize_reason() != kNoReason) {
      shared_info->DisableOptimization(lit->dont_optimize_reason());
    }
    shared_info->set_dont_crankshaft(
        shared_info->dont_crankshaft() ||
        (lit->flags() & AstProperties::kDontCrankshaft));
  }
  return true;
}

bool UseTurboFan(Handle<SharedFunctionInfo> shared) {
  bool optimization_disabled = shared->optimization_disabled();
  bool dont_crankshaft = shared->dont_crankshaft();

  // Check the enabling conditions for Turbofan.
  // 1. "use asm" code.
  bool is_turbofanable_asm =
      FLAG_turbo_asm && shared->asm_function() && !optimization_disabled;

  // 2. Fallback for features unsupported by Crankshaft.
  bool is_unsupported_by_crankshaft_but_turbofanable =
      dont_crankshaft && strcmp(FLAG_turbo_filter, "~~") == 0 &&
      !optimization_disabled;

  // 3. Explicitly enabled by the command-line filter.
  bool passes_turbo_filter = shared->PassesFilter(FLAG_turbo_filter);

  return is_turbofanable_asm || is_unsupported_by_crankshaft_but_turbofanable ||
         passes_turbo_filter;
}

bool GetOptimizedCodeNow(CompilationJob* job) {
  CompilationInfo* info = job->info();
  Isolate* isolate = info->isolate();

  // Parsing is not required when optimizing from existing bytecode.
  if (!info->is_optimizing_from_bytecode()) {
    if (!Compiler::ParseAndAnalyze(info->parse_info())) return false;
  }

  TimerEventScope<TimerEventRecompileSynchronous> timer(isolate);
  TRACE_EVENT0("v8", "V8.RecompileSynchronous");

  if (job->CreateGraph() != CompilationJob::SUCCEEDED ||
      job->OptimizeGraph() != CompilationJob::SUCCEEDED ||
      job->GenerateCode() != CompilationJob::SUCCEEDED) {
    if (FLAG_trace_opt) {
      PrintF("[aborted optimizing ");
      info->closure()->ShortPrint();
      PrintF(" because: %s]\n", GetBailoutReason(info->bailout_reason()));
    }
    return false;
  }

  // Success!
  job->RecordOptimizationStats();
  DCHECK(!isolate->has_pending_exception());
  InsertCodeIntoOptimizedCodeMap(info);
  RecordFunctionCompilation(Logger::LAZY_COMPILE_TAG, info);
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

  // All handles below this point will be allocated in a deferred handle scope
  // that is detached and handed off to the background thread when we return.
  CompilationHandleScope handle_scope(info);

  // Parsing is not required when optimizing from existing bytecode.
  if (!info->is_optimizing_from_bytecode()) {
    if (!Compiler::ParseAndAnalyze(info->parse_info())) return false;
  }

  // Reopen handles in the new CompilationHandleScope.
  info->ReopenHandlesInNewHandleScope();
  info->parse_info()->ReopenHandlesInNewHandleScope();

  TimerEventScope<TimerEventRecompileSynchronous> timer(info->isolate());
  TRACE_EVENT0("v8", "V8.RecompileSynchronous");

  if (job->CreateGraph() != CompilationJob::SUCCEEDED) return false;
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

  Handle<Code> cached_code;
  if (GetCodeFromOptimizedCodeMap(function, osr_ast_id)
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
  if (shared->is_compiled()) {
    shared->code()->set_profiler_ticks(0);
  }

  VMState<COMPILER> state(isolate);
  DCHECK(!isolate->has_pending_exception());
  PostponeInterruptsScope postpone(isolate);
  bool use_turbofan = UseTurboFan(shared);
  base::SmartPointer<CompilationJob> job(
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
    info->AbortOptimization(kOptimizedTooManyTimes);
    return MaybeHandle<Code>();
  }

  CanonicalHandleScope canonical(isolate);
  TimerEventScope<TimerEventOptimizeCode> optimize_code_timer(isolate);
  TRACE_EVENT0("v8", "V8.OptimizeCode");

  // TurboFan can optimize directly from existing bytecode.
  if (FLAG_turbo_from_bytecode && use_turbofan &&
      info->shared_info()->HasBytecodeArray()) {
    info->MarkAsOptimizeFromBytecode();
  }

  if (IsEvalToplevel(shared)) {
    parse_info->set_eval();
    if (function->context()->IsNativeContext()) parse_info->set_global();
    parse_info->set_toplevel();
    parse_info->set_allow_lazy_parsing(false);
    parse_info->set_lazy(false);
  }

  if (mode == Compiler::CONCURRENT) {
    if (GetOptimizedCodeLater(job.get())) {
      job.Detach();   // The background recompile job owns this now.
      return isolate->builtins()->InOptimizationQueue();
    }
  } else {
    if (GetOptimizedCodeNow(job.get())) return info->code();
  }

  if (isolate->has_pending_exception()) isolate->clear_pending_exception();
  return MaybeHandle<Code>();
}

class InterpreterActivationsFinder : public ThreadVisitor,
                                     public OptimizedFunctionVisitor {
 public:
  SharedFunctionInfo* shared_;
  bool has_activations_;

  explicit InterpreterActivationsFinder(SharedFunctionInfo* shared)
      : shared_(shared), has_activations_(false) {}

  void VisitThread(Isolate* isolate, ThreadLocalTop* top) {
    JavaScriptFrameIterator it(isolate, top);
    for (; !it.done() && !has_activations_; it.Advance()) {
      JavaScriptFrame* frame = it.frame();
      if (!frame->is_interpreted()) continue;
      if (frame->function()->shared() == shared_) has_activations_ = true;
    }
  }

  void VisitFunction(JSFunction* function) {
    if (function->Inlines(shared_)) has_activations_ = true;
  }

  void EnterContext(Context* context) {}
  void LeaveContext(Context* context) {}
};

bool HasInterpreterActivations(Isolate* isolate, SharedFunctionInfo* shared) {
  InterpreterActivationsFinder activations_finder(shared);
  activations_finder.VisitThread(isolate, isolate->thread_local_top());
  isolate->thread_manager()->IterateArchivedThreads(&activations_finder);
  if (FLAG_turbo_from_bytecode) {
    // If we are able to optimize functions directly from bytecode, then there
    // might be optimized functions that rely on bytecode being around. We need
    // to prevent switching the given function to baseline code in those cases.
    Deoptimizer::VisitAllOptimizedFunctions(isolate, &activations_finder);
  }
  return activations_finder.has_activations_;
}

MaybeHandle<Code> GetBaselineCode(Handle<JSFunction> function) {
  Isolate* isolate = function->GetIsolate();
  VMState<COMPILER> state(isolate);
  PostponeInterruptsScope postpone(isolate);
  Zone zone(isolate->allocator());
  ParseInfo parse_info(&zone, function);
  CompilationInfo info(&parse_info, function);

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

  // TODO(4280): For now we do not switch generators to baseline code because
  // there might be suspended activations stored in generator objects on the
  // heap. We could eventually go directly to TurboFan in this case.
  if (function->shared()->is_generator()) {
    return MaybeHandle<Code>();
  }

  // TODO(4280): For now we disable switching to baseline code in the presence
  // of interpreter activations of the given function. The reasons are:
  //  1) The debugger assumes each function is either full-code or bytecode.
  //  2) The underlying bytecode is cleared below, breaking stack unwinding.
  if (HasInterpreterActivations(isolate, function->shared())) {
    if (FLAG_trace_opt) {
      OFStream os(stdout);
      os << "[unable to switch " << Brief(*function) << " due to activations]"
         << std::endl;
    }
    return MaybeHandle<Code>();
  }

  if (FLAG_trace_opt) {
    OFStream os(stdout);
    os << "[switching method " << Brief(*function) << " to baseline code]"
       << std::endl;
  }

  // Parse and update CompilationInfo with the results.
  if (!Parser::ParseStatic(info.parse_info())) return MaybeHandle<Code>();
  Handle<SharedFunctionInfo> shared = info.shared_info();
  DCHECK_EQ(shared->language_mode(), info.literal()->language_mode());

  // Compile baseline code using the full code generator.
  if (!Compiler::Analyze(info.parse_info()) ||
      !FullCodeGenerator::MakeCode(&info)) {
    if (!isolate->has_pending_exception()) isolate->StackOverflow();
    return MaybeHandle<Code>();
  }

  // TODO(4280): For now we play it safe and remove the bytecode array when we
  // switch to baseline code. We might consider keeping around the bytecode so
  // that it can be used as the "source of truth" eventually.
  shared->ClearBytecodeArray();

  // Update the shared function info with the scope info.
  InstallSharedScopeInfo(&info, shared);

  // Install compilation result on the shared function info
  InstallSharedCompilationResult(&info, shared);

  // Record the function compilation event.
  RecordFunctionCompilation(Logger::LAZY_COMPILE_TAG, &info);

  return info.code();
}

MaybeHandle<Code> GetLazyCode(Handle<JSFunction> function) {
  Isolate* isolate = function->GetIsolate();
  DCHECK(!isolate->has_pending_exception());
  DCHECK(!function->is_compiled());
  TimerEventScope<TimerEventCompileCode> compile_timer(isolate);
  TRACE_EVENT0("v8", "V8.CompileCode");
  AggregatedHistogramTimerScope timer(isolate->counters()->compile_lazy());

  if (FLAG_turbo_cache_shared_code) {
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
  }

  if (function->shared()->is_compiled()) {
    return Handle<Code>(function->shared()->code());
  }

  Zone zone(isolate->allocator());
  ParseInfo parse_info(&zone, function);
  CompilationInfo info(&parse_info, function);
  Handle<Code> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result, GetUnoptimizedCode(&info), Code);

  if (FLAG_always_opt) {
    Handle<Code> opt_code;
    if (GetOptimizedCode(function, Compiler::NOT_CONCURRENT)
            .ToHandle(&opt_code)) {
      result = opt_code;
    }
  }

  return result;
}


Handle<SharedFunctionInfo> NewSharedFunctionInfoForLiteral(
    Isolate* isolate, FunctionLiteral* literal, Handle<Script> script) {
  Handle<Code> code = isolate->builtins()->CompileLazy();
  Handle<ScopeInfo> scope_info = handle(ScopeInfo::Empty(isolate));
  Handle<SharedFunctionInfo> result = isolate->factory()->NewSharedFunctionInfo(
      literal->name(), literal->materialized_literal_count(), literal->kind(),
      code, scope_info);
  SharedFunctionInfo::InitFromFunctionLiteral(result, literal);
  SharedFunctionInfo::SetScript(result, script);
  return result;
}

Handle<SharedFunctionInfo> CompileToplevel(CompilationInfo* info) {
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

      // Consider parsing eagerly when targeting the code cache.
      parse_allow_lazy &= !(FLAG_serialize_eager && info->will_serialize());

      // Consider parsing eagerly when targeting Ignition.
      parse_allow_lazy &= !(FLAG_ignition && FLAG_ignition_eager &&
                            !isolate->serializer_enabled());

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

    FunctionLiteral* lit = parse_info->literal();

    // Measure how long it takes to do the compilation; only take the
    // rest of the function into account to avoid overlap with the
    // parsing statistics.
    RuntimeCallTimerScope runtimeTimer(
        isolate, parse_info->is_eval() ? &RuntimeCallStats::CompileEval
                                       : &RuntimeCallStats::Compile);
    HistogramTimer* rate = parse_info->is_eval()
                               ? info->isolate()->counters()->compile_eval()
                               : info->isolate()->counters()->compile();
    HistogramTimerScope timer(rate);
    TRACE_EVENT0("v8", parse_info->is_eval() ? "V8.CompileEval" : "V8.Compile");

    // Allocate a shared function info object.
    DCHECK_EQ(RelocInfo::kNoPosition, lit->function_token_position());
    result = NewSharedFunctionInfoForLiteral(isolate, lit, script);
    result->set_is_toplevel(true);
    if (parse_info->is_eval()) {
      // Eval scripts cannot be (re-)compiled without context.
      result->set_allows_lazy_compilation_without_context(false);
    }
    parse_info->set_shared_info(result);

    // Compile the code.
    if (!CompileUnoptimizedCode(info)) {
      return Handle<SharedFunctionInfo>::null();
    }

    // Update the shared function info with the scope info.
    InstallSharedScopeInfo(info, result);

    // Install compilation result on the shared function info
    InstallSharedCompilationResult(info, result);

    Handle<String> script_name =
        script->name()->IsString()
            ? Handle<String>(String::cast(script->name()))
            : isolate->factory()->empty_string();
    Logger::LogEventsAndTags log_tag =
        parse_info->is_eval()
            ? Logger::EVAL_TAG
            : Logger::ToNativeByScript(Logger::SCRIPT_TAG, *script);

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
    } else {
      Zone zone(isolate->allocator());
      ParseInfo parse_info(&zone, function);
      CompilationInfo info(&parse_info, function);
      if (!GetUnoptimizedCode(&info).ToHandle(&code)) {
        return false;
      }
    }
  }

  // Install code on closure.
  function->ReplaceCode(*code);

  // Check postconditions on success.
  DCHECK(!isolate->has_pending_exception());
  DCHECK(function->shared()->is_compiled());
  DCHECK(function->is_compiled());
  return true;
}

bool Compiler::CompileDebugCode(Handle<JSFunction> function) {
  Isolate* isolate = function->GetIsolate();
  DCHECK(AllowCompilation::IsAllowed(isolate));

  // Start a compilation.
  Zone zone(isolate->allocator());
  ParseInfo parse_info(&zone, function);
  CompilationInfo info(&parse_info, Handle<JSFunction>::null());
  if (IsEvalToplevel(handle(function->shared()))) {
    parse_info.set_eval();
    if (function->context()->IsNativeContext()) parse_info.set_global();
    parse_info.set_toplevel();
    parse_info.set_allow_lazy_parsing(false);
    parse_info.set_lazy(false);
  }
  info.MarkAsDebug();
  if (GetUnoptimizedCode(&info).is_null()) {
    isolate->clear_pending_exception();
    return false;
  }

  // Check postconditions on success.
  DCHECK(!isolate->has_pending_exception());
  DCHECK(function->shared()->is_compiled());
  DCHECK(function->shared()->HasDebugCode());
  return true;
}

bool Compiler::CompileDebugCode(Handle<SharedFunctionInfo> shared) {
  Isolate* isolate = shared->GetIsolate();
  DCHECK(AllowCompilation::IsAllowed(isolate));

  // Start a compilation.
  Zone zone(isolate->allocator());
  ParseInfo parse_info(&zone, shared);
  CompilationInfo info(&parse_info, Handle<JSFunction>::null());
  DCHECK(shared->allows_lazy_compilation_without_context());
  DCHECK(!IsEvalToplevel(shared));
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
  Handle<Object> old_function_infos(script->shared_function_infos(), isolate);
  script->set_shared_function_infos(Smi::FromInt(0));

  // Start a compilation.
  Zone zone(isolate->allocator());
  ParseInfo parse_info(&zone, script);
  CompilationInfo info(&parse_info, Handle<JSFunction>::null());
  parse_info.set_global();
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

// TODO(turbofan): In the future, unoptimized code with deopt support could
// be generated lazily once deopt is triggered.
bool Compiler::EnsureDeoptimizationSupport(CompilationInfo* info) {
  DCHECK_NOT_NULL(info->literal());
  DCHECK_NOT_NULL(info->scope());
  Handle<SharedFunctionInfo> shared = info->shared_info();
  if (!shared->has_deoptimization_support()) {
    Zone zone(info->isolate()->allocator());
    CompilationInfo unoptimized(info->parse_info(), info->closure());
    unoptimized.EnableDeoptimizationSupport();

    // TODO(4280): For now we do not switch generators to baseline code because
    // there might be suspended activations stored in generator objects on the
    // heap. We could eventually go directly to TurboFan in this case.
    if (shared->is_generator()) return false;

    // TODO(4280): For now we disable switching to baseline code in the presence
    // of interpreter activations of the given function. The reasons are:
    //  1) The debugger assumes each function is either full-code or bytecode.
    //  2) The underlying bytecode is cleared below, breaking stack unwinding.
    // The expensive check for activations only needs to be done when the given
    // function has bytecode, otherwise we can be sure there are no activations.
    if (shared->HasBytecodeArray() &&
        HasInterpreterActivations(info->isolate(), *shared)) {
      return false;
    }

    // If the current code has reloc info for serialization, also include
    // reloc info for serialization for the new code, so that deopt support
    // can be added without losing IC state.
    if (shared->code()->kind() == Code::FUNCTION &&
        shared->code()->has_reloc_info_for_serialization()) {
      unoptimized.PrepareForSerializing();
    }
    EnsureFeedbackVector(&unoptimized);
    if (!FullCodeGenerator::MakeCode(&unoptimized)) return false;

    // TODO(4280): For now we play it safe and remove the bytecode array when we
    // switch to baseline code. We might consider keeping around the bytecode so
    // that it can be used as the "source of truth" eventually.
    shared->ClearBytecodeArray();

    // The scope info might not have been set if a lazily compiled
    // function is inlined before being called for the first time.
    if (shared->scope_info() == ScopeInfo::Empty(info->isolate())) {
      InstallSharedScopeInfo(info, shared);
    }

    // Install compilation result on the shared function info
    shared->EnableDeoptimizationSupport(*unoptimized.code());

    // The existing unoptimized code was replaced with the new one.
    RecordFunctionCompilation(Logger::LAZY_COMPILE_TAG, &unoptimized);
  }
  return true;
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
    if (!script_name.is_null()) {
      script->set_name(*script_name);
      script->set_line_offset(line_offset);
      script->set_column_offset(column_offset);
    }
    script->set_origin_options(options);
    script->set_compilation_type(Script::COMPILATION_TYPE_EVAL);
    Script::SetEvalOrigin(script, outer_info, eval_position);

    Zone zone(isolate->allocator());
    ParseInfo parse_info(&zone, script);
    CompilationInfo info(&parse_info, Handle<JSFunction>::null());
    parse_info.set_eval();
    if (context->IsNativeContext()) parse_info.set_global();
    parse_info.set_language_mode(language_mode);
    parse_info.set_parse_restriction(restriction);
    parse_info.set_context(context);

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

  if (!maybe_result.ToHandle(&result) ||
      (FLAG_serialize_toplevel &&
       compile_options == ScriptCompiler::kProduceCodeCache)) {
    // No cache entry found, or embedder wants a code cache. Compile the script.

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
    Zone zone(isolate->allocator());
    ParseInfo parse_info(&zone, script);
    CompilationInfo info(&parse_info, Handle<JSFunction>::null());
    if (is_module) {
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
        static_cast<LanguageMode>(parse_info.language_mode() | language_mode));
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
  if (outer_info->shared_info()->never_compiled()) {
    // On the first compile, there are no existing shared function info for
    // inner functions yet, so do not try to find them. All bets are off for
    // live edit though.
    SLOW_DCHECK(script->FindSharedFunctionInfo(literal).is_null() ||
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
    DCHECK(!existing->is_toplevel());
    if (!outer_info->is_debug() || existing->HasDebugCode()) {
      return existing;
    }
  }

  // Allocate a shared function info object.
  Handle<SharedFunctionInfo> result;
  if (!maybe_existing.ToHandle(&result)) {
    result = NewSharedFunctionInfoForLiteral(isolate, literal, script);
    result->set_is_toplevel(false);

    // If the outer function has been compiled before, we cannot be sure that
    // shared function info for this function literal has been created for the
    // first time. It may have already been compiled previously.
    result->set_never_compiled(outer_info->shared_info()->never_compiled());
  }

  Zone zone(isolate->allocator());
  ParseInfo parse_info(&zone, script);
  CompilationInfo info(&parse_info, Handle<JSFunction>::null());
  parse_info.set_literal(literal);
  parse_info.set_shared_info(result);
  parse_info.set_scope(literal->scope());
  parse_info.set_language_mode(literal->scope()->language_mode());
  if (outer_info->will_serialize()) info.PrepareForSerializing();
  if (outer_info->is_debug()) info.MarkAsDebug();

  // Determine if the function can be lazily compiled. This is necessary to
  // allow some of our builtin JS files to be lazily compiled. These
  // builtins cannot be handled lazily by the parser, since we have to know
  // if a function uses the special natives syntax, which is something the
  // parser records.
  // If the debugger requests compilation for break points, we cannot be
  // aggressive about lazy compilation, because it might trigger compilation
  // of functions without an outer context when setting a breakpoint through
  // Debug::FindSharedFunctionInfoInScript.
  bool allow_lazy = literal->AllowsLazyCompilation() && !info.is_debug();
  bool lazy = FLAG_lazy && allow_lazy && !literal->should_eager_compile();

  // Consider compiling eagerly when targeting the code cache.
  lazy &= !(FLAG_serialize_eager && info.will_serialize());

  // Consider compiling eagerly when compiling bytecode for Ignition.
  lazy &=
      !(FLAG_ignition && FLAG_ignition_eager && !isolate->serializer_enabled());

  // Generate code
  TimerEventScope<TimerEventCompileCode> timer(isolate);
  TRACE_EVENT0("v8", "V8.CompileCode");
  if (lazy) {
    info.SetCode(isolate->builtins()->CompileLazy());
  } else if (Renumber(info.parse_info()) && GenerateUnoptimizedCode(&info)) {
    // Code generation will ensure that the feedback vector is present and
    // appropriately sized.
    DCHECK(!info.code().is_null());
    if (literal->should_eager_compile() &&
        literal->should_be_used_once_hint()) {
      info.code()->MarkToBeExecutedOnce(isolate);
    }
    // Update the shared function info with the scope info.
    InstallSharedScopeInfo(&info, result);
    // Install compilation result on the shared function info.
    InstallSharedCompilationResult(&info, result);
  } else {
    return Handle<SharedFunctionInfo>::null();
  }

  if (maybe_existing.is_null()) {
    RecordFunctionCompilation(Logger::FUNCTION_TAG, &info);
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
  const int literals = fun->NumberOfLiterals();
  Handle<Code> code = Handle<Code>(fun->shared()->code());
  Handle<Code> construct_stub = Handle<Code>(fun->shared()->construct_stub());
  Handle<SharedFunctionInfo> shared = isolate->factory()->NewSharedFunctionInfo(
      name, literals, FunctionKind::kNormalFunction, code,
      Handle<ScopeInfo>(fun->shared()->scope_info()));
  shared->set_construct_stub(*construct_stub);
  shared->set_feedback_vector(fun->shared()->feedback_vector());

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

void Compiler::FinalizeCompilationJob(CompilationJob* raw_job) {
  // Take ownership of compilation job.  Deleting job also tears down the zone.
  base::SmartPointer<CompilationJob> job(raw_job);
  CompilationInfo* info = job->info();
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
  if (job->last_status() == CompilationJob::SUCCEEDED) {
    if (shared->optimization_disabled()) {
      job->RetryOptimization(kOptimizationDisabled);
    } else if (info->dependencies()->HasAborted()) {
      job->RetryOptimization(kBailedOutDueToDependencyChange);
    } else if (job->GenerateCode() == CompilationJob::SUCCEEDED) {
      job->RecordOptimizationStats();
      RecordFunctionCompilation(Logger::LAZY_COMPILE_TAG, info);
      if (shared->SearchOptimizedCodeMap(info->context()->native_context(),
                                         info->osr_ast_id()).code == nullptr) {
        InsertCodeIntoOptimizedCodeMap(info);
      }
      if (FLAG_trace_opt) {
        PrintF("[completed optimizing ");
        info->closure()->ShortPrint();
        PrintF("]\n");
      }
      info->closure()->ReplaceCode(*info->code());
      return;
    }
  }

  DCHECK(job->last_status() != CompilationJob::SUCCEEDED);
  if (FLAG_trace_opt) {
    PrintF("[aborted optimizing ");
    info->closure()->ShortPrint();
    PrintF(" because: %s]\n", GetBailoutReason(info->bailout_reason()));
  }
  info->closure()->ReplaceCode(shared->code());
}

void Compiler::PostInstantiation(Handle<JSFunction> function,
                                 PretenureFlag pretenure) {
  Handle<SharedFunctionInfo> shared(function->shared());

  if (FLAG_always_opt && shared->allows_lazy_compilation()) {
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
    function->set_literals(cached.literals);
  } else {
    Isolate* isolate = function->GetIsolate();
    int number_of_literals = shared->num_literals();
    Handle<LiteralsArray> literals =
        LiteralsArray::New(isolate, handle(shared->feedback_vector()),
                           number_of_literals, pretenure);
    function->set_literals(*literals);

    // Cache context-specific literals.
    MaybeHandle<Code> code;
    if (cached.code != nullptr) code = handle(cached.code);
    Handle<Context> native_context(function->context()->native_context());
    SharedFunctionInfo::AddToOptimizedCodeMap(shared, native_context, code,
                                              literals, BailoutId::None());
  }
}

}  // namespace internal
}  // namespace v8
