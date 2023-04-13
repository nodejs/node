#include <js_native_api.h>

#include "../common.h"
#include "test_null.h"

static napi_value CreatePromise(napi_env env, napi_callback_info info) {
  napi_value return_value, promise;
  napi_deferred deferred;

  NODE_API_CALL(env, napi_create_object(env, &return_value));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      napi_invalid_arg,
                      napi_create_promise(NULL, &deferred, &promise));

  napi_create_promise(env, NULL, &promise);
  add_last_status(env, "deferredIsNull", return_value);

  napi_create_promise(env, &deferred, NULL);
  add_last_status(env, "promiseIsNull", return_value);

  return return_value;
}

static napi_value test_resolution_api(napi_env env,
                                      napi_callback_info info,
                                      napi_status (*api)(napi_env,
                                                         napi_deferred,
                                                         napi_value)) {
  napi_value return_value, promise, undefined;
  napi_deferred deferred;

  NODE_API_CALL(env, napi_create_object(env, &return_value));
  NODE_API_CALL(env, napi_create_promise(env, &deferred, &promise));
  NODE_API_CALL(env, napi_get_undefined(env, &undefined));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      napi_invalid_arg,
                      api(NULL, deferred, undefined));

  api(env, NULL, undefined);
  add_last_status(env, "deferredIsNull", return_value);

  api(env, deferred, NULL);
  add_last_status(env, "valueIsNull", return_value);

  // We need to call the api will all parameters given because doing so frees a
  // reference the implementation keeps internally.
  napi_status status = api(env, deferred, undefined);
  if (status != napi_ok) {
    // This will make the test fail since the test doesn't expect that the
    // nothing-is-null case will be recorded.
    add_last_status(env, "nothingIsNull", return_value);
  }

  NODE_API_CALL(env,
                napi_set_named_property(env, return_value, "promise", promise));

  return return_value;
}

static napi_value ResolveDeferred(napi_env env, napi_callback_info info) {
  return test_resolution_api(env, info, napi_resolve_deferred);
}

static napi_value RejectDeferred(napi_env env, napi_callback_info info) {
  return test_resolution_api(env, info, napi_reject_deferred);
}

void init_test_null(napi_env env, napi_value exports) {
  napi_value test_null;

  const napi_property_descriptor test_null_props[] = {
      DECLARE_NODE_API_PROPERTY("createPromise", CreatePromise),
      DECLARE_NODE_API_PROPERTY("resolveDeferred", ResolveDeferred),
      DECLARE_NODE_API_PROPERTY("rejectDeferred", RejectDeferred),
  };

  NODE_API_CALL_RETURN_VOID(env, napi_create_object(env, &test_null));
  NODE_API_CALL_RETURN_VOID(
      env,
      napi_define_properties(env,
                             test_null,
                             sizeof(test_null_props) / sizeof(*test_null_props),
                             test_null_props));

  NODE_API_CALL_RETURN_VOID(
      env, napi_set_named_property(env, exports, "testNull", test_null));
}
