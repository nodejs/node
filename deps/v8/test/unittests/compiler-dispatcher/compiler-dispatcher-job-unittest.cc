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
#include "src/flags.h"
#include "src/isolate-inl.h"
#include "src/parsing/parse-info.h"
#include "src/v8.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

typedef TestWithContext CompilerDispatcherJobTest;

class IgnitionCompilerDispatcherJobTest : public TestWithContext {
 public:
  IgnitionCompilerDispatcherJobTest() {}
  ~IgnitionCompilerDispatcherJobTest() override {}

  static void SetUpTestCase() {
    old_flag_ = i::FLAG_ignition;
    i::FLAG_ignition = true;
    i::FLAG_never_compact = true;
    TestWithContext::SetUpTestCase();
  }

  static void TearDownTestCase() {
    TestWithContext::TearDownTestCase();
    i::FLAG_ignition = old_flag_;
  }

 private:
  static bool old_flag_;
  DISALLOW_COPY_AND_ASSIGN(IgnitionCompilerDispatcherJobTest);
};

bool IgnitionCompilerDispatcherJobTest::old_flag_;

namespace {

const char test_script[] = "(x) { x*x; }";

class ScriptResource : public v8::String::ExternalOneByteStringResource {
 public:
  ScriptResource(const char* data, size_t length)
      : data_(data), length_(length) {}
  ~ScriptResource() override = default;

  const char* data() const override { return data_; }
  size_t length() const override { return length_; }

 private:
  const char* data_;
  size_t length_;

  DISALLOW_COPY_AND_ASSIGN(ScriptResource);
};

Handle<SharedFunctionInfo> CreateSharedFunctionInfo(
    Isolate* isolate, ExternalOneByteString::Resource* maybe_resource) {
  HandleScope scope(isolate);
  Handle<String> source;
  if (maybe_resource) {
    source = isolate->factory()
                 ->NewExternalStringFromOneByte(maybe_resource)
                 .ToHandleChecked();
  } else {
    source = isolate->factory()->NewStringFromAsciiChecked(test_script);
  }
  Handle<Script> script = isolate->factory()->NewScript(source);
  Handle<SharedFunctionInfo> shared = isolate->factory()->NewSharedFunctionInfo(
      isolate->factory()->NewStringFromAsciiChecked("f"),
      isolate->builtins()->CompileLazy(), false);
  SharedFunctionInfo::SetScript(shared, script);
  shared->set_end_position(source->length());
  shared->set_outer_scope_info(ScopeInfo::Empty(isolate));
  return scope.CloseAndEscape(shared);
}

Handle<Object> RunJS(v8::Isolate* isolate, const char* script) {
  return Utils::OpenHandle(
      *v8::Script::Compile(
           isolate->GetCurrentContext(),
           v8::String::NewFromUtf8(isolate, script, v8::NewStringType::kNormal)
               .ToLocalChecked())
           .ToLocalChecked()
           ->Run(isolate->GetCurrentContext())
           .ToLocalChecked());
}

}  // namespace

TEST_F(CompilerDispatcherJobTest, Construct) {
  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      i_isolate(), CreateSharedFunctionInfo(i_isolate(), nullptr),
      FLAG_stack_size));
}

TEST_F(CompilerDispatcherJobTest, CanParseOnBackgroundThread) {
  {
    std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
        i_isolate(), CreateSharedFunctionInfo(i_isolate(), nullptr),
        FLAG_stack_size));
    ASSERT_FALSE(job->can_parse_on_background_thread());
  }
  {
    ScriptResource script(test_script, strlen(test_script));
    std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
        i_isolate(), CreateSharedFunctionInfo(i_isolate(), &script),
        FLAG_stack_size));
    ASSERT_TRUE(job->can_parse_on_background_thread());
  }
}

TEST_F(CompilerDispatcherJobTest, StateTransitions) {
  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      i_isolate(), CreateSharedFunctionInfo(i_isolate(), nullptr),
      FLAG_stack_size));

  ASSERT_TRUE(job->status() == CompileJobStatus::kInitial);
  job->PrepareToParseOnMainThread();
  ASSERT_TRUE(job->status() == CompileJobStatus::kReadyToParse);
  job->Parse();
  ASSERT_TRUE(job->status() == CompileJobStatus::kParsed);
  ASSERT_TRUE(job->FinalizeParsingOnMainThread());
  ASSERT_TRUE(job->status() == CompileJobStatus::kReadyToAnalyse);
  ASSERT_TRUE(job->PrepareToCompileOnMainThread());
  ASSERT_TRUE(job->status() == CompileJobStatus::kReadyToCompile);
  job->Compile();
  ASSERT_TRUE(job->status() == CompileJobStatus::kCompiled);
  ASSERT_TRUE(job->FinalizeCompilingOnMainThread());
  ASSERT_TRUE(job->status() == CompileJobStatus::kDone);
  job->ResetOnMainThread();
  ASSERT_TRUE(job->status() == CompileJobStatus::kInitial);
}

TEST_F(CompilerDispatcherJobTest, SyntaxError) {
  ScriptResource script("^^^", strlen("^^^"));
  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      i_isolate(), CreateSharedFunctionInfo(i_isolate(), &script),
      FLAG_stack_size));

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
  Handle<JSFunction> f = Handle<JSFunction>::cast(RunJS(isolate(), script));

  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      i_isolate(), handle(f->shared()), FLAG_stack_size));

  job->PrepareToParseOnMainThread();
  job->Parse();
  ASSERT_TRUE(job->FinalizeParsingOnMainThread());
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
  Handle<JSFunction> f = Handle<JSFunction>::cast(RunJS(isolate(), script));
  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      i_isolate(), handle(f->shared()), FLAG_stack_size));

  job->PrepareToParseOnMainThread();
  job->Parse();
  job->FinalizeParsingOnMainThread();
  job->PrepareToCompileOnMainThread();
  job->Compile();
  ASSERT_TRUE(job->FinalizeCompilingOnMainThread());
  ASSERT_TRUE(job->status() == CompileJobStatus::kDone);

  Smi* value = Smi::cast(*RunJS(isolate(), "f(100);"));
  ASSERT_TRUE(value == Smi::FromInt(160));

  job->ResetOnMainThread();
  ASSERT_TRUE(job->status() == CompileJobStatus::kInitial);
}

TEST_F(CompilerDispatcherJobTest, CompileFailureToPrepare) {
  std::string raw_script("() { var a = ");
  for (int i = 0; i < 100000; i++) {
    raw_script += "'x' + ";
  }
  raw_script += " 'x'; }";
  ScriptResource script(raw_script.c_str(), strlen(raw_script.c_str()));
  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      i_isolate(), CreateSharedFunctionInfo(i_isolate(), &script), 100));

  job->PrepareToParseOnMainThread();
  job->Parse();
  job->FinalizeParsingOnMainThread();
  ASSERT_FALSE(job->PrepareToCompileOnMainThread());
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
  ScriptResource script(raw_script.c_str(), strlen(raw_script.c_str()));
  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      i_isolate(), CreateSharedFunctionInfo(i_isolate(), &script), 50));

  job->PrepareToParseOnMainThread();
  job->Parse();
  job->FinalizeParsingOnMainThread();
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

TEST_F(IgnitionCompilerDispatcherJobTest, CompileOnBackgroundThread) {
  const char* raw_script =
      "(a, b) {\n"
      "  var c = a + b;\n"
      "  function bar() { return b }\n"
      "  var d = { foo: 100, bar : bar() }\n"
      "  return bar;"
      "}";
  ScriptResource script(raw_script, strlen(raw_script));
  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      i_isolate(), CreateSharedFunctionInfo(i_isolate(), &script), 100));

  job->PrepareToParseOnMainThread();
  job->Parse();
  job->FinalizeParsingOnMainThread();
  job->PrepareToCompileOnMainThread();
  ASSERT_TRUE(job->can_compile_on_background_thread());

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

}  // namespace internal
}  // namespace v8
