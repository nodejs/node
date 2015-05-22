#include "node.h"
#include "node_buffer.h"
#include "v8.h"
#include "env.h"
#include "env-inl.h"

namespace node {
namespace dlopen {

using v8::Boolean;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Handle;
using v8::Integer;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;
using v8::Uint32;


static void Dlopen(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  const char *filename;
  if (args[0]->IsNull()) {
    filename = NULL;
  } else if (args[0]->IsString()) {
    node::Utf8Value name(env->isolate(), args[0]);
    filename = *name;
  } else {
    return env->ThrowTypeError("expected a string filename or null as first argument");
  }

  if (!Buffer::HasInstance(args[1]))
    return env->ThrowTypeError("expected a Buffer instance as second argument");

  Local<Object> buf = args[1].As<Object>();
  uv_lib_t *lib = reinterpret_cast<uv_lib_t *>(Buffer::Data(buf));

  int r = uv_dlopen(filename, lib);

  args.GetReturnValue().Set(r);
}


static void Dlclose(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!Buffer::HasInstance(args[0]))
    return env->ThrowTypeError("expected a Buffer instance as first argument");

  Local<Object> buf = args[0].As<Object>();
  uv_lib_t *lib = reinterpret_cast<uv_lib_t *>(Buffer::Data(buf));

  uv_dlclose(lib);
}


static void Dlsym(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!Buffer::HasInstance(args[0]))
    return env->ThrowTypeError("expected a Buffer instance as first argument");
  if (!args[1]->IsString())
    return env->ThrowTypeError("expected a string as second argument");
  if (!Buffer::HasInstance(args[2]))
    return env->ThrowTypeError("expected a Buffer instance as third argument");

  Local<Object> buf = args[0].As<Object>();
  uv_lib_t *lib = reinterpret_cast<uv_lib_t *>(Buffer::Data(buf));

  Local<Object> sym_buf = args[2].As<Object>();
  void *sym = reinterpret_cast<void *>(Buffer::Data(sym_buf));

  node::Utf8Value name(env->isolate(), args[1]);

  int r = uv_dlsym(lib, *name, &sym);

  args.GetReturnValue().Set(r);
}


static void Dlerror(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!Buffer::HasInstance(args[0]))
    return env->ThrowTypeError("expected a Buffer instance as first argument");

  Local<Object> buf = args[0].As<Object>();
  uv_lib_t *lib = reinterpret_cast<uv_lib_t *>(Buffer::Data(buf));

  args.GetReturnValue().Set(OneByteString(env->isolate(), uv_dlerror(lib)));
}


void Initialize(Handle<Object> target,
                Handle<Value> unused,
                Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  env->SetMethod(target, "dlopen", Dlopen);
  env->SetMethod(target, "dlclose", Dlclose);
  env->SetMethod(target, "dlsym", Dlsym);
  env->SetMethod(target, "dlerror", Dlerror);

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "sizeof_uv_lib_t"),
              Uint32::NewFromUnsigned(env->isolate(), static_cast<uint32_t>(sizeof(uv_lib_t))));
}

}  // namespace dlopen
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(dlopen, node::dlopen::Initialize)
