#ifndef TEST_JS_NATIVE_API_6_OBJECT_WRAP_NESTED_WRAP_H_
#define TEST_JS_NATIVE_API_6_OBJECT_WRAP_NESTED_WRAP_H_

#include <js_native_api.h>

/**
 * Test that an napi_ref can be nested inside another ObjectWrap.
 *
 * This test shows a critical case where a finalizer deletes an napi_ref
 * whose finalizer is also scheduled.
 */

class NestedWrap {
 public:
  static void Init(napi_env env, napi_value exports);
  static void Destructor(node_api_basic_env env,
                         void* nativeObject,
                         void* finalize_hint);

 private:
  explicit NestedWrap();
  ~NestedWrap();

  static napi_value New(napi_env env, napi_callback_info info);

  static napi_ref constructor;

  napi_env env_{};
  napi_ref wrapper_{};
  napi_ref nested_{};
};

#endif  // TEST_JS_NATIVE_API_6_OBJECT_WRAP_NESTED_WRAP_H_
