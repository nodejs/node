// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_DISPATCHER_COMPILER_DISPATCHER_JOB_H_
#define V8_COMPILER_DISPATCHER_COMPILER_DISPATCHER_JOB_H_

#include <memory>

#include "include/v8.h"
#include "src/base/macros.h"
#include "src/globals.h"
#include "src/handles.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // nogncheck

namespace v8 {
namespace internal {

class AstValueFactory;
class AstStringConstants;
class CompilerDispatcherTracer;
class CompilationInfo;
class CompilationJob;
class DeferredHandles;
class FunctionLiteral;
class Isolate;
class ParseInfo;
class Parser;
class SharedFunctionInfo;
class String;
class UnicodeCache;
class Utf16CharacterStream;

enum class CompileJobStatus {
  kInitial,
  kReadyToParse,
  kParsed,
  kReadyToAnalyze,
  kAnalyzed,
  kReadyToCompile,
  kCompiled,
  kFailed,
  kDone,
};

class V8_EXPORT_PRIVATE CompileJobFinishCallback {
 public:
  virtual ~CompileJobFinishCallback() {}
  virtual void ParseFinished(std::unique_ptr<ParseInfo> parse_info) = 0;
};

class V8_EXPORT_PRIVATE CompilerDispatcherJob {
 public:
  // Creates a CompilerDispatcherJob in the initial state.
  CompilerDispatcherJob(Isolate* isolate, CompilerDispatcherTracer* tracer,
                        Handle<SharedFunctionInfo> shared,
                        size_t max_stack_size);
  // TODO(wiktorg) document it better once I know how it relates to whole stuff
  // Creates a CompilerDispatcherJob in ready to parse top-level function state.
  CompilerDispatcherJob(CompilerDispatcherTracer* tracer, size_t max_stack_size,
                        Handle<String> source, int start_position,
                        int end_position, LanguageMode language_mode,
                        int function_literal_id, bool native, bool module,
                        bool is_named_expression, uint32_t hash_seed,
                        AccountingAllocator* zone_allocator, int compiler_hints,
                        const AstStringConstants* ast_string_constants,
                        CompileJobFinishCallback* finish_callback);
  // Creates a CompilerDispatcherJob in the analyzed state.
  CompilerDispatcherJob(Isolate* isolate, CompilerDispatcherTracer* tracer,
                        Handle<Script> script,
                        Handle<SharedFunctionInfo> shared,
                        FunctionLiteral* literal,
                        std::shared_ptr<Zone> parse_zone,
                        std::shared_ptr<DeferredHandles> parse_handles,
                        std::shared_ptr<DeferredHandles> compile_handles,
                        size_t max_stack_size);
  ~CompilerDispatcherJob();

  CompileJobStatus status() const { return status_; }

  bool has_context() const { return !context_.is_null(); }
  Context* context() { return *context_; }

  Handle<SharedFunctionInfo> shared() const { return shared_; }

  // Returns true if this CompilerDispatcherJob was created for the given
  // function.
  bool IsAssociatedWith(Handle<SharedFunctionInfo> shared) const;

  // Transition from kInitial to kReadyToParse.
  void PrepareToParseOnMainThread();

  // Transition from kReadyToParse to kParsed (or kDone if there is
  // finish_callback).
  void Parse();

  // Transition from kParsed to kReadyToAnalyze (or kFailed). Returns false
  // when transitioning to kFailed. In that case, an exception is pending.
  bool FinalizeParsingOnMainThread();

  // Transition from kReadyToAnalyze to kAnalyzed (or kFailed). Returns
  // false when transitioning to kFailed. In that case, an exception is pending.
  bool AnalyzeOnMainThread();

  // Transition from kAnalyzed to kReadyToCompile (or kFailed). Returns
  // false when transitioning to kFailed. In that case, an exception is pending.
  bool PrepareToCompileOnMainThread();

  // Transition from kReadyToCompile to kCompiled.
  void Compile();

  // Transition from kCompiled to kDone (or kFailed). Returns false when
  // transitioning to kFailed. In that case, an exception is pending.
  bool FinalizeCompilingOnMainThread();

  // Transition from any state to kInitial and free all resources.
  void ResetOnMainThread();

  // Estimate how long the next step will take using the tracer.
  double EstimateRuntimeOfNextStepInMs() const;

  // Even though the name does not imply this, ShortPrint() must only be invoked
  // on the main thread.
  void ShortPrint();

 private:
  FRIEND_TEST(CompilerDispatcherJobTest, ScopeChain);

  CompileJobStatus status_;
  Isolate* isolate_;
  CompilerDispatcherTracer* tracer_;
  Handle<Context> context_;            // Global handle.
  Handle<SharedFunctionInfo> shared_;  // Global handle.
  Handle<String> source_;        // Global handle.
  Handle<String> wrapper_;       // Global handle.
  std::unique_ptr<v8::String::ExternalStringResourceBase> source_wrapper_;
  size_t max_stack_size_;
  CompileJobFinishCallback* finish_callback_ = nullptr;

  // Members required for parsing.
  std::unique_ptr<UnicodeCache> unicode_cache_;
  std::unique_ptr<Utf16CharacterStream> character_stream_;
  std::unique_ptr<ParseInfo> parse_info_;
  std::unique_ptr<Parser> parser_;

  // Members required for compiling a parsed function.
  std::shared_ptr<Zone> parse_zone_;

  // Members required for compiling.
  std::unique_ptr<CompilationInfo> compile_info_;
  std::unique_ptr<CompilationJob> compile_job_;

  bool trace_compiler_dispatcher_jobs_;

  DISALLOW_COPY_AND_ASSIGN(CompilerDispatcherJob);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_DISPATCHER_COMPILER_DISPATCHER_JOB_H_
