#include "myobject.h"
#include "../common.h"

node_api_ref MyObject::constructor;

MyObject::MyObject(double value)
    : value_(value), env_(nullptr), wrapper_(nullptr) {}

MyObject::~MyObject() { node_api_delete_reference(env_, wrapper_); }

void MyObject::Destructor(
  node_api_env env, void* nativeObject, void* /*finalize_hint*/) {
  MyObject* obj = static_cast<MyObject*>(nativeObject);
  delete obj;
}

void MyObject::Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor properties[] = {
    { "value", nullptr, nullptr, GetValue, SetValue, 0, node_api_default, 0 },
    { "valueReadonly", nullptr, nullptr, GetValue, nullptr, 0, node_api_default,
      0 },
    DECLARE_NODE_API_PROPERTY("plusOne", PlusOne),
    DECLARE_NODE_API_PROPERTY("multiply", Multiply),
  };

  node_api_value cons;
  NODE_API_CALL_RETURN_VOID(env, node_api_define_class(
      env, "MyObject", -1, New, nullptr,
      sizeof(properties) / sizeof(node_api_property_descriptor),
      properties, &cons));

  NODE_API_CALL_RETURN_VOID(env,
      node_api_create_reference(env, cons, 1, &constructor));

  NODE_API_CALL_RETURN_VOID(env,
      node_api_set_named_property(env, exports, "MyObject", cons));
}

node_api_value MyObject::New(node_api_env env, node_api_callback_info info) {
  node_api_value new_target;
  NODE_API_CALL(env, node_api_get_new_target(env, info, &new_target));
  bool is_constructor = (new_target != nullptr);

  size_t argc = 1;
  node_api_value args[1];
  node_api_value _this;
  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, &argc, args, &_this, nullptr));

  if (is_constructor) {
    // Invoked as constructor: `new MyObject(...)`
    double value = 0;

    node_api_valuetype valuetype;
    NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype));

    if (valuetype != node_api_undefined) {
      NODE_API_CALL(env, node_api_get_value_double(env, args[0], &value));
    }

    MyObject* obj = new MyObject(value);

    obj->env_ = env;
    NODE_API_CALL(env, node_api_wrap(env,
                                     _this,
                                     obj,
                                     MyObject::Destructor,
                                     nullptr,  // finalize_hint
                                     &obj->wrapper_));

    return _this;
  }

  // Invoked as plain function `MyObject(...)`, turn into construct call.
  argc = 1;
  node_api_value argv[1] = {args[0]};

  node_api_value cons;
  NODE_API_CALL(env, node_api_get_reference_value(env, constructor, &cons));

  node_api_value instance;
  NODE_API_CALL(env, node_api_new_instance(env, cons, argc, argv, &instance));

  return instance;
}

node_api_value
MyObject::GetValue(node_api_env env, node_api_callback_info info) {
  node_api_value _this;
  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, nullptr, nullptr, &_this, nullptr));

  MyObject* obj;
  NODE_API_CALL(env,
      node_api_unwrap(env, _this, reinterpret_cast<void**>(&obj)));

  node_api_value num;
  NODE_API_CALL(env, node_api_create_double(env, obj->value_, &num));

  return num;
}

node_api_value
MyObject::SetValue(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  node_api_value _this;
  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, &argc, args, &_this, nullptr));

  MyObject* obj;
  NODE_API_CALL(env,
      node_api_unwrap(env, _this, reinterpret_cast<void**>(&obj)));

  NODE_API_CALL(env, node_api_get_value_double(env, args[0], &obj->value_));

  return nullptr;
}

node_api_value
MyObject::PlusOne(node_api_env env, node_api_callback_info info) {
  node_api_value _this;
  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, nullptr, nullptr, &_this, nullptr));

  MyObject* obj;
  NODE_API_CALL(env,
      node_api_unwrap(env, _this, reinterpret_cast<void**>(&obj)));

  obj->value_ += 1;

  node_api_value num;
  NODE_API_CALL(env, node_api_create_double(env, obj->value_, &num));

  return num;
}

node_api_value
MyObject::Multiply(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  node_api_value _this;
  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, &argc, args, &_this, nullptr));

  double multiple = 1;
  if (argc >= 1) {
    NODE_API_CALL(env, node_api_get_value_double(env, args[0], &multiple));
  }

  MyObject* obj;
  NODE_API_CALL(env,
      node_api_unwrap(env, _this, reinterpret_cast<void**>(&obj)));

  node_api_value cons;
  NODE_API_CALL(env, node_api_get_reference_value(env, constructor, &cons));

  const int kArgCount = 1;
  node_api_value argv[kArgCount];
  NODE_API_CALL(env,
      node_api_create_double(env, obj->value_ * multiple, argv));

  node_api_value instance;
  NODE_API_CALL(env,
      node_api_new_instance(env, cons, kArgCount, argv, &instance));

  return instance;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  MyObject::Init(env, exports);
  return exports;
}
EXTERN_C_END
