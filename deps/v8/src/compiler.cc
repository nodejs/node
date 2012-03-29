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
#include "full-codegen.h"
#include "gdb-jit.h"
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


CompilationInfo::CompilationInfo(Handle<Script> script)
    : isolate_(script->GetIsolate()),
      flags_(LanguageModeField::encode(CLASSIC_MODE)),
      function_(NULL),
      scope_(NULL),
      global_scope_(NULL),
      script_(script),
      extension_(NULL),
      pre_parse_data_(NULL),
      osr_ast_id_(AstNode::kNoNumber) {
  Initialize(BASE);
}


CompilationInfo::CompilationInfo(Handle<SharedFunctionInfo> shared_info)
    : isolate_(shared_info->GetIsolate()),
      flags_(LanguageModeField::encode(CLASSIC_MODE) |
             IsLazy::encode(true)),
      function_(NULL),
      scope_(NULL),
      global_scope_(NULL),
      shared_info_(shared_info),
      script_(Handle<Script>(Script::cast(shared_info->script()))),
      extension_(NULL),
      pre_parse_data_(NULL),
      osr_ast_id_(AstNode::kNoNumber) {
  Initialize(BASE);
}


CompilationInfo::CompilationInfo(Handle<JSFunction> closure)
    : isolate_(closure->GetIsolate()),
      flags_(LanguageModeField::encode(CLASSIC_MODE) |
             IsLazy::encode(true)),
      function_(NULL),
      scope_(NULL),
      global_scope_(NULL),
      closure_(closure),
      shared_info_(Handle<SharedFunctionInfo>(closure->shared())),
      script_(Handle<Script>(Script::cast(shared_info_->script()))),
      extension_(NULL),
      pre_parse_data_(NULL),
      osr_ast_id_(AstNode::kNoNumber) {
  Initialize(BASE);
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
      function()->scope()->allows_lazy_recompilation() &&
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
static bool is_debugging_active() {
#ifdef ENABLE_DEBUGGER_SUPPORT
  Isolate* isolate = Isolate::Current();
  return V8::UseCrankshaft() ?
    isolate->debug()->has_break_points() :
    isolate->debugger()->IsDebuggerActive();
#else
  return false;
#endif
}


static bool AlwaysFullCompiler() {
  return FLAG_always_full_compiler || is_debugging_active();
}


static void FinishOptimization(Handle<JSFunction> function, int64_t start) {
  int opt_count = function->shared()->opt_count();
  function->shared()->set_opt_count(opt_count + 1);
  double ms = static_cast<double>(OS::Ticks() - start) / 1000;
  if (FLAG_trace_opt) {
    PrintF("[optimizing: ");
    function->PrintName();
    PrintF(" / %" V8PRIxPTR, reinterpret_cast<intptr_t>(*function));
    PrintF(" - took %0.3f ms]\n", ms);
  }
  if (FLAG_trace_opt_stats) {
    static double compilation_time = 0.0;
    static int compiled_functions = 0;
    static int code_size = 0;

    compilation_time += ms;
    compiled_functions++;
    code_size += function->shared()->SourceSize();
    PrintF("Compiled: %d functions with %d byte source size in %fms.\n",
           compiled_functions,
           code_size,
           compilation_time);
  }
}


static bool MakeCrankshaftCode(CompilationInfo* info) {
  // Test if we can optimize this function when asked to. We can only
  // do this after the scopes are computed.
  if (!V8::UseCrankshaft()) {
    info->DisableOptimization();
  }

  // In case we are not optimizing simply return the code from
  // the full code generator.
  if (!info->IsOptimizing()) {
    return FullCodeGenerator::MakeCode(info);
  }

  // We should never arrive here if there is not code object on the
  // shared function object.
  Handle<Code> code(info->shared_info()->code());
  ASSERT(code->kind() == Code::FUNCTION);

  // We should never arrive here if optimization has been disabled on the
  // shared function info.
  ASSERT(!info->shared_info()->optimization_disabled());

  // Fall back to using the full code generator if it's not possible
  // to use the Hydrogen-based optimizing compiler. We already have
  // generated code for this from the shared function object.
  if (AlwaysFullCompiler()) {
    info->SetCode(code);
    return true;
  }

  // Limit the number of times we re-compile a functions with
  // the optimizing compiler.
  const int kMaxOptCount =
      FLAG_deopt_every_n_times == 0 ? Compiler::kDefaultMaxOptCount : 1000;
  if (info->shared_info()->opt_count() > kMaxOptCount) {
    info->AbortOptimization();
    info->shared_info()->DisableOptimization();
    // True indicates the compilation pipeline is still going, not
    // necessarily that we optimized the code.
    return true;
  }

  // Due to an encoding limit on LUnallocated operands in the Lithium
  // language, we cannot optimize functions with too many formal parameters
  // or perform on-stack replacement for function with too many
  // stack-allocated local variables.
  //
  // The encoding is as a signed value, with parameters and receiver using
  // the negative indices and locals the non-negative ones.
  const int parameter_limit = -LUnallocated::kMinFixedIndex;
  const int locals_limit = LUnallocated::kMaxFixedIndex;
  Scope* scope = info->scope();
  if ((scope->num_parameters() + 1) > parameter_limit ||
      (info->osr_ast_id() != AstNode::kNoNumber &&
       scope->num_parameters() + 1 + scope->num_stack_slots() > locals_limit)) {
    info->AbortOptimization();
    info->shared_info()->DisableOptimization();
    // True indicates the compilation pipeline is still going, not
    // necessarily that we optimized the code.
    return true;
  }

  // Take --hydrogen-filter into account.
  Handle<String> name = info->function()->debug_name();
  if (*FLAG_hydrogen_filter != '\0') {
    Vector<const char> filter = CStrVector(FLAG_hydrogen_filter);
    if ((filter[0] == '-'
         && name->IsEqualTo(filter.SubVector(1, filter.length())))
        || (filter[0] != '-' && !name->IsEqualTo(filter))) {
      info->SetCode(code);
      return true;
    }
  }

  // Recompile the unoptimized version of the code if the current version
  // doesn't have deoptimization support. Alternatively, we may decide to
  // run the full code generator to get a baseline for the compile-time
  // performance of the hydrogen-based compiler.
  int64_t start = OS::Ticks();
  bool should_recompile = !info->shared_info()->has_deoptimization_support();
  if (should_recompile || FLAG_hydrogen_stats) {
    HPhase phase(HPhase::kFullCodeGen);
    CompilationInfo unoptimized(info->shared_info());
    // Note that we use the same AST that we will use for generating the
    // optimized code.
    unoptimized.SetFunction(info->function());
    unoptimized.SetScope(info->scope());
    if (should_recompile) unoptimized.EnableDeoptimizationSupport();
    bool succeeded = FullCodeGenerator::MakeCode(&unoptimized);
    if (should_recompile) {
      if (!succeeded) return false;
      Handle<SharedFunctionInfo> shared = info->shared_info();
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
  ASSERT(info->shared_info()->has_deoptimization_support());

  if (FLAG_trace_hydrogen) {
    PrintF("-----------------------------------------------------------\n");
    PrintF("Compiling method %s using hydrogen\n", *name->ToCString());
    HTracer::Instance()->TraceCompilation(info->function());
  }

  Handle<Context> global_context(info->closure()->context()->global_context());
  TypeFeedbackOracle oracle(code, global_context, info->isolate());
  HGraphBuilder builder(info, &oracle);
  HPhase phase(HPhase::kTotal);
  HGraph* graph = builder.CreateGraph();
  if (info->isolate()->has_pending_exception()) {
    info->SetCode(Handle<Code>::null());
    return false;
  }

  if (graph != NULL) {
    Handle<Code> optimized_code = graph->Compile(info);
    if (!optimized_code.is_null()) {
      info->SetCode(optimized_code);
      FinishOptimization(info->closure(), start);
      return true;
    }
  }

  // Keep using the shared code.
  info->AbortOptimization();
  if (!builder.inline_bailout()) {
    // Mark the shared code as unoptimizable unless it was an inlined
    // function that bailed out.
    info->shared_info()->DisableOptimization();
  }
  // True indicates the compilation pipeline is still going, not necessarily
  // that we optimized the code.
  return true;
}


static bool GenerateCode(CompilationInfo* info) {
  return info->IsCompilingForDebugging() || !V8::UseCrankshaft() ?
      FullCodeGenerator::MakeCode(info) :
      MakeCrankshaftCode(info);
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
    Handle<ScopeInfo> scope_info = ScopeInfo::Create(info->scope());
    info->shared_info()->set_scope_info(*scope_info);
  }
  return succeeded;
}
#endif


static Handle<SharedFunctionInfo> MakeFunctionInfo(CompilationInfo* info) {
  Isolate* isolate = info->isolate();
  ZoneScope zone_scope(isolate, DELETE_ON_EXIT);
  PostponeInterruptsScope postpone(isolate);

  ASSERT(!isolate->global_context().is_null());
  Handle<Script> script = info->script();
  script->set_context_data((*isolate->global_context())->data());

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
  ParsingFlags flags = kNoParsingFlags;
  if (info->pre_parse_data() != NULL ||
      String::cast(script->source())->length() > FLAG_min_preparse_length) {
    flags = kAllowLazy;
  }
  if (!ParserApi::Parse(info, flags)) {
    return Handle<SharedFunctionInfo>::null();
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
          info->code(),
          ScopeInfo::Create(info->scope()));

  ASSERT_EQ(RelocInfo::kNoPosition, lit->function_token_position());
  Compiler::SetFunctionInfo(result, lit, true, script);

  if (script->name()->IsString()) {
    PROFILE(isolate, CodeCreateEvent(
        info->is_eval()
            ? Logger::EVAL_TAG
            : Logger::ToNativeByScript(Logger::SCRIPT_TAG, *script),
        *info->code(),
        *result,
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

  live_edit_tracker.RecordFunctionInfo(result, lit);

  return result;
}


Handle<SharedFunctionInfo> Compiler::Compile(Handle<String> source,
                                             Handle<Object> script_name,
                                             int line_offset,
                                             int column_offset,
                                             v8::Extension* extension,
                                             ScriptDataImpl* pre_data,
                                             Handle<Object> script_data,
                                             NativesFlag natives) {
  Isolate* isolate = source->GetIsolate();
  int source_length = source->length();
  isolate->counters()->total_load_size()->Increment(source_length);
  isolate->counters()->total_compile_size()->Increment(source_length);

  // The VM is in the COMPILER state until exiting this function.
  VMState state(isolate, COMPILER);

  CompilationCache* compilation_cache = isolate->compilation_cache();

  // Do a lookup in the compilation cache but not for extensions.
  Handle<SharedFunctionInfo> result;
  if (extension == NULL) {
    result = compilation_cache->LookupScript(source,
                                             script_name,
                                             line_offset,
                                             column_offset);
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
    Handle<Script> script = FACTORY->NewScript(source);
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
    CompilationInfo info(script);
    info.MarkAsGlobal();
    info.SetExtension(extension);
    info.SetPreParseData(pre_data);
    if (FLAG_use_strict) {
      info.SetLanguageMode(FLAG_harmony_scoping ? EXTENDED_MODE : STRICT_MODE);
    }
    result = MakeFunctionInfo(&info);
    if (extension == NULL && !result.is_null()) {
      compilation_cache->PutScript(source, result);
    }
  }

  if (result.is_null()) isolate->ReportPendingMessages();
  return result;
}


Handle<SharedFunctionInfo> Compiler::CompileEval(Handle<String> source,
                                                 Handle<Context> context,
                                                 bool is_global,
                                                 LanguageMode language_mode,
                                                 int scope_position) {
  Isolate* isolate = source->GetIsolate();
  int source_length = source->length();
  isolate->counters()->total_eval_size()->Increment(source_length);
  isolate->counters()->total_compile_size()->Increment(source_length);

  // The VM is in the COMPILER state until exiting this function.
  VMState state(isolate, COMPILER);

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
    CompilationInfo info(script);
    info.MarkAsEval();
    if (is_global) info.MarkAsGlobal();
    info.SetLanguageMode(language_mode);
    info.SetCallingContext(context);
    result = MakeFunctionInfo(&info);
    if (!result.is_null()) {
      // Explicitly disable optimization for eval code. We're not yet prepared
      // to handle eval-code in the optimizing compiler.
      result->DisableOptimization();

      // If caller is strict mode, the result must be in strict mode or
      // extended mode as well, but not the other way around. Consider:
      // eval("'use strict'; ...");
      ASSERT(language_mode != STRICT_MODE || !result->is_classic_mode());
      // If caller is in extended mode, the result must also be in
      // extended mode.
      ASSERT(language_mode != EXTENDED_MODE ||
             result->is_extended_mode());
      compilation_cache->PutEval(
          source, context, is_global, result, scope_position);
    }
  }

  return result;
}


bool Compiler::CompileLazy(CompilationInfo* info) {
  Isolate* isolate = info->isolate();

  ZoneScope zone_scope(isolate, DELETE_ON_EXIT);

  // The VM is in the COMPILER state until exiting this function.
  VMState state(isolate, COMPILER);

  PostponeInterruptsScope postpone(isolate);

  Handle<SharedFunctionInfo> shared = info->shared_info();
  int compiled_size = shared->end_position() - shared->start_position();
  isolate->counters()->total_compile_size()->Increment(compiled_size);

  // Generate the AST for the lazily compiled function.
  if (ParserApi::Parse(info, kNoParsingFlags)) {
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
      ASSERT(!info->code().is_null());
      Handle<Code> code = info->code();
      // Set optimizable to false if this is disallowed by the shared
      // function info, e.g., we might have flushed the code and must
      // reset this bit when lazy compiling the code again.
      if (shared->optimization_disabled()) code->set_optimizable(false);

      Handle<JSFunction> function = info->closure();
      RecordFunctionCompilation(Logger::LAZY_COMPILE_TAG, info, shared);

      if (info->IsOptimizing()) {
        ASSERT(shared->scope_info() != ScopeInfo::Empty());
        function->ReplaceCode(*code);
      } else {
        // Update the shared function info with the compiled code and the
        // scope info.  Please note, that the order of the shared function
        // info initialization is important since set_scope_info might
        // trigger a GC, causing the ASSERT below to be invalid if the code
        // was flushed. By setting the code object last we avoid this.
        Handle<ScopeInfo> scope_info = ScopeInfo::Create(info->scope());
        shared->set_scope_info(*scope_info);
        shared->set_code(*code);
        if (!function.is_null()) {
          function->ReplaceCode(*code);
          ASSERT(!function->IsOptimized());
        }

        // Set the expected number of properties for instances.
        FunctionLiteral* lit = info->function();
        int expected = lit->expected_property_count();
        SetExpectedNofPropertiesFromEstimate(shared, expected);

        // Set the optimization hints after performing lazy compilation, as
        // these are not set when the function is set up as a lazily
        // compiled function.
        shared->SetThisPropertyAssignmentsInfo(
            lit->has_only_simple_this_property_assignments(),
            *lit->this_property_assignments());

        // Check the function has compiled code.
        ASSERT(shared->is_compiled());
        shared->set_code_age(0);
        shared->set_dont_optimize(lit->flags()->Contains(kDontOptimize));
        shared->set_dont_inline(lit->flags()->Contains(kDontInline));
        shared->set_ast_node_count(lit->ast_node_count());

        if (V8::UseCrankshaft()&&
            !function.is_null() &&
            !shared->optimization_disabled()) {
          // If we're asked to always optimize, we compile the optimized
          // version of the function right away - unless the debugger is
          // active as it makes no sense to compile optimized code then.
          if (FLAG_always_opt &&
              !Isolate::Current()->DebuggerHasBreakPoints()) {
            CompilationInfo optimized(function);
            optimized.SetOptimizing(AstNode::kNoNumber);
            return CompileLazy(&optimized);
          }
        }
      }

      return true;
    }
  }

  ASSERT(info->code().is_null());
  return false;
}


Handle<SharedFunctionInfo> Compiler::BuildFunctionInfo(FunctionLiteral* literal,
                                                       Handle<Script> script) {
  // Precondition: code has been parsed and scopes have been analyzed.
  CompilationInfo info(script);
  info.SetFunction(literal);
  info.SetScope(literal->scope());
  info.SetLanguageMode(literal->scope()->language_mode());

  LiveEditFunctionTracker live_edit_tracker(info.isolate(), literal);
  // Determine if the function can be lazily compiled. This is necessary to
  // allow some of our builtin JS files to be lazily compiled. These
  // builtins cannot be handled lazily by the parser, since we have to know
  // if a function uses the special natives syntax, which is something the
  // parser records.
  bool allow_lazy = literal->AllowsLazyCompilation() &&
      !LiveEditFunctionTracker::IsActive(info.isolate());

  Handle<ScopeInfo> scope_info(ScopeInfo::Empty());

  // Generate code
  if (FLAG_lazy && allow_lazy) {
    Handle<Code> code = info.isolate()->builtins()->LazyCompile();
    info.SetCode(code);
  } else if ((V8::UseCrankshaft() && MakeCrankshaftCode(&info)) ||
             (!V8::UseCrankshaft() && FullCodeGenerator::MakeCode(&info))) {
    ASSERT(!info.code().is_null());
    scope_info = ScopeInfo::Create(info.scope());
  } else {
    return Handle<SharedFunctionInfo>::null();
  }

  // Create a shared function info object.
  Handle<SharedFunctionInfo> result =
      FACTORY->NewSharedFunctionInfo(literal->name(),
                                     literal->materialized_literal_count(),
                                     info.code(),
                                     scope_info);
  SetFunctionInfo(result, literal, false, script);
  RecordFunctionCompilation(Logger::FUNCTION_TAG, &info, result);
  result->set_allows_lazy_compilation(allow_lazy);

  // Set the expected number of properties for instances and return
  // the resulting function.
  SetExpectedNofPropertiesFromEstimate(result,
                                       literal->expected_property_count());
  live_edit_tracker.RecordFunctionInfo(result, literal);
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
  function_info->SetThisPropertyAssignmentsInfo(
      lit->has_only_simple_this_property_assignments(),
      *lit->this_property_assignments());
  function_info->set_allows_lazy_compilation(lit->AllowsLazyCompilation());
  function_info->set_language_mode(lit->language_mode());
  function_info->set_uses_arguments(lit->scope()->arguments() != NULL);
  function_info->set_has_duplicate_parameters(lit->has_duplicate_parameters());
  function_info->set_ast_node_count(lit->ast_node_count());
  function_info->set_is_function(lit->is_function());
  function_info->set_dont_optimize(lit->flags()->Contains(kDontOptimize));
  function_info->set_dont_inline(lit->flags()->Contains(kDontInline));
}


void Compiler::RecordFunctionCompilation(Logger::LogEventsAndTags tag,
                                         CompilationInfo* info,
                                         Handle<SharedFunctionInfo> shared) {
  // SharedFunctionInfo is passed separately, because if CompilationInfo
  // was created using Script object, it will not have it.

  // Log the code generation. If source information is available include
  // script name and line number. Check explicitly whether logging is
  // enabled as finding the line number is not free.
  if (info->isolate()->logger()->is_logging() ||
      CpuProfiler::is_profiling(info->isolate())) {
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
                              String::cast(script->name()),
                              line_num));
    } else {
      PROFILE(info->isolate(),
              CodeCreateEvent(Logger::ToNativeByScript(tag, *script),
                              *code,
                              *shared,
                              shared->DebugName()));
    }
  }

  GDBJIT(AddCode(Handle<String>(shared->DebugName()),
                 Handle<Script>(info->script()),
                 Handle<Code>(info->code()),
                 info));
}

} }  // namespace v8::internal
