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

napi_value DrainFinalizingQueue(napi_env env, napi_callback_info info) {
  napi_value global;
  NODE_API_CALL(env, napi_get_global(env, &global));

  // Getting properties has a side effect of draining the finalizing queue.
  napi_value properties;
  NODE_API_CALL(env, napi_get_property_names(env, global, &properties));

  return properties;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  NODE_API_CALL(env, TestObject::Init(env));

  napi_property_descriptor descriptors[] = {
      DECLARE_NODE_API_GETTER("finalizeCount", TestObject::GetFinalizeCount),
      DECLARE_NODE_API_PROPERTY("createObject", CreateObject),
      DECLARE_NODE_API_PROPERTY("drainFinalizingQueue", DrainFinalizingQueue),
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
