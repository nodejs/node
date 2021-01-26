#include <js_native_api.h>
#include "../common.h"

static double value_ = 1;
static double static_value_ = 10;

static node_api_value
TestDefineClass(node_api_env env, node_api_callback_info info) {
  node_api_status status;
  node_api_value result, return_value;

  node_api_property_descriptor property_descriptor = {
    "TestDefineClass",
    NULL,
    TestDefineClass,
    NULL,
    NULL,
    NULL,
    node_api_enumerable | node_api_static,
    NULL};

  NODE_API_CALL(env, node_api_create_object(env, &return_value));

  status = node_api_define_class(NULL,
                                 "TrackedFunction",
                                 NODE_API_AUTO_LENGTH,
                                 TestDefineClass,
                                 NULL,
                                 1,
                                 &property_descriptor,
                                 &result);

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      node_api_invalid_arg,
                      status);

  node_api_define_class(env,
                        NULL,
                        NODE_API_AUTO_LENGTH,
                        TestDefineClass,
                        NULL,
                        1,
                        &property_descriptor,
                        &result);

  add_last_status(env, "nameIsNull", return_value);

  node_api_define_class(env,
                        "TrackedFunction",
                        NODE_API_AUTO_LENGTH,
                        NULL,
                        NULL,
                        1,
                        &property_descriptor,
                        &result);

  add_last_status(env, "cbIsNull", return_value);

  node_api_define_class(env,
                        "TrackedFunction",
                        NODE_API_AUTO_LENGTH,
                        TestDefineClass,
                        NULL,
                        1,
                        &property_descriptor,
                        &result);

  add_last_status(env, "cbDataIsNull", return_value);

  node_api_define_class(env,
                        "TrackedFunction",
                        NODE_API_AUTO_LENGTH,
                        TestDefineClass,
                        NULL,
                        1,
                        NULL,
                        &result);

  add_last_status(env, "propertiesIsNull", return_value);


  node_api_define_class(env,
                        "TrackedFunction",
                        NODE_API_AUTO_LENGTH,
                        TestDefineClass,
                        NULL,
                        1,
                        &property_descriptor,
                        NULL);

  add_last_status(env, "resultIsNull", return_value);

  return return_value;
}

static node_api_value GetValue(node_api_env env, node_api_callback_info info) {
  size_t argc = 0;
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, NULL, NULL, NULL));

  NODE_API_ASSERT(env, argc == 0, "Wrong number of arguments");

  node_api_value number;
  NODE_API_CALL(env, node_api_create_double(env, value_, &number));

  return number;
}

static node_api_value SetValue(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments");

  NODE_API_CALL(env, node_api_get_value_double(env, args[0], &value_));

  return NULL;
}

static node_api_value Echo(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments");

  return args[0];
}

static node_api_value New(node_api_env env, node_api_callback_info info) {
  node_api_value _this;
  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, NULL, NULL, &_this, NULL));

  return _this;
}

static node_api_value
GetStaticValue(node_api_env env, node_api_callback_info info) {
  size_t argc = 0;
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, NULL, NULL, NULL));

  NODE_API_ASSERT(env, argc == 0, "Wrong number of arguments");

  node_api_value number;
  NODE_API_CALL(env, node_api_create_double(env, static_value_, &number));

  return number;
}


static node_api_value NewExtra(node_api_env env, node_api_callback_info info) {
  node_api_value _this;
  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, NULL, NULL, &_this, NULL));

  return _this;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_value number, cons;
  NODE_API_CALL(env, node_api_create_double(env, value_, &number));

  NODE_API_CALL(env, node_api_define_class(
      env, "MyObject_Extra", 8, NewExtra, NULL, 0, NULL, &cons));

  node_api_property_descriptor properties[] = {
    { "echo", NULL, Echo, NULL, NULL, NULL, node_api_enumerable, NULL },
    { "readwriteValue", NULL, NULL, NULL, NULL, number,
        node_api_enumerable | node_api_writable, NULL },
    { "readonlyValue", NULL, NULL, NULL, NULL, number, node_api_enumerable,
        NULL },
    { "hiddenValue", NULL, NULL, NULL, NULL, number, node_api_default, NULL },
    { "readwriteAccessor1", NULL, NULL, GetValue, SetValue, NULL,
        node_api_default, NULL },
    { "readwriteAccessor2", NULL, NULL, GetValue, SetValue, NULL,
        node_api_writable, NULL },
    { "readonlyAccessor1", NULL, NULL, GetValue, NULL, NULL, node_api_default,
        NULL },
    { "readonlyAccessor2", NULL, NULL, GetValue, NULL, NULL, node_api_writable,
        NULL },
    { "staticReadonlyAccessor1", NULL, NULL, GetStaticValue, NULL, NULL,
        node_api_default | node_api_static, NULL},
    { "constructorName", NULL, NULL, NULL, NULL, cons,
        node_api_enumerable | node_api_static, NULL },
    { "TestDefineClass", NULL, TestDefineClass, NULL, NULL, NULL,
        node_api_enumerable | node_api_static, NULL },
  };

  NODE_API_CALL(env,
      node_api_define_class(env, "MyObject", NODE_API_AUTO_LENGTH, New,
          NULL, sizeof(properties)/sizeof(*properties), properties, &cons));

  return cons;
}
EXTERN_C_END
