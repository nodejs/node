#include "embedtest_node_api.h"

#include <atomic>
#include <cstdio>
#include <cstring>

static napi_value NAPI_CDECL InitGreeterModule(node_embedding_runtime runtime,
                                               void* cb_data,
                                               napi_env env,
                                               const char* module_name,
                                               napi_value exports) {
  std::atomic<int32_t>* counter_ptr =
      static_cast<std::atomic<int32_t>*>(cb_data);
  counter_ptr->fetch_add(1);

  napi_value greet_func{};
  napi_create_function(
      env,
      "greet",
      NAPI_AUTO_LENGTH,
      [](napi_env env, napi_callback_info info) -> napi_value {
        std::string greeting = "Hello, ";
        napi_value arg;
        size_t arg_count = 1;
        napi_get_cb_info(env, info, &arg_count, &arg, nullptr, nullptr);
        AddUtf8String(greeting, env, arg);
        napi_value result;
        napi_create_string_utf8(
            env, greeting.c_str(), greeting.size(), &result);
        return result;
      },
      nullptr,
      &greet_func);
  napi_set_named_property(env, exports, "greet", greet_func);
  return exports;
}

static napi_value NAPI_CDECL
InitReplicatorModule(node_embedding_runtime runtime,
                     void* cb_data,
                     napi_env env,
                     const char* module_name,
                     napi_value exports) {
  std::atomic<int32_t>* counter_ptr =
      static_cast<std::atomic<int32_t>*>(cb_data);
  counter_ptr->fetch_add(1);

  napi_value greet_func{};
  napi_create_function(
      env,
      "replicate",
      NAPI_AUTO_LENGTH,
      [](napi_env env, napi_callback_info info) -> napi_value {
        std::string str;
        napi_value arg;
        size_t arg_count = 1;
        napi_get_cb_info(env, info, &arg_count, &arg, nullptr, nullptr);
        AddUtf8String(str, env, arg);
        str += " " + str;
        napi_value result;
        napi_create_string_utf8(env, str.c_str(), str.size(), &result);
        return result;
      },
      nullptr,
      &greet_func);
  napi_set_named_property(env, exports, "replicate", greet_func);
  return exports;
}

extern "C" int32_t test_main_linked_modules_node_api(int32_t argc,
                                                     char* argv[]) {
  CHECK_TRUE(argc == 4);
  int32_t expectedGreeterModuleInitCallCount = atoi(argv[2]);
  int32_t expectedReplicatorModuleInitCallCount = atoi(argv[2]);

  std::atomic<int32_t> greeterModuleInitCallCount{0};
  std::atomic<int32_t> replicatorModuleInitCallCount{0};

  CHECK(node_embedding_on_error(HandleTestError, argv[0]));

  node_embedding_platform platform;
  CHECK(node_embedding_create_platform(NODE_EMBEDDING_VERSION, &platform));
  CHECK(node_embedding_platform_set_args(platform, argc, argv));
  bool early_return = false;
  CHECK(node_embedding_platform_initialize(platform, &early_return));
  if (early_return) {
    return 0;
  }

  node_embedding_runtime runtime;
  CHECK(node_embedding_create_runtime(platform, &runtime));
  CHECK(node_embedding_runtime_set_node_api_version(runtime, NAPI_VERSION));

  CHECK(node_embedding_runtime_on_preload(
      runtime,
      [](node_embedding_runtime runtime,
         void* /*cb_data*/,
         napi_env env,
         napi_value process,
         napi_value /*require*/
      ) {
        napi_value global;
        napi_get_global(env, &global);
        napi_set_named_property(env, global, "process", process);
      },
      nullptr));

  CHECK(node_embedding_runtime_add_module(runtime,
                                          "greeter_module",
                                          &InitGreeterModule,
                                          &greeterModuleInitCallCount,
                                          NAPI_VERSION));
  CHECK(node_embedding_runtime_add_module(runtime,
                                          "replicator_module",
                                          &InitReplicatorModule,
                                          &replicatorModuleInitCallCount,
                                          NAPI_VERSION));

  CHECK(node_embedding_runtime_initialize(runtime, main_script));

  CHECK(node_embedding_runtime_complete_event_loop(runtime));
  CHECK(node_embedding_delete_runtime(runtime));
  CHECK(node_embedding_delete_platform(platform));

  CHECK_TRUE(greeterModuleInitCallCount == expectedGreeterModuleInitCallCount);
  CHECK_TRUE(replicatorModuleInitCallCount ==
             expectedReplicatorModuleInitCallCount);
  return 0;
}

extern "C" int32_t test_main_modules_node_api(int32_t argc, char* argv[]) {
  /*
  if (argc < 3) {
    fprintf(stderr, "node_api_modules <cjs.cjs> <es6.mjs>\n");
    return 2;
  }

  CHECK(node_embedding_on_error(HandleTestError, argv[0]));

  node_embedding_platform platform;
  CHECK(node_embedding_create_platform(NODE_EMBEDDING_VERSION, &platform));
  CHECK(node_embedding_platform_set_args(platform, argc, argv));
  bool early_return = false;
  CHECK(node_embedding_platform_initialize(platform, &early_return));
  if (early_return) {
    return 0;
  }

  node_embedding_runtime runtime;
  CHECK(node_embedding_create_runtime(platform, &runtime));
  CHECK(node_embedding_runtime_set_node_api_version(runtime, NAPI_VERSION));
  CHECK(node_embedding_runtime_initialize_from_script(runtime, nullptr));
  int32_t exit_code = 0;
  CHECK(InvokeNodeApi(runtime, [&](napi_env env) {
    napi_value global, import_name, require_name, import, require, cjs, es6,
        value;
    NODE_API_CALL(napi_get_global(env, &global));
    NODE_API_CALL(
        napi_create_string_utf8(env, "import", strlen("import"), &import_name));
    NODE_API_CALL(napi_create_string_utf8(
        env, "require", strlen("require"), &require_name));
    NODE_API_CALL(napi_get_property(env, global, import_name, &import));
    NODE_API_CALL(napi_get_property(env, global, require_name, &require));

    NODE_API_CALL(napi_create_string_utf8(env, argv[1], strlen(argv[1]), &cjs));
    NODE_API_CALL(napi_create_string_utf8(env, argv[2], strlen(argv[2]), &es6));
    NODE_API_CALL(
        napi_create_string_utf8(env, "value", strlen("value"), &value));

    napi_value es6_module, es6_promise, cjs_module, es6_result, cjs_result;
    char buffer[32];
    size_t bufferlen;

    NODE_API_CALL(
        napi_call_function(env, global, import, 1, &es6, &es6_promise));
    node_embedding_promise_state es6_promise_state;
    CHECK_RETURN_VOID(node_embedding_runtime_await_promise(
        runtime, es6_promise, &es6_promise_state, &es6_module, nullptr));

    NODE_API_CALL(napi_get_property(env, es6_module, value, &es6_result));
    NODE_API_CALL(napi_get_value_string_utf8(
        env, es6_result, buffer, sizeof(buffer), &bufferlen));
    if (strncmp(buffer, "genuine", bufferlen) != 0) {
      FAIL_RETURN_VOID("Unexpected value: %s\n", buffer);
    }

    NODE_API_CALL(
        napi_call_function(env, global, require, 1, &cjs, &cjs_module));
    NODE_API_CALL(napi_get_property(env, cjs_module, value, &cjs_result));
    NODE_API_CALL(napi_get_value_string_utf8(
        env, cjs_result, buffer, sizeof(buffer), &bufferlen));
    if (strncmp(buffer, "original", bufferlen) != 0) {
      FAIL_RETURN_VOID("Unexpected value: %s\n", buffer);
    }
  }));
  CHECK(exit_code);
  CHECK(node_embedding_delete_runtime(runtime));
  CHECK(node_embedding_delete_platform(platform));
*/
  return 0;
}
