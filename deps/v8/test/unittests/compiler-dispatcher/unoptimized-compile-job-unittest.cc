// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "include/v8.h"
#include "src/api.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/base/platform/semaphore.h"
#include "src/compiler-dispatcher/compiler-dispatcher-job.h"
#include "src/compiler-dispatcher/compiler-dispatcher-tracer.h"
#include "src/compiler-dispatcher/unoptimized-compile-job.h"
#include "src/flags.h"
#include "src/isolate-inl.h"
#include "src/parsing/parse-info.h"
#include "src/v8.h"
#include "test/unittests/test-helpers.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

class UnoptimizedCompileJobTest : public TestWithNativeContext {
 public:
  UnoptimizedCompileJobTest() : tracer_(isolate()) {}
  ~UnoptimizedCompileJobTest() override {}

  CompilerDispatcherTracer* tracer() { return &tracer_; }

  static void SetUpTestCase() {
    CHECK_NULL(save_flags_);
    save_flags_ = new SaveFlags();
    TestWithNativeContext ::SetUpTestCase();
  }

  static void TearDownTestCase() {
    TestWithNativeContext ::TearDownTestCase();
    CHECK_NOT_NULL(save_flags_);
    delete save_flags_;
    save_flags_ = nullptr;
  }

  static Variable* LookupVariableByName(UnoptimizedCompileJob* job,
                                        const char* name) {
    const AstRawString* name_raw_string =
        job->parse_info_->ast_value_factory()->GetOneByteString(name);
    return job->parse_info_->literal()->scope()->Lookup(name_raw_string);
  }

 private:
  CompilerDispatcherTracer tracer_;
  static SaveFlags* save_flags_;

  DISALLOW_COPY_AND_ASSIGN(UnoptimizedCompileJobTest);
};

SaveFlags* UnoptimizedCompileJobTest::save_flags_ = nullptr;

#define ASSERT_JOB_STATUS(STATUS, JOB) ASSERT_EQ(STATUS, JOB->status())

TEST_F(UnoptimizedCompileJobTest, Construct) {
  std::unique_ptr<UnoptimizedCompileJob> job(new UnoptimizedCompileJob(
      isolate(), tracer(), test::CreateSharedFunctionInfo(isolate(), nullptr),
      FLAG_stack_size));
}

TEST_F(UnoptimizedCompileJobTest, StateTransitions) {
  std::unique_ptr<UnoptimizedCompileJob> job(new UnoptimizedCompileJob(
      isolate(), tracer(), test::CreateSharedFunctionInfo(isolate(), nullptr),
      FLAG_stack_size));

  ASSERT_JOB_STATUS(CompilerDispatcherJob::Status::kInitial, job);
  job->PrepareOnMainThread(isolate());
  ASSERT_FALSE(job->IsFailed());
  ASSERT_JOB_STATUS(CompilerDispatcherJob::Status::kPrepared, job);
  job->Compile(false);
  ASSERT_FALSE(job->IsFailed());
  ASSERT_JOB_STATUS(CompilerDispatcherJob::Status::kCompiled, job);
  job->FinalizeOnMainThread(isolate());
  ASSERT_FALSE(job->IsFailed());
  ASSERT_JOB_STATUS(CompilerDispatcherJob::Status::kDone, job);
  job->ResetOnMainThread(isolate());
  ASSERT_JOB_STATUS(CompilerDispatcherJob::Status::kInitial, job);
}

TEST_F(UnoptimizedCompileJobTest, SyntaxError) {
  test::ScriptResource script("^^^", strlen("^^^"));
  std::unique_ptr<UnoptimizedCompileJob> job(new UnoptimizedCompileJob(
      isolate(), tracer(), test::CreateSharedFunctionInfo(isolate(), &script),
      FLAG_stack_size));

  job->PrepareOnMainThread(isolate());
  ASSERT_FALSE(job->IsFailed());
  job->Compile(false);
  ASSERT_FALSE(job->IsFailed());
  job->ReportErrorsOnMainThread(isolate());
  ASSERT_TRUE(job->IsFailed());
  ASSERT_JOB_STATUS(CompilerDispatcherJob::Status::kFailed, job);
  ASSERT_TRUE(isolate()->has_pending_exception());

  isolate()->clear_pending_exception();

  job->ResetOnMainThread(isolate());
  ASSERT_JOB_STATUS(CompilerDispatcherJob::Status::kInitial, job);
}

TEST_F(UnoptimizedCompileJobTest, CompileAndRun) {
  const char script[] =
      "function g() {\n"
      "  f = function(a) {\n"
      "        for (var i = 0; i < 3; i++) { a += 20; }\n"
      "        return a;\n"
      "      }\n"
      "  return f;\n"
      "}\n"
      "g();";
  Handle<JSFunction> f = RunJS<JSFunction>(script);
  std::unique_ptr<UnoptimizedCompileJob> job(new UnoptimizedCompileJob(
      isolate(), tracer(), handle(f->shared()), FLAG_stack_size));

  job->PrepareOnMainThread(isolate());
  ASSERT_FALSE(job->IsFailed());
  job->Compile(false);
  ASSERT_FALSE(job->IsFailed());
  job->FinalizeOnMainThread(isolate());
  ASSERT_FALSE(job->IsFailed());
  ASSERT_JOB_STATUS(CompilerDispatcherJob::Status::kDone, job);

  Smi* value = Smi::cast(*RunJS("f(100);"));
  ASSERT_TRUE(value == Smi::FromInt(160));

  job->ResetOnMainThread(isolate());
  ASSERT_JOB_STATUS(CompilerDispatcherJob::Status::kInitial, job);
}

TEST_F(UnoptimizedCompileJobTest, CompileFailureToAnalyse) {
  std::string raw_script("() { var a = ");
  for (int i = 0; i < 500000; i++) {
    // TODO(leszeks): Figure out a more "unit-test-y" way of forcing an analysis
    // failure than a binop stack overflow.

    // Alternate + and - to avoid n-ary operation nodes.
    raw_script += "'x' + 'x' - ";
  }
  raw_script += " 'x'; }";
  test::ScriptResource script(raw_script.c_str(), strlen(raw_script.c_str()));
  std::unique_ptr<UnoptimizedCompileJob> job(new UnoptimizedCompileJob(
      isolate(), tracer(), test::CreateSharedFunctionInfo(isolate(), &script),
      100));

  job->PrepareOnMainThread(isolate());
  ASSERT_FALSE(job->IsFailed());
  job->Compile(false);
  ASSERT_FALSE(job->IsFailed());
  job->ReportErrorsOnMainThread(isolate());
  ASSERT_TRUE(job->IsFailed());
  ASSERT_JOB_STATUS(CompilerDispatcherJob::Status::kFailed, job);
  ASSERT_TRUE(isolate()->has_pending_exception());

  isolate()->clear_pending_exception();
  job->ResetOnMainThread(isolate());
  ASSERT_JOB_STATUS(CompilerDispatcherJob::Status::kInitial, job);
}

TEST_F(UnoptimizedCompileJobTest, CompileFailureToFinalize) {
  std::string raw_script("() { var a = ");
  for (int i = 0; i < 500; i++) {
    // Alternate + and - to avoid n-ary operation nodes.
    raw_script += "'x' + 'x' - ";
  }
  raw_script += " 'x'; }";
  test::ScriptResource script(raw_script.c_str(), strlen(raw_script.c_str()));
  std::unique_ptr<UnoptimizedCompileJob> job(new UnoptimizedCompileJob(
      isolate(), tracer(), test::CreateSharedFunctionInfo(isolate(), &script),
      50));

  job->PrepareOnMainThread(isolate());
  ASSERT_FALSE(job->IsFailed());
  job->Compile(false);
  ASSERT_FALSE(job->IsFailed());
  job->ReportErrorsOnMainThread(isolate());
  ASSERT_TRUE(job->IsFailed());
  ASSERT_JOB_STATUS(CompilerDispatcherJob::Status::kFailed, job);
  ASSERT_TRUE(isolate()->has_pending_exception());

  isolate()->clear_pending_exception();
  job->ResetOnMainThread(isolate());
  ASSERT_JOB_STATUS(CompilerDispatcherJob::Status::kInitial, job);
}

class CompileTask : public Task {
 public:
  CompileTask(UnoptimizedCompileJob* job, base::Semaphore* semaphore)
      : job_(job), semaphore_(semaphore) {}
  ~CompileTask() override {}

  void Run() override {
    job_->Compile(true);
    ASSERT_FALSE(job_->IsFailed());
    semaphore_->Signal();
  }

 private:
  UnoptimizedCompileJob* job_;
  base::Semaphore* semaphore_;
  DISALLOW_COPY_AND_ASSIGN(CompileTask);
};

TEST_F(UnoptimizedCompileJobTest, CompileOnBackgroundThread) {
  const char* raw_script =
      "(a, b) {\n"
      "  var c = a + b;\n"
      "  function bar() { return b }\n"
      "  var d = { foo: 100, bar : bar() }\n"
      "  return bar;"
      "}";
  test::ScriptResource script(raw_script, strlen(raw_script));
  std::unique_ptr<UnoptimizedCompileJob> job(new UnoptimizedCompileJob(
      isolate(), tracer(), test::CreateSharedFunctionInfo(isolate(), &script),
      100));

  job->PrepareOnMainThread(isolate());
  ASSERT_FALSE(job->IsFailed());

  base::Semaphore semaphore(0);
  CompileTask* background_task = new CompileTask(job.get(), &semaphore);
  ASSERT_JOB_STATUS(CompilerDispatcherJob::Status::kPrepared, job);
  V8::GetCurrentPlatform()->CallOnBackgroundThread(background_task,
                                                   Platform::kShortRunningTask);
  semaphore.Wait();
  job->FinalizeOnMainThread(isolate());
  ASSERT_FALSE(job->IsFailed());
  ASSERT_JOB_STATUS(CompilerDispatcherJob::Status::kDone, job);

  job->ResetOnMainThread(isolate());
  ASSERT_JOB_STATUS(CompilerDispatcherJob::Status::kInitial, job);
}

TEST_F(UnoptimizedCompileJobTest, LazyInnerFunctions) {
  const char script[] =
      "f = function() {\n"
      "  e = (function() { return 42; });\n"
      "  return e;\n"
      "};\n"
      "f;";
  Handle<JSFunction> f = RunJS<JSFunction>(script);

  std::unique_ptr<UnoptimizedCompileJob> job(new UnoptimizedCompileJob(
      isolate(), tracer(), handle(f->shared()), FLAG_stack_size));

  job->PrepareOnMainThread(isolate());
  ASSERT_FALSE(job->IsFailed());
  job->Compile(false);
  ASSERT_FALSE(job->IsFailed());
  job->FinalizeOnMainThread(isolate());
  ASSERT_FALSE(job->IsFailed());
  ASSERT_JOB_STATUS(CompilerDispatcherJob::Status::kDone, job);

  Handle<JSFunction> e = RunJS<JSFunction>("f();");

  ASSERT_FALSE(e->shared()->is_compiled());

  job->ResetOnMainThread(isolate());
  ASSERT_JOB_STATUS(CompilerDispatcherJob::Status::kInitial, job);
}

#undef ASSERT_JOB_STATUS

}  // namespace internal
}  // namespace v8
