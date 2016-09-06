// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "include/v8.h"
#include "src/api.h"
#include "src/ast/scopes.h"
#include "src/compiler-dispatcher/compiler-dispatcher-job.h"
#include "src/flags.h"
#include "src/isolate-inl.h"
#include "src/parsing/parse-info.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

typedef TestWithContext CompilerDispatcherJobTest;

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

Handle<JSFunction> CreateFunction(
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
      isolate->factory()->NewStringFromAsciiChecked("f"), MaybeHandle<Code>(),
      false);
  SharedFunctionInfo::SetScript(shared, script);
  shared->set_end_position(source->length());
  Handle<JSFunction> function =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          shared, handle(isolate->context(), isolate));
  return scope.CloseAndEscape(function);
}

}  // namespace

TEST_F(CompilerDispatcherJobTest, Construct) {
  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      i_isolate(), CreateFunction(i_isolate(), nullptr), FLAG_stack_size));
}

TEST_F(CompilerDispatcherJobTest, CanParseOnBackgroundThread) {
  {
    std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
        i_isolate(), CreateFunction(i_isolate(), nullptr), FLAG_stack_size));
    ASSERT_FALSE(job->can_parse_on_background_thread());
  }
  {
    ScriptResource script(test_script, strlen(test_script));
    std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
        i_isolate(), CreateFunction(i_isolate(), &script), FLAG_stack_size));
    ASSERT_TRUE(job->can_parse_on_background_thread());
  }
}

TEST_F(CompilerDispatcherJobTest, StateTransitions) {
  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      i_isolate(), CreateFunction(i_isolate(), nullptr), FLAG_stack_size));

  ASSERT_TRUE(job->status() == CompileJobStatus::kInitial);
  job->PrepareToParseOnMainThread();
  ASSERT_TRUE(job->status() == CompileJobStatus::kReadyToParse);
  job->Parse();
  ASSERT_TRUE(job->status() == CompileJobStatus::kParsed);
  ASSERT_TRUE(job->FinalizeParsingOnMainThread());
  ASSERT_TRUE(job->status() == CompileJobStatus::kReadyToCompile);
  job->ResetOnMainThread();
  ASSERT_TRUE(job->status() == CompileJobStatus::kInitial);
}

TEST_F(CompilerDispatcherJobTest, SyntaxError) {
  ScriptResource script("^^^", strlen("^^^"));
  std::unique_ptr<CompilerDispatcherJob> job(new CompilerDispatcherJob(
      i_isolate(), CreateFunction(i_isolate(), &script), FLAG_stack_size));

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
      "function g() { var g = 1; function f(x) { return x * g }; return f; } "
      "g();";
  Handle<JSFunction> f = Handle<JSFunction>::cast(Utils::OpenHandle(
      *v8::Script::Compile(isolate()->GetCurrentContext(),
                           v8::String::NewFromUtf8(isolate(), script,
                                                   v8::NewStringType::kNormal)
                               .ToLocalChecked())
           .ToLocalChecked()
           ->Run(isolate()->GetCurrentContext())
           .ToLocalChecked()));

  std::unique_ptr<CompilerDispatcherJob> job(
      new CompilerDispatcherJob(i_isolate(), f, FLAG_stack_size));

  job->PrepareToParseOnMainThread();
  job->Parse();
  ASSERT_TRUE(job->FinalizeParsingOnMainThread());
  ASSERT_TRUE(job->status() == CompileJobStatus::kReadyToCompile);

  const AstRawString* var_x =
      job->parse_info_->ast_value_factory()->GetOneByteString("x");
  Variable* var = job->parse_info_->literal()->scope()->Lookup(var_x);
  ASSERT_TRUE(var);
  ASSERT_TRUE(var->IsUnallocated());

  const AstRawString* var_g =
      job->parse_info_->ast_value_factory()->GetOneByteString("g");
  var = job->parse_info_->literal()->scope()->Lookup(var_g);
  ASSERT_TRUE(var);
  ASSERT_TRUE(var->IsContextSlot());

  job->ResetOnMainThread();
  ASSERT_TRUE(job->status() == CompileJobStatus::kInitial);
}

}  // namespace internal
}  // namespace v8
