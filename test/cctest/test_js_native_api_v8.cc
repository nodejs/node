#include <stdio.h>
#include <cstdio>
#include <string>
#include "env-inl.h"
#include "gtest/gtest.h"
#include "js_native_api_v8.h"
#include "node_api_internals.h"
#include "node_binding.h"
#include "node_test_fixture.h"

namespace v8impl {

using v8::Local;
using v8::Object;

static napi_env addon_env;
static uint32_t finalizer_call_count = 0;

class JsNativeApiV8Test : public EnvironmentTestFixture {
 private:
  void SetUp() override {
    EnvironmentTestFixture::SetUp();
    finalizer_call_count = 0;
  }

  void TearDown() override { NodeTestFixture::TearDown(); }
};

TEST_F(JsNativeApiV8Test, Reference) {
  const v8::HandleScope handle_scope(isolate_);
  Argv argv;

  napi_ref ref;
  void* embedder_fields[v8::kEmbedderFieldsInWeakCallback] = {nullptr, nullptr};
  v8::WeakCallbackInfo<Reference::SecondPassCallParameterRef>::Callback
      callback;
  Reference::SecondPassCallParameterRef* parameter = nullptr;

  {
    Env test_env{handle_scope, argv};

    node::Environment* env = *test_env;
    node::LoadEnvironment(env, "");

    napi_addon_register_func init = [](napi_env env, napi_value exports) {
      addon_env = env;
      return exports;
    };
    Local<Object> module_obj = Object::New(isolate_);
    Local<Object> exports_obj = Object::New(isolate_);
    napi_module_register_by_symbol(
        exports_obj, module_obj, env->context(), init);
    ASSERT_NE(addon_env, nullptr);
    node_napi_env internal_env = reinterpret_cast<node_napi_env>(addon_env);
    EXPECT_EQ(internal_env->node_env(), env);

    // Create a new scope to manage the handles.
    {
      const v8::HandleScope handle_scope(isolate_);
      napi_value value;
      napi_create_object(addon_env, &value);
      // Create a weak reference;
      napi_add_finalizer(
          addon_env,
          value,
          nullptr,
          [](napi_env env, void* finalize_data, void* finalize_hint) {
            finalizer_call_count++;
          },
          nullptr,
          &ref);
      parameter = reinterpret_cast<Reference*>(ref)->_secondPassParameter;
    }

    // We can hardly trigger a non-forced Garbage Collection in a stable way.
    // Here we just invoke the weak callbacks directly.
    // The persistant handles should be reset in the weak callback in respect
    // to the API contract of v8 weak callbacks.
    v8::WeakCallbackInfo<Reference::SecondPassCallParameterRef> data(
        reinterpret_cast<v8::Isolate*>(isolate_),
        parameter,
        embedder_fields,
        &callback);
    Reference::FinalizeCallback(data);
    EXPECT_EQ(callback, &Reference::SecondPassCallback);
  }
  // Env goes out of scope, the environment has been teardown. All node-api ref
  // trackers should have been destroyed.

  // Now we call the second pass callback to verify the method do not abort with
  // memory violations.
  v8::WeakCallbackInfo<Reference::SecondPassCallParameterRef> data(
      reinterpret_cast<v8::Isolate*>(isolate_),
      parameter,
      embedder_fields,
      nullptr);
  Reference::SecondPassCallback(data);

  // After Environment Teardown
  EXPECT_EQ(finalizer_call_count, uint32_t(1));
}
}  // namespace v8impl
