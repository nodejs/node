// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate.h"

#include <memory>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "include/libplatform/libplatform.h"
#include "include/v8-function.h"
#include "include/v8-platform.h"
#include "include/v8-template.h"
#include "src/base/platform/semaphore.h"
#include "src/init/v8.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {

using IsolateTest = TestWithIsolate;

namespace {

class MemoryPressureTask : public v8::Task {
 public:
  MemoryPressureTask(Isolate* isolate, base::Semaphore* semaphore)
      : isolate_(isolate), semaphore_(semaphore) {}
  ~MemoryPressureTask() override = default;
  MemoryPressureTask(const MemoryPressureTask&) = delete;
  MemoryPressureTask& operator=(const MemoryPressureTask&) = delete;

  // v8::Task implementation.
  void Run() override {
    isolate_->MemoryPressureNotification(MemoryPressureLevel::kCritical);
    semaphore_->Signal();
  }

 private:
  Isolate* isolate_;
  base::Semaphore* semaphore_;
};

}  // namespace

// Check that triggering a memory pressure notification on the isolate thread
// doesn't request a GC interrupt.
TEST_F(IsolateTest, MemoryPressureNotificationForeground) {
  internal::Isolate* i_isolate =
      reinterpret_cast<internal::Isolate*>(isolate());

  ASSERT_FALSE(i_isolate->stack_guard()->CheckGC());
  isolate()->MemoryPressureNotification(MemoryPressureLevel::kCritical);
  ASSERT_FALSE(i_isolate->stack_guard()->CheckGC());
}

// Check that triggering a memory pressure notification on an background thread
// requests a GC interrupt.
TEST_F(IsolateTest, MemoryPressureNotificationBackground) {
  internal::Isolate* i_isolate =
      reinterpret_cast<internal::Isolate*>(isolate());

  base::Semaphore semaphore(0);

  internal::V8::GetCurrentPlatform()->PostTaskOnWorkerThread(
      TaskPriority::kUserVisible,
      std::make_unique<MemoryPressureTask>(isolate(), &semaphore));

  semaphore.Wait();

  ASSERT_TRUE(i_isolate->stack_guard()->CheckGC());
  v8::platform::PumpMessageLoop(internal::V8::GetCurrentPlatform(), isolate());
}

using IncumbentContextTest = TestWithIsolate;

// Check that Isolate::GetIncumbentContext() returns the correct one in basic
// scenarios.
TEST_F(IncumbentContextTest, Basic) {
  auto Str = [&](const char* s) {
    return String::NewFromUtf8(isolate(), s).ToLocalChecked();
  };
  auto Run = [&](Local<Context> context, const char* script) {
    Context::Scope scope(context);
    return Script::Compile(context, Str(script))
        .ToLocalChecked()
        ->Run(context)
        .ToLocalChecked();
  };

  // Set up the test environment; three contexts with getIncumbentGlobal()
  // function.
  Local<FunctionTemplate> get_incumbent_global = FunctionTemplate::New(
      isolate(), [](const FunctionCallbackInfo<Value>& info) {
        Local<Context> incumbent_context =
            info.GetIsolate()->GetIncumbentContext();
        info.GetReturnValue().Set(incumbent_context->Global());
      });
  Local<ObjectTemplate> global_template = ObjectTemplate::New(isolate());
  global_template->Set(isolate(), "getIncumbentGlobal", get_incumbent_global);

  Local<Context> context_a = Context::New(isolate(), nullptr, global_template);
  Local<Context> context_b = Context::New(isolate(), nullptr, global_template);
  Local<Context> context_c = Context::New(isolate(), nullptr, global_template);
  Local<Object> global_a = context_a->Global();
  Local<Object> global_b = context_b->Global();
  Local<Object> global_c = context_c->Global();

  Local<String> security_token = Str("security_token");
  context_a->SetSecurityToken(security_token);
  context_b->SetSecurityToken(security_token);
  context_c->SetSecurityToken(security_token);

  global_a->Set(context_a, Str("b"), global_b).ToChecked();
  global_b->Set(context_b, Str("c"), global_c).ToChecked();

  // Test scenario 2: A -> B -> C, then the incumbent is C.
  Run(context_a, "funcA = function() { return b.funcB(); }");
  Run(context_b, "funcB = function() { return c.getIncumbentGlobal(); }");
  // Without BackupIncumbentScope.
  EXPECT_EQ(global_b, Run(context_a, "funcA()"));
  {
    // With BackupIncumbentScope.
    Context::BackupIncumbentScope backup_incumbent(context_a);
    EXPECT_EQ(global_b, Run(context_a, "funcA()"));
  }

  // Test scenario 2: A -> B -> C -> C, then the incumbent is C.
  Run(context_a, "funcA = function() { return b.funcB(); }");
  Run(context_b, "funcB = function() { return c.funcC(); }");
  Run(context_c, "funcC = function() { return getIncumbentGlobal(); }");
  // Without BackupIncumbentScope.
  EXPECT_EQ(global_c, Run(context_a, "funcA()"));
  {
    // With BackupIncumbentScope.
    Context::BackupIncumbentScope backup_incumbent(context_a);
    EXPECT_EQ(global_c, Run(context_a, "funcA()"));
  }
}

namespace {
thread_local std::multimap<v8::CrashKeyId, std::string> crash_keys;
void CrashKeyCallback(v8::CrashKeyId id, const std::string& value) {
  crash_keys.insert({id, value});
}
}  // namespace
TEST_F(IsolateTest, SetAddCrashKeyCallback) {
  isolate()->SetAddCrashKeyCallback(CrashKeyCallback);

  i::Isolate* i_isolate = reinterpret_cast<internal::Isolate*>(isolate());
  i::Heap* heap = i_isolate->heap();

  size_t expected_keys_count = 5;
  EXPECT_EQ(crash_keys.count(v8::CrashKeyId::kIsolateAddress), 1u);
  EXPECT_EQ(crash_keys.count(v8::CrashKeyId::kReadonlySpaceFirstPageAddress),
            1u);
  EXPECT_EQ(crash_keys.count(v8::CrashKeyId::kOldSpaceFirstPageAddress), 1u);
  EXPECT_EQ(crash_keys.count(v8::CrashKeyId::kSnapshotChecksumCalculated), 1u);
  EXPECT_EQ(crash_keys.count(v8::CrashKeyId::kSnapshotChecksumExpected), 1u);

  if (heap->code_range_base()) {
    ++expected_keys_count;
    EXPECT_EQ(crash_keys.count(v8::CrashKeyId::kCodeRangeBaseAddress), 1u);
  }
  if (heap->code_space()->first_page()) {
    ++expected_keys_count;
    EXPECT_EQ(crash_keys.count(v8::CrashKeyId::kCodeSpaceFirstPageAddress), 1u);
  }
  EXPECT_EQ(crash_keys.size(), expected_keys_count);
}

TEST_F(IsolateTest, DebugTraceMinimal) {
  i::CrashKeyStore crash_key_store(i_isolate());
  HandleScope handle_scope(isolate());
  Local<FunctionTemplate> print_stack = FunctionTemplate::New(
      isolate(), [](const FunctionCallbackInfo<Value>& info) {
        internal::Isolate* i_isolate =
            reinterpret_cast<internal::Isolate*>(info.GetIsolate());
        i_isolate->ReportStackAsCrashKey();
      });

  Local<Context> context = Context::New(isolate());
  Context::Scope context_scope(context);
  context->Global()
      ->Set(context,
            String::NewFromUtf8(isolate(), "printStack").ToLocalChecked(),
            print_stack->GetFunction(context).ToLocalChecked())
      .Check();

  const char* script_src =
      "function f3() { return printStack(); }\n"
      "function f2() { return f3(); }\n"
      "function f1() { return f2(); }\n"
      "f1();";

  Local<String> source =
      String::NewFromUtf8(isolate(), script_src).ToLocalChecked();
  ScriptOrigin origin(
      String::NewFromUtf8(isolate(), "test.js").ToLocalChecked());
  Local<Script> script =
      Script::Compile(context, source, &origin).ToLocalChecked();
  script->Run(context).ToLocalChecked();

  EXPECT_TRUE(crash_key_store.HasKey("v8-oom-stack"));
  std::string output = crash_key_store.ValueForKey("v8-oom-stack");
  const char* expected_output =
      "f3 in test.js\n"
      "f2 in =\n"
      "f1 in =\n"
      "<none> in =\n"
      "$\n";
  EXPECT_EQ(output, expected_output);
}

TEST_F(IsolateTest, DebugTraceMaxLength) {
  i::CrashKeyStore crash_key_store(i_isolate());
  HandleScope handle_scope(isolate());
  Local<FunctionTemplate> print_stack = FunctionTemplate::New(
      isolate(), [](const FunctionCallbackInfo<Value>& info) {
        internal::Isolate* i_isolate =
            reinterpret_cast<internal::Isolate*>(info.GetIsolate());
        i_isolate->ReportStackAsCrashKey();
      });

  Local<Context> context = Context::New(isolate());
  Context::Scope context_scope(context);
  context->Global()
      ->Set(context,
            String::NewFromUtf8(isolate(), "printStack").ToLocalChecked(),
            print_stack->GetFunction(context).ToLocalChecked())
      .Check();

  // Build a deep stack with long function names to exceed the 1024-byte cap.
  std::string script_src = "function leaf() { return printStack(); }\n";
  const int frame_count = 40;
  std::string callee = "leaf";
  for (int i = 0; i < frame_count; ++i) {
    std::string name = "fn_" + std::to_string(i) + std::string(60, 'a');
    script_src += "function " + name + "() { return " + callee + "(); }\n";
    callee = name;
  }
  script_src += callee + "();";

  Local<String> source =
      String::NewFromUtf8(isolate(), script_src.c_str()).ToLocalChecked();
  ScriptOrigin origin(
      String::NewFromUtf8(isolate(), "test.js").ToLocalChecked());
  Local<Script> script =
      Script::Compile(context, source, &origin).ToLocalChecked();
  script->Run(context).ToLocalChecked();

  EXPECT_TRUE(crash_key_store.HasKey("v8-oom-stack"));
  std::string output = crash_key_store.ValueForKey("v8-oom-stack");
  // The reported stack trace is only cut off between frames but not in the
  // middle, so we should exceed the 1024 bytes. However, limit this to at most
  // 100 bytes (60 bytes function name + some space for file name/etc.).
  // The full stack trace has over 3000 characters.
  static constexpr size_t kMaxLength = 1024u;
  EXPECT_LE(kMaxLength, output.size());
  EXPECT_LE(output.size(), kMaxLength + 100u);
}
}  // namespace v8
