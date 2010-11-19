// Copyright 2010 the V8 project authors. All rights reserved.
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
#include "liveedit.h"
#include "oprofile-agent.h"
#include "parser.h"
#include "rewriter.h"
#include "scopeinfo.h"
#include "scopes.h"

namespace v8 {
namespace internal {


CompilationInfo::CompilationInfo(Handle<Script> script)
    : flags_(0),
      function_(NULL),
      scope_(NULL),
      script_(script),
      extension_(NULL),
      pre_parse_data_(NULL) {
}


CompilationInfo::CompilationInfo(Handle<SharedFunctionInfo> shared_info)
    : flags_(IsLazy::encode(true)),
      function_(NULL),
      scope_(NULL),
      shared_info_(shared_info),
      script_(Handle<Script>(Script::cast(shared_info->script()))),
      extension_(NULL),
      pre_parse_data_(NULL) {
}


CompilationInfo::CompilationInfo(Handle<JSFunction> closure)
    : flags_(IsLazy::encode(true)),
      function_(NULL),
      scope_(NULL),
      closure_(closure),
      shared_info_(Handle<SharedFunctionInfo>(closure->shared())),
      script_(Handle<Script>(Script::cast(shared_info_->script()))),
      extension_(NULL),
      pre_parse_data_(NULL) {
}


// For normal operation the syntax checker is used to determine whether to
// use the full compiler for top level code or not. However if the flag
// --always-full-compiler is specified or debugging is active the full
// compiler will be used for all code.
static bool AlwaysFullCompiler() {
#ifdef ENABLE_DEBUGGER_SUPPORT
  return FLAG_always_full_compiler || Debugger::IsDebuggerActive();
#else
  return FLAG_always_full_compiler;
#endif
}


static bool MakeCode(CompilationInfo* info) {
  // Precondition: code has been parsed.  Postcondition: the code field in
  // the compilation info is set if compilation succeeded.
  ASSERT(info->function() != NULL);

  if (Rewriter::Rewrite(info) &&
      Scope::Analyze(info) &&
      Rewriter::Analyze(info)) {
    // Generate code and return it.  Code generator selection is governed by
    // which backends are enabled and whether the function is considered
    // run-once code or not.
    //
    // --full-compiler enables the dedicated backend for code we expect to
    // be run once
    //
    // The normal choice of backend can be overridden with the flags
    // --always-full-compiler.
    Handle<SharedFunctionInfo> shared = info->shared_info();
    bool is_run_once = (shared.is_null())
        ? info->scope()->is_global_scope()
        : (shared->is_toplevel() || shared->try_full_codegen());
    bool can_use_full =
        FLAG_full_compiler && !info->function()->contains_loops();
    if (AlwaysFullCompiler() || (is_run_once && can_use_full)) {
      return FullCodeGenerator::MakeCode(info);
    } else {
      AssignedVariablesAnalyzer ava;
      return ava.Analyze(info) && CodeGenerator::MakeCode(info);
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

  ASSERT(!info->code().is_null());
  if (script->name()->IsString()) {
    PROFILE(CodeCreateEvent(
        info->is_eval()
            ? Logger::EVAL_TAG
            : Logger::ToNativeByScript(Logger::SCRIPT_TAG, *script),
        *info->code(),
        String::cast(script->name())));
    OPROFILE(CreateNativeCodeRegion(String::cast(script->name()),
                                    info->code()->instruction_start(),
                                    info->code()->instruction_size()));
  } else {
    PROFILE(CodeCreateEvent(
        info->is_eval()
            ? Logger::EVAL_TAG
            : Logger::ToNativeByScript(Logger::SCRIPT_TAG, *script),
        *info->code(),
        ""));
    OPROFILE(CreateNativeCodeRegion(info->is_eval() ? "Eval" : "Script",
                                    info->code()->instruction_start(),
                                    info->code()->instruction_size()));
  }

  // Allocate function.
  Handle<SharedFunctionInfo> result =
      Factory::NewSharedFunctionInfo(
          lit->name(),
          lit->materialized_literal_count(),
          info->code(),
          SerializedScopeInfo::Create(info->scope()));

  ASSERT_EQ(RelocInfo::kNoPosition, lit->function_token_position());
  Compiler::SetFunctionInfo(result, lit, true, script);

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
      pre_data = ParserApi::PartialPreParse(source, NULL, extension);
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
                                                 bool is_global) {
  int source_length = source->length();
  Counters::total_eval_size.Increment(source_length);
  Counters::total_compile_size.Increment(source_length);

  // The VM is in the COMPILER state until exiting this function.
  VMState state(COMPILER);

  // Do a lookup in the compilation cache; if the entry is not there, invoke
  // the compiler and add the result to the cache.
  Handle<SharedFunctionInfo> result;
  result = CompilationCache::LookupEval(source, context, is_global);

  if (result.is_null()) {
    // Create a script object describing the script to be compiled.
    Handle<Script> script = Factory::NewScript(source);
    CompilationInfo info(script);
    info.MarkAsEval();
    if (is_global) info.MarkAsGlobal();
    info.SetCallingContext(context);
    result = MakeFunctionInfo(&info);
    if (!result.is_null()) {
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
      Top::StackOverflow();
    } else {
      ASSERT(!info->code().is_null());
      RecordFunctionCompilation(Logger::LAZY_COMPILE_TAG,
                                Handle<String>(shared->DebugName()),
                                shared->start_position(),
                                info);

      // Update the shared function info with the compiled code and the
      // scope info.  Please note, that the order of the sharedfunction
      // initialization is important since SerializedScopeInfo::Create might
      // trigger a GC, causing the ASSERT below to be invalid if the code
      // was flushed. By setting the code object last we avoid this.
      Handle<SerializedScopeInfo> scope_info =
          SerializedScopeInfo::Create(info->scope());
      shared->set_scope_info(*scope_info);
      shared->set_code(*info->code());
      if (!info->closure().is_null()) {
        info->closure()->set_code(*info->code());
      }

      // Set the expected number of properties for instances.
      FunctionLiteral* lit = info->function();
      SetExpectedNofPropertiesFromEstimate(shared,
                                           lit->expected_property_count());

      // Set the optimization hints after performing lazy compilation, as
      // these are not set when the function is set up as a lazily compiled
      // function.
      shared->SetThisPropertyAssignmentsInfo(
          lit->has_only_simple_this_property_assignments(),
          *lit->this_property_assignments());

      // Check the function has compiled code.
      ASSERT(shared->is_compiled());
      shared->set_code_age(0);
      ASSERT(!info->code().is_null());
      return true;
    }
  }

  ASSERT(info->code().is_null());
  return false;
}


Handle<SharedFunctionInfo> Compiler::BuildFunctionInfo(FunctionLiteral* literal,
                                                       Handle<Script> script) {
#ifdef DEBUG
  // We should not try to compile the same function literal more than
  // once.
  literal->mark_as_compiled();
#endif

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
    // Generate code and return it.  The way that the compilation mode
    // is controlled by the command-line flags is described in
    // the static helper function MakeCode.
    //
    // The bodies of function literals have not yet been visited by
    // the AST analyzer.
    if (!Rewriter::Analyze(&info)) return Handle<SharedFunctionInfo>::null();

    bool is_run_once = literal->try_full_codegen();
    bool use_full = FLAG_full_compiler && !literal->contains_loops();
    if (AlwaysFullCompiler() || (use_full && is_run_once)) {
      if (!FullCodeGenerator::MakeCode(&info)) {
        return Handle<SharedFunctionInfo>::null();
      }
    } else {
      // We fall back to the classic V8 code generator.
      AssignedVariablesAnalyzer ava;
      if (!ava.Analyze(&info)) return Handle<SharedFunctionInfo>::null();
      if (!CodeGenerator::MakeCode(&info)) {
        return Handle<SharedFunctionInfo>::null();
      }
    }

    // Function compilation complete.
    RecordFunctionCompilation(Logger::FUNCTION_TAG,
                              literal->debug_name(),
                              literal->start_position(),
                              &info);
    scope_info = SerializedScopeInfo::Create(info.scope());
  }

  // Create a shared function info object.
  Handle<SharedFunctionInfo> result =
      Factory::NewSharedFunctionInfo(literal->name(),
                                     literal->materialized_literal_count(),
                                     info.code(),
                                     scope_info);
  SetFunctionInfo(result, literal, false, script);

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
}


void Compiler::RecordFunctionCompilation(Logger::LogEventsAndTags tag,
                                         Handle<String> name,
                                         int start_position,
                                         CompilationInfo* info) {
  // Log the code generation. If source information is available include
  // script name and line number. Check explicitly whether logging is
  // enabled as finding the line number is not free.
  if (Logger::is_logging() ||
      OProfileAgent::is_enabled() ||
      CpuProfiler::is_profiling()) {
    Handle<Script> script = info->script();
    Handle<Code> code = info->code();
    if (script->name()->IsString()) {
      int line_num = GetScriptLineNumber(script, start_position) + 1;
      USE(line_num);
      PROFILE(CodeCreateEvent(Logger::ToNativeByScript(tag, *script),
                              *code,
                              *name,
                              String::cast(script->name()),
                              line_num));
      OPROFILE(CreateNativeCodeRegion(*name,
                                      String::cast(script->name()),
                                      line_num,
                                      code->instruction_start(),
                                      code->instruction_size()));
    } else {
      PROFILE(CodeCreateEvent(Logger::ToNativeByScript(tag, *script),
                              *code,
                              *name));
      OPROFILE(CreateNativeCodeRegion(*name,
                                      code->instruction_start(),
                                      code->instruction_size()));
    }
  }
}

} }  // namespace v8::internal
