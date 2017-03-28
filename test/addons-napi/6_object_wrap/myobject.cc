#include "myobject.h"

napi_ref MyObject::constructor;

MyObject::MyObject(double value)
    : value_(value), env_(nullptr), wrapper_(nullptr) {}

MyObject::~MyObject() { napi_delete_reference(env_, wrapper_); }

void MyObject::Destructor(
  napi_env env, void* nativeObject, void* /*finalize_hint*/) {
  MyObject* obj = static_cast<MyObject*>(nativeObject);
  delete obj;
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, func, 0, 0, 0, napi_default, 0 }

void MyObject::Init(napi_env env, napi_value exports) {
  napi_status status;
  napi_property_descriptor properties[] = {
      { "value", nullptr, GetValue, SetValue, 0, napi_default, 0 },
      DECLARE_NAPI_METHOD("plusOne", PlusOne),
      DECLARE_NAPI_METHOD("multiply", Multiply),
  };

  napi_value cons;
  status =
      napi_define_class(env, "MyObject", New, nullptr, 3, properties, &cons);
  if (status != napi_ok) return;

  status = napi_create_reference(env, cons, 1, &constructor);
  if (status != napi_ok) return;

  status = napi_set_named_property(env, exports, "MyObject", cons);
  if (status != napi_ok) return;
}

void MyObject::New(napi_env env, napi_callback_info info) {
  napi_status status;

  bool is_constructor;
  status = napi_is_construct_call(env, info, &is_constructor);
  if (status != napi_ok) return;

  if (is_constructor) {
    // Invoked as constructor: `new MyObject(...)`
    napi_value args[1];
    status = napi_get_cb_args(env, info, args, 1);
    if (status != napi_ok) return;

    double value = 0;

    napi_valuetype valuetype;
    status = napi_typeof(env, args[0], &valuetype);
    if (status != napi_ok) return;

    if (valuetype != napi_undefined) {
      status = napi_get_value_double(env, args[0], &value);
      if (status != napi_ok) return;
    }

    MyObject* obj = new MyObject(value);

    napi_value jsthis;
    status = napi_get_cb_this(env, info, &jsthis);
    if (status != napi_ok) return;

    obj->env_ = env;
    status = napi_wrap(env,
                       jsthis,
                       obj,
                       MyObject::Destructor,
                       nullptr,  // finalize_hint
                       &obj->wrapper_);
    if (status != napi_ok) return;

    status = napi_set_return_value(env, info, jsthis);
    if (status != napi_ok) return;
  } else {
    // Invoked as plain function `MyObject(...)`, turn into construct call.
    napi_value args[1];
    status = napi_get_cb_args(env, info, args, 1);
    if (status != napi_ok) return;

    const int argc = 1;
    napi_value argv[argc] = {args[0]};

    napi_value cons;
    status = napi_get_reference_value(env, constructor, &cons);
    if (status != napi_ok) return;

    napi_value instance;
    status = napi_new_instance(env, cons, argc, argv, &instance);
    if (status != napi_ok) return;

    status = napi_set_return_value(env, info, instance);
    if (status != napi_ok) return;
  }
}

void MyObject::GetValue(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value jsthis;
  status = napi_get_cb_this(env, info, &jsthis);
  if (status != napi_ok) return;

  MyObject* obj;
  status = napi_unwrap(env, jsthis, reinterpret_cast<void**>(&obj));
  if (status != napi_ok) return;

  napi_value num;
  status = napi_create_number(env, obj->value_, &num);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, num);
  if (status != napi_ok) return;
}

void MyObject::SetValue(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value value;
  status = napi_get_cb_args(env, info, &value, 1);
  if (status != napi_ok) return;

  napi_value jsthis;
  status = napi_get_cb_this(env, info, &jsthis);
  if (status != napi_ok) return;

  MyObject* obj;
  status = napi_unwrap(env, jsthis, reinterpret_cast<void**>(&obj));
  if (status != napi_ok) return;

  status = napi_get_value_double(env, value, &obj->value_);
  if (status != napi_ok) return;
}

void MyObject::PlusOne(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value jsthis;
  status = napi_get_cb_this(env, info, &jsthis);
  if (status != napi_ok) return;

  MyObject* obj;
  status = napi_unwrap(env, jsthis, reinterpret_cast<void**>(&obj));
  if (status != napi_ok) return;

  obj->value_ += 1;

  napi_value num;
  status = napi_create_number(env, obj->value_, &num);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, num);
  if (status != napi_ok) return;
}

void MyObject::Multiply(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value args[1];
  status = napi_get_cb_args(env, info, args, 1);
  if (status != napi_ok) return;

  napi_valuetype valuetype;
  status = napi_typeof(env, args[0], &valuetype);
  if (status != napi_ok) return;

  double multiple = 1;
  if (valuetype != napi_undefined) {
    status = napi_get_value_double(env, args[0], &multiple);
    if (status != napi_ok) return;
  }

  napi_value jsthis;
  status = napi_get_cb_this(env, info, &jsthis);
  if (status != napi_ok) return;

  MyObject* obj;
  status = napi_unwrap(env, jsthis, reinterpret_cast<void**>(&obj));
  if (status != napi_ok) return;

  napi_value cons;
  status = napi_get_reference_value(env, constructor, &cons);
  if (status != napi_ok) return;

  const int kArgCount = 1;
  napi_value argv[kArgCount];
  status = napi_create_number(env, obj->value_ * multiple, argv);
  if (status != napi_ok) return;

  napi_value instance;
  status = napi_new_instance(env, cons, kArgCount, argv, &instance);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, instance);
  if (status != napi_ok) return;
}
