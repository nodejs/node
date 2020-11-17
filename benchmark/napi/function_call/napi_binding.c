#include <assert.h>
#define NAPI_EXPERIMENTAL
#include <node_api.h>

static int32_t increment = 0;

static napi_value Hello(napi_env env, napi_callback_info info) {
  napi_value result;
  napi_status status = napi_create_int32(env, increment++, &result);
  assert(status == napi_ok);
  return result;
}

NAPI_MODULE_INIT() {
  napi_value hello, klass, subklass;

  // Create plain function and attach to exports.
  napi_status status =
      napi_create_function(env,
                           "hello",
                           NAPI_AUTO_LENGTH,
                           Hello,
                           NULL,
                           &hello);
  assert(status == napi_ok);
  status = napi_set_named_property(env, exports, "hello", hello);
  assert(status == napi_ok);

  // Create class and attach to exports.
  napi_property_descriptor prop =
    { "fn", NULL, Hello, NULL, NULL, NULL, napi_enumerable, NULL };
  status = napi_define_class(env,
                             "Class",
                             NAPI_AUTO_LENGTH,
                             Hello,
                             NULL,
                             1,
                             &prop,
                             &klass);
  assert (status == napi_ok);
  status = napi_set_named_property(env, exports, "Class", klass);
  assert(status == napi_ok);

  // Create subclass and attach to exports.
  status = napi_define_subclass(env,
                                NULL,
                                "Class",
                                NAPI_AUTO_LENGTH,
                                Hello,
                                NULL,
                                NULL,
                                1,
                                &prop,
                                &subklass);
  assert (status == napi_ok);
  status = napi_set_named_property(env, exports, "Subclass", subklass);
  assert(status == napi_ok);

  return exports;
}
