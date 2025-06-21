#include "node.h"

#include <string>
#include "gtest/gtest.h"
#include "node_test_fixture.h"

using node::Environment;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::SealHandleScope;
using v8::String;
using v8::Value;

bool report_callback_called = false;

class ReportTest : public EnvironmentTestFixture {
 private:
  void TearDown() override {
    EnvironmentTestFixture::TearDown();
    report_callback_called = false;
  }
};

TEST_F(ReportTest, ReportWithNoIsolate) {
  SealHandleScope handle_scope(isolate_);

  std::ostringstream oss;
  node::GetNodeReport(static_cast<Isolate*>(nullptr),
                      "FooMessage",
                      "BarTrigger",
                      Local<Value>(),
                      oss);

  // Simple checks on the output string contains the message and trigger.
  std::string actual = oss.str();
  EXPECT_NE(actual.find("FooMessage"), std::string::npos);
  EXPECT_NE(actual.find("BarTrigger"), std::string::npos);
}

TEST_F(ReportTest, ReportWithNoEnv) {
  SealHandleScope handle_scope(isolate_);

  std::ostringstream oss;
  node::GetNodeReport(static_cast<Environment*>(nullptr),
                      "FooMessage",
                      "BarTrigger",
                      Local<Value>(),
                      oss);

  // Simple checks on the output string contains the message and trigger.
  std::string actual = oss.str();
  EXPECT_NE(actual.find("FooMessage"), std::string::npos);
  EXPECT_NE(actual.find("BarTrigger"), std::string::npos);
}

TEST_F(ReportTest, ReportWithIsolate) {
  const HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv};

  Local<Context> context = isolate_->GetCurrentContext();
  Local<Function> fn =
      Function::New(context, [](const FunctionCallbackInfo<Value>& args) {
        Isolate* isolate = args.GetIsolate();
        HandleScope scope(isolate);

        std::ostringstream oss;
        node::GetNodeReport(isolate, "FooMessage", "BarTrigger", args[0], oss);

        // Simple checks on the output string contains the message and trigger.
        std::string actual = oss.str();
        EXPECT_NE(actual.find("FooMessage"), std::string::npos);
        EXPECT_NE(actual.find("BarTrigger"), std::string::npos);

        report_callback_called = true;
      }).ToLocalChecked();

  context->Global()
      ->Set(context, String::NewFromUtf8(isolate_, "foo").ToLocalChecked(), fn)
      .FromJust();

  node::LoadEnvironment(*env, "foo()").ToLocalChecked();

  EXPECT_TRUE(report_callback_called);
}

TEST_F(ReportTest, ReportWithEnv) {
  const HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv};

  Local<Context> context = isolate_->GetCurrentContext();
  Local<Function> fn =
      Function::New(context, [](const FunctionCallbackInfo<Value>& args) {
        Isolate* isolate = args.GetIsolate();
        HandleScope scope(isolate);

        std::ostringstream oss;
        node::GetNodeReport(
            node::GetCurrentEnvironment(isolate->GetCurrentContext()),
            "FooMessage",
            "BarTrigger",
            args[0],
            oss);

        // Simple checks on the output string contains the message and trigger.
        std::string actual = oss.str();
        EXPECT_NE(actual.find("FooMessage"), std::string::npos);
        EXPECT_NE(actual.find("BarTrigger"), std::string::npos);

        report_callback_called = true;
      }).ToLocalChecked();

  context->Global()
      ->Set(context, String::NewFromUtf8(isolate_, "foo").ToLocalChecked(), fn)
      .FromJust();

  node::LoadEnvironment(*env, "foo()").ToLocalChecked();

  EXPECT_TRUE(report_callback_called);
}
