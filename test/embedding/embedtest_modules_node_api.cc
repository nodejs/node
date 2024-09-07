#include "embedtest_node_api.h"

#include <cstdio>
#include <cstring>

extern "C" int32_t test_main_modules_node_api(int32_t argc, char* argv[]) {
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
  CHECK(node_embedding_runtime_initialize(runtime, nullptr));
  napi_env env;
  CHECK(node_embedding_runtime_get_node_api_env(runtime, &env));

  CHECK(node_embedding_runtime_open_scope(runtime));

  napi_value global, import_name, require_name, import, require, cjs, es6,
      value;
  CHECK(napi_get_global(env, &global));
  CHECK(napi_create_string_utf8(env, "import", strlen("import"), &import_name));
  CHECK(napi_create_string_utf8(
      env, "require", strlen("require"), &require_name));
  CHECK(napi_get_property(env, global, import_name, &import));
  CHECK(napi_get_property(env, global, require_name, &require));

  CHECK(napi_create_string_utf8(env, argv[1], strlen(argv[1]), &cjs));
  CHECK(napi_create_string_utf8(env, argv[2], strlen(argv[2]), &es6));
  CHECK(napi_create_string_utf8(env, "value", strlen("value"), &value));

  napi_value es6_module, es6_promise, cjs_module, es6_result, cjs_result;
  char buffer[32];
  size_t bufferlen;

  CHECK(napi_call_function(env, global, import, 1, &es6, &es6_promise));
  CHECK(node_embedding_runtime_await_promise(
      runtime, es6_promise, &es6_module, nullptr));

  CHECK(napi_get_property(env, es6_module, value, &es6_result));
  CHECK(napi_get_value_string_utf8(
      env, es6_result, buffer, sizeof(buffer), &bufferlen));
  if (strncmp(buffer, "genuine", bufferlen) != 0) {
    FAIL("Unexpected value: %s\n", buffer);
  }

  CHECK(napi_call_function(env, global, require, 1, &cjs, &cjs_module));
  CHECK(napi_get_property(env, cjs_module, value, &cjs_result));
  CHECK(napi_get_value_string_utf8(
      env, cjs_result, buffer, sizeof(buffer), &bufferlen));
  if (strncmp(buffer, "original", bufferlen) != 0) {
    FAIL("Unexpected value: %s\n", buffer);
  }

  CHECK(node_embedding_runtime_close_scope(runtime));
  CHECK(node_embedding_delete_runtime(runtime));
  CHECK(node_embedding_delete_platform(platform));
  return 0;
}
