#include "myobject.h"
#include "../common.h"

static int finalize_count = 0;

MyObject::MyObject() : env_(nullptr), wrapper_(nullptr) {}

MyObject::~MyObject() { napi_delete_reference(env_, wrapper_); }

void MyObject::Destructor(napi_env env,
                          void* nativeObject,
                          void* /*finalize_hint*/) {
  ++finalize_count;
  MyObject* obj = static_cast<MyObject*>(nativeObject);
  delete obj;
}

napi_value MyObject::GetFinalizeCount(napi_env env, napi_callback_info info) {
  napi_value result;
  NODE_API_CALL(env, napi_create_int32(env, finalize_count, &result));
  return result;
}

napi_ref MyObject::constructor;

napi_status MyObject::Init(napi_env env) {
  napi_status status;
  napi_property_descriptor properties[] = {
    DECLARE_NODE_API_PROPERTY("plusOne", PlusOne),
  };

  napi_value cons;
  status = napi_define_class(
      env, "MyObject", -1, New, nullptr, 1, properties, &cons);
  if (status != napi_ok) return status;

  status = napi_create_reference(env, cons, 1, &constructor);
  if (status != napi_ok) return status;

  return napi_ok;
}

napi_value MyObject::New(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  napi_value _this;
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, &_this, nullptr));

  napi_valuetype valuetype;
  NODE_API_CALL(env, napi_typeof(env, args[0], &valuetype));

  MyObject* obj = new MyObject();

  if (valuetype == napi_undefined) {
    obj->counter_ = 0;
  } else {
    NODE_API_CALL(env, napi_get_value_uint32(env, args[0], &obj->counter_));
  }

  obj->env_ = env;
  NODE_API_CALL(env,
      napi_wrap(
          env, _this, obj, MyObject::Destructor, nullptr /* finalize_hint */,
          &obj->wrapper_));

  return _this;
}

napi_status MyObject::NewInstance(napi_env env,
                                  napi_value arg,
                                  napi_value* instance) {
  napi_status status;

  const int argc = 1;
  napi_value argv[argc] = {arg};

  napi_value cons;
  status = napi_get_reference_value(env, constructor, &cons);
  if (status != napi_ok) return status;

  status = napi_new_instance(env, cons, argc, argv, instance);
  if (status != napi_ok) return status;

  return napi_ok;
}

napi_value MyObject::PlusOne(napi_env env, napi_callback_info info) {
  napi_value _this;
  NODE_API_CALL(env,
      napi_get_cb_info(env, info, nullptr, nullptr, &_this, nullptr));

  MyObject* obj;
  NODE_API_CALL(env, napi_unwrap(env, _this, reinterpret_cast<void**>(&obj)));

  obj->counter_ += 1;

  napi_value num;
  NODE_API_CALL(env, napi_create_uint32(env, obj->counter_, &num));

  return num;
}
