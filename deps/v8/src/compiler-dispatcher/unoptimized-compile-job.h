// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_DISPATCHER_UNOPTIMIZED_COMPILE_JOB_H_
#define V8_COMPILER_DISPATCHER_UNOPTIMIZED_COMPILE_JOB_H_

#include <memory>

#include "include/v8.h"
#include "src/base/macros.h"
#include "src/compiler-dispatcher/compiler-dispatcher-job.h"
#include "src/globals.h"

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

class V8_EXPORT_PRIVATE UnoptimizedCompileJob : public CompilerDispatcherJob {
 public:
  // Creates a UnoptimizedCompileJob in the initial state.
  UnoptimizedCompileJob(Isolate* isolate, CompilerDispatcherTracer* tracer,
                        Handle<SharedFunctionInfo> shared,
                        size_t max_stack_size);
  ~UnoptimizedCompileJob() override;

  Handle<SharedFunctionInfo> shared() const { return shared_; }

  // Returns true if this UnoptimizedCompileJob was created for the given
  // function.
  bool IsAssociatedWith(Handle<SharedFunctionInfo> shared) const;

  // CompilerDispatcherJob implementation.
  void PrepareOnMainThread(Isolate* isolate) override;
  void Compile(bool on_background_thread) override;
  void FinalizeOnMainThread(Isolate* isolate) override;
  void ReportErrorsOnMainThread(Isolate* isolate) override;
  void ResetOnMainThread(Isolate* isolate) override;
  double EstimateRuntimeOfNextStepInMs() const override;
  void ShortPrintOnMainThread() override;

 private:
  friend class CompilerDispatcherTest;
  friend class UnoptimizedCompileJobTest;

  void ResetDataOnMainThread(Isolate* isolate);

  Context* context() { return *context_; }

  int main_thread_id_;
  CompilerDispatcherTracer* tracer_;
  AccountingAllocator* allocator_;
  Handle<Context> context_;            // Global handle.
  Handle<SharedFunctionInfo> shared_;  // Global handle.
  Handle<String> source_;              // Global handle.
  Handle<String> wrapper_;             // Global handle.
  std::unique_ptr<v8::String::ExternalStringResourceBase> source_wrapper_;
  size_t max_stack_size_;

  // Members required for parsing.
  std::unique_ptr<UnicodeCache> unicode_cache_;
  std::unique_ptr<ParseInfo> parse_info_;
  std::unique_ptr<Parser> parser_;

  // Members required for compiling.
  std::unique_ptr<CompilationJob> compilation_job_;

  bool trace_compiler_dispatcher_jobs_;

  DISALLOW_COPY_AND_ASSIGN(UnoptimizedCompileJob);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_DISPATCHER_UNOPTIMIZED_COMPILE_JOB_H_
