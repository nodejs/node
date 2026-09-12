#include <stdio.h>
#include <cstdio>
#include <string>
#include "env-inl.h"
#include "gtest/gtest.h"
#include "node_api_internals.h"
#include "node_binding.h"
#include "node_test_fixture.h"

using v8::Local;
using v8::Object;

static napi_env addon_env;

class NodeApiTest : public EnvironmentTestFixture {
 private:
  void SetUp() override { EnvironmentTestFixture::SetUp(); }

  void TearDown() override { EnvironmentTestFixture::TearDown(); }
};

TEST_F(NodeApiTest, CreateNodeApiEnv) {
  const v8::HandleScope handle_scope(isolate_);
  Argv argv;

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
      exports_obj, module_obj, env->context(), init, NAPI_VERSION);
  ASSERT_NE(addon_env, nullptr);
  v8impl::NodeApiEnv* internal_env =
      static_cast<v8impl::NodeApiEnv*>(addon_env);
  EXPECT_EQ(internal_env->node_env(), env);
}

namespace {

struct ContextCheckState {
  napi_async_work work = nullptr;
  bool complete_called = false;
  bool complete_in_own_context = false;
  bool call_js_called = false;
  bool call_js_in_own_context = false;
  bool tsfn_finalize_called = false;
  bool tsfn_finalize_in_own_context = false;
};

bool InOwnContext(napi_env env) {
  node_napi_env internal_env = reinterpret_cast<node_napi_env>(env);
  return internal_env->isolate->GetCurrentContext() == internal_env->context();
}

}  // namespace

TEST_F(NodeApiTest, AsyncCallbacksEnterOwnContext) {
  const v8::HandleScope handle_scope(isolate_);
  Argv argv;

  Env env1{handle_scope, argv};
  node::LoadEnvironment(*env1, "");
  Env env2{handle_scope, argv, node::EnvironmentFlags::kNoFlags};
  node::LoadEnvironment(*env2, "");
  ASSERT_EQ(isolate_->GetCurrentContext(), env2.context());

  ContextCheckState state;
  {
    v8::Context::Scope context_scope(env1.context());
    napi_addon_register_func init = [](napi_env env, napi_value exports) {
      addon_env = env;
      return exports;
    };
    addon_env = nullptr;
    napi_module_register_by_symbol(Object::New(isolate_),
                                   Object::New(isolate_),
                                   env1.context(),
                                   init,
                                   NAPI_VERSION);
    ASSERT_NE(addon_env, nullptr);

    napi_value resource_name;
    ASSERT_EQ(napi_create_string_utf8(
                  addon_env, "cctest", NAPI_AUTO_LENGTH, &resource_name),
              napi_ok);

    ASSERT_EQ(napi_create_async_work(
                  addon_env,
                  nullptr,
                  resource_name,
                  [](napi_env env, void* data) {},
                  [](napi_env env, napi_status status, void* data) {
                    auto* state = static_cast<ContextCheckState*>(data);
                    state->complete_called = true;
                    state->complete_in_own_context = InOwnContext(env);
                    napi_delete_async_work(env, state->work);
                  },
                  &state,
                  &state.work),
              napi_ok);
    ASSERT_EQ(napi_queue_async_work(addon_env, state.work), napi_ok);

    napi_threadsafe_function tsfn;
    ASSERT_EQ(napi_create_threadsafe_function(
                  addon_env,
                  nullptr,
                  nullptr,
                  resource_name,
                  0,
                  1,
                  &state,
                  [](napi_env env, void* finalize_data, void* hint) {
                    auto* state =
                        static_cast<ContextCheckState*>(finalize_data);
                    state->tsfn_finalize_called = true;
                    state->tsfn_finalize_in_own_context = InOwnContext(env);
                  },
                  &state,
                  [](napi_env env, napi_value cb, void* context, void* data) {
                    auto* state = static_cast<ContextCheckState*>(context);
                    state->call_js_called = true;
                    state->call_js_in_own_context = InOwnContext(env);
                  },
                  &tsfn),
              napi_ok);
    ASSERT_EQ(napi_call_threadsafe_function(tsfn, nullptr, napi_tsfn_blocking),
              napi_ok);
    ASSERT_EQ(napi_release_threadsafe_function(tsfn, napi_tsfn_release),
              napi_ok);
  }

  ASSERT_EQ(isolate_->GetCurrentContext(), env2.context());
  uv_run(&current_loop, UV_RUN_DEFAULT);

  EXPECT_TRUE(state.complete_called);
  EXPECT_TRUE(state.complete_in_own_context);
  EXPECT_TRUE(state.call_js_called);
  EXPECT_TRUE(state.call_js_in_own_context);
  EXPECT_TRUE(state.tsfn_finalize_called);
  EXPECT_TRUE(state.tsfn_finalize_in_own_context);
  EXPECT_EQ(isolate_->GetCurrentContext(), env2.context());
}
