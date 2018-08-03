#include "node_internals.h"
#include "node_errors.h"
#include "env-inl.h"

using v8::Boolean;
using v8::Context;
using v8::EscapableHandleScope;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Object;
using v8::Value;

namespace node {

MaybeLocal<Object> AccessControl::ToObject(Local<Context> context) {
  Isolate* isolate = context->GetIsolate();
  EscapableHandleScope handle_scope(isolate);

  Local<Object> obj = Object::New(isolate);

#define V(kind)                                                               \
  if (obj->Set(context,                                                       \
               FIXED_ONE_BYTE_STRING(isolate, #kind),                         \
               Boolean::New(isolate, has_permission(kind))).IsNothing()) {    \
    return MaybeLocal<Object>();                                              \
  }
  ACCESS_CONTROL_FLAGS(V)
#undef V

  return handle_scope.Escape(obj);
}

Maybe<AccessControl> AccessControl::FromObject(Local<Context> context,
                                               Local<Object> obj) {
  Isolate* isolate = context->GetIsolate();
  HandleScope scope(isolate);

  AccessControl ret;

  Local<Value> field;
#define V(kind)                                                               \
  if (!obj->Get(context, FIXED_ONE_BYTE_STRING(isolate, #kind))               \
          .ToLocal(&field)) {                                                 \
    return Nothing<AccessControl>();                                          \
  }                                                                           \
  ret.set_permission(kind, !field->IsFalse());
  ACCESS_CONTROL_FLAGS(V)
#undef V

  return Just(ret);
}

AccessControl::Permission AccessControl::PermissionFromString(const char* str) {
#define V(kind) if (strcmp(str, #kind) == 0) return kind;
  ACCESS_CONTROL_FLAGS(V)
#undef V
  return kNumPermissions;
}

const char* AccessControl::PermissionToString(Permission perm) {
  switch (perm) {
#define V(kind) case kind: return #kind;
    ACCESS_CONTROL_FLAGS(V)
#undef V
    default: return nullptr;
  }
}

void AccessControl::ThrowAccessDenied(Environment* env, Permission perm) {
  Local<Value> err = ERR_ACCESS_DENIED(env->isolate());
  CHECK(err->IsObject());
  err.As<Object>()->Set(
      env->context(),
      env->permission_string(),
      v8::String::NewFromUtf8(env->isolate(),
                              PermissionToString(perm),
                              v8::NewStringType::kNormal).ToLocalChecked())
          .FromMaybe(false);  // Nothing to do about an error at this point.
  env->isolate()->ThrowException(err);
}

namespace ac {

void Apply(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  AccessControl access_control;
  Local<Object> obj;
  if (!args[0]->ToObject(env->context()).ToLocal(&obj) ||
      !AccessControl::FromObject(env->context(), obj).To(&access_control)) {
    return;
  }

  env->access_control()->apply(access_control);
}

void GetCurrent(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Local<Object> ret;
  if (env->access_control()->ToObject(env->context()).ToLocal(&ret))
    args.GetReturnValue().Set(ret);
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  env->SetMethod(target, "apply", Apply);
  env->SetMethod(target, "getCurrent", GetCurrent);
}

}  // namespace ac
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(access_control, node::ac::Initialize)
