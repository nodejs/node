#ifndef TEST_JS_NATIVE_API_TEST_FINALIZER_EXCEPTION_TESTOBJECT_H_
#define TEST_JS_NATIVE_API_TEST_FINALIZER_EXCEPTION_TESTOBJECT_H_

#include <js_native_api.h>

class TestObject {
 public:
  static napi_status Init(napi_env env);
  static void Destructor(napi_env env, void* nativeObject, void* finalize_hint);
  static napi_value GetFinalizeCount(napi_env env, napi_callback_info info);
  static napi_status NewInstance(napi_env env,
                                 napi_value arg,
                                 napi_value* instance);

 private:
  explicit TestObject(bool throw_js_in_destructor);
  ~TestObject();

  static napi_value New(napi_env env, napi_callback_info info);

 private:
  static napi_ref constructor;
  napi_env env_;
  napi_ref wrapper_;
  bool throw_js_in_destructor_{false};
};

#endif  // TEST_JS_NATIVE_API_TEST_FINALIZER_EXCEPTION_TESTOBJECT_H_
