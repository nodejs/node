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
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace v8 {
namespace internal {

class CompilerDispatcherTracer;
class CompilationInfo;
class CompilationJob;
class Isolate;
class ParseInfo;
class Parser;
class SharedFunctionInfo;
class String;
class UnicodeCache;
class Utf16CharacterStream;
class Zone;

enum class CompileJobStatus {
  kInitial,
  kReadyToParse,
  kParsed,
  kReadyToAnalyse,
  kReadyToCompile,
  kCompiled,
  kFailed,
  kDone,
};

class V8_EXPORT_PRIVATE CompilerDispatcherJob {
 public:
  CompilerDispatcherJob(Isolate* isolate, CompilerDispatcherTracer* tracer,
                        Handle<SharedFunctionInfo> shared,
                        size_t max_stack_size);
  ~CompilerDispatcherJob();

  CompileJobStatus status() const { return status_; }

  // Returns true if this CompilerDispatcherJob was created for the given
  // function.
  bool IsAssociatedWith(Handle<SharedFunctionInfo> shared) const;

  // Transition from kInitial to kReadyToParse.
  void PrepareToParseOnMainThread();

  // Transition from kReadyToParse to kParsed.
  void Parse();

  // Transition from kParsed to kReadyToAnalyse (or kFailed). Returns false
  // when transitioning to kFailed. In that case, an exception is pending.
  bool FinalizeParsingOnMainThread();

  // Transition from kReadyToAnalyse to kReadyToCompile (or kFailed). Returns
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

  CompileJobStatus status_ = CompileJobStatus::kInitial;
  Isolate* isolate_;
  CompilerDispatcherTracer* tracer_;
  Handle<SharedFunctionInfo> shared_;  // Global handle.
  Handle<String> source_;        // Global handle.
  Handle<String> wrapper_;       // Global handle.
  std::unique_ptr<v8::String::ExternalStringResourceBase> source_wrapper_;
  size_t max_stack_size_;

  // Members required for parsing.
  std::unique_ptr<UnicodeCache> unicode_cache_;
  std::unique_ptr<Zone> zone_;
  std::unique_ptr<Utf16CharacterStream> character_stream_;
  std::unique_ptr<ParseInfo> parse_info_;
  std::unique_ptr<Parser> parser_;
  std::unique_ptr<DeferredHandles> handles_from_parsing_;

  // Members required for compiling.
  std::unique_ptr<CompilationInfo> compile_info_;
  std::unique_ptr<CompilationJob> compile_job_;

  bool trace_compiler_dispatcher_jobs_;

  DISALLOW_COPY_AND_ASSIGN(CompilerDispatcherJob);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_DISPATCHER_COMPILER_DISPATCHER_JOB_H_
