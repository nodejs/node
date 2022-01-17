#include "node_test_fixture.h"
#include "node_api.h"

void InitializeBinding(v8::Local<v8::Object> exports,
                       v8::Local<v8::Value> module,
                       v8::Local<v8::Context> context,
                       void* priv) {
  v8::Isolate* isolate = context->GetIsolate();
  exports->Set(
      context,
      v8::String::NewFromOneByte(isolate,
                                 reinterpret_cast<const uint8_t*>("key"))
                                 .ToLocalChecked(),
      v8::String::NewFromOneByte(isolate,
                                 reinterpret_cast<const uint8_t*>("value"))
                                 .ToLocalChecked())
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
                                 reinterpret_cast<const uint8_t*>(run_script))
                                 .ToLocalChecked())
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
                                 reinterpret_cast<const uint8_t*>("key"))
                                 .ToLocalChecked(),
      v8::String::NewFromOneByte(isolate,
                                 reinterpret_cast<const uint8_t*>("value"))
                                 .ToLocalChecked())
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
                                 reinterpret_cast<const uint8_t*>(run_script))
                                 .ToLocalChecked())
      .ToLocalChecked();
  v8::Local<v8::Value> completion_value = script->Run(context).ToLocalChecked();
  v8::String::Utf8Value utf8val(isolate_, completion_value);
  CHECK_NOT_NULL(*utf8val);
  CHECK_EQ(strcmp(*utf8val, "value"), 0);
  CHECK_EQ(calls, 1);
}

napi_value InitializeLocalNapiBinding(napi_env env, napi_value exports) {
  napi_value key, value;
  CHECK_EQ(
      napi_create_string_utf8(env, "hello", NAPI_AUTO_LENGTH, &key), napi_ok);
  CHECK_EQ(
      napi_create_string_utf8(env, "world", NAPI_AUTO_LENGTH, &value), napi_ok);
  CHECK_EQ(napi_set_property(env, exports, key, value), napi_ok);
  return nullptr;
}

static napi_module local_linked_napi = {
  NAPI_MODULE_VERSION,
  node::ModuleFlags::kLinked,
  nullptr,
  InitializeLocalNapiBinding,
  "local_linked_napi",
  nullptr,
  {0},
};

TEST_F(LinkedBindingTest, LocallyDefinedLinkedBindingNapiTest) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env test_env {handle_scope, argv};

  AddLinkedBinding(*test_env, local_linked_napi);

  v8::Local<v8::Context> context = isolate_->GetCurrentContext();

  const char* run_script =
      "process._linkedBinding('local_linked_napi').hello";
  v8::Local<v8::Script> script = v8::Script::Compile(
      context,
      v8::String::NewFromOneByte(isolate_,
                                 reinterpret_cast<const uint8_t*>(run_script))
                                 .ToLocalChecked())
      .ToLocalChecked();
  v8::Local<v8::Value> completion_value = script->Run(context).ToLocalChecked();
  v8::String::Utf8Value utf8val(isolate_, completion_value);
  CHECK_NOT_NULL(*utf8val);
  CHECK_EQ(strcmp(*utf8val, "world"), 0);
}

napi_value NapiLinkedWithInstanceData(napi_env env, napi_value exports) {
  int* instance_data = new int(0);
  CHECK_EQ(
      napi_set_instance_data(
          env,
          instance_data,
          [](napi_env env, void* data, void* hint) {
            ++*static_cast<int*>(data);
          }, nullptr),
      napi_ok);

  napi_value key, value;
  CHECK_EQ(
      napi_create_string_utf8(env, "hello", NAPI_AUTO_LENGTH, &key), napi_ok);
  CHECK_EQ(
      napi_create_external(env, instance_data, nullptr, nullptr, &value),
      napi_ok);
  CHECK_EQ(napi_set_property(env, exports, key, value), napi_ok);
  return nullptr;
}

static napi_module local_linked_napi_id = {
  NAPI_MODULE_VERSION,
  node::ModuleFlags::kLinked,
  nullptr,
  NapiLinkedWithInstanceData,
  "local_linked_napi_id",
  nullptr,
  {0},
};

TEST_F(LinkedBindingTest, LocallyDefinedLinkedBindingNapiInstanceDataTest) {
  const v8::HandleScope handle_scope(isolate_);
  int* instance_data = nullptr;

  {
    const Argv argv;
    Env test_env {handle_scope, argv};

    AddLinkedBinding(*test_env, local_linked_napi_id);

    v8::Local<v8::Context> context = isolate_->GetCurrentContext();

    const char* run_script =
        "process._linkedBinding('local_linked_napi_id').hello";
    v8::Local<v8::Script> script = v8::Script::Compile(
        context,
        v8::String::NewFromOneByte(isolate_,
                                   reinterpret_cast<const uint8_t*>(run_script))
                                   .ToLocalChecked())
        .ToLocalChecked();
    v8::Local<v8::Value> completion_value =
        script->Run(context).ToLocalChecked();
    CHECK(completion_value->IsExternal());
    instance_data = static_cast<int*>(
        completion_value.As<v8::External>()->Value());
    CHECK_NE(instance_data, nullptr);
    CHECK_EQ(*instance_data, 0);
  }

  CHECK_EQ(*instance_data, 1);
  delete instance_data;
}

TEST_F(LinkedBindingTest, ManyBindingsTest) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env test_env {handle_scope, argv};

  int calls = 0;
  AddLinkedBinding(*test_env, "local_linked1", InitializeLocalBinding, &calls);
  AddLinkedBinding(*test_env, "local_linked2", InitializeLocalBinding, &calls);
  AddLinkedBinding(*test_env, "local_linked3", InitializeLocalBinding, &calls);
  AddLinkedBinding(*test_env, local_linked_napi);  // Add a N-API addon as well.
  AddLinkedBinding(*test_env, "local_linked4", InitializeLocalBinding, &calls);
  AddLinkedBinding(*test_env, "local_linked5", InitializeLocalBinding, &calls);

  v8::Local<v8::Context> context = isolate_->GetCurrentContext();

  const char* run_script =
      "for (let i = 1; i <= 5; i++)process._linkedBinding(`local_linked${i}`);"
      "process._linkedBinding('local_linked_napi').hello";
  v8::Local<v8::Script> script = v8::Script::Compile(
      context,
      v8::String::NewFromOneByte(isolate_,
                                 reinterpret_cast<const uint8_t*>(run_script))
                                 .ToLocalChecked())
      .ToLocalChecked();
  v8::Local<v8::Value> completion_value = script->Run(context).ToLocalChecked();
  v8::String::Utf8Value utf8val(isolate_, completion_value);
  CHECK_NOT_NULL(*utf8val);
  CHECK_EQ(strcmp(*utf8val, "world"), 0);
  CHECK_EQ(calls, 5);
}

