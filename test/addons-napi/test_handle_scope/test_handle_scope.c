#include <node_api.h>
#include "../common.h"
#include <string.h>

// these tests validate the handle scope functions in the normal
// flow.  Forcing gc behavior to fully validate they are doing
// the right right thing would be quite hard so we keep it
// simple for now.

napi_value NewScope(napi_env env, napi_callback_info info) {
  napi_handle_scope scope;
  napi_value output = NULL;

  NAPI_CALL(env, napi_open_handle_scope(env, &scope));
  NAPI_CALL(env, napi_create_object(env, &output));
  NAPI_CALL(env, napi_close_handle_scope(env, scope));
  return NULL;
}

napi_value NewScopeEscape(napi_env env, napi_callback_info info) {
  napi_escapable_handle_scope scope;
  napi_value output = NULL;
  napi_value escapee = NULL;

  NAPI_CALL(env, napi_open_escapable_handle_scope(env, &scope));
  NAPI_CALL(env, napi_create_object(env, &output));
  NAPI_CALL(env, napi_escape_handle(env, scope, output, &escapee));
  NAPI_CALL(env, napi_close_escapable_handle_scope(env, scope));
  return escapee;
}

napi_value NewScopeEscapeTwice(napi_env env, napi_callback_info info) {
  napi_escapable_handle_scope scope;
  napi_value output = NULL;
  napi_value escapee = NULL;
  napi_status status;

  NAPI_CALL(env, napi_open_escapable_handle_scope(env, &scope));
  NAPI_CALL(env, napi_create_object(env, &output));
  NAPI_CALL(env, napi_escape_handle(env, scope, output, &escapee));
  status = napi_escape_handle(env, scope, output, &escapee);
  NAPI_ASSERT(env, status == napi_escape_called_twice, "Escaping twice fails");
  NAPI_CALL(env, napi_close_escapable_handle_scope(env, scope));
  return NULL;
}

napi_value NewScopeWithException(napi_env env, napi_callback_info info) {
  napi_handle_scope scope;
  size_t argc;
  napi_value exception_function;
  napi_status status;
  napi_value output = NULL;

  NAPI_CALL(env, napi_open_handle_scope(env, &scope));
  NAPI_CALL(env, napi_create_object(env, &output));

  argc = 1;
  NAPI_CALL(env, napi_get_cb_info(
      env, info, &argc, &exception_function, NULL, NULL));

  status = napi_call_function(
      env, output, exception_function, 0, NULL, NULL);
  NAPI_ASSERT(env, status == napi_pending_exception,
      "Function should have thrown.");

  // Closing a handle scope should still work while an exception is pending.
  NAPI_CALL(env, napi_close_handle_scope(env, scope));
  return NULL;
}

napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor properties[] = {
    DECLARE_NAPI_PROPERTY("NewScope", NewScope),
    DECLARE_NAPI_PROPERTY("NewScopeEscape", NewScopeEscape),
    DECLARE_NAPI_PROPERTY("NewScopeEscapeTwice", NewScopeEscapeTwice),
    DECLARE_NAPI_PROPERTY("NewScopeWithException", NewScopeWithException),
  };

  NAPI_CALL(env, napi_define_properties(
      env, exports, sizeof(properties) / sizeof(*properties), properties));

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
