#include <stdio.h>
#include <string.h>
#define NAPI_EXPERIMENTAL
#include <node_api.h>
#include <node_api_embedding.h>

#define CHECK(op, msg)                                                         \
  if (op != napi_ok) {                                                         \
    fprintf(stderr, "Failed: %s\n", msg);                                      \
    return -1;                                                                 \
  }

int main(int argc, char* argv[]) {
  napi_platform platform;

  if (argc < 3) {
    fprintf(stderr, "napi_modules <cjs.cjs> <es6.mjs>\n");
    return -2;
  }

  CHECK(napi_create_platform(0, NULL, NULL, &platform),
        "Failed creating the platform");

  napi_env env;
  CHECK(napi_create_environment(platform, NULL, NULL, NAPI_VERSION, &env),
        "Failed running JS");

  napi_handle_scope scope;
  CHECK(napi_open_handle_scope(env, &scope), "Failed creating a scope");

  napi_value global, import_name, require_name, import, require, cjs, es6,
      value;
  CHECK(napi_get_global(env, &global), "napi_get_global");
  CHECK(napi_create_string_utf8(env, "import", strlen("import"), &import_name),
        "create_string");
  CHECK(
      napi_create_string_utf8(env, "require", strlen("require"), &require_name),
      "create_string");
  CHECK(napi_get_property(env, global, import_name, &import), "import");
  CHECK(napi_get_property(env, global, require_name, &require), "require");

  CHECK(napi_create_string_utf8(env, argv[1], strlen(argv[1]), &cjs),
        "create_string");
  CHECK(napi_create_string_utf8(env, argv[2], strlen(argv[2]), &es6),
        "create_string");
  CHECK(napi_create_string_utf8(env, "value", strlen("value"), &value),
        "create_string");

  napi_value es6_module, es6_promise, cjs_module, es6_result, cjs_result;
  char buffer[32];
  size_t bufferlen;

  CHECK(napi_call_function(env, global, import, 1, &es6, &es6_promise),
        "import");
  CHECK(napi_await_promise(env, es6_promise, &es6_module), "await");

  CHECK(napi_get_property(env, es6_module, value, &es6_result), "value");
  CHECK(napi_get_value_string_utf8(
            env, es6_result, buffer, sizeof(buffer), &bufferlen),
        "string");
  if (strncmp(buffer, "genuine", bufferlen)) {
    fprintf(stderr, "Unexpected value: %s\n", buffer);
    return -1;
  }

  CHECK(napi_call_function(env, global, require, 1, &cjs, &cjs_module),
        "require");
  CHECK(napi_get_property(env, cjs_module, value, &cjs_result), "value");
  CHECK(napi_get_value_string_utf8(
            env, cjs_result, buffer, sizeof(buffer), &bufferlen),
        "string");
  if (strncmp(buffer, "original", bufferlen)) {
    fprintf(stderr, "Unexpected value: %s\n", buffer);
    return -1;
  }

  CHECK(napi_close_handle_scope(env, scope), "Failed destroying handle scope");
  CHECK(napi_destroy_environment(env, NULL), "destroy");
  CHECK(napi_destroy_platform(platform), "Failed destroying the platform");
  return 0;
}
