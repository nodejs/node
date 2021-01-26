#ifndef TEST_JS_NATIVE_API_7_FACTORY_WRAP_MYOBJECT_H_
#define TEST_JS_NATIVE_API_7_FACTORY_WRAP_MYOBJECT_H_

#include <js_native_api.h>

class MyObject {
 public:
  static node_api_status Init(node_api_env env);
  static void
  Destructor(node_api_env env, void* nativeObject, void* finalize_hint);
  static node_api_value
  GetFinalizeCount(node_api_env env, node_api_callback_info info);
  static node_api_status NewInstance(node_api_env env,
                                     node_api_value arg,
                                     node_api_value* instance);

 private:
  MyObject();
  ~MyObject();

  static node_api_ref constructor;
  static node_api_value New(node_api_env env, node_api_callback_info info);
  static node_api_value PlusOne(node_api_env env, node_api_callback_info info);
  uint32_t counter_;
  node_api_env env_;
  node_api_ref wrapper_;
};

#endif  // TEST_JS_NATIVE_API_7_FACTORY_WRAP_MYOBJECT_H_
