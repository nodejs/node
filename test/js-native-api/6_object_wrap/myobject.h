#ifndef TEST_JS_NATIVE_API_6_OBJECT_WRAP_MYOBJECT_H_
#define TEST_JS_NATIVE_API_6_OBJECT_WRAP_MYOBJECT_H_

#include <js_native_api.h>

class MyObject {
 public:
  static void Init(node_api_env env, node_api_value exports);
  static void
  Destructor(node_api_env env, void* nativeObject, void* finalize_hint);

 private:
  explicit MyObject(double value_ = 0);
  ~MyObject();

  static node_api_value New(node_api_env env, node_api_callback_info info);
  static node_api_value
  GetValue(node_api_env env, node_api_callback_info info);
  static node_api_value
  SetValue(node_api_env env, node_api_callback_info info);
  static node_api_value PlusOne(node_api_env env, node_api_callback_info info);
  static node_api_value
  Multiply(node_api_env env, node_api_callback_info info);
  static node_api_ref constructor;
  double value_;
  node_api_env env_;
  node_api_ref wrapper_;
};
#endif  // TEST_JS_NATIVE_API_6_OBJECT_WRAP_MYOBJECT_H_
