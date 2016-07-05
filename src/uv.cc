#include "uv.h"
#include "node.h"
#include "env.h"
#include "env-inl.h"

#include <stdio.h>  // snprintf

namespace node {
namespace uv {

#define RETURN_RANGE_ERROR(...)                                               \
  do {                                                                        \
    char buf[100];                                                            \
    snprintf(buf, sizeof(buf), __VA_ARGS__);                                  \
    return env->ThrowRangeError(buf);                                         \
  } while (0)

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;


void ErrName(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  int err = args[0]->Int32Value();
  if (err >= 0 || err < UV_ERRNO_MAX)
    RETURN_RANGE_ERROR("err is an invalid error code: %i", err);
  const char* name = uv_err_name(err);
  args.GetReturnValue().Set(OneByteString(env->isolate(), name));
}


void StrError(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  int err = args[0]->Int32Value();
  if (err >= 0 || err < UV_ERRNO_MAX)
    RETURN_RANGE_ERROR("err is an invalid error code: %i", err);
  const char* name = uv_strerror(err);
  args.GetReturnValue().Set(OneByteString(env->isolate(), name));
}


void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "errname"),
              env->NewFunctionTemplate(ErrName)->GetFunction());
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "strerror"),
              env->NewFunctionTemplate(StrError)->GetFunction());
#define V(name, _)                                                            \
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "UV_" # name),            \
              Integer::New(env->isolate(), UV_ ## name));
  UV_ERRNO_MAP(V)
#undef V
}


}  // namespace uv
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(uv, node::uv::Initialize)
