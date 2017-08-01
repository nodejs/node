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
#include "src/flags.h"
#include "src/isolate-inl.h"
#include "src/parsing/parse-info.h"
#include "src/v8.h"
#include "test/unittests/test-helpers.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

class CompilerDispatcherJobTest : public TestWithContext {
 public:
  CompilerDispatcherJobTest() : tracer_(i_isolate()) {}
  ~CompilerDispatcherJobTest() override {}

  CompilerDispatcherTracer* tracer() { return &tracer_; }

  static void SetUpTestCase() {
    CHECK_NULL(save_flags_);
    save_flags_ = new SaveFlags();
    FLAG_ignition = true;
    TestWithContext::SetUpTestCase();
  }

  static void TearDownTestCase() {
    TestWithContext::TearDownTestCase();
    CHECK_NOT_NULL(save_flags_);
    delete save_flags_;
    save_flags_ = nullptr;
  }

 private:
  CompilerDispatcherTracer tracer_;
  static SaveFlags* save_flags_;

  DISALLOW_COPY_AND_ASSIGN(CompilerDispatcherJobTest);
};

SaveFlags* CompilerDispatcherJobTest::save_flags_ = nullptr;

namespace {

const char test_script[] = "(x) { x*x; }";

}  // namespace

TEST_F(CompilerDispatcherJobTest, Construct) {
  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      i_isolate(), tracer(),
      test::CreateSharedFunctionInfo(i_isolate(), nullptr), FLAG_stack_size));
}

TEST_F(CompilerDispatcherJobTest, ConstructWithoutSFI) {
  std::unique_ptr<test::FinishCallback> callback(new test::FinishCallback());
  std::unique_ptr<test::ScriptResource> resource(
      new test::ScriptResource(test_script, strlen(test_script)));
  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      tracer(), FLAG_stack_size,
      test::CreateSource(i_isolate(), resource.get()), 0,
      static_cast<int>(resource->length()), SLOPPY, 1, false, false, false,
      i_isolate()->heap()->HashSeed(), i_isolate()->allocator(),
      ScriptCompiler::kNoCompileOptions, i_isolate()->ast_string_constants(),
      callback.get()));
}

TEST_F(CompilerDispatcherJobTest, StateTransitions) {
  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      i_isolate(), tracer(),
      test::CreateSharedFunctionInfo(i_isolate(), nullptr), FLAG_stack_size));

  ASSERT_TRUE(job->status() == CompileJobStatus::kInitial);
  job->PrepareToParseOnMainThread();
  ASSERT_TRUE(job->status() == CompileJobStatus::kReadyToParse);
  job->Parse();
  ASSERT_TRUE(job->status() == CompileJobStatus::kParsed);
  ASSERT_TRUE(job->FinalizeParsingOnMainThread());
  ASSERT_TRUE(job->status() == CompileJobStatus::kReadyToAnalyze);
  ASSERT_TRUE(job->AnalyzeOnMainThread());
  ASSERT_TRUE(job->status() == CompileJobStatus::kAnalyzed);
  ASSERT_TRUE(job->PrepareToCompileOnMainThread());
  ASSERT_TRUE(job->status() == CompileJobStatus::kReadyToCompile);
  job->Compile();
  ASSERT_TRUE(job->status() == CompileJobStatus::kCompiled);
  ASSERT_TRUE(job->FinalizeCompilingOnMainThread());
  ASSERT_TRUE(job->status() == CompileJobStatus::kDone);
  job->ResetOnMainThread();
  ASSERT_TRUE(job->status() == CompileJobStatus::kInitial);
}

TEST_F(CompilerDispatcherJobTest, StateTransitionsParseWithCallback) {
  std::unique_ptr<test::FinishCallback> callback(new test::FinishCallback());
  std::unique_ptr<test::ScriptResource> resource(
      new test::ScriptResource(test_script, strlen(test_script)));
  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      tracer(), FLAG_stack_size,
      test::CreateSource(i_isolate(), resource.get()), 0,
      static_cast<int>(resource->length()), SLOPPY, 1, false, false, false,
      i_isolate()->heap()->HashSeed(), i_isolate()->allocator(),
      ScriptCompiler::kNoCompileOptions, i_isolate()->ast_string_constants(),
      callback.get()));
  ASSERT_TRUE(job->status() == CompileJobStatus::kReadyToParse);
  job->Parse();
  ASSERT_TRUE(job->status() == CompileJobStatus::kDone);
  job->ResetOnMainThread();
  ASSERT_TRUE(job->status() == CompileJobStatus::kInitial);
  ASSERT_TRUE(callback->result() != nullptr);
}

TEST_F(CompilerDispatcherJobTest, SyntaxError) {
  test::ScriptResource script("^^^", strlen("^^^"));
  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      i_isolate(), tracer(),
      test::CreateSharedFunctionInfo(i_isolate(), &script), FLAG_stack_size));

  job->PrepareToParseOnMainThread();
  job->Parse();
  ASSERT_FALSE(job->FinalizeParsingOnMainThread());
  ASSERT_TRUE(job->status() == CompileJobStatus::kFailed);
  ASSERT_TRUE(i_isolate()->has_pending_exception());

  i_isolate()->clear_pending_exception();

  job->ResetOnMainThread();
  ASSERT_TRUE(job->status() == CompileJobStatus::kInitial);
}

TEST_F(CompilerDispatcherJobTest, ScopeChain) {
  const char script[] =
      "function g() { var y = 1; function f(x) { return x * y }; return f; } "
      "g();";
  Handle<JSFunction> f =
      Handle<JSFunction>::cast(test::RunJS(isolate(), script));

  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      i_isolate(), tracer(), handle(f->shared()), FLAG_stack_size));

  job->PrepareToParseOnMainThread();
  job->Parse();
  ASSERT_TRUE(job->FinalizeParsingOnMainThread());
  ASSERT_TRUE(job->AnalyzeOnMainThread());
  ASSERT_TRUE(job->PrepareToCompileOnMainThread());
  ASSERT_TRUE(job->status() == CompileJobStatus::kReadyToCompile);

  const AstRawString* var_x =
      job->parse_info_->ast_value_factory()->GetOneByteString("x");
  Variable* var = job->parse_info_->literal()->scope()->Lookup(var_x);
  ASSERT_TRUE(var);
  ASSERT_TRUE(var->IsParameter());

  const AstRawString* var_y =
      job->parse_info_->ast_value_factory()->GetOneByteString("y");
  var = job->parse_info_->literal()->scope()->Lookup(var_y);
  ASSERT_TRUE(var);
  ASSERT_TRUE(var->IsContextSlot());

  job->ResetOnMainThread();
  ASSERT_TRUE(job->status() == CompileJobStatus::kInitial);
}

TEST_F(CompilerDispatcherJobTest, CompileAndRun) {
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
  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      i_isolate(), tracer(), handle(f->shared()), FLAG_stack_size));

  job->PrepareToParseOnMainThread();
  job->Parse();
  job->FinalizeParsingOnMainThread();
  job->AnalyzeOnMainThread();
  job->PrepareToCompileOnMainThread();
  job->Compile();
  ASSERT_TRUE(job->FinalizeCompilingOnMainThread());
  ASSERT_TRUE(job->status() == CompileJobStatus::kDone);

  Smi* value = Smi::cast(*test::RunJS(isolate(), "f(100);"));
  ASSERT_TRUE(value == Smi::FromInt(160));

  job->ResetOnMainThread();
  ASSERT_TRUE(job->status() == CompileJobStatus::kInitial);
}

TEST_F(CompilerDispatcherJobTest, CompileFailureToAnalyse) {
  std::string raw_script("() { var a = ");
  for (int i = 0; i < 100000; i++) {
    raw_script += "'x' + ";
  }
  raw_script += " 'x'; }";
  test::ScriptResource script(raw_script.c_str(), strlen(raw_script.c_str()));
  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      i_isolate(), tracer(),
      test::CreateSharedFunctionInfo(i_isolate(), &script), 100));

  job->PrepareToParseOnMainThread();
  job->Parse();
  job->FinalizeParsingOnMainThread();
  ASSERT_FALSE(job->AnalyzeOnMainThread());
  ASSERT_TRUE(job->status() == CompileJobStatus::kFailed);
  ASSERT_TRUE(i_isolate()->has_pending_exception());

  i_isolate()->clear_pending_exception();
  job->ResetOnMainThread();
  ASSERT_TRUE(job->status() == CompileJobStatus::kInitial);
}

TEST_F(CompilerDispatcherJobTest, CompileFailureToFinalize) {
  std::string raw_script("() { var a = ");
  for (int i = 0; i < 1000; i++) {
    raw_script += "'x' + ";
  }
  raw_script += " 'x'; }";
  test::ScriptResource script(raw_script.c_str(), strlen(raw_script.c_str()));
  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      i_isolate(), tracer(),
      test::CreateSharedFunctionInfo(i_isolate(), &script), 50));

  job->PrepareToParseOnMainThread();
  job->Parse();
  job->FinalizeParsingOnMainThread();
  job->AnalyzeOnMainThread();
  job->PrepareToCompileOnMainThread();
  job->Compile();
  ASSERT_FALSE(job->FinalizeCompilingOnMainThread());
  ASSERT_TRUE(job->status() == CompileJobStatus::kFailed);
  ASSERT_TRUE(i_isolate()->has_pending_exception());

  i_isolate()->clear_pending_exception();
  job->ResetOnMainThread();
  ASSERT_TRUE(job->status() == CompileJobStatus::kInitial);
}

class CompileTask : public Task {
 public:
  CompileTask(CompilerDispatcherJob* job, base::Semaphore* semaphore)
      : job_(job), semaphore_(semaphore) {}
  ~CompileTask() override {}

  void Run() override {
    job_->Compile();
    semaphore_->Signal();
  }

 private:
  CompilerDispatcherJob* job_;
  base::Semaphore* semaphore_;
  DISALLOW_COPY_AND_ASSIGN(CompileTask);
};

TEST_F(CompilerDispatcherJobTest, CompileOnBackgroundThread) {
  const char* raw_script =
      "(a, b) {\n"
      "  var c = a + b;\n"
      "  function bar() { return b }\n"
      "  var d = { foo: 100, bar : bar() }\n"
      "  return bar;"
      "}";
  test::ScriptResource script(raw_script, strlen(raw_script));
  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      i_isolate(), tracer(),
      test::CreateSharedFunctionInfo(i_isolate(), &script), 100));

  job->PrepareToParseOnMainThread();
  job->Parse();
  job->FinalizeParsingOnMainThread();
  job->AnalyzeOnMainThread();
  job->PrepareToCompileOnMainThread();

  base::Semaphore semaphore(0);
  CompileTask* background_task = new CompileTask(job.get(), &semaphore);
  V8::GetCurrentPlatform()->CallOnBackgroundThread(background_task,
                                                   Platform::kShortRunningTask);
  semaphore.Wait();
  ASSERT_TRUE(job->FinalizeCompilingOnMainThread());
  ASSERT_TRUE(job->status() == CompileJobStatus::kDone);

  job->ResetOnMainThread();
  ASSERT_TRUE(job->status() == CompileJobStatus::kInitial);
}

TEST_F(CompilerDispatcherJobTest, LazyInnerFunctions) {
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

  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      i_isolate(), tracer(), handle(f->shared()), FLAG_stack_size));

  job->PrepareToParseOnMainThread();
  job->Parse();
  ASSERT_TRUE(job->FinalizeParsingOnMainThread());
  ASSERT_TRUE(job->AnalyzeOnMainThread());
  ASSERT_TRUE(job->PrepareToCompileOnMainThread());
  job->Compile();
  ASSERT_TRUE(job->FinalizeCompilingOnMainThread());
  ASSERT_TRUE(job->status() == CompileJobStatus::kDone);

  Handle<JSFunction> e =
      Handle<JSFunction>::cast(test::RunJS(isolate(), "f();"));

  ASSERT_FALSE(e->shared()->HasBaselineCode());

  job->ResetOnMainThread();
  ASSERT_TRUE(job->status() == CompileJobStatus::kInitial);
}

}  // namespace internal
}  // namespace v8
