#include <js_native_api.h>
#include "../common.h"
#include <string.h>

// these tests validate the handle scope functions in the normal
// flow.  Forcing gc behavior to fully validate they are doing
// the right right thing would be quite hard so we keep it
// simple for now.

static node_api_value NewScope(node_api_env env, node_api_callback_info info) {
  node_api_handle_scope scope;
  node_api_value output = NULL;

  NODE_API_CALL(env, node_api_open_handle_scope(env, &scope));
  NODE_API_CALL(env, node_api_create_object(env, &output));
  NODE_API_CALL(env, node_api_close_handle_scope(env, scope));
  return NULL;
}

static node_api_value
NewScopeEscape(node_api_env env, node_api_callback_info info) {
  node_api_escapable_handle_scope scope;
  node_api_value output = NULL;
  node_api_value escapee = NULL;

  NODE_API_CALL(env, node_api_open_escapable_handle_scope(env, &scope));
  NODE_API_CALL(env, node_api_create_object(env, &output));
  NODE_API_CALL(env, node_api_escape_handle(env, scope, output, &escapee));
  NODE_API_CALL(env, node_api_close_escapable_handle_scope(env, scope));
  return escapee;
}

static node_api_value
NewScopeEscapeTwice(node_api_env env, node_api_callback_info info) {
  node_api_escapable_handle_scope scope;
  node_api_value output = NULL;
  node_api_value escapee = NULL;
  node_api_status status;

  NODE_API_CALL(env, node_api_open_escapable_handle_scope(env, &scope));
  NODE_API_CALL(env, node_api_create_object(env, &output));
  NODE_API_CALL(env, node_api_escape_handle(env, scope, output, &escapee));
  status = node_api_escape_handle(env, scope, output, &escapee);
  NODE_API_ASSERT(env, status == node_api_escape_called_twice,
      "Escaping twice fails");
  NODE_API_CALL(env, node_api_close_escapable_handle_scope(env, scope));
  return NULL;
}

static node_api_value
NewScopeWithException(node_api_env env, node_api_callback_info info) {
  node_api_handle_scope scope;
  size_t argc;
  node_api_value exception_function;
  node_api_status status;
  node_api_value output = NULL;

  NODE_API_CALL(env, node_api_open_handle_scope(env, &scope));
  NODE_API_CALL(env, node_api_create_object(env, &output));

  argc = 1;
  NODE_API_CALL(env, node_api_get_cb_info(
      env, info, &argc, &exception_function, NULL, NULL));

  status = node_api_call_function(
      env, output, exception_function, 0, NULL, NULL);
  NODE_API_ASSERT(env, status == node_api_pending_exception,
      "Function should have thrown.");

  // Closing a handle scope should still work while an exception is pending.
  NODE_API_CALL(env, node_api_close_handle_scope(env, scope));
  return NULL;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor properties[] = {
    DECLARE_NODE_API_PROPERTY("NewScope", NewScope),
    DECLARE_NODE_API_PROPERTY("NewScopeEscape", NewScopeEscape),
    DECLARE_NODE_API_PROPERTY("NewScopeEscapeTwice", NewScopeEscapeTwice),
    DECLARE_NODE_API_PROPERTY("NewScopeWithException", NewScopeWithException),
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(properties) / sizeof(*properties), properties));

  return exports;
}
EXTERN_C_END
