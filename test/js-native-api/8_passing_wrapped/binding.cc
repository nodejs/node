#include <js_native_api.h>
#include "myobject.h"
#include "../common.h"

extern size_t finalize_count;

static node_api_value
CreateObject(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  node_api_value instance;
  NODE_API_CALL(env, MyObject::NewInstance(env, args[0], &instance));

  return instance;
}

static node_api_value Add(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value args[2];
  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  MyObject* obj1;
  NODE_API_CALL(env,
      node_api_unwrap(env, args[0], reinterpret_cast<void**>(&obj1)));

  MyObject* obj2;
  NODE_API_CALL(env,
      node_api_unwrap(env, args[1], reinterpret_cast<void**>(&obj2)));

  node_api_value sum;
  NODE_API_CALL(env,
      node_api_create_double(env, obj1->Val() + obj2->Val(), &sum));

  return sum;
}

static node_api_value
FinalizeCount(node_api_env env, node_api_callback_info info) {
  node_api_value return_value;
  NODE_API_CALL(env,
      node_api_create_uint32(env, finalize_count, &return_value));
  return return_value;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  MyObject::Init(env);

  node_api_property_descriptor desc[] = {
    DECLARE_NODE_API_PROPERTY("createObject", CreateObject),
    DECLARE_NODE_API_PROPERTY("add", Add),
    DECLARE_NODE_API_PROPERTY("finalizeCount", FinalizeCount),
  };

  NODE_API_CALL(env, node_api_define_properties(env,
                                                exports,
                                                sizeof(desc) / sizeof(*desc),
                                                desc));

  return exports;
}
EXTERN_C_END
