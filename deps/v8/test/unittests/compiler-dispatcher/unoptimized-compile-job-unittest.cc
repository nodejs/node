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

class UnoptimizedCompileJobTest : public TestWithContext {
 public:
  UnoptimizedCompileJobTest() : tracer_(i_isolate()) {}
  ~UnoptimizedCompileJobTest() override {}

  CompilerDispatcherTracer* tracer() { return &tracer_; }

  static void SetUpTestCase() {
    CHECK_NULL(save_flags_);
    save_flags_ = new SaveFlags();
    TestWithContext::SetUpTestCase();
  }

  static void TearDownTestCase() {
    TestWithContext::TearDownTestCase();
    CHECK_NOT_NULL(save_flags_);
    delete save_flags_;
    save_flags_ = nullptr;
  }

  static UnoptimizedCompileJob::Status GetStatus(UnoptimizedCompileJob* job) {
    return job->status();
  }

  static UnoptimizedCompileJob::Status GetStatus(
      const std::unique_ptr<UnoptimizedCompileJob>& job) {
    return GetStatus(job.get());
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

#define ASSERT_JOB_STATUS(STATUS, JOB) ASSERT_EQ(STATUS, GetStatus(JOB))

TEST_F(UnoptimizedCompileJobTest, Construct) {
  std::unique_ptr<UnoptimizedCompileJob> job(new UnoptimizedCompileJob(
      i_isolate(), tracer(),
      test::CreateSharedFunctionInfo(i_isolate(), nullptr), FLAG_stack_size));
}

TEST_F(UnoptimizedCompileJobTest, StateTransitions) {
  std::unique_ptr<UnoptimizedCompileJob> job(new UnoptimizedCompileJob(
      i_isolate(), tracer(),
      test::CreateSharedFunctionInfo(i_isolate(), nullptr), FLAG_stack_size));

  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kInitial, job);
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kReadyToParse, job);
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kParsed, job);
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kReadyToAnalyze, job);
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kAnalyzed, job);
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kReadyToCompile, job);
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kCompiled, job);
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kDone, job);
  job->ResetOnMainThread(i_isolate());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kInitial, job);
}

TEST_F(UnoptimizedCompileJobTest, SyntaxError) {
  test::ScriptResource script("^^^", strlen("^^^"));
  std::unique_ptr<UnoptimizedCompileJob> job(new UnoptimizedCompileJob(
      i_isolate(), tracer(),
      test::CreateSharedFunctionInfo(i_isolate(), &script), FLAG_stack_size));

  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_TRUE(job->IsFailed());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kFailed, job);
  ASSERT_TRUE(i_isolate()->has_pending_exception());

  i_isolate()->clear_pending_exception();

  job->ResetOnMainThread(i_isolate());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kInitial, job);
}

TEST_F(UnoptimizedCompileJobTest, ScopeChain) {
  const char script[] =
      "function g() { var y = 1; function f(x) { return x * y }; return f; } "
      "g();";
  Handle<JSFunction> f =
      Handle<JSFunction>::cast(test::RunJS(isolate(), script));

  std::unique_ptr<UnoptimizedCompileJob> job(new UnoptimizedCompileJob(
      i_isolate(), tracer(), handle(f->shared()), FLAG_stack_size));

  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kReadyToCompile, job);

  Variable* var = LookupVariableByName(job.get(), "x");
  ASSERT_TRUE(var);
  ASSERT_TRUE(var->IsParameter());

  var = LookupVariableByName(job.get(), "y");
  ASSERT_TRUE(var);
  ASSERT_TRUE(var->IsContextSlot());

  job->ResetOnMainThread(i_isolate());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kInitial, job);
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
  Handle<JSFunction> f =
      Handle<JSFunction>::cast(test::RunJS(isolate(), script));
  std::unique_ptr<UnoptimizedCompileJob> job(new UnoptimizedCompileJob(
      i_isolate(), tracer(), handle(f->shared()), FLAG_stack_size));

  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kDone, job);

  Smi* value = Smi::cast(*test::RunJS(isolate(), "f(100);"));
  ASSERT_TRUE(value == Smi::FromInt(160));

  job->ResetOnMainThread(i_isolate());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kInitial, job);
}

TEST_F(UnoptimizedCompileJobTest, CompileFailureToAnalyse) {
  std::string raw_script("() { var a = ");
  for (int i = 0; i < 100000; i++) {
    raw_script += "'x' + ";
  }
  raw_script += " 'x'; }";
  test::ScriptResource script(raw_script.c_str(), strlen(raw_script.c_str()));
  std::unique_ptr<UnoptimizedCompileJob> job(new UnoptimizedCompileJob(
      i_isolate(), tracer(),
      test::CreateSharedFunctionInfo(i_isolate(), &script), 100));

  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_TRUE(job->IsFailed());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kFailed, job);
  ASSERT_TRUE(i_isolate()->has_pending_exception());

  i_isolate()->clear_pending_exception();
  job->ResetOnMainThread(i_isolate());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kInitial, job);
}

TEST_F(UnoptimizedCompileJobTest, CompileFailureToFinalize) {
  std::string raw_script("() { var a = ");
  for (int i = 0; i < 1000; i++) {
    raw_script += "'x' + ";
  }
  raw_script += " 'x'; }";
  test::ScriptResource script(raw_script.c_str(), strlen(raw_script.c_str()));
  std::unique_ptr<UnoptimizedCompileJob> job(new UnoptimizedCompileJob(
      i_isolate(), tracer(),
      test::CreateSharedFunctionInfo(i_isolate(), &script), 50));

  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_TRUE(job->IsFailed());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kFailed, job);
  ASSERT_TRUE(i_isolate()->has_pending_exception());

  i_isolate()->clear_pending_exception();
  job->ResetOnMainThread(i_isolate());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kInitial, job);
}

class CompileTask : public Task {
 public:
  CompileTask(UnoptimizedCompileJob* job, base::Semaphore* semaphore)
      : job_(job), semaphore_(semaphore) {}
  ~CompileTask() override {}

  void Run() override {
    job_->StepNextOnBackgroundThread();
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
      i_isolate(), tracer(),
      test::CreateSharedFunctionInfo(i_isolate(), &script), 100));

  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());

  base::Semaphore semaphore(0);
  CompileTask* background_task = new CompileTask(job.get(), &semaphore);
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kReadyToCompile, job);
  V8::GetCurrentPlatform()->CallOnBackgroundThread(background_task,
                                                   Platform::kShortRunningTask);
  semaphore.Wait();
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kDone, job);

  job->ResetOnMainThread(i_isolate());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kInitial, job);
}

TEST_F(UnoptimizedCompileJobTest, LazyInnerFunctions) {
  const char script[] =
      "function g() {\n"
      "  f = function() {\n"
      "    e = (function() { return 42; });\n"
      "    return e;\n"
      "  };\n"
      "  return f;\n"
      "}\n"
      "g();";
  Handle<JSFunction> f =
      Handle<JSFunction>::cast(test::RunJS(isolate(), script));

  std::unique_ptr<UnoptimizedCompileJob> job(new UnoptimizedCompileJob(
      i_isolate(), tracer(), handle(f->shared()), FLAG_stack_size));

  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  job->StepNextOnMainThread(i_isolate());
  ASSERT_FALSE(job->IsFailed());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kDone, job);

  Handle<JSFunction> e =
      Handle<JSFunction>::cast(test::RunJS(isolate(), "f();"));

  ASSERT_FALSE(e->shared()->HasBaselineCode());

  job->ResetOnMainThread(i_isolate());
  ASSERT_JOB_STATUS(UnoptimizedCompileJob::Status::kInitial, job);
}

}  // namespace internal
}  // namespace v8
