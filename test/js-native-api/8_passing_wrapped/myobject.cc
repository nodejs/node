#include "myobject.h"
#include "../common.h"

size_t finalize_count = 0;

MyObject::MyObject() : env_(nullptr), wrapper_(nullptr) {}

MyObject::~MyObject() {
  finalize_count++;
  node_api_delete_reference(env_, wrapper_);
}

void MyObject::Destructor(
  node_api_env env, void* nativeObject, void* /*finalize_hint*/) {
  MyObject* obj = static_cast<MyObject*>(nativeObject);
  delete obj;
}

node_api_ref MyObject::constructor;

node_api_status MyObject::Init(node_api_env env) {
  node_api_status status;

  node_api_value cons;
  status = node_api_define_class(
      env, "MyObject", -1, New, nullptr, 0, nullptr, &cons);
  if (status != node_api_ok) return status;

  status = node_api_create_reference(env, cons, 1, &constructor);
  if (status != node_api_ok) return status;

  return node_api_ok;
}

node_api_value MyObject::New(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  node_api_value _this;
  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, &argc, args, &_this, nullptr));

  MyObject* obj = new MyObject();

  node_api_valuetype valuetype;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype));

  if (valuetype == node_api_undefined) {
    obj->val_ = 0;
  } else {
    NODE_API_CALL(env, node_api_get_value_double(env, args[0], &obj->val_));
  }

  obj->env_ = env;

  // The below call to node_api_wrap() must request a reference to the wrapped
  // object via the out-parameter, because this ensures that we test the code
  // path that deals with a reference that is destroyed from its own finalizer.
  NODE_API_CALL(env, node_api_wrap(env,
                                   _this,
                                   obj,
                                   MyObject::Destructor,
                                   nullptr,  // finalize_hint
                                   &obj->wrapper_));

  return _this;
}

node_api_status MyObject::NewInstance(node_api_env env,
                                      node_api_value arg,
                                      node_api_value* instance) {
  node_api_status status;

  const int argc = 1;
  node_api_value argv[argc] = {arg};

  node_api_value cons;
  status = node_api_get_reference_value(env, constructor, &cons);
  if (status != node_api_ok) return status;

  status = node_api_new_instance(env, cons, argc, argv, instance);
  if (status != node_api_ok) return status;

  return node_api_ok;
}
