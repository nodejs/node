#include <stdio.h>
#include <cstdio>
#include <string>
#include "env-inl.h"
#include "gtest/gtest.h"
#include "node_api_internals.h"
#include "node_binding.h"
#include "node_test_fixture.h"


class NodeApiTestEnv : public NodeTestFixture {
 public:
  napi_env env;
  node::Environment* environment_;
  v8::Global<v8::Context>* context_;
 private:
  void SetUp() override {
    NodeTestFixture::SetUp();
    v8::HandleScope handle_scope(isolate_);
    Argv argv;
    v8::Local<v8::Context> context = node::NewContext(isolate_);
    context_ = new v8::Global<v8::Context>(isolate_, context);
    CHECK(!context.IsEmpty());
    context->Enter();

    isolate_data_ = node::CreateIsolateData(
        isolate_, &NodeTestFixture::current_loop, platform.get());
    CHECK_NE(nullptr, isolate_data_);
    std::vector<std::string> args(*argv, *argv + 1);
    std::vector<std::string> exec_args(*argv, *argv + 1);
    environment_ = node::CreateEnvironment(isolate_data_,
                                context,
                                args,
                                exec_args,
                                node::EnvironmentFlags::kDefaultFlags);
    CHECK_NE(nullptr, environment_);

    node::LoadEnvironment(environment_, "");

    node_napi_env result;
    result = new node_napi_env__(isolate_->GetCurrentContext(), "");
    result->node_env()->AddCleanupHook(
        [](void* arg) { static_cast<napi_env>(arg)->Unref(); },
        static_cast<void*>(result));
    this->env = reinterpret_cast<napi_env>(result);
  }

  void TearDown() override {
    {
      v8::HandleScope handleScope(isolate_);
      context_->Get(isolate_)->Exit();
      delete context_;
    }
    node::FreeEnvironment(environment_);
    node::FreeIsolateData(isolate_data_);
    NodeTestFixture::TearDown();
  }

  node::IsolateData* isolate_data_;
};

static napi_env addon_env;
TEST_F(NodeApiTestEnv, CreateNodeApiEnv) {
  v8::HandleScope handleScope(isolate_);
  napi_addon_register_func init = [](napi_env env, napi_value exports) {
    addon_env = env;
    return exports;
  };
  v8::Local<v8::Object> module_obj = v8::Object::New(isolate_);
  v8::Local<v8::Object> exports_obj = v8::Object::New(isolate_);
  napi_module_register_by_symbol(exports_obj, module_obj,context_->Get(isolate_), init);
  ASSERT_NE(addon_env, nullptr);
  node_napi_env internal_env = reinterpret_cast<node_napi_env>(addon_env);
  EXPECT_EQ(internal_env->node_env(), environment_);
}

TEST_F(NodeApiTestEnv, number) {
  v8::HandleScope handleScope(isolate_);

  // uint32
  napi_value node_uint32_result;
  CHECK_EQ(napi_create_uint32(env, 1, &node_uint32_result) ,napi_ok);
  uint32_t uint32_result;
  CHECK_EQ(napi_get_value_uint32(env, node_uint32_result, &uint32_result),
              napi_ok);
  EXPECT_EQ(uint32_result, 1);

  // int32
  napi_value node_int32_result;
  CHECK_EQ(napi_create_int32(env, -1, &node_int32_result) ,napi_ok);
  int32_t int32_result;
  CHECK_EQ(napi_get_value_int32(env, node_int32_result, &int32_result),
              napi_ok);
  EXPECT_EQ(int32_result, -1);

  // int64
  napi_value node_int64_result;
  CHECK_EQ(napi_create_int64(env, -1, &node_int64_result), napi_ok);
  int64_t int64_result;
  CHECK_EQ(napi_get_value_int64(env, node_int64_result, &int64_result),
              napi_ok);
  EXPECT_EQ(int64_result, -1);

  // double
  napi_value node_double_result;
  CHECK_EQ(napi_create_double(env, 0.1, &node_double_result), napi_ok);
  double double_result;
  CHECK_EQ(napi_get_value_double(env, node_double_result, &double_result),
           napi_ok);
  EXPECT_EQ(double_result, 0.1);
}

static int globalPromiseValue = 0;
TEST_F(NodeApiTestEnv, promise) {
  v8::HandleScope handleScope(isolate_);

  ASSERT_TRUE(isolate_->GetMicrotasksPolicy() ==
              v8::MicrotasksPolicy::kExplicit);

  napi_value node_promise;
  napi_deferred node_deferred;
  CHECK_EQ(napi_create_promise(env, &node_deferred, &node_promise), napi_ok);

  bool is_promise;
  CHECK_EQ(napi_is_promise(env, node_promise, &is_promise), napi_ok);
  EXPECT_TRUE(is_promise);

  napi_value node_resolution;
  CHECK_EQ(napi_create_int32(env, 1, &node_resolution) ,napi_ok);
  CHECK_EQ(napi_resolve_deferred(env, node_deferred, node_resolution), napi_ok);

  CHECK_EQ(napi_promise_then_resolve(env,
                    node_promise,
                    [](napi_env env, napi_callback_info info) -> napi_value {

                    globalPromiseValue++;

                    size_t argc = 1;
                    napi_value argv[1];
                    CHECK_EQ(napi_get_cb_info(
                                 env, info, &argc, argv, nullptr, nullptr),
                             napi_ok);

                    int result;
                    CHECK_EQ(napi_get_value_int32(env, argv[0], &result),
                             napi_ok);
                    EXPECT_TRUE(result == 1);
                    return argv[0];
                    }),napi_ok);

  EXPECT_EQ(globalPromiseValue, 0);
  // perform Microtask 
  isolate_->PerformMicrotaskCheckpoint();
  EXPECT_EQ(globalPromiseValue, 1);

  CHECK_EQ(napi_promise_then(
               env,
               node_promise,
               [](napi_env env, napi_callback_info info) -> napi_value {
                 globalPromiseValue++;

                 size_t argc = 1;
                 napi_value argv[1];
                 CHECK_EQ(
                     napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr),
                     napi_ok);

                 int result;
                 CHECK_EQ(napi_get_value_int32(env, argv[0], &result), napi_ok);
                 EXPECT_TRUE(result == 1);

                 return nullptr;
               },
               [](napi_env env, napi_callback_info info) -> napi_value {
                 globalPromiseValue += 2;
                 return nullptr;
               }),
           napi_ok);

  EXPECT_EQ(globalPromiseValue, 1);
  // perform Microtask
  isolate_->PerformMicrotaskCheckpoint();
  EXPECT_EQ(globalPromiseValue, 2);

  napi_value node_promise2;
  napi_deferred node_deferred2;
  CHECK_EQ(napi_create_promise(env, &node_deferred2, &node_promise2), napi_ok);

  bool is_promise2;
  CHECK_EQ(napi_is_promise(env, node_promise2, &is_promise2), napi_ok);
  EXPECT_TRUE(is_promise2);

  napi_value node_resolution2;
  CHECK_EQ(napi_create_int32(env, -1, &node_resolution2), napi_ok);
  CHECK_EQ(napi_reject_deferred(env, node_deferred2, node_resolution2), napi_ok);

   
  CHECK_EQ(napi_promise_then(
               env,
               node_promise2,
               [](napi_env env, napi_callback_info info) -> napi_value {
                 globalPromiseValue++;
                 return nullptr;
               },
               [](napi_env env, napi_callback_info info) -> napi_value {
                 globalPromiseValue += 2;
                 size_t argc = 1;
                 napi_value argv[1];
                 CHECK_EQ(
                     napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr),
                     napi_ok);

                 int result;
                 CHECK_EQ(napi_get_value_int32(env, argv[0], &result), napi_ok);
                 EXPECT_TRUE(result == -1);
                 return argv[0];
               }),
           napi_ok);

  EXPECT_EQ(globalPromiseValue, 2);
  // perform Microtask
  isolate_->PerformMicrotaskCheckpoint();
  EXPECT_EQ(globalPromiseValue, 4);

  CHECK_EQ(napi_promise_catch(
               env,
               node_promise2,
               [](napi_env env, napi_callback_info info) -> napi_value {
                 globalPromiseValue += 2;

                 size_t argc = 1;
                 napi_value argv[1];
                 CHECK_EQ(
                     napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr),
                     napi_ok);

                 int result;
                 CHECK_EQ(napi_get_value_int32(env, argv[0], &result), napi_ok);
                 EXPECT_TRUE(result == -1);

                 return nullptr;
               }),
           napi_ok);

  EXPECT_EQ(globalPromiseValue, 4);
  // perform Microtask
  isolate_->PerformMicrotaskCheckpoint();
  EXPECT_EQ(globalPromiseValue, 6);
}
