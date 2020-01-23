#include "node_test_fixture.h"
#include "node_internals.h"  // RunBootstrapping()

void InitializeBinding(v8::Local<v8::Object> exports,
                       v8::Local<v8::Value> module,
                       v8::Local<v8::Context> context,
                       void* priv) {
  v8::Isolate* isolate = context->GetIsolate();
  exports->Set(
      context,
      v8::String::NewFromOneByte(isolate,
                                 reinterpret_cast<const uint8_t*>("key"),
                                 v8::NewStringType::kNormal).ToLocalChecked(),
      v8::String::NewFromOneByte(isolate,
                                 reinterpret_cast<const uint8_t*>("value"),
                                 v8::NewStringType::kNormal).ToLocalChecked())
      .FromJust();
}

NODE_MODULE_LINKED(cctest_linkedbinding, InitializeBinding)

class LinkedBindingTest : public EnvironmentTestFixture {};

TEST_F(LinkedBindingTest, SimpleTest) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env test_env {handle_scope, argv};

  v8::Local<v8::Context> context = isolate_->GetCurrentContext();

  const char* run_script =
      "process._linkedBinding('cctest_linkedbinding').key";
  v8::Local<v8::Script> script = v8::Script::Compile(
      context,
      v8::String::NewFromOneByte(isolate_,
                                 reinterpret_cast<const uint8_t*>(run_script),
                                 v8::NewStringType::kNormal).ToLocalChecked())
      .ToLocalChecked();
  v8::Local<v8::Value> completion_value = script->Run(context).ToLocalChecked();
  v8::String::Utf8Value utf8val(isolate_, completion_value);
  CHECK_NOT_NULL(*utf8val);
  CHECK_EQ(strcmp(*utf8val, "value"), 0);
}

void InitializeLocalBinding(v8::Local<v8::Object> exports,
                            v8::Local<v8::Value> module,
                            v8::Local<v8::Context> context,
                            void* priv) {
  ++*static_cast<int*>(priv);
  v8::Isolate* isolate = context->GetIsolate();
  exports->Set(
      context,
      v8::String::NewFromOneByte(isolate,
                                 reinterpret_cast<const uint8_t*>("key"),
                                 v8::NewStringType::kNormal).ToLocalChecked(),
      v8::String::NewFromOneByte(isolate,
                                 reinterpret_cast<const uint8_t*>("value"),
                                 v8::NewStringType::kNormal).ToLocalChecked())
      .FromJust();
}

TEST_F(LinkedBindingTest, LocallyDefinedLinkedBindingTest) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env test_env {handle_scope, argv};

  int calls = 0;
  AddLinkedBinding(*test_env, "local_linked", InitializeLocalBinding, &calls);

  v8::Local<v8::Context> context = isolate_->GetCurrentContext();

  const char* run_script =
      "process._linkedBinding('local_linked').key";
  v8::Local<v8::Script> script = v8::Script::Compile(
      context,
      v8::String::NewFromOneByte(isolate_,
                                 reinterpret_cast<const uint8_t*>(run_script),
                                 v8::NewStringType::kNormal).ToLocalChecked())
      .ToLocalChecked();
  v8::Local<v8::Value> completion_value = script->Run(context).ToLocalChecked();
  v8::String::Utf8Value utf8val(isolate_, completion_value);
  CHECK_NOT_NULL(*utf8val);
  CHECK_EQ(strcmp(*utf8val, "value"), 0);
  CHECK_EQ(calls, 1);
}
