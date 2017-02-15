// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_DISPATCHER_COMPILER_DISPATCHER_JOB_H_
#define V8_COMPILER_DISPATCHER_COMPILER_DISPATCHER_JOB_H_

#include <memory>

#include "src/base/macros.h"
#include "src/handles.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace v8 {
namespace internal {

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

class CompilerDispatcherJob {
 public:
  CompilerDispatcherJob(Isolate* isolate, Handle<SharedFunctionInfo> shared,
                        size_t max_stack_size);
  ~CompilerDispatcherJob();

  CompileJobStatus status() const { return status_; }
  bool can_parse_on_background_thread() const {
    return can_parse_on_background_thread_;
  }
  // Should only be called after kReadyToCompile.
  bool can_compile_on_background_thread() const {
    DCHECK(compile_job_.get());
    return can_compile_on_background_thread_;
  }

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

 private:
  FRIEND_TEST(CompilerDispatcherJobTest, ScopeChain);

  CompileJobStatus status_ = CompileJobStatus::kInitial;
  Isolate* isolate_;
  Handle<SharedFunctionInfo> shared_;  // Global handle.
  Handle<String> source_;        // Global handle.
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

  bool can_parse_on_background_thread_;
  bool can_compile_on_background_thread_;

  DISALLOW_COPY_AND_ASSIGN(CompilerDispatcherJob);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_DISPATCHER_COMPILER_DISPATCHER_JOB_H_
