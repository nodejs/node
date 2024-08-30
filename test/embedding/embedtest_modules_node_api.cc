#include "embedtest_node_api.h"

#include <stdio.h>
#include <string.h>

extern "C" int32_t test_main_modules_node_api(int32_t argc, char* argv[]) {
  if (argc < 3) {
    fprintf(stderr, "node_api_modules <cjs.cjs> <es6.mjs>\n");
    return 2;
  }

  CHECK(node_api_init_once_per_process(
      argc, argv, node_api_platform_no_flags, NULL, NULL, NULL, NULL));

  node_api_env_options options;
  CHECK(node_api_create_env_options(&options));
  napi_env env;
  CHECK(node_api_create_env(options, NULL, NULL, NULL, NAPI_VERSION, &env));

  CHECK(node_api_open_env_scope(env));

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
  CHECK(node_api_await_promise(env, es6_promise, &es6_module));

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

  CHECK(node_api_close_env_scope(env));
  CHECK(node_api_delete_env(env, NULL));
  CHECK(node_api_uninit_once_per_process());
  return 0;
}
