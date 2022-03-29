#define NAPI_EXPERIMENTAL
#include <js_native_api.h>
#include "../common.h"
#include "testobject.h"

napi_value CreateObject(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {};
  NODE_API_CALL(env,
                napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  napi_value instance;
  NODE_API_CALL(env, TestObject::NewInstance(env, args[0], &instance));

  return instance;
}

napi_value SetFinalizerErrorHandler(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {};
  NODE_API_CALL(env,
                napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  NODE_API_CALL(env, node_api_set_finalizer_error_handler(env, args[0]));

  napi_value undefined;
  NODE_API_CALL(env, napi_get_undefined(env, &undefined));
  return undefined;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  NODE_API_CALL(env, TestObject::Init(env));

  napi_property_descriptor descriptors[] = {
      DECLARE_NODE_API_GETTER("finalizeCount", TestObject::GetFinalizeCount),
      DECLARE_NODE_API_PROPERTY("createObject", CreateObject),
      DECLARE_NODE_API_PROPERTY("setFinalizerErrorHandler",
                                SetFinalizerErrorHandler),
  };

  NODE_API_CALL(
      env,
      napi_define_properties(env,
                             exports,
                             sizeof(descriptors) / sizeof(*descriptors),
                             descriptors));

  return exports;
}
EXTERN_C_END
