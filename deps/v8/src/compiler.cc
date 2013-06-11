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


CompilationInfo::CompilationInfo(Handle<Script> script, Zone* zone)
    : flags_(LanguageModeField::encode(CLASSIC_MODE)),
      script_(script),
      osr_ast_id_(BailoutId::None()) {
  Initialize(script->GetIsolate(), BASE, zone);
}


CompilationInfo::CompilationInfo(Handle<SharedFunctionInfo> shared_info,
                                 Zone* zone)
    : flags_(LanguageModeField::encode(CLASSIC_MODE) | IsLazy::encode(true)),
      shared_info_(shared_info),
      script_(Handle<Script>(Script::cast(shared_info->script()))),
      osr_ast_id_(BailoutId::None()) {
  Initialize(script_->GetIsolate(), BASE, zone);
}


CompilationInfo::CompilationInfo(Handle<JSFunction> closure, Zone* zone)
    : flags_(LanguageModeField::encode(CLASSIC_MODE) | IsLazy::encode(true)),
      closure_(closure),
      shared_info_(Handle<SharedFunctionInfo>(closure->shared())),
      script_(Handle<Script>(Script::cast(shared_info_->script()))),
      context_(closure->context()),
      osr_ast_id_(BailoutId::None()) {
  Initialize(script_->GetIsolate(), BASE, zone);
}


CompilationInfo::CompilationInfo(HydrogenCodeStub* stub,
                                 Isolate* isolate, Zone* zone)
    : flags_(LanguageModeField::encode(CLASSIC_MODE) |
             IsLazy::encode(true)),
      osr_ast_id_(BailoutId::None()) {
  Initialize(isolate, STUB, zone);
  code_stub_ = stub;
}


void CompilationInfo::Initialize(Isolate* isolate, Mode mode, Zone* zone) {
  isolate_ = isolate;
  function_ = NULL;
  scope_ = NULL;
  global_scope_ = NULL;
  extension_ = NULL;
  pre_parse_data_ = NULL;
  zone_ = zone;
  deferred_handles_ = NULL;
  code_stub_ = NULL;
  prologue_offset_ = kPrologueOffsetNotSet;
  opt_count_ = shared_info().is_null() ? 0 : shared_info()->opt_count();
  no_frame_ranges_ = isolate->cpu_profiler()->is_profiling()
                   ? new List<OffsetRange>(2) : NULL;
  if (mode == STUB) {
    mode_ = STUB;
    return;
  }
  mode_ = V8::UseCrankshaft() ? mode : NONOPT;
  if (script_->type()->value() == Script::TYPE_NATIVE) {
    MarkAsNative();
  }
  if (!shared_info_.is_null()) {
    ASSERT(language_mode() == CLASSIC_MODE);
    SetLanguageMode(shared_info_->language_mode());
  }
  set_bailout_reason("unknown");
}


CompilationInfo::~CompilationInfo() {
  delete deferred_handles_;
  delete no_frame_ranges_;
}


int CompilationInfo::num_parameters() const {
  ASSERT(!IsStub());
  return scope()->num_parameters();
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
  return FLAG_self_optimization &&
      FLAG_crankshaft &&
      !function()->flags()->Contains(kDontSelfOptimize) &&
      !function()->flags()->Contains(kDontOptimize) &&
      function()->scope()->AllowsLazyCompilation() &&
      (shared_info().is_null() || !shared_info()->optimization_disabled());
}


void CompilationInfo::AbortOptimization() {
  Handle<Code> code(shared_info()->code());
  SetCode(code);
}


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
  return V8::UseCrankshaft() ?
    isolate->debug()->has_break_points() :
    isolate->debugger()->IsDebuggerActive();
#else
  return false;
#endif
}


static bool AlwaysFullCompiler(Isolate* isolate) {
  return FLAG_always_full_compiler || IsDebuggerActive(isolate);
}


void OptimizingCompiler::RecordOptimizationStats() {
  Handle<JSFunction> function = info()->closure();
  int opt_count = function->shared()->opt_count();
  function->shared()->set_opt_count(opt_count + 1);
  double ms_creategraph =
      static_cast<double>(time_taken_to_create_graph_) / 1000;
  double ms_optimize = static_cast<double>(time_taken_to_optimize_) / 1000;
  double ms_codegen = static_cast<double>(time_taken_to_codegen_) / 1000;
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


// A return value of true indicates the compilation pipeline is still
// going, not necessarily that we optimized the code.
static bool MakeCrankshaftCode(CompilationInfo* info) {
  OptimizingCompiler compiler(info);
  OptimizingCompiler::Status status = compiler.CreateGraph();

  if (status != OptimizingCompiler::SUCCEEDED) {
    return status != OptimizingCompiler::FAILED;
  }
  status = compiler.OptimizeGraph();
  if (status != OptimizingCompiler::SUCCEEDED) {
    status = compiler.AbortOptimization();
    return status != OptimizingCompiler::FAILED;
  }
  status = compiler.GenerateAndInstallCode();
  return status != OptimizingCompiler::FAILED;
}


OptimizingCompiler::Status OptimizingCompiler::CreateGraph() {
  ASSERT(V8::UseCrankshaft());
  ASSERT(info()->IsOptimizing());
  ASSERT(!info()->IsCompilingForDebugging());

  // We should never arrive here if there is no code object on the
  // shared function object.
  Handle<Code> code(info()->shared_info()->code());
  ASSERT(code->kind() == Code::FUNCTION);

  // We should never arrive here if optimization has been disabled on the
  // shared function info.
  ASSERT(!info()->shared_info()->optimization_disabled());

  // Fall back to using the full code generator if it's not possible
  // to use the Hydrogen-based optimizing compiler. We already have
  // generated code for this from the shared function object.
  if (AlwaysFullCompiler(isolate())) {
    info()->SetCode(code);
    return SetLastStatus(BAILED_OUT);
  }

  // Limit the number of times we re-compile a functions with
  // the optimizing compiler.
  const int kMaxOptCount =
      FLAG_deopt_every_n_times == 0 ? FLAG_max_opt_count : 1000;
  if (info()->opt_count() > kMaxOptCount) {
    info()->set_bailout_reason("optimized too many times");
    return AbortOptimization();
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
    info()->set_bailout_reason("too many parameters");
    return AbortOptimization();
  }

  const int locals_limit = LUnallocated::kMaxFixedSlotIndex;
  if (!info()->osr_ast_id().IsNone() &&
      scope->num_parameters() + 1 + scope->num_stack_slots() > locals_limit) {
    info()->set_bailout_reason("too many parameters/locals");
    return AbortOptimization();
  }

  // Take --hydrogen-filter into account.
  if (!info()->closure()->PassesHydrogenFilter()) {
      info()->SetCode(code);
      return SetLastStatus(BAILED_OUT);
  }

  // Recompile the unoptimized version of the code if the current version
  // doesn't have deoptimization support. Alternatively, we may decide to
  // run the full code generator to get a baseline for the compile-time
  // performance of the hydrogen-based compiler.
  bool should_recompile = !info()->shared_info()->has_deoptimization_support();
  if (should_recompile || FLAG_hydrogen_stats) {
    HPhase phase(HPhase::kFullCodeGen, isolate());
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
  }

  // Check that the unoptimized, shared code is ready for
  // optimizations.  When using the always_opt flag we disregard the
  // optimizable marker in the code object and optimize anyway. This
  // is safe as long as the unoptimized code has deoptimization
  // support.
  ASSERT(FLAG_always_opt || code->optimizable());
  ASSERT(info()->shared_info()->has_deoptimization_support());

  if (FLAG_trace_hydrogen) {
    Handle<String> name = info()->function()->debug_name();
    PrintF("-----------------------------------------------------------\n");
    PrintF("Compiling method %s using hydrogen\n", *name->ToCString());
    isolate()->GetHTracer()->TraceCompilation(info());
  }

  // Type-check the function.
  AstTyper::Type(info());

  graph_builder_ = new(info()->zone()) HOptimizedGraphBuilder(info());

  Timer t(this, &time_taken_to_create_graph_);
  graph_ = graph_builder_->CreateGraph();

  if (isolate()->has_pending_exception()) {
    info()->SetCode(Handle<Code>::null());
    return SetLastStatus(FAILED);
  }

  // The function being compiled may have bailed out due to an inline
  // candidate bailing out.  In such a case, we don't disable
  // optimization on the shared_info.
  ASSERT(!graph_builder_->inline_bailout() || graph_ == NULL);
  if (graph_ == NULL) {
    if (graph_builder_->inline_bailout()) {
      info_->AbortOptimization();
      return SetLastStatus(BAILED_OUT);
    } else {
      return AbortOptimization();
    }
  }

  return SetLastStatus(SUCCEEDED);
}

OptimizingCompiler::Status OptimizingCompiler::OptimizeGraph() {
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  ASSERT(last_status() == SUCCEEDED);
  Timer t(this, &time_taken_to_optimize_);
  ASSERT(graph_ != NULL);
  SmartArrayPointer<char> bailout_reason;
  if (!graph_->Optimize(&bailout_reason)) {
    if (!bailout_reason.is_empty()) graph_builder_->Bailout(*bailout_reason);
    return SetLastStatus(BAILED_OUT);
  } else {
    chunk_ = LChunk::NewChunk(graph_);
    if (chunk_ == NULL) {
      return SetLastStatus(BAILED_OUT);
    }
  }
  return SetLastStatus(SUCCEEDED);
}


OptimizingCompiler::Status OptimizingCompiler::GenerateAndInstallCode() {
  ASSERT(last_status() == SUCCEEDED);
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
      info()->set_bailout_reason("code generation failed");
      return AbortOptimization();
    }
    info()->SetCode(optimized_code);
  }
  RecordOptimizationStats();
  return SetLastStatus(SUCCEEDED);
}


static bool GenerateCode(CompilationInfo* info) {
  bool is_optimizing = V8::UseCrankshaft() &&
                       !info->IsCompilingForDebugging() &&
                       info->IsOptimizing();
  if (is_optimizing) {
    Logger::TimerEventScope timer(
        info->isolate(), Logger::TimerEventScope::v8_recompile_synchronous);
    return MakeCrankshaftCode(info);
  } else {
    if (info->IsOptimizing()) {
      // Have the CompilationInfo decide if the compilation should be
      // BASE or NONOPT.
      info->DisableOptimization();
    }
    Logger::TimerEventScope timer(
        info->isolate(), Logger::TimerEventScope::v8_compile_full_code);
    return FullCodeGenerator::MakeCode(info);
  }
}


static bool MakeCode(CompilationInfo* info) {
  // Precondition: code has been parsed.  Postcondition: the code field in
  // the compilation info is set if compilation succeeded.
  ASSERT(info->function() != NULL);
  return Rewriter::Rewrite(info) && Scope::Analyze(info) && GenerateCode(info);
}


#ifdef ENABLE_DEBUGGER_SUPPORT
bool Compiler::MakeCodeForLiveEdit(CompilationInfo* info) {
  // Precondition: code has been parsed.  Postcondition: the code field in
  // the compilation info is set if compilation succeeded.
  bool succeeded = MakeCode(info);
  if (!info->shared_info().is_null()) {
    Handle<ScopeInfo> scope_info = ScopeInfo::Create(info->scope(),
                                                     info->zone());
    info->shared_info()->set_scope_info(*scope_info);
  }
  return succeeded;
}
#endif


static bool DebuggerWantsEagerCompilation(CompilationInfo* info,
                                          bool allow_lazy_without_ctx = false) {
  return LiveEditFunctionTracker::IsActive(info->isolate()) ||
         (info->isolate()->DebuggerHasBreakPoints() && !allow_lazy_without_ctx);
}


static Handle<SharedFunctionInfo> MakeFunctionInfo(CompilationInfo* info) {
  Isolate* isolate = info->isolate();
  ZoneScope zone_scope(info->zone(), DELETE_ON_EXIT);
  PostponeInterruptsScope postpone(isolate);

  ASSERT(!isolate->native_context().is_null());
  Handle<Script> script = info->script();
  // TODO(svenpanne) Obscure place for this, perhaps move to OnBeforeCompile?
  FixedArray* array = isolate->native_context()->embedder_data();
  script->set_context_data(array->get(0));

#ifdef ENABLE_DEBUGGER_SUPPORT
  if (info->is_eval()) {
    Script::CompilationType compilation_type = Script::COMPILATION_TYPE_EVAL;
    script->set_compilation_type(Smi::FromInt(compilation_type));
    // For eval scripts add information on the function from which eval was
    // called.
    if (info->is_eval()) {
      StackTraceFrameIterator it(isolate);
      if (!it.done()) {
        script->set_eval_from_shared(
            JSFunction::cast(it.frame()->function())->shared());
        Code* code = it.frame()->LookupCode();
        int offset = static_cast<int>(
            it.frame()->pc() - code->instruction_start());
        script->set_eval_from_instructions_offset(Smi::FromInt(offset));
      }
    }
  }

  // Notify debugger
  isolate->debugger()->OnBeforeCompile(script);
#endif

  // Only allow non-global compiles for eval.
  ASSERT(info->is_eval() || info->is_global());
  {
    Parser parser(info);
    if ((info->pre_parse_data() != NULL ||
         String::cast(script->source())->length() > FLAG_min_preparse_length) &&
        !DebuggerWantsEagerCompilation(info))
      parser.set_allow_lazy(true);
    if (!parser.Parse()) {
      return Handle<SharedFunctionInfo>::null();
    }
  }

  // Measure how long it takes to do the compilation; only take the
  // rest of the function into account to avoid overlap with the
  // parsing statistics.
  HistogramTimer* rate = info->is_eval()
      ? info->isolate()->counters()->compile_eval()
      : info->isolate()->counters()->compile();
  HistogramTimerScope timer(rate);

  // Compile the code.
  FunctionLiteral* lit = info->function();
  LiveEditFunctionTracker live_edit_tracker(isolate, lit);
  if (!MakeCode(info)) {
    if (!isolate->has_pending_exception()) isolate->StackOverflow();
    return Handle<SharedFunctionInfo>::null();
  }

  // Allocate function.
  ASSERT(!info->code().is_null());
  Handle<SharedFunctionInfo> result =
      isolate->factory()->NewSharedFunctionInfo(
          lit->name(),
          lit->materialized_literal_count(),
          lit->is_generator(),
          info->code(),
          ScopeInfo::Create(info->scope(), info->zone()));

  ASSERT_EQ(RelocInfo::kNoPosition, lit->function_token_position());
  Compiler::SetFunctionInfo(result, lit, true, script);

  if (script->name()->IsString()) {
    PROFILE(isolate, CodeCreateEvent(
        info->is_eval()
            ? Logger::EVAL_TAG
            : Logger::ToNativeByScript(Logger::SCRIPT_TAG, *script),
        *info->code(),
        *result,
        info,
        String::cast(script->name())));
    GDBJIT(AddCode(Handle<String>(String::cast(script->name())),
                   script,
                   info->code(),
                   info));
  } else {
    PROFILE(isolate, CodeCreateEvent(
        info->is_eval()
            ? Logger::EVAL_TAG
            : Logger::ToNativeByScript(Logger::SCRIPT_TAG, *script),
        *info->code(),
        *result,
        info,
        isolate->heap()->empty_string()));
    GDBJIT(AddCode(Handle<String>(), script, info->code(), info));
  }

  // Hint to the runtime system used when allocating space for initial
  // property space by setting the expected number of properties for
  // the instances of the function.
  SetExpectedNofPropertiesFromEstimate(result, lit->expected_property_count());

  script->set_compilation_state(
      Smi::FromInt(Script::COMPILATION_STATE_COMPILED));

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Notify debugger
  isolate->debugger()->OnAfterCompile(
      script, Debugger::NO_AFTER_COMPILE_FLAGS);
#endif

  live_edit_tracker.RecordFunctionInfo(result, lit, info->zone());

  return result;
}


Handle<SharedFunctionInfo> Compiler::Compile(Handle<String> source,
                                             Handle<Object> script_name,
                                             int line_offset,
                                             int column_offset,
                                             Handle<Context> context,
                                             v8::Extension* extension,
                                             ScriptDataImpl* pre_data,
                                             Handle<Object> script_data,
                                             NativesFlag natives) {
  Isolate* isolate = source->GetIsolate();
  int source_length = source->length();
  isolate->counters()->total_load_size()->Increment(source_length);
  isolate->counters()->total_compile_size()->Increment(source_length);

  // The VM is in the COMPILER state until exiting this function.
  VMState<COMPILER> state(isolate);

  CompilationCache* compilation_cache = isolate->compilation_cache();

  // Do a lookup in the compilation cache but not for extensions.
  Handle<SharedFunctionInfo> result;
  if (extension == NULL) {
    result = compilation_cache->LookupScript(source,
                                             script_name,
                                             line_offset,
                                             column_offset,
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

    script->set_data(script_data.is_null() ? HEAP->undefined_value()
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
    result = MakeFunctionInfo(&info);
    if (extension == NULL && !result.is_null() && !result->dont_cache()) {
      compilation_cache->PutScript(source, context, result);
    }
  } else {
    if (result->ic_age() != HEAP->global_ic_age()) {
      result->ResetForNewContext(HEAP->global_ic_age());
    }
  }

  if (result.is_null()) isolate->ReportPendingMessages();
  return result;
}


Handle<SharedFunctionInfo> Compiler::CompileEval(Handle<String> source,
                                                 Handle<Context> context,
                                                 bool is_global,
                                                 LanguageMode language_mode,
                                                 ParseRestriction restriction,
                                                 int scope_position) {
  Isolate* isolate = source->GetIsolate();
  int source_length = source->length();
  isolate->counters()->total_eval_size()->Increment(source_length);
  isolate->counters()->total_compile_size()->Increment(source_length);

  // The VM is in the COMPILER state until exiting this function.
  VMState<COMPILER> state(isolate);

  // Do a lookup in the compilation cache; if the entry is not there, invoke
  // the compiler and add the result to the cache.
  Handle<SharedFunctionInfo> result;
  CompilationCache* compilation_cache = isolate->compilation_cache();
  result = compilation_cache->LookupEval(source,
                                         context,
                                         is_global,
                                         language_mode,
                                         scope_position);

  if (result.is_null()) {
    // Create a script object describing the script to be compiled.
    Handle<Script> script = isolate->factory()->NewScript(source);
    CompilationInfoWithZone info(script);
    info.MarkAsEval();
    if (is_global) info.MarkAsGlobal();
    info.SetLanguageMode(language_mode);
    info.SetParseRestriction(restriction);
    info.SetContext(context);
    result = MakeFunctionInfo(&info);
    if (!result.is_null()) {
      // Explicitly disable optimization for eval code. We're not yet prepared
      // to handle eval-code in the optimizing compiler.
      result->DisableOptimization("eval");

      // If caller is strict mode, the result must be in strict mode or
      // extended mode as well, but not the other way around. Consider:
      // eval("'use strict'; ...");
      ASSERT(language_mode != STRICT_MODE || !result->is_classic_mode());
      // If caller is in extended mode, the result must also be in
      // extended mode.
      ASSERT(language_mode != EXTENDED_MODE ||
             result->is_extended_mode());
      if (!result->dont_cache()) {
        compilation_cache->PutEval(
            source, context, is_global, result, scope_position);
      }
    }
  } else {
    if (result->ic_age() != HEAP->global_ic_age()) {
      result->ResetForNewContext(HEAP->global_ic_age());
    }
  }

  return result;
}


static bool InstallFullCode(CompilationInfo* info) {
  // Update the shared function info with the compiled code and the
  // scope info.  Please note, that the order of the shared function
  // info initialization is important since set_scope_info might
  // trigger a GC, causing the ASSERT below to be invalid if the code
  // was flushed. By setting the code object last we avoid this.
  Handle<SharedFunctionInfo> shared = info->shared_info();
  Handle<Code> code = info->code();
  Handle<JSFunction> function = info->closure();
  Handle<ScopeInfo> scope_info =
      ScopeInfo::Create(info->scope(), info->zone());
  shared->set_scope_info(*scope_info);
  shared->ReplaceCode(*code);
  if (!function.is_null()) {
    function->ReplaceCode(*code);
    ASSERT(!function->IsOptimized());
  }

  // Set the expected number of properties for instances.
  FunctionLiteral* lit = info->function();
  int expected = lit->expected_property_count();
  SetExpectedNofPropertiesFromEstimate(shared, expected);

  // Check the function has compiled code.
  ASSERT(shared->is_compiled());
  shared->set_code_age(0);
  shared->set_dont_optimize(lit->flags()->Contains(kDontOptimize));
  shared->set_dont_inline(lit->flags()->Contains(kDontInline));
  shared->set_ast_node_count(lit->ast_node_count());

  if (V8::UseCrankshaft() &&
      !function.is_null() &&
      !shared->optimization_disabled()) {
    // If we're asked to always optimize, we compile the optimized
    // version of the function right away - unless the debugger is
    // active as it makes no sense to compile optimized code then.
    if (FLAG_always_opt &&
        !Isolate::Current()->DebuggerHasBreakPoints()) {
      CompilationInfoWithZone optimized(function);
      optimized.SetOptimizing(BailoutId::None());
      return Compiler::CompileLazy(&optimized);
    }
  }
  return true;
}


static void InstallCodeCommon(CompilationInfo* info) {
  Handle<SharedFunctionInfo> shared = info->shared_info();
  Handle<Code> code = info->code();
  ASSERT(!code.is_null());

  // Set optimizable to false if this is disallowed by the shared
  // function info, e.g., we might have flushed the code and must
  // reset this bit when lazy compiling the code again.
  if (shared->optimization_disabled()) code->set_optimizable(false);

  if (shared->code() == *code) {
    // Do not send compilation event for the same code twice.
    return;
  }
  Compiler::RecordFunctionCompilation(Logger::LAZY_COMPILE_TAG, info, shared);
}


static void InsertCodeIntoOptimizedCodeMap(CompilationInfo* info) {
  Handle<Code> code = info->code();
  if (FLAG_cache_optimized_code &&
      info->osr_ast_id().IsNone() &&
      code->kind() == Code::OPTIMIZED_FUNCTION) {
    Handle<JSFunction> function = info->closure();
    Handle<SharedFunctionInfo> shared(function->shared());
    Handle<FixedArray> literals(function->literals());
    Handle<Context> native_context(function->context()->native_context());
    SharedFunctionInfo::AddToOptimizedCodeMap(
        shared, native_context, code, literals);
  }
}


static bool InstallCodeFromOptimizedCodeMap(CompilationInfo* info) {
  if (FLAG_cache_optimized_code &&
      info->osr_ast_id().IsNone() &&
      info->IsOptimizing()) {
    Handle<SharedFunctionInfo> shared = info->shared_info();
    Handle<JSFunction> function = info->closure();
    ASSERT(!function.is_null());
    Handle<Context> native_context(function->context()->native_context());
    int index = shared->SearchOptimizedCodeMap(*native_context);
    if (index > 0) {
      if (FLAG_trace_opt) {
        PrintF("[found optimized code for ");
        function->ShortPrint();
        PrintF("]\n");
      }
      // Caching of optimized code enabled and optimized code found.
      shared->InstallFromOptimizedCodeMap(*function, index);
      return true;
    }
  }
  return false;
}


bool Compiler::CompileLazy(CompilationInfo* info) {
  Isolate* isolate = info->isolate();

  ZoneScope zone_scope(info->zone(), DELETE_ON_EXIT);

  // The VM is in the COMPILER state until exiting this function.
  VMState<COMPILER> state(isolate);

  PostponeInterruptsScope postpone(isolate);

  Handle<SharedFunctionInfo> shared = info->shared_info();
  int compiled_size = shared->end_position() - shared->start_position();
  isolate->counters()->total_compile_size()->Increment(compiled_size);

  if (InstallCodeFromOptimizedCodeMap(info)) return true;

  // Generate the AST for the lazily compiled function.
  if (Parser::Parse(info)) {
    // Measure how long it takes to do the lazy compilation; only take the
    // rest of the function into account to avoid overlap with the lazy
    // parsing statistics.
    HistogramTimerScope timer(isolate->counters()->compile_lazy());

    // After parsing we know the function's language mode. Remember it.
    LanguageMode language_mode = info->function()->language_mode();
    info->SetLanguageMode(language_mode);
    shared->set_language_mode(language_mode);

    // Compile the code.
    if (!MakeCode(info)) {
      if (!isolate->has_pending_exception()) {
        isolate->StackOverflow();
      }
    } else {
      InstallCodeCommon(info);

      if (info->IsOptimizing()) {
        Handle<Code> code = info->code();
        ASSERT(shared->scope_info() != ScopeInfo::Empty(isolate));
        info->closure()->ReplaceCode(*code);
        InsertCodeIntoOptimizedCodeMap(info);
        return true;
      } else {
        return InstallFullCode(info);
      }
    }
  }

  ASSERT(info->code().is_null());
  return false;
}


void Compiler::RecompileParallel(Handle<JSFunction> closure) {
  ASSERT(closure->IsMarkedForParallelRecompilation());

  Isolate* isolate = closure->GetIsolate();
  // Here we prepare compile data for the parallel recompilation thread, but
  // this still happens synchronously and interrupts execution.
  Logger::TimerEventScope timer(
      isolate, Logger::TimerEventScope::v8_recompile_synchronous);

  if (!isolate->optimizing_compiler_thread()->IsQueueAvailable()) {
    if (FLAG_trace_parallel_recompilation) {
      PrintF("  ** Compilation queue, will retry opting on next run.\n");
    }
    return;
  }

  SmartPointer<CompilationInfo> info(new CompilationInfoWithZone(closure));
  VMState<COMPILER> state(isolate);
  PostponeInterruptsScope postpone(isolate);

  Handle<SharedFunctionInfo> shared = info->shared_info();
  int compiled_size = shared->end_position() - shared->start_position();
  isolate->counters()->total_compile_size()->Increment(compiled_size);
  info->SetOptimizing(BailoutId::None());

  {
    CompilationHandleScope handle_scope(*info);

    if (InstallCodeFromOptimizedCodeMap(*info)) {
      return;
    }

    if (Parser::Parse(*info)) {
      LanguageMode language_mode = info->function()->language_mode();
      info->SetLanguageMode(language_mode);
      shared->set_language_mode(language_mode);
      info->SaveHandles();

      if (Rewriter::Rewrite(*info) && Scope::Analyze(*info)) {
        OptimizingCompiler* compiler =
            new(info->zone()) OptimizingCompiler(*info);
        OptimizingCompiler::Status status = compiler->CreateGraph();
        if (status == OptimizingCompiler::SUCCEEDED) {
          info.Detach();
          shared->code()->set_profiler_ticks(0);
          isolate->optimizing_compiler_thread()->QueueForOptimization(compiler);
        } else if (status == OptimizingCompiler::BAILED_OUT) {
          isolate->clear_pending_exception();
          InstallFullCode(*info);
        }
      }
    }
  }

  if (shared->code()->back_edges_patched_for_osr()) {
    // At this point we either put the function on recompilation queue or
    // aborted optimization.  In either case we want to continue executing
    // the unoptimized code without running into OSR.  If the unoptimized
    // code has been patched for OSR, unpatch it.
    InterruptStub interrupt_stub;
    Handle<Code> interrupt_code = interrupt_stub.GetCode(isolate);
    Handle<Code> replacement_code =
        isolate->builtins()->OnStackReplacement();
    Deoptimizer::RevertInterruptCode(shared->code(),
                                     *interrupt_code,
                                     *replacement_code);
  }

  if (isolate->has_pending_exception()) isolate->clear_pending_exception();
}


void Compiler::InstallOptimizedCode(OptimizingCompiler* optimizing_compiler) {
  SmartPointer<CompilationInfo> info(optimizing_compiler->info());
  // The function may have already been optimized by OSR.  Simply continue.
  // Except when OSR already disabled optimization for some reason.
  if (info->shared_info()->optimization_disabled()) {
    info->SetCode(Handle<Code>(info->shared_info()->code()));
    InstallFullCode(*info);
    if (FLAG_trace_parallel_recompilation) {
      PrintF("  ** aborting optimization for ");
      info->closure()->PrintName();
      PrintF(" as it has been disabled.\n");
    }
    ASSERT(!info->closure()->IsMarkedForInstallingRecompiledCode());
    return;
  }

  Isolate* isolate = info->isolate();
  VMState<COMPILER> state(isolate);
  Logger::TimerEventScope timer(
      isolate, Logger::TimerEventScope::v8_recompile_synchronous);
  // If crankshaft succeeded, install the optimized code else install
  // the unoptimized code.
  OptimizingCompiler::Status status = optimizing_compiler->last_status();
  if (status != OptimizingCompiler::SUCCEEDED) {
    optimizing_compiler->info()->set_bailout_reason(
        "failed/bailed out last time");
    status = optimizing_compiler->AbortOptimization();
  } else {
    status = optimizing_compiler->GenerateAndInstallCode();
    ASSERT(status == OptimizingCompiler::SUCCEEDED ||
           status == OptimizingCompiler::BAILED_OUT);
  }

  InstallCodeCommon(*info);
  if (status == OptimizingCompiler::SUCCEEDED) {
    Handle<Code> code = info->code();
    ASSERT(info->shared_info()->scope_info() != ScopeInfo::Empty(isolate));
    info->closure()->ReplaceCode(*code);
    if (info->shared_info()->SearchOptimizedCodeMap(
            info->closure()->context()->native_context()) == -1) {
      InsertCodeIntoOptimizedCodeMap(*info);
    }
    if (FLAG_trace_parallel_recompilation) {
      PrintF("  ** Optimized code for ");
      info->closure()->PrintName();
      PrintF(" installed.\n");
    }
  } else {
    info->SetCode(Handle<Code>(info->shared_info()->code()));
    InstallFullCode(*info);
  }
  // Optimized code is finally replacing unoptimized code.  Reset the latter's
  // profiler ticks to prevent too soon re-opt after a deopt.
  info->shared_info()->code()->set_profiler_ticks(0);
  ASSERT(!info->closure()->IsMarkedForInstallingRecompiledCode());
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

  Handle<ScopeInfo> scope_info(ScopeInfo::Empty(isolate));

  // Generate code
  if (FLAG_lazy && allow_lazy && !literal->is_parenthesized()) {
    Handle<Code> code = isolate->builtins()->LazyCompile();
    info.SetCode(code);
  } else if (GenerateCode(&info)) {
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


// Sets the function info on a function.
// The start_position points to the first '(' character after the function name
// in the full script source. When counting characters in the script source the
// the first character is number 0 (not 1).
void Compiler::SetFunctionInfo(Handle<SharedFunctionInfo> function_info,
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
  function_info->set_dont_optimize(lit->flags()->Contains(kDontOptimize));
  function_info->set_dont_inline(lit->flags()->Contains(kDontInline));
  function_info->set_dont_cache(lit->flags()->Contains(kDontCache));
  function_info->set_is_generator(lit->is_generator());
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
    if (*code == info->isolate()->builtins()->builtin(Builtins::kLazyCompile))
      return;
    if (script->name()->IsString()) {
      int line_num = GetScriptLineNumber(script, shared->start_position()) + 1;
      USE(line_num);
      PROFILE(info->isolate(),
              CodeCreateEvent(Logger::ToNativeByScript(tag, *script),
                              *code,
                              *shared,
                              info,
                              String::cast(script->name()),
                              line_num));
    } else {
      PROFILE(info->isolate(),
              CodeCreateEvent(Logger::ToNativeByScript(tag, *script),
                              *code,
                              *shared,
                              info,
                              shared->DebugName()));
    }
  }

  GDBJIT(AddCode(Handle<String>(shared->DebugName()),
                 Handle<Script>(info->script()),
                 Handle<Code>(info->code()),
                 info));
}

} }  // namespace v8::internal
