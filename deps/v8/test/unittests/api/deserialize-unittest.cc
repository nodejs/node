// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/libplatform/libplatform.h"
#include "include/v8-context.h"
#include "include/v8-function.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-platform.h"
#include "include/v8-primitive.h"
#include "include/v8-script.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {

class DeserializeTest : public TestWithPlatform {
 public:
  class IsolateAndContextScope {
   public:
    explicit IsolateAndContextScope(DeserializeTest* test)
        : test_(test),
          isolate_wrapper_(kNoCounters),
          isolate_scope_(isolate_wrapper_.isolate()),
          handle_scope_(isolate_wrapper_.isolate()),
          context_(Context::New(isolate_wrapper_.isolate())),
          context_scope_(context_) {
      CHECK_NULL(test->isolate_);
      CHECK(test->context_.IsEmpty());
      test->isolate_ = isolate_wrapper_.isolate();
      test->context_ = context_;
    }
    ~IsolateAndContextScope() {
      test_->isolate_ = nullptr;
      test_->context_ = {};
    }

   private:
    DeserializeTest* test_;
    v8::IsolateWrapper isolate_wrapper_;
    v8::Isolate::Scope isolate_scope_;
    v8::HandleScope handle_scope_;
    v8::Local<v8::Context> context_;
    v8::Context::Scope context_scope_;
  };

  Local<String> NewString(const char* val) {
    return String::NewFromUtf8(isolate(), val).ToLocalChecked();
  }

  Local<Value> RunGlobalFunc(const char* name) {
    Local<Value> func_val =
        context()->Global()->Get(context(), NewString(name)).ToLocalChecked();
    CHECK(func_val->IsFunction());
    Local<Function> func = Local<Function>::Cast(func_val);
    return func->Call(context(), Undefined(isolate()), 0, nullptr)
        .ToLocalChecked();
  }

  Isolate* isolate() { return isolate_; }
  v8::Local<v8::Context> context() { return context_.ToLocalChecked(); }

 private:
  Isolate* isolate_ = nullptr;
  v8::MaybeLocal<v8::Context> context_;
};

// Check that deserialization works.
TEST_F(DeserializeTest, Deserialize) {
  std::unique_ptr<v8::ScriptCompiler::CachedData> cached_data;

  {
    IsolateAndContextScope scope(this);

    Local<String> source_code = NewString("function foo() { return 42; }");
    Local<Script> script =
        Script::Compile(context(), source_code).ToLocalChecked();

    CHECK(!script->Run(context()).IsEmpty());
    CHECK_EQ(RunGlobalFunc("foo"), Integer::New(isolate(), 42));

    cached_data.reset(
        ScriptCompiler::CreateCodeCache(script->GetUnboundScript()));
  }

  {
    IsolateAndContextScope scope(this);

    Local<String> source_code = NewString("function foo() { return 42; }");
    ScriptCompiler::Source source(source_code, cached_data.release());
    Local<Script> script =
        ScriptCompiler::Compile(context(), &source,
                                ScriptCompiler::kConsumeCodeCache)
            .ToLocalChecked();

    CHECK(!source.GetCachedData()->rejected);
    CHECK(!script->Run(context()).IsEmpty());
    CHECK_EQ(RunGlobalFunc("foo"), v8::Integer::New(isolate(), 42));
  }
}

// Check that deserialization with a different script rejects the cache but
// still works via standard compilation.
TEST_F(DeserializeTest, DeserializeRejectsDifferentSource) {
  std::unique_ptr<v8::ScriptCompiler::CachedData> cached_data;

  {
    IsolateAndContextScope scope(this);

    Local<String> source_code = NewString("function foo() { return 42; }");
    Local<Script> script =
        Script::Compile(context(), source_code).ToLocalChecked();

    CHECK(!script->Run(context()).IsEmpty());
    CHECK_EQ(RunGlobalFunc("foo"), Integer::New(isolate(), 42));

    cached_data.reset(
        ScriptCompiler::CreateCodeCache(script->GetUnboundScript()));
  }

  {
    IsolateAndContextScope scope(this);

    // The source hash is based on the source length, so have to make sure that
    // this is different here.
    Local<String> source_code = NewString("function bar() { return 142; }");
    ScriptCompiler::Source source(source_code, cached_data.release());
    Local<Script> script =
        ScriptCompiler::Compile(context(), &source,
                                ScriptCompiler::kConsumeCodeCache)
            .ToLocalChecked();

    CHECK(source.GetCachedData()->rejected);
    CHECK(!script->Run(context()).IsEmpty());
    CHECK_EQ(RunGlobalFunc("bar"), v8::Integer::New(isolate(), 142));
  }
}

class DeserializeThread : public base::Thread {
 public:
  explicit DeserializeThread(ScriptCompiler::ConsumeCodeCacheTask* task)
      : Thread(base::Thread::Options("DeserializeThread")), task_(task) {}

  void Run() override { task_->Run(); }

  std::unique_ptr<ScriptCompiler::ConsumeCodeCacheTask> TakeTask() {
    return std::move(task_);
  }

 private:
  std::unique_ptr<ScriptCompiler::ConsumeCodeCacheTask> task_;
};

// Check that off-thread deserialization works.
TEST_F(DeserializeTest, OffThreadDeserialize) {
  std::unique_ptr<v8::ScriptCompiler::CachedData> cached_data;

  {
    IsolateAndContextScope scope(this);

    Local<String> source_code = NewString("function foo() { return 42; }");
    Local<Script> script =
        Script::Compile(context(), source_code).ToLocalChecked();

    CHECK(!script->Run(context()).IsEmpty());
    CHECK_EQ(RunGlobalFunc("foo"), Integer::New(isolate(), 42));

    cached_data.reset(
        ScriptCompiler::CreateCodeCache(script->GetUnboundScript()));
  }

  {
    IsolateAndContextScope scope(this);

    DeserializeThread deserialize_thread(
        ScriptCompiler::StartConsumingCodeCache(
            isolate(), std::make_unique<ScriptCompiler::CachedData>(
                           cached_data->data, cached_data->length,
                           ScriptCompiler::CachedData::BufferNotOwned)));
    CHECK(deserialize_thread.Start());
    deserialize_thread.Join();

    Local<String> source_code = NewString("function foo() { return 42; }");
    ScriptCompiler::Source source(source_code, cached_data.release(),
                                  deserialize_thread.TakeTask().release());
    Local<Script> script =
        ScriptCompiler::Compile(context(), &source,
                                ScriptCompiler::kConsumeCodeCache)
            .ToLocalChecked();

    CHECK(!source.GetCachedData()->rejected);
    CHECK(!script->Run(context()).IsEmpty());
    CHECK_EQ(RunGlobalFunc("foo"), v8::Integer::New(isolate(), 42));
  }
}

// Check that off-thread deserialization works.
TEST_F(DeserializeTest, OffThreadDeserializeRejectsDifferentSource) {
  std::unique_ptr<v8::ScriptCompiler::CachedData> cached_data;

  {
    IsolateAndContextScope scope(this);

    Local<String> source_code = NewString("function foo() { return 42; }");
    Local<Script> script =
        Script::Compile(context(), source_code).ToLocalChecked();

    CHECK(!script->Run(context()).IsEmpty());
    CHECK_EQ(RunGlobalFunc("foo"), Integer::New(isolate(), 42));

    cached_data.reset(
        ScriptCompiler::CreateCodeCache(script->GetUnboundScript()));
  }

  {
    IsolateAndContextScope scope(this);

    DeserializeThread deserialize_thread(
        ScriptCompiler::StartConsumingCodeCache(
            isolate(), std::make_unique<ScriptCompiler::CachedData>(
                           cached_data->data, cached_data->length,
                           ScriptCompiler::CachedData::BufferNotOwned)));
    CHECK(deserialize_thread.Start());
    deserialize_thread.Join();

    Local<String> source_code = NewString("function bar() { return 142; }");
    ScriptCompiler::Source source(source_code, cached_data.release(),
                                  deserialize_thread.TakeTask().release());
    Local<Script> script =
        ScriptCompiler::Compile(context(), &source,
                                ScriptCompiler::kConsumeCodeCache)
            .ToLocalChecked();

    CHECK(source.GetCachedData()->rejected);
    CHECK(!script->Run(context()).IsEmpty());
    CHECK_EQ(RunGlobalFunc("bar"), v8::Integer::New(isolate(), 142));
  }
}

}  // namespace v8
