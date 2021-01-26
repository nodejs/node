#include <js_native_api.h>
#include "../common.h"

static node_api_value
TestCreateFunctionParameters(node_api_env env, node_api_callback_info info) {
  node_api_status status;
  node_api_value result, return_value;

  NODE_API_CALL(env, node_api_create_object(env, &return_value));

  status = node_api_create_function(NULL,
                                    "TrackedFunction",
                                    NODE_API_AUTO_LENGTH,
                                    TestCreateFunctionParameters,
                                    NULL,
                                    &result);

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      node_api_invalid_arg,
                      status);

  node_api_create_function(env,
                           NULL,
                           NODE_API_AUTO_LENGTH,
                           TestCreateFunctionParameters,
                           NULL,
                           &result);

  add_last_status(env, "nameIsNull", return_value);

  node_api_create_function(env,
                           "TrackedFunction",
                           NODE_API_AUTO_LENGTH,
                           NULL,
                           NULL,
                           &result);

  add_last_status(env, "cbIsNull", return_value);

  node_api_create_function(env,
                           "TrackedFunction",
                           NODE_API_AUTO_LENGTH,
                           TestCreateFunctionParameters,
                           NULL,
                           NULL);

  add_last_status(env, "resultIsNull", return_value);

  return return_value;
}

static node_api_value
TestCallFunction(node_api_env env, node_api_callback_info info) {
  size_t argc = 10;
  node_api_value args[10];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc > 0, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == node_api_function,
      "Wrong type of arguments. Expects a function as first argument.");

  node_api_value* argv = args + 1;
  argc = argc - 1;

  node_api_value global;
  NODE_API_CALL(env, node_api_get_global(env, &global));

  node_api_value result;
  NODE_API_CALL(env,
      node_api_call_function(env, global, args[0], argc, argv, &result));

  return result;
}

static node_api_value
TestFunctionName(node_api_env env, node_api_callback_info info) {
  return NULL;
}

static void finalize_function(node_api_env env, void* data, void* hint) {
  node_api_ref ref = data;

  // Retrieve the JavaScript undefined value.
  node_api_value undefined;
  NODE_API_CALL_RETURN_VOID(env, node_api_get_undefined(env, &undefined));

  // Retrieve the JavaScript function we must call.
  node_api_value js_function;
  NODE_API_CALL_RETURN_VOID(env,
      node_api_get_reference_value(env, ref, &js_function));

  // Call the JavaScript function to indicate that the generated JavaScript
  // function is about to be gc-ed.
  NODE_API_CALL_RETURN_VOID(env,
      node_api_call_function(env, undefined, js_function, 0, NULL, NULL));

  // Destroy the persistent reference to the function we just called so as to
  // properly clean up.
  NODE_API_CALL_RETURN_VOID(env, node_api_delete_reference(env, ref));
}

static node_api_value
MakeTrackedFunction(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value js_finalize_cb;
  node_api_valuetype arg_type;

  // Retrieve and validate from the arguments the function we will use to
  // indicate to JavaScript that the function we are about to create is about to
  // be gc-ed.
  NODE_API_CALL(env, node_api_get_cb_info(env,
                                          info,
                                          &argc,
                                          &js_finalize_cb,
                                          NULL,
                                          NULL));
  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments");
  NODE_API_CALL(env, node_api_typeof(env, js_finalize_cb, &arg_type));
  NODE_API_ASSERT(env, arg_type == node_api_function,
      "Argument must be a function");

  // Dynamically create a function.
  node_api_value result;
  NODE_API_CALL(env, node_api_create_function(env,
                                              "TrackedFunction",
                                              NODE_API_AUTO_LENGTH,
                                              TestFunctionName,
                                              NULL,
                                              &result));

  // Create a strong reference to the function we will call when the tracked
  // function is about to be gc-ed.
  node_api_ref js_finalize_cb_ref;
  NODE_API_CALL(env, node_api_create_reference(env,
                                               js_finalize_cb,
                                               1,
                                               &js_finalize_cb_ref));

  // Attach a finalizer to the dynamically created function and pass it the
  // strong reference we created in the previous step.
  NODE_API_CALL(env, node_api_wrap(env,
                                   result,
                                   js_finalize_cb_ref,
                                   finalize_function,
                                   NULL,
                                   NULL));

  return result;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_value fn1;
  NODE_API_CALL(env, node_api_create_function(
      env, NULL, NODE_API_AUTO_LENGTH, TestCallFunction, NULL, &fn1));

  node_api_value fn2;
  NODE_API_CALL(env, node_api_create_function(
      env, "Name", NODE_API_AUTO_LENGTH, TestFunctionName, NULL, &fn2));

  node_api_value fn3;
  NODE_API_CALL(env, node_api_create_function(
      env, "Name_extra", 5, TestFunctionName, NULL, &fn3));

  node_api_value fn4;
  NODE_API_CALL(env, node_api_create_function(env,
                                              "MakeTrackedFunction",
                                              NODE_API_AUTO_LENGTH,
                                              MakeTrackedFunction,
                                              NULL,
                                              &fn4));

  node_api_value fn5;
  NODE_API_CALL(env, node_api_create_function(env,
                                              "TestCreateFunctionParameters",
                                              NODE_API_AUTO_LENGTH,
                                              TestCreateFunctionParameters,
                                              NULL,
                                              &fn5));

  NODE_API_CALL(env,
      node_api_set_named_property(env, exports, "TestCall", fn1));
  NODE_API_CALL(env,
      node_api_set_named_property(env, exports, "TestName", fn2));
  NODE_API_CALL(env,
      node_api_set_named_property(env, exports, "TestNameShort", fn3));
  NODE_API_CALL(env,
      node_api_set_named_property(env, exports, "MakeTrackedFunction", fn4));

  NODE_API_CALL(env, node_api_set_named_property(env,
                                                exports,
                                                "TestCreateFunctionParameters",
                                                fn5));

  return exports;
}
EXTERN_C_END
