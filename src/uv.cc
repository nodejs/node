#include "uv.h"
#include "node.h"
#include "env.h"
#include "env-inl.h"

namespace node {
namespace uv {

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Integer;
using v8::Local;
using v8::Object;
using v8::Value;


void ErrName(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  int err = args[0]->Int32Value();
  if (err >= 0)
    return env->ThrowError("err >= 0");
  const char* name = uv_err_name(err);
  args.GetReturnValue().Set(OneByteString(env->isolate(), name));
}


void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "errname"),
              env->NewFunctionTemplate(ErrName)->GetFunction());
#define V(name, _)                                                            \
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "UV_" # name),            \
              Integer::New(env->isolate(), UV_ ## name));
  UV_ERRNO_MAP(V)
#undef V
}


}  // namespace uv
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(uv, node::uv::Initialize)
