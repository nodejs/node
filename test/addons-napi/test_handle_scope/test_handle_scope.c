#include <node_api.h>
#include "../common.h"
#include <string.h>

// these tests validate the handle scope functions in the normal
// flow.  Forcing gc behaviour to fully validate they are doing
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

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_property_descriptor properties[] = {
    DECLARE_NAPI_PROPERTY("NewScope", NewScope),
    DECLARE_NAPI_PROPERTY("NewScopeEscape", NewScopeEscape),
  };

  NAPI_CALL_RETURN_VOID(env, napi_define_properties(
    env, exports, sizeof(properties) / sizeof(*properties), properties));
}

NAPI_MODULE(addon, Init)
