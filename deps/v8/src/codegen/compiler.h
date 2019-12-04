// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_COMPILER_H_
#define V8_CODEGEN_COMPILER_H_

#include <forward_list>
#include <memory>

#include "src/base/platform/elapsed-timer.h"
#include "src/codegen/bailout-reason.h"
#include "src/execution/isolate.h"
#include "src/logging/code-events.h"
#include "src/utils/allocation.h"
#include "src/objects/contexts.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

// Forward declarations.
class AstRawString;
class BackgroundCompileTask;
class IsCompiledScope;
class JavaScriptFrame;
class OptimizedCompilationInfo;
class OptimizedCompilationJob;
class ParseInfo;
class Parser;
class ScriptData;
struct ScriptStreamingData;
class TimedHistogram;
class UnoptimizedCompilationInfo;
class UnoptimizedCompilationJob;
class WorkerThreadRuntimeCallStats;

using UnoptimizedCompilationJobList =
    std::forward_list<std::unique_ptr<UnoptimizedCompilationJob>>;

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
                      ClearExceptionFlag flag,
                      IsCompiledScope* is_compiled_scope);
  static bool Compile(Handle<JSFunction> function, ClearExceptionFlag flag,
                      IsCompiledScope* is_compiled_scope);
  static bool CompileOptimized(Handle<JSFunction> function, ConcurrencyMode);

  // Collect source positions for a function that has already been compiled to
  // bytecode, but for which source positions were not collected (e.g. because
  // they were not immediately needed).
  static bool CollectSourcePositions(Isolate* isolate,
                                     Handle<SharedFunctionInfo> shared);

  V8_WARN_UNUSED_RESULT static MaybeHandle<SharedFunctionInfo>
  CompileForLiveEdit(ParseInfo* parse_info, Isolate* isolate);

  // Finalize and install code from previously run background compile task.
  static bool FinalizeBackgroundCompileTask(
      BackgroundCompileTask* task, Handle<SharedFunctionInfo> shared_info,
      Isolate* isolate, ClearExceptionFlag flag);

  // Finalize and install optimized code from previously run job.
  static bool FinalizeOptimizedCompilationJob(OptimizedCompilationJob* job,
                                              Isolate* isolate);

  // Give the compiler a chance to perform low-latency initialization tasks of
  // the given {function} on its instantiation. Note that only the runtime will
  // offer this chance, optimized closure instantiation will not call this.
  static void PostInstantiation(Handle<JSFunction> function);

  // Parser::Parse, then Compiler::Analyze.
  static bool ParseAndAnalyze(ParseInfo* parse_info,
                              Handle<SharedFunctionInfo> shared_info,
                              Isolate* isolate);
  // Rewrite and analyze scopes.
  static bool Analyze(ParseInfo* parse_info);

  // ===========================================================================
  // The following family of methods instantiates new functions for scripts or
  // function literals. The decision whether those functions will be compiled,
  // is left to the discretion of the compiler.
  //
  // Please note this interface returns shared function infos.  This means you
  // need to call Factory::NewFunctionFromSharedFunctionInfo before you have a
  // real function with a context.

  // Create a (bound) function for a String source within a context for eval.
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSFunction> GetFunctionFromEval(
      Handle<String> source, Handle<SharedFunctionInfo> outer_info,
      Handle<Context> context, LanguageMode language_mode,
      ParseRestriction restriction, int parameters_end_pos,
      int eval_scope_position, int eval_position);

  struct ScriptDetails {
    ScriptDetails() : line_offset(0), column_offset(0) {}
    explicit ScriptDetails(Handle<Object> script_name)
        : line_offset(0), column_offset(0), name_obj(script_name) {}

    int line_offset;
    int column_offset;
    i::MaybeHandle<i::Object> name_obj;
    i::MaybeHandle<i::Object> source_map_url;
    i::MaybeHandle<i::FixedArray> host_defined_options;
  };

  // Create a function that results from wrapping |source| in a function,
  // with |arguments| being a list of parameters for that function.
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSFunction> GetWrappedFunction(
      Handle<String> source, Handle<FixedArray> arguments,
      Handle<Context> context, const ScriptDetails& script_details,
      ScriptOriginOptions origin_options, ScriptData* cached_data,
      v8::ScriptCompiler::CompileOptions compile_options,
      v8::ScriptCompiler::NoCacheReason no_cache_reason);

  // Create a (bound) function for a String source within a context for eval.
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSFunction> GetFunctionFromString(
      Handle<Context> context, Handle<i::Object> source,
      ParseRestriction restriction, int parameters_end_pos);

  // Decompose GetFunctionFromString into two functions, to allow callers to
  // deal seperately with a case of object not handled by the embedder.
  V8_WARN_UNUSED_RESULT static std::pair<MaybeHandle<String>, bool>
  ValidateDynamicCompilationSource(Isolate* isolate, Handle<Context> context,
                                   Handle<i::Object> source_object);
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSFunction>
  GetFunctionFromValidatedString(Handle<Context> context,
                                 MaybeHandle<String> source,
                                 ParseRestriction restriction,
                                 int parameters_end_pos);

  // Create a shared function info object for a String source.
  static MaybeHandle<SharedFunctionInfo> GetSharedFunctionInfoForScript(
      Isolate* isolate, Handle<String> source,
      const ScriptDetails& script_details, ScriptOriginOptions origin_options,
      v8::Extension* extension, ScriptData* cached_data,
      ScriptCompiler::CompileOptions compile_options,
      ScriptCompiler::NoCacheReason no_cache_reason,
      NativesFlag is_natives_code);

  // Create a shared function info object for a Script source that has already
  // been parsed and possibly compiled on a background thread while being loaded
  // from a streamed source. On return, the data held by |streaming_data| will
  // have been released, however the object itself isn't freed and is still
  // owned by the caller.
  static MaybeHandle<SharedFunctionInfo> GetSharedFunctionInfoForStreamedScript(
      Isolate* isolate, Handle<String> source,
      const ScriptDetails& script_details, ScriptOriginOptions origin_options,
      ScriptStreamingData* streaming_data);

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
  V8_WARN_UNUSED_RESULT static MaybeHandle<Code> GetOptimizedCodeForOSR(
      Handle<JSFunction> function, BailoutId osr_offset,
      JavaScriptFrame* osr_frame);
};

// A base class for compilation jobs intended to run concurrent to the main
// thread. The current state of the job can be checked using {state()}.
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

  explicit CompilationJob(State initial_state) : state_(initial_state) {
    timer_.Start();
  }
  virtual ~CompilationJob() = default;

  State state() const { return state_; }

 protected:
  V8_WARN_UNUSED_RESULT base::TimeDelta ElapsedTime() const {
    return timer_.Elapsed();
  }

  V8_WARN_UNUSED_RESULT Status UpdateState(Status status, State next_state) {
    if (status == SUCCEEDED) {
      state_ = next_state;
    } else {
      state_ = State::kFailed;
    }
    return status;
  }

 private:
  State state_;
  base::ElapsedTimer timer_;
};

// A base class for unoptimized compilation jobs.
//
// The job is split into two phases which are called in sequence on
// different threads and with different limitations:
//  1) ExecuteJob:   Runs concurrently. No heap allocation or handle derefs.
//  2) FinalizeJob:  Runs on main thread. No dependency changes.
//
// Either of phases can either fail or succeed.
class UnoptimizedCompilationJob : public CompilationJob {
 public:
  UnoptimizedCompilationJob(uintptr_t stack_limit, ParseInfo* parse_info,
                            UnoptimizedCompilationInfo* compilation_info)
      : CompilationJob(State::kReadyToExecute),
        stack_limit_(stack_limit),
        parse_info_(parse_info),
        compilation_info_(compilation_info) {}

  // Executes the compile job. Can be called on a background thread.
  V8_WARN_UNUSED_RESULT Status ExecuteJob();

  // Finalizes the compile job. Must be called on the main thread.
  V8_WARN_UNUSED_RESULT Status
  FinalizeJob(Handle<SharedFunctionInfo> shared_info, Isolate* isolate);

  void RecordCompilationStats(Isolate* isolate) const;
  void RecordFunctionCompilation(CodeEventListener::LogEventsAndTags tag,
                                 Handle<SharedFunctionInfo> shared,
                                 Isolate* isolate) const;

  ParseInfo* parse_info() const { return parse_info_; }
  UnoptimizedCompilationInfo* compilation_info() const {
    return compilation_info_;
  }

  uintptr_t stack_limit() const { return stack_limit_; }

 protected:
  // Overridden by the actual implementation.
  virtual Status ExecuteJobImpl() = 0;
  virtual Status FinalizeJobImpl(Handle<SharedFunctionInfo> shared_info,
                                 Isolate* isolate) = 0;

 private:
  uintptr_t stack_limit_;
  ParseInfo* parse_info_;
  UnoptimizedCompilationInfo* compilation_info_;
  base::TimeDelta time_taken_to_execute_;
  base::TimeDelta time_taken_to_finalize_;
};

// A base class for optimized compilation jobs.
//
// The job is split into three phases which are called in sequence on
// different threads and with different limitations:
//  1) PrepareJob:   Runs on main thread. No major limitations.
//  2) ExecuteJob:   Runs concurrently. No heap allocation or handle derefs.
//  3) FinalizeJob:  Runs on main thread. No dependency changes.
//
// Each of the three phases can either fail or succeed.
class OptimizedCompilationJob : public CompilationJob {
 public:
  OptimizedCompilationJob(OptimizedCompilationInfo* compilation_info,
                          const char* compiler_name,
                          State initial_state = State::kReadyToPrepare)
      : CompilationJob(initial_state),
        compilation_info_(compilation_info),
        compiler_name_(compiler_name) {}

  // Prepare the compile job. Must be called on the main thread.
  V8_WARN_UNUSED_RESULT Status PrepareJob(Isolate* isolate);

  // Executes the compile job. Can be called on a background thread if
  // can_execute_on_background_thread() returns true.
  V8_WARN_UNUSED_RESULT Status ExecuteJob();

  // Finalizes the compile job. Must be called on the main thread.
  V8_WARN_UNUSED_RESULT Status FinalizeJob(Isolate* isolate);

  // Report a transient failure, try again next time. Should only be called on
  // optimization compilation jobs.
  Status RetryOptimization(BailoutReason reason);

  // Report a persistent failure, disable future optimization on the function.
  // Should only be called on optimization compilation jobs.
  Status AbortOptimization(BailoutReason reason);

  enum CompilationMode { kConcurrent, kSynchronous };
  void RecordCompilationStats(CompilationMode mode, Isolate* isolate) const;
  void RecordFunctionCompilation(CodeEventListener::LogEventsAndTags tag,
                                 Isolate* isolate) const;

  OptimizedCompilationInfo* compilation_info() const {
    return compilation_info_;
  }

 protected:
  // Overridden by the actual implementation.
  virtual Status PrepareJobImpl(Isolate* isolate) = 0;
  virtual Status ExecuteJobImpl() = 0;
  virtual Status FinalizeJobImpl(Isolate* isolate) = 0;

 private:
  OptimizedCompilationInfo* compilation_info_;
  base::TimeDelta time_taken_to_prepare_;
  base::TimeDelta time_taken_to_execute_;
  base::TimeDelta time_taken_to_finalize_;
  const char* compiler_name_;
};

class V8_EXPORT_PRIVATE BackgroundCompileTask {
 public:
  // Creates a new task that when run will parse and compile the streamed
  // script associated with |data| and can be finalized with
  // Compiler::GetSharedFunctionInfoForStreamedScript.
  // Note: does not take ownership of |data|.
  BackgroundCompileTask(ScriptStreamingData* data, Isolate* isolate);
  ~BackgroundCompileTask();

  // Creates a new task that when run will parse and compile the
  // |function_literal| and can be finalized with
  // Compiler::FinalizeBackgroundCompileTask.
  BackgroundCompileTask(
      AccountingAllocator* allocator, const ParseInfo* outer_parse_info,
      const AstRawString* function_name,
      const FunctionLiteral* function_literal,
      WorkerThreadRuntimeCallStats* worker_thread_runtime_stats,
      TimedHistogram* timer, int max_stack_size);

  void Run();

  ParseInfo* info() { return info_.get(); }
  Parser* parser() { return parser_.get(); }
  UnoptimizedCompilationJob* outer_function_job() {
    return outer_function_job_.get();
  }
  UnoptimizedCompilationJobList* inner_function_jobs() {
    return &inner_function_jobs_;
  }

 private:
  // Data needed for parsing, and data needed to to be passed between thread
  // between parsing and compilation. These need to be initialized before the
  // compilation starts.
  std::unique_ptr<ParseInfo> info_;
  std::unique_ptr<Parser> parser_;

  // Data needed for finalizing compilation after background compilation.
  std::unique_ptr<UnoptimizedCompilationJob> outer_function_job_;
  UnoptimizedCompilationJobList inner_function_jobs_;

  int stack_size_;
  WorkerThreadRuntimeCallStats* worker_thread_runtime_call_stats_;
  AccountingAllocator* allocator_;
  TimedHistogram* timer_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundCompileTask);
};

// Contains all data which needs to be transmitted between threads for
// background parsing and compiling and finalizing it on the main thread.
struct ScriptStreamingData {
  ScriptStreamingData(
      std::unique_ptr<ScriptCompiler::ExternalSourceStream> source_stream,
      ScriptCompiler::StreamedSource::Encoding encoding);
  ~ScriptStreamingData();

  void Release();

  // Internal implementation of v8::ScriptCompiler::StreamedSource.
  std::unique_ptr<ScriptCompiler::ExternalSourceStream> source_stream;
  ScriptCompiler::StreamedSource::Encoding encoding;

  // Task that performs background parsing and compilation.
  std::unique_ptr<BackgroundCompileTask> task;

  DISALLOW_COPY_AND_ASSIGN(ScriptStreamingData);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_COMPILER_H_
