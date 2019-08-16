// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "include/v8.h"
#include "src/api/api-inl.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/base/platform/semaphore.h"
#include "src/base/template-utils.h"
#include "src/codegen/compiler.h"
#include "src/execution/isolate-inl.h"
#include "src/flags/flags.h"
#include "src/init/v8.h"
#include "src/objects/smi.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parser.h"
#include "src/parsing/preparse-data.h"
#include "src/zone/zone-list-inl.h"
#include "test/unittests/test-helpers.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

class BackgroundCompileTaskTest : public TestWithNativeContext {
 public:
  BackgroundCompileTaskTest() : allocator_(isolate()->allocator()) {}
  ~BackgroundCompileTaskTest() override = default;

  AccountingAllocator* allocator() { return allocator_; }

  static void SetUpTestCase() {
    CHECK_NULL(save_flags_);
    save_flags_ = new SaveFlags();
    TestWithNativeContext::SetUpTestCase();
  }

  static void TearDownTestCase() {
    TestWithNativeContext::TearDownTestCase();
    CHECK_NOT_NULL(save_flags_);
    delete save_flags_;
    save_flags_ = nullptr;
  }

  BackgroundCompileTask* NewBackgroundCompileTask(
      Isolate* isolate, Handle<SharedFunctionInfo> shared,
      size_t stack_size = FLAG_stack_size) {
    std::unique_ptr<ParseInfo> outer_parse_info =
        test::OuterParseInfoForShared(isolate, shared);
    AstValueFactory* ast_value_factory =
        outer_parse_info->GetOrCreateAstValueFactory();
    AstNodeFactory ast_node_factory(ast_value_factory,
                                    outer_parse_info->zone());

    const AstRawString* function_name =
        ast_value_factory->GetOneByteString("f");
    DeclarationScope* script_scope = new (outer_parse_info->zone())
        DeclarationScope(outer_parse_info->zone(), ast_value_factory);
    DeclarationScope* function_scope =
        new (outer_parse_info->zone()) DeclarationScope(
            outer_parse_info->zone(), script_scope, FUNCTION_SCOPE);
    function_scope->set_start_position(shared->StartPosition());
    function_scope->set_end_position(shared->EndPosition());
    std::vector<void*> buffer;
    ScopedPtrList<Statement> statements(&buffer);
    const FunctionLiteral* function_literal =
        ast_node_factory.NewFunctionLiteral(
            function_name, function_scope, statements, -1, -1, -1,
            FunctionLiteral::kNoDuplicateParameters,
            FunctionLiteral::kAnonymousExpression,
            FunctionLiteral::kShouldEagerCompile, shared->StartPosition(), true,
            shared->function_literal_id(), nullptr);

    return new BackgroundCompileTask(
        allocator(), outer_parse_info.get(), function_name, function_literal,
        isolate->counters()->worker_thread_runtime_call_stats(),
        isolate->counters()->compile_function_on_background(), FLAG_stack_size);
  }

 private:
  AccountingAllocator* allocator_;
  static SaveFlags* save_flags_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundCompileTaskTest);
};

SaveFlags* BackgroundCompileTaskTest::save_flags_ = nullptr;

TEST_F(BackgroundCompileTaskTest, Construct) {
  Handle<SharedFunctionInfo> shared =
      test::CreateSharedFunctionInfo(isolate(), nullptr);
  ASSERT_FALSE(shared->is_compiled());
  std::unique_ptr<BackgroundCompileTask> task(
      NewBackgroundCompileTask(isolate(), shared));
}

TEST_F(BackgroundCompileTaskTest, SyntaxError) {
  test::ScriptResource* script = new test::ScriptResource("^^^", strlen("^^^"));
  Handle<SharedFunctionInfo> shared =
      test::CreateSharedFunctionInfo(isolate(), script);
  std::unique_ptr<BackgroundCompileTask> task(
      NewBackgroundCompileTask(isolate(), shared));

  task->Run();
  ASSERT_FALSE(Compiler::FinalizeBackgroundCompileTask(
      task.get(), shared, isolate(), Compiler::KEEP_EXCEPTION));
  ASSERT_TRUE(isolate()->has_pending_exception());

  isolate()->clear_pending_exception();
}

TEST_F(BackgroundCompileTaskTest, CompileAndRun) {
  const char raw_script[] =
      "function g() {\n"
      "  f = function(a) {\n"
      "        for (var i = 0; i < 3; i++) { a += 20; }\n"
      "        return a;\n"
      "      }\n"
      "  return f;\n"
      "}\n"
      "g();";
  test::ScriptResource* script =
      new test::ScriptResource(raw_script, strlen(raw_script));
  Handle<JSFunction> f = RunJS<JSFunction>(script);
  Handle<SharedFunctionInfo> shared = handle(f->shared(), isolate());
  ASSERT_FALSE(shared->is_compiled());
  std::unique_ptr<BackgroundCompileTask> task(
      NewBackgroundCompileTask(isolate(), shared));

  task->Run();
  ASSERT_TRUE(Compiler::FinalizeBackgroundCompileTask(
      task.get(), shared, isolate(), Compiler::KEEP_EXCEPTION));
  ASSERT_TRUE(shared->is_compiled());

  Smi value = Smi::cast(*RunJS("f(100);"));
  ASSERT_TRUE(value == Smi::FromInt(160));
}

TEST_F(BackgroundCompileTaskTest, CompileFailure) {
  std::string raw_script("() { var a = ");
  for (int i = 0; i < 10000; i++) {
    // TODO(leszeks): Figure out a more "unit-test-y" way of forcing an analysis
    // failure than a binop stack overflow.

    // Alternate + and - to avoid n-ary operation nodes.
    raw_script += "'x' + 'x' - ";
  }
  raw_script += " 'x'; }";
  test::ScriptResource* script =
      new test::ScriptResource(raw_script.c_str(), strlen(raw_script.c_str()));
  Handle<SharedFunctionInfo> shared =
      test::CreateSharedFunctionInfo(isolate(), script);
  std::unique_ptr<BackgroundCompileTask> task(
      NewBackgroundCompileTask(isolate(), shared, 100));

  task->Run();
  ASSERT_FALSE(Compiler::FinalizeBackgroundCompileTask(
      task.get(), shared, isolate(), Compiler::KEEP_EXCEPTION));
  ASSERT_TRUE(isolate()->has_pending_exception());

  isolate()->clear_pending_exception();
}

class CompileTask : public Task {
 public:
  CompileTask(BackgroundCompileTask* task, base::Semaphore* semaphore)
      : task_(task), semaphore_(semaphore) {}
  ~CompileTask() override = default;

  void Run() override {
    task_->Run();
    semaphore_->Signal();
  }

 private:
  BackgroundCompileTask* task_;
  base::Semaphore* semaphore_;
  DISALLOW_COPY_AND_ASSIGN(CompileTask);
};

TEST_F(BackgroundCompileTaskTest, CompileOnBackgroundThread) {
  const char* raw_script =
      "(a, b) {\n"
      "  var c = a + b;\n"
      "  function bar() { return b }\n"
      "  var d = { foo: 100, bar : bar() }\n"
      "  return bar;"
      "}";
  test::ScriptResource* script =
      new test::ScriptResource(raw_script, strlen(raw_script));
  Handle<SharedFunctionInfo> shared =
      test::CreateSharedFunctionInfo(isolate(), script);
  std::unique_ptr<BackgroundCompileTask> task(
      NewBackgroundCompileTask(isolate(), shared));

  base::Semaphore semaphore(0);
  auto background_task = base::make_unique<CompileTask>(task.get(), &semaphore);

  V8::GetCurrentPlatform()->CallOnWorkerThread(std::move(background_task));
  semaphore.Wait();
  ASSERT_TRUE(Compiler::FinalizeBackgroundCompileTask(
      task.get(), shared, isolate(), Compiler::KEEP_EXCEPTION));
  ASSERT_TRUE(shared->is_compiled());
}

TEST_F(BackgroundCompileTaskTest, EagerInnerFunctions) {
  const char raw_script[] =
      "function g() {\n"
      "  f = function() {\n"
      "    // Simulate an eager IIFE with brackets.\n "
      "    var e = (function () { return 42; });\n"
      "    return e;\n"
      "  }\n"
      "  return f;\n"
      "}\n"
      "g();";
  test::ScriptResource* script =
      new test::ScriptResource(raw_script, strlen(raw_script));
  Handle<JSFunction> f = RunJS<JSFunction>(script);
  Handle<SharedFunctionInfo> shared = handle(f->shared(), isolate());
  ASSERT_FALSE(shared->is_compiled());
  std::unique_ptr<BackgroundCompileTask> task(
      NewBackgroundCompileTask(isolate(), shared));

  task->Run();
  ASSERT_TRUE(Compiler::FinalizeBackgroundCompileTask(
      task.get(), shared, isolate(), Compiler::KEEP_EXCEPTION));
  ASSERT_TRUE(shared->is_compiled());

  Handle<JSFunction> e = RunJS<JSFunction>("f();");

  ASSERT_TRUE(e->shared().is_compiled());
}

TEST_F(BackgroundCompileTaskTest, LazyInnerFunctions) {
  const char raw_script[] =
      "function g() {\n"
      "  f = function() {\n"
      "    function e() { return 42; };\n"
      "    return e;\n"
      "  }\n"
      "  return f;\n"
      "}\n"
      "g();";
  test::ScriptResource* script =
      new test::ScriptResource(raw_script, strlen(raw_script));
  Handle<JSFunction> f = RunJS<JSFunction>(script);
  Handle<SharedFunctionInfo> shared = handle(f->shared(), isolate());
  ASSERT_FALSE(shared->is_compiled());
  std::unique_ptr<BackgroundCompileTask> task(
      NewBackgroundCompileTask(isolate(), shared));

  task->Run();
  ASSERT_TRUE(Compiler::FinalizeBackgroundCompileTask(
      task.get(), shared, isolate(), Compiler::KEEP_EXCEPTION));
  ASSERT_TRUE(shared->is_compiled());

  Handle<JSFunction> e = RunJS<JSFunction>("f();");

  ASSERT_FALSE(e->shared().is_compiled());
}

}  // namespace internal
}  // namespace v8
