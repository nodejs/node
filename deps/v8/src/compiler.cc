// Copyright 2011 the V8 project authors. All rights reserved.
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
#include "codegen-inl.h"
#include "compilation-cache.h"
#include "data-flow.h"
#include "debug.h"
#include "full-codegen.h"
#include "gdb-jit.h"
#include "hydrogen.h"
#include "lithium.h"
#include "liveedit.h"
#include "parser.h"
#include "rewriter.h"
#include "runtime-profiler.h"
#include "scopeinfo.h"
#include "scopes.h"
#include "vm-state-inl.h"

namespace v8 {
namespace internal {


CompilationInfo::CompilationInfo(Handle<Script> script)
    : flags_(0),
      function_(NULL),
      scope_(NULL),
      script_(script),
      extension_(NULL),
      pre_parse_data_(NULL),
      supports_deoptimization_(false),
      osr_ast_id_(AstNode::kNoNumber) {
  Initialize(NONOPT);
}


CompilationInfo::CompilationInfo(Handle<SharedFunctionInfo> shared_info)
    : flags_(IsLazy::encode(true)),
      function_(NULL),
      scope_(NULL),
      shared_info_(shared_info),
      script_(Handle<Script>(Script::cast(shared_info->script()))),
      extension_(NULL),
      pre_parse_data_(NULL),
      supports_deoptimization_(false),
      osr_ast_id_(AstNode::kNoNumber) {
  Initialize(BASE);
}


CompilationInfo::CompilationInfo(Handle<JSFunction> closure)
    : flags_(IsLazy::encode(true)),
      function_(NULL),
      scope_(NULL),
      closure_(closure),
      shared_info_(Handle<SharedFunctionInfo>(closure->shared())),
      script_(Handle<Script>(Script::cast(shared_info_->script()))),
      extension_(NULL),
      pre_parse_data_(NULL),
      supports_deoptimization_(false),
      osr_ast_id_(AstNode::kNoNumber) {
  Initialize(BASE);
}


void CompilationInfo::DisableOptimization() {
  if (FLAG_optimize_closures) {
    // If we allow closures optimizations and it's an optimizable closure
    // mark it correspondingly.
    bool is_closure = closure_.is_null() && !scope_->HasTrivialOuterContext();
    if (is_closure) {
      bool is_optimizable_closure =
          !scope_->outer_scope_calls_eval() && !scope_->inside_with();
      if (is_optimizable_closure) {
        SetMode(BASE);
        return;
      }
    }
  }

  SetMode(NONOPT);
}


// Determine whether to use the full compiler for all code. If the flag
// --always-full-compiler is specified this is the case. For the virtual frame
// based compiler the full compiler is also used if a debugger is connected, as
// the code from the full compiler supports mode precise break points. For the
// crankshaft adaptive compiler debugging the optimized code is not possible at
// all. However crankshaft support recompilation of functions, so in this case
// the full compiler need not be be used if a debugger is attached, but only if
// break points has actually been set.
static bool AlwaysFullCompiler() {
#ifdef ENABLE_DEBUGGER_SUPPORT
  if (V8::UseCrankshaft()) {
    return FLAG_always_full_compiler || Debug::has_break_points();
  } else {
    return FLAG_always_full_compiler || Debugger::IsDebuggerActive();
  }
#else
  return FLAG_always_full_compiler;
#endif
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


static void AbortAndDisable(CompilationInfo* info) {
  // Disable optimization for the shared function info and mark the
  // code as non-optimizable. The marker on the shared function info
  // is there because we flush non-optimized code thereby loosing the
  // non-optimizable information for the code. When the code is
  // regenerated and set on the shared function info it is marked as
  // non-optimizable if optimization is disabled for the shared
  // function info.
  Handle<SharedFunctionInfo> shared = info->shared_info();
  shared->set_optimization_disabled(true);
  Handle<Code> code = Handle<Code>(shared->code());
  ASSERT(code->kind() == Code::FUNCTION);
  code->set_optimizable(false);
  info->SetCode(code);
  if (FLAG_trace_opt) {
    PrintF("[disabled optimization for: ");
    info->closure()->PrintName();
    PrintF(" / %" V8PRIxPTR "]\n",
           reinterpret_cast<intptr_t>(*info->closure()));
  }
}


static bool MakeCrankshaftCode(CompilationInfo* info) {
  // Test if we can optimize this function when asked to. We can only
  // do this after the scopes are computed.
  if (!info->AllowOptimize()) info->DisableOptimization();

  // In case we are not optimizing simply return the code from
  // the full code generator.
  if (!info->IsOptimizing()) {
    return FullCodeGenerator::MakeCode(info);
  }

  // We should never arrive here if there is not code object on the
  // shared function object.
  Handle<Code> code(info->shared_info()->code());
  ASSERT(code->kind() == Code::FUNCTION);

  // Fall back to using the full code generator if it's not possible
  // to use the Hydrogen-based optimizing compiler. We already have
  // generated code for this from the shared function object.
  if (AlwaysFullCompiler() || !FLAG_use_hydrogen) {
    info->SetCode(code);
    return true;
  }

  // Limit the number of times we re-compile a functions with
  // the optimizing compiler.
  const int kMaxOptCount =
      FLAG_deopt_every_n_times == 0 ? Compiler::kDefaultMaxOptCount : 1000;
  if (info->shared_info()->opt_count() > kMaxOptCount) {
    AbortAndDisable(info);
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
    AbortAndDisable(info);
    // True indicates the compilation pipeline is still going, not
    // necessarily that we optimized the code.
    return true;
  }

  // Take --hydrogen-filter into account.
  Vector<const char> filter = CStrVector(FLAG_hydrogen_filter);
  Handle<String> name = info->function()->debug_name();
  bool match = filter.is_empty() || name->IsEqualTo(filter);
  if (!match) {
    info->SetCode(code);
    return true;
  }

  // Recompile the unoptimized version of the code if the current version
  // doesn't have deoptimization support. Alternatively, we may decide to
  // run the full code generator to get a baseline for the compile-time
  // performance of the hydrogen-based compiler.
  int64_t start = OS::Ticks();
  bool should_recompile = !info->shared_info()->has_deoptimization_support();
  if (should_recompile || FLAG_time_hydrogen) {
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

  TypeFeedbackOracle oracle(
      code, Handle<Context>(info->closure()->context()->global_context()));
  HGraphBuilder builder(&oracle);
  HPhase phase(HPhase::kTotal);
  HGraph* graph = builder.CreateGraph(info);
  if (Top::has_pending_exception()) {
    info->SetCode(Handle<Code>::null());
    return false;
  }

  if (graph != NULL && FLAG_build_lithium) {
    Handle<Code> optimized_code = graph->Compile();
    if (!optimized_code.is_null()) {
      info->SetCode(optimized_code);
      FinishOptimization(info->closure(), start);
      return true;
    }
  }

  // Compilation with the Hydrogen compiler failed. Keep using the
  // shared code but mark it as unoptimizable.
  AbortAndDisable(info);
  // True indicates the compilation pipeline is still going, not necessarily
  // that we optimized the code.
  return true;
}


static bool MakeCode(CompilationInfo* info) {
  // Precondition: code has been parsed.  Postcondition: the code field in
  // the compilation info is set if compilation succeeded.
  ASSERT(info->function() != NULL);

  if (Rewriter::Rewrite(info) && Scope::Analyze(info)) {
    if (V8::UseCrankshaft()) return MakeCrankshaftCode(info);

    // Generate code and return it.  Code generator selection is governed by
    // which backends are enabled and whether the function is considered
    // run-once code or not.
    //
    // --full-compiler enables the dedicated backend for code we expect to
    // be run once
    //
    // The normal choice of backend can be overridden with the flags
    // --always-full-compiler.
    if (Rewriter::Analyze(info)) {
      Handle<SharedFunctionInfo> shared = info->shared_info();
      bool is_run_once = (shared.is_null())
          ? info->scope()->is_global_scope()
          : (shared->is_toplevel() || shared->try_full_codegen());
      bool can_use_full =
          FLAG_full_compiler && !info->function()->contains_loops();
      if (AlwaysFullCompiler() || (is_run_once && can_use_full)) {
        return FullCodeGenerator::MakeCode(info);
      } else {
        return AssignedVariablesAnalyzer::Analyze(info) &&
            CodeGenerator::MakeCode(info);
      }
    }
  }

  return false;
}


#ifdef ENABLE_DEBUGGER_SUPPORT
bool Compiler::MakeCodeForLiveEdit(CompilationInfo* info) {
  // Precondition: code has been parsed.  Postcondition: the code field in
  // the compilation info is set if compilation succeeded.
  bool succeeded = MakeCode(info);
  if (!info->shared_info().is_null()) {
    Handle<SerializedScopeInfo> scope_info =
        SerializedScopeInfo::Create(info->scope());
    info->shared_info()->set_scope_info(*scope_info);
  }
  return succeeded;
}
#endif


static Handle<SharedFunctionInfo> MakeFunctionInfo(CompilationInfo* info) {
  CompilationZoneScope zone_scope(DELETE_ON_EXIT);

  PostponeInterruptsScope postpone;

  ASSERT(!i::Top::global_context().is_null());
  Handle<Script> script = info->script();
  script->set_context_data((*i::Top::global_context())->data());

#ifdef ENABLE_DEBUGGER_SUPPORT
  if (info->is_eval()) {
    Script::CompilationType compilation_type = Script::COMPILATION_TYPE_EVAL;
    script->set_compilation_type(Smi::FromInt(compilation_type));
    // For eval scripts add information on the function from which eval was
    // called.
    if (info->is_eval()) {
      StackTraceFrameIterator it;
      if (!it.done()) {
        script->set_eval_from_shared(
            JSFunction::cast(it.frame()->function())->shared());
        int offset = static_cast<int>(
            it.frame()->pc() - it.frame()->code()->instruction_start());
        script->set_eval_from_instructions_offset(Smi::FromInt(offset));
      }
    }
  }

  // Notify debugger
  Debugger::OnBeforeCompile(script);
#endif

  // Only allow non-global compiles for eval.
  ASSERT(info->is_eval() || info->is_global());

  if (!ParserApi::Parse(info)) return Handle<SharedFunctionInfo>::null();

  // Measure how long it takes to do the compilation; only take the
  // rest of the function into account to avoid overlap with the
  // parsing statistics.
  HistogramTimer* rate = info->is_eval()
      ? &Counters::compile_eval
      : &Counters::compile;
  HistogramTimerScope timer(rate);

  // Compile the code.
  FunctionLiteral* lit = info->function();
  LiveEditFunctionTracker live_edit_tracker(lit);
  if (!MakeCode(info)) {
    Top::StackOverflow();
    return Handle<SharedFunctionInfo>::null();
  }

  // Allocate function.
  ASSERT(!info->code().is_null());
  Handle<SharedFunctionInfo> result =
      Factory::NewSharedFunctionInfo(
          lit->name(),
          lit->materialized_literal_count(),
          info->code(),
          SerializedScopeInfo::Create(info->scope()));

  ASSERT_EQ(RelocInfo::kNoPosition, lit->function_token_position());
  Compiler::SetFunctionInfo(result, lit, true, script);

  if (script->name()->IsString()) {
    PROFILE(CodeCreateEvent(
        info->is_eval()
            ? Logger::EVAL_TAG
            : Logger::ToNativeByScript(Logger::SCRIPT_TAG, *script),
        *info->code(),
        *result,
        String::cast(script->name())));
    GDBJIT(AddCode(Handle<String>(String::cast(script->name())),
                   script,
                   info->code()));
  } else {
    PROFILE(CodeCreateEvent(
        info->is_eval()
            ? Logger::EVAL_TAG
            : Logger::ToNativeByScript(Logger::SCRIPT_TAG, *script),
        *info->code(),
        *result,
        Heap::empty_string()));
    GDBJIT(AddCode(Handle<String>(), script, info->code()));
  }

  // Hint to the runtime system used when allocating space for initial
  // property space by setting the expected number of properties for
  // the instances of the function.
  SetExpectedNofPropertiesFromEstimate(result, lit->expected_property_count());

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Notify debugger
  Debugger::OnAfterCompile(script, Debugger::NO_AFTER_COMPILE_FLAGS);
#endif

  live_edit_tracker.RecordFunctionInfo(result, lit);

  return result;
}


Handle<SharedFunctionInfo> Compiler::Compile(Handle<String> source,
                                             Handle<Object> script_name,
                                             int line_offset,
                                             int column_offset,
                                             v8::Extension* extension,
                                             ScriptDataImpl* input_pre_data,
                                             Handle<Object> script_data,
                                             NativesFlag natives) {
  int source_length = source->length();
  Counters::total_load_size.Increment(source_length);
  Counters::total_compile_size.Increment(source_length);

  // The VM is in the COMPILER state until exiting this function.
  VMState state(COMPILER);

  // Do a lookup in the compilation cache but not for extensions.
  Handle<SharedFunctionInfo> result;
  if (extension == NULL) {
    result = CompilationCache::LookupScript(source,
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
    ScriptDataImpl* pre_data = input_pre_data;
    if (pre_data == NULL
        && source_length >= FLAG_min_preparse_length) {
      if (source->IsExternalTwoByteString()) {
        ExternalTwoByteStringUC16CharacterStream stream(
            Handle<ExternalTwoByteString>::cast(source), 0, source->length());
        pre_data = ParserApi::PartialPreParse(&stream, extension);
      } else {
        GenericStringUC16CharacterStream stream(source, 0, source->length());
        pre_data = ParserApi::PartialPreParse(&stream, extension);
      }
    }

    // Create a script object describing the script to be compiled.
    Handle<Script> script = Factory::NewScript(source);
    if (natives == NATIVES_CODE) {
      script->set_type(Smi::FromInt(Script::TYPE_NATIVE));
    }
    if (!script_name.is_null()) {
      script->set_name(*script_name);
      script->set_line_offset(Smi::FromInt(line_offset));
      script->set_column_offset(Smi::FromInt(column_offset));
    }

    script->set_data(script_data.is_null() ? Heap::undefined_value()
                                           : *script_data);

    // Compile the function and add it to the cache.
    CompilationInfo info(script);
    info.MarkAsGlobal();
    info.SetExtension(extension);
    info.SetPreParseData(pre_data);
    result = MakeFunctionInfo(&info);
    if (extension == NULL && !result.is_null()) {
      CompilationCache::PutScript(source, result);
    }

    // Get rid of the pre-parsing data (if necessary).
    if (input_pre_data == NULL && pre_data != NULL) {
      delete pre_data;
    }
  }

  if (result.is_null()) Top::ReportPendingMessages();
  return result;
}


Handle<SharedFunctionInfo> Compiler::CompileEval(Handle<String> source,
                                                 Handle<Context> context,
                                                 bool is_global,
                                                 StrictModeFlag strict_mode) {
  int source_length = source->length();
  Counters::total_eval_size.Increment(source_length);
  Counters::total_compile_size.Increment(source_length);

  // The VM is in the COMPILER state until exiting this function.
  VMState state(COMPILER);

  // Do a lookup in the compilation cache; if the entry is not there, invoke
  // the compiler and add the result to the cache.
  Handle<SharedFunctionInfo> result;
  result = CompilationCache::LookupEval(source,
                                        context,
                                        is_global,
                                        strict_mode);

  if (result.is_null()) {
    // Create a script object describing the script to be compiled.
    Handle<Script> script = Factory::NewScript(source);
    CompilationInfo info(script);
    info.MarkAsEval();
    if (is_global) info.MarkAsGlobal();
    if (strict_mode == kStrictMode) info.MarkAsStrict();
    info.SetCallingContext(context);
    result = MakeFunctionInfo(&info);
    if (!result.is_null()) {
      // If caller is strict mode, the result must be strict as well,
      // but not the other way around. Consider:
      // eval("'use strict'; ...");
      ASSERT(strict_mode == kNonStrictMode || result->strict_mode());
      CompilationCache::PutEval(source, context, is_global, result);
    }
  }

  return result;
}


bool Compiler::CompileLazy(CompilationInfo* info) {
  CompilationZoneScope zone_scope(DELETE_ON_EXIT);

  // The VM is in the COMPILER state until exiting this function.
  VMState state(COMPILER);

  PostponeInterruptsScope postpone;

  Handle<SharedFunctionInfo> shared = info->shared_info();
  int compiled_size = shared->end_position() - shared->start_position();
  Counters::total_compile_size.Increment(compiled_size);

  // Generate the AST for the lazily compiled function.
  if (ParserApi::Parse(info)) {
    // Measure how long it takes to do the lazy compilation; only take the
    // rest of the function into account to avoid overlap with the lazy
    // parsing statistics.
    HistogramTimerScope timer(&Counters::compile_lazy);

    // Compile the code.
    if (!MakeCode(info)) {
      if (!Top::has_pending_exception()) {
        Top::StackOverflow();
      }
    } else {
      ASSERT(!info->code().is_null());
      Handle<Code> code = info->code();
      Handle<JSFunction> function = info->closure();
      RecordFunctionCompilation(Logger::LAZY_COMPILE_TAG, info, shared);

      if (info->IsOptimizing()) {
        function->ReplaceCode(*code);
      } else {
        // Update the shared function info with the compiled code and the
        // scope info.  Please note, that the order of the shared function
        // info initialization is important since set_scope_info might
        // trigger a GC, causing the ASSERT below to be invalid if the code
        // was flushed. By settting the code object last we avoid this.
        Handle<SerializedScopeInfo> scope_info =
            SerializedScopeInfo::Create(info->scope());
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

        if (V8::UseCrankshaft() && info->AllowOptimize()) {
          // If we're asked to always optimize, we compile the optimized
          // version of the function right away - unless the debugger is
          // active as it makes no sense to compile optimized code then.
          if (FLAG_always_opt && !Debug::has_break_points()) {
            CompilationInfo optimized(function);
            optimized.SetOptimizing(AstNode::kNoNumber);
            return CompileLazy(&optimized);
          } else if (CompilationCache::ShouldOptimizeEagerly(function)) {
            RuntimeProfiler::OptimizeSoon(*function);
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

  LiveEditFunctionTracker live_edit_tracker(literal);
  // Determine if the function can be lazily compiled. This is necessary to
  // allow some of our builtin JS files to be lazily compiled. These
  // builtins cannot be handled lazily by the parser, since we have to know
  // if a function uses the special natives syntax, which is something the
  // parser records.
  bool allow_lazy = literal->AllowsLazyCompilation() &&
      !LiveEditFunctionTracker::IsActive();

  Handle<SerializedScopeInfo> scope_info(SerializedScopeInfo::Empty());

  // Generate code
  if (FLAG_lazy && allow_lazy) {
    Handle<Code> code(Builtins::builtin(Builtins::LazyCompile));
    info.SetCode(code);
  } else {
    if (V8::UseCrankshaft()) {
      if (!MakeCrankshaftCode(&info)) {
        return Handle<SharedFunctionInfo>::null();
      }
    } else {
      // The bodies of function literals have not yet been visited by the
      // AST optimizer/analyzer.
      if (!Rewriter::Analyze(&info)) return Handle<SharedFunctionInfo>::null();

      bool is_run_once = literal->try_full_codegen();
      bool can_use_full = FLAG_full_compiler && !literal->contains_loops();

      if (AlwaysFullCompiler() || (is_run_once && can_use_full)) {
        if (!FullCodeGenerator::MakeCode(&info)) {
          return Handle<SharedFunctionInfo>::null();
        }
      } else {
        // We fall back to the classic V8 code generator.
        if (!AssignedVariablesAnalyzer::Analyze(&info) ||
            !CodeGenerator::MakeCode(&info)) {
          return Handle<SharedFunctionInfo>::null();
        }
      }
    }
    ASSERT(!info.code().is_null());

    // Function compilation complete.
    scope_info = SerializedScopeInfo::Create(info.scope());
  }

  // Create a shared function info object.
  Handle<SharedFunctionInfo> result =
      Factory::NewSharedFunctionInfo(literal->name(),
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
  function_info->set_length(lit->num_parameters());
  function_info->set_formal_parameter_count(lit->num_parameters());
  function_info->set_script(*script);
  function_info->set_function_token_position(lit->function_token_position());
  function_info->set_start_position(lit->start_position());
  function_info->set_end_position(lit->end_position());
  function_info->set_is_expression(lit->is_expression());
  function_info->set_is_toplevel(is_toplevel);
  function_info->set_inferred_name(*lit->inferred_name());
  function_info->SetThisPropertyAssignmentsInfo(
      lit->has_only_simple_this_property_assignments(),
      *lit->this_property_assignments());
  function_info->set_try_full_codegen(lit->try_full_codegen());
  function_info->set_allows_lazy_compilation(lit->AllowsLazyCompilation());
  function_info->set_strict_mode(lit->strict_mode());
}


void Compiler::RecordFunctionCompilation(Logger::LogEventsAndTags tag,
                                         CompilationInfo* info,
                                         Handle<SharedFunctionInfo> shared) {
  // SharedFunctionInfo is passed separately, because if CompilationInfo
  // was created using Script object, it will not have it.

  // Log the code generation. If source information is available include
  // script name and line number. Check explicitly whether logging is
  // enabled as finding the line number is not free.
  if (Logger::is_logging() || CpuProfiler::is_profiling()) {
    Handle<Script> script = info->script();
    Handle<Code> code = info->code();
    if (*code == Builtins::builtin(Builtins::LazyCompile)) return;
    if (script->name()->IsString()) {
      int line_num = GetScriptLineNumber(script, shared->start_position()) + 1;
      USE(line_num);
      PROFILE(CodeCreateEvent(Logger::ToNativeByScript(tag, *script),
                              *code,
                              *shared,
                              String::cast(script->name()),
                              line_num));
    } else {
      PROFILE(CodeCreateEvent(Logger::ToNativeByScript(tag, *script),
                              *code,
                              *shared,
                              shared->DebugName()));
    }
  }

  GDBJIT(AddCode(name,
                 Handle<Script>(info->script()),
                 Handle<Code>(info->code())));
}

} }  // namespace v8::internal
