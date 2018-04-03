// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_H_
#define V8_COMPILER_H_

#include <forward_list>
#include <memory>

#include "src/allocation.h"
#include "src/bailout-reason.h"
#include "src/code-events.h"
#include "src/contexts.h"
#include "src/isolate.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

// Forward declarations.
class CompilationInfo;
class CompilationJob;
class JavaScriptFrame;
class ParseInfo;
class ScriptData;
template <typename T>
class ThreadedList;
template <typename T>
class ThreadedListZoneEntry;

typedef std::forward_list<std::unique_ptr<CompilationJob>> CompilationJobList;

// The V8 compiler API.
//
// This is the central hub for dispatching to the various compilers within V8.
// Logic for which compiler to choose and how to wire compilation results into
// the object heap should be kept inside this class.
//
// General strategy: Scripts are translated into anonymous functions w/o
// parameters which then can be executed. If the source code contains other
// functions, they might be compiled and allocated as part of the compilation
// of the source code or deferred for lazy compilation at a later point.
class V8_EXPORT_PRIVATE Compiler : public AllStatic {
 public:
  enum ClearExceptionFlag { KEEP_EXCEPTION, CLEAR_EXCEPTION };

  // ===========================================================================
  // The following family of methods ensures a given function is compiled. The
  // general contract is that failures will be reported by returning {false},
  // whereas successful compilation ensures the {is_compiled} predicate on the
  // given function holds (except for live-edit, which compiles the world).

  static bool Compile(Handle<SharedFunctionInfo> shared,
                      ClearExceptionFlag flag);
  static bool Compile(Handle<JSFunction> function, ClearExceptionFlag flag);
  static bool CompileOptimized(Handle<JSFunction> function, ConcurrencyMode);
  static MaybeHandle<JSArray> CompileForLiveEdit(Handle<Script> script);

  // Compile top level code on a background thread. Should be finalized by
  // GetSharedFunctionInfoForBackgroundCompile.
  static std::unique_ptr<CompilationJob> CompileTopLevelOnBackgroundThread(
      ParseInfo* parse_info, AccountingAllocator* allocator,
      CompilationJobList* inner_function_jobs);

  // Generate and install code from previously queued compilation job.
  static bool FinalizeCompilationJob(CompilationJob* job, Isolate* isolate);

  // Give the compiler a chance to perform low-latency initialization tasks of
  // the given {function} on its instantiation. Note that only the runtime will
  // offer this chance, optimized closure instantiation will not call this.
  static void PostInstantiation(Handle<JSFunction> function, PretenureFlag);

  typedef ThreadedList<ThreadedListZoneEntry<FunctionLiteral*>>
      EagerInnerFunctionLiterals;

  // Parser::Parse, then Compiler::Analyze.
  static bool ParseAndAnalyze(ParseInfo* parse_info,
                              Handle<SharedFunctionInfo> shared_info,
                              Isolate* isolate);
  // Rewrite, analyze scopes, and renumber. If |eager_literals| is non-null, it
  // is appended with inner function literals which should be eagerly compiled.
  static bool Analyze(ParseInfo* parse_info,
                      EagerInnerFunctionLiterals* eager_literals = nullptr);

  // ===========================================================================
  // The following family of methods instantiates new functions for scripts or
  // function literals. The decision whether those functions will be compiled,
  // is left to the discretion of the compiler.
  //
  // Please note this interface returns shared function infos.  This means you
  // need to call Factory::NewFunctionFromSharedFunctionInfo before you have a
  // real function with a context.

  // Create a (bound) function for a String source within a context for eval.
  MUST_USE_RESULT static MaybeHandle<JSFunction> GetFunctionFromEval(
      Handle<String> source, Handle<SharedFunctionInfo> outer_info,
      Handle<Context> context, LanguageMode language_mode,
      ParseRestriction restriction, int parameters_end_pos,
      int eval_scope_position, int eval_position, int line_offset = 0,
      int column_offset = 0, Handle<Object> script_name = Handle<Object>(),
      ScriptOriginOptions options = ScriptOriginOptions());

  // Create a function that results from wrapping |source| in a function,
  // with |arguments| being a list of parameters for that function.
  MUST_USE_RESULT static MaybeHandle<JSFunction> GetWrappedFunction(
      Handle<String> source, Handle<FixedArray> arguments,
      Handle<Context> context, int line_offset = 0, int column_offset = 0,
      Handle<Object> script_name = Handle<Object>(),
      ScriptOriginOptions options = ScriptOriginOptions());

  // Returns true if the embedder permits compiling the given source string in
  // the given context.
  static bool CodeGenerationFromStringsAllowed(Isolate* isolate,
                                               Handle<Context> context,
                                               Handle<String> source);

  // Create a (bound) function for a String source within a context for eval.
  MUST_USE_RESULT static MaybeHandle<JSFunction> GetFunctionFromString(
      Handle<Context> context, Handle<String> source,
      ParseRestriction restriction, int parameters_end_pos);

  // Create a shared function info object for a String source within a context.
  static MaybeHandle<SharedFunctionInfo> GetSharedFunctionInfoForScript(
      Handle<String> source, MaybeHandle<Object> maybe_script_name,
      int line_offset, int column_offset, ScriptOriginOptions resource_options,
      MaybeHandle<Object> maybe_source_map_url, Handle<Context> context,
      v8::Extension* extension, ScriptData** cached_data,
      ScriptCompiler::CompileOptions compile_options,
      ScriptCompiler::NoCacheReason no_cache_reason,
      NativesFlag is_natives_code,
      MaybeHandle<FixedArray> maybe_host_defined_options);

  // Create a shared function info object for a Script that has already been
  // parsed while the script was being loaded from a streamed source.
  static Handle<SharedFunctionInfo> GetSharedFunctionInfoForStreamedScript(
      Handle<Script> script, ParseInfo* info, int source_length);

  // Create a shared function info object for a Script that has already been
  // compiled on a background thread.
  static Handle<SharedFunctionInfo> GetSharedFunctionInfoForBackgroundCompile(
      Handle<Script> script, ParseInfo* parse_info, int source_length,
      CompilationJob* outer_function_job,
      CompilationJobList* inner_function_jobs);

  // Create a shared function info object for the given function literal
  // node (the code may be lazily compiled).
  static Handle<SharedFunctionInfo> GetSharedFunctionInfo(FunctionLiteral* node,
                                                          Handle<Script> script,
                                                          Isolate* isolate);

  // ===========================================================================
  // The following family of methods provides support for OSR. Code generated
  // for entry via OSR might not be suitable for normal entry, hence will be
  // returned directly to the caller.
  //
  // Please note this interface is the only part dealing with {Code} objects
  // directly. Other methods are agnostic to {Code} and can use an interpreter
  // instead of generating JIT code for a function at all.

  // Generate and return optimized code for OSR, or empty handle on failure.
  MUST_USE_RESULT static MaybeHandle<Code> GetOptimizedCodeForOSR(
      Handle<JSFunction> function, BailoutId osr_offset,
      JavaScriptFrame* osr_frame);
};

// A base class for compilation jobs intended to run concurrent to the main
// thread. The job is split into three phases which are called in sequence on
// different threads and with different limitations:
//  1) PrepareJob:   Runs on main thread. No major limitations.
//  2) ExecuteJob:   Runs concurrently. No heap allocation or handle derefs.
//  3) FinalizeJob:  Runs on main thread. No dependency changes.
//
// Each of the three phases can either fail or succeed. The current state of
// the job can be checked using {state()}.
class V8_EXPORT_PRIVATE CompilationJob {
 public:
  enum Status { SUCCEEDED, FAILED };
  enum class State {
    kReadyToPrepare,
    kReadyToExecute,
    kReadyToFinalize,
    kSucceeded,
    kFailed,
  };
  CompilationJob(uintptr_t stack_limit, ParseInfo* parse_info,
                 CompilationInfo* compilation_info, const char* compiler_name,
                 State initial_state = State::kReadyToPrepare);
  virtual ~CompilationJob() {}

  // Prepare the compile job. Must be called on the main thread.
  MUST_USE_RESULT Status PrepareJob(Isolate* isolate);

  // Executes the compile job. Can be called on a background thread if
  // can_execute_on_background_thread() returns true.
  MUST_USE_RESULT Status ExecuteJob();

  // Finalizes the compile job. Must be called on the main thread.
  MUST_USE_RESULT Status FinalizeJob(Isolate* isolate);

  // Report a transient failure, try again next time. Should only be called on
  // optimization compilation jobs.
  Status RetryOptimization(BailoutReason reason);

  // Report a persistent failure, disable future optimization on the function.
  // Should only be called on optimization compilation jobs.
  Status AbortOptimization(BailoutReason reason);

  void RecordOptimizedCompilationStats() const;
  void RecordUnoptimizedCompilationStats(Isolate* isolate) const;
  void RecordFunctionCompilation(CodeEventListener::LogEventsAndTags tag,
                                 Isolate* isolate) const;

  void set_stack_limit(uintptr_t stack_limit) { stack_limit_ = stack_limit; }
  uintptr_t stack_limit() const { return stack_limit_; }

  State state() const { return state_; }
  ParseInfo* parse_info() const { return parse_info_; }
  CompilationInfo* compilation_info() const { return compilation_info_; }
  virtual size_t AllocatedMemory() const { return 0; }

 protected:
  // Overridden by the actual implementation.
  virtual Status PrepareJobImpl(Isolate* isolate) = 0;
  virtual Status ExecuteJobImpl() = 0;
  virtual Status FinalizeJobImpl(Isolate* isolate) = 0;

 private:
  // TODO(6409): Remove parse_info once Fullcode and AstGraphBuilder are gone.
  ParseInfo* parse_info_;
  CompilationInfo* compilation_info_;
  base::TimeDelta time_taken_to_prepare_;
  base::TimeDelta time_taken_to_execute_;
  base::TimeDelta time_taken_to_finalize_;
  const char* compiler_name_;
  State state_;
  uintptr_t stack_limit_;

  MUST_USE_RESULT Status UpdateState(Status status, State next_state) {
    if (status == SUCCEEDED) {
      state_ = next_state;
    } else {
      state_ = State::kFailed;
    }
    return status;
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_H_
