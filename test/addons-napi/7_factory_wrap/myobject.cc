#include "myobject.h"

MyObject::MyObject() : env_(nullptr), wrapper_(nullptr) {}

MyObject::~MyObject() { napi_delete_reference(env_, wrapper_); }

void MyObject::Destructor(napi_env env,
                          void* nativeObject,
                          void* /*finalize_hint*/) {
  MyObject* obj = static_cast<MyObject*>(nativeObject);
  delete obj;
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, func, 0, 0, 0, napi_default, 0 }

napi_ref MyObject::constructor;

napi_status MyObject::Init(napi_env env) {
  napi_status status;
  napi_property_descriptor properties[] = {
      DECLARE_NAPI_METHOD("plusOne", PlusOne),
  };

  napi_value cons;
  status =
      napi_define_class(env, "MyObject", New, nullptr, 1, properties, &cons);
  if (status != napi_ok) return status;

  status = napi_create_reference(env, cons, 1, &constructor);
  if (status != napi_ok) return status;

  return napi_ok;
}

void MyObject::New(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value args[1];
  status = napi_get_cb_args(env, info, args, 1);
  if (status != napi_ok) return;

  napi_valuetype valuetype;
  status = napi_typeof(env, args[0], &valuetype);
  if (status != napi_ok) return;

  MyObject* obj = new MyObject();

  if (valuetype == napi_undefined) {
    obj->counter_ = 0;
  } else {
    status = napi_get_value_double(env, args[0], &obj->counter_);
    if (status != napi_ok) return;
  }

  napi_value jsthis;
  status = napi_get_cb_this(env, info, &jsthis);
  if (status != napi_ok) return;

  obj->env_ = env;
  status = napi_wrap(env,
                     jsthis,
                     obj,
                     MyObject::Destructor,
                     nullptr, /* finalize_hint */
                     &obj->wrapper_);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, jsthis);
  if (status != napi_ok) return;
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

void MyObject::PlusOne(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value jsthis;
  status = napi_get_cb_this(env, info, &jsthis);
  if (status != napi_ok) return;

  MyObject* obj;
  status = napi_unwrap(env, jsthis, reinterpret_cast<void**>(&obj));
  if (status != napi_ok) return;

  obj->counter_ += 1;

  napi_value num;
  status = napi_create_number(env, obj->counter_, &num);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, num);
  if (status != napi_ok) return;
}
