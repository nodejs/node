#include "node.h"
#include "node_buffer.h"
#include "v8.h"
#include "env.h"
#include "env-inl.h"
#include "ffi.h"

#include <stdlib.h>  // for abs()

namespace node {
namespace ffi {

using v8::Array;
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


static void PrepCif(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  unsigned int nargs;
  char *rtype, *atypes, *cif;
  ffi_status status;
  ffi_abi abi;

  if (args.Length() != 5)
    return env->ThrowTypeError("expected 5 arguments");
  if (!Buffer::HasInstance(args[0]))
    return env->ThrowTypeError("expected Buffer instance as first argument");
  if (!args[1]->IsNumber())
    return env->ThrowTypeError("expected Number as second argument");
  if (!args[2]->IsNumber())
    return env->ThrowTypeError("expected Number as third argument");
  if (!Buffer::HasInstance(args[3]))
    return env->ThrowTypeError("expected Buffer instance as fourth argument");
  if (!Buffer::HasInstance(args[4]))
    return env->ThrowTypeError("expected Buffer instance as fifth argument");

  cif = Buffer::Data(args[0].As<Object>());
  abi = static_cast<ffi_abi>(args[1]->Int32Value());
  nargs = args[2]->Uint32Value();
  rtype = Buffer::Data(args[3].As<Object>());
  atypes = Buffer::Data(args[4].As<Object>());

  status = ffi_prep_cif(
      reinterpret_cast<ffi_cif*>(cif),
      abi,
      nargs,
      reinterpret_cast<ffi_type*>(rtype),
      reinterpret_cast<ffi_type**>(atypes));

  args.GetReturnValue().Set(status);
}


static void Call(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

}


void Initialize(Handle<Object> target,
                Handle<Value> unused,
                Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  Handle<Object> sizeofmap = Object::New(env->isolate());
  Handle<Object> alignofmap = Object::New(env->isolate());
#define SET_TYPE(name, type) \
  sizeofmap->Set(FIXED_ONE_BYTE_STRING(env->isolate(), #name), \
               Uint32::NewFromUnsigned(env->isolate(), \
                                       static_cast<uint32_t>(sizeof(type)))); \
  alignofmap->Set(FIXED_ONE_BYTE_STRING(env->isolate(), #name), \
               Uint32::NewFromUnsigned(env->isolate(), \
                                       static_cast<uint32_t>(alignof(type))));
  // fixed sizes
  SET_TYPE(int8, int8_t);
  SET_TYPE(uint8, uint8_t);
  SET_TYPE(int16, int16_t);
  SET_TYPE(uint16, uint16_t);
  SET_TYPE(int32, int32_t);
  SET_TYPE(uint32, uint32_t);
  SET_TYPE(int64, int64_t);
  SET_TYPE(uint64, uint64_t);
  SET_TYPE(float, float);
  SET_TYPE(double, double);

  // (potentially) variable sizes
  SET_TYPE(bool, bool);
  SET_TYPE(byte, unsigned char);
  SET_TYPE(char, char);
  SET_TYPE(uchar, unsigned char);
  SET_TYPE(short, short);
  SET_TYPE(ushort, unsigned short);
  SET_TYPE(int, int);
  SET_TYPE(uint, unsigned int);
  SET_TYPE(long, long);
  SET_TYPE(ulong, unsigned long);
  SET_TYPE(longlong, long long);
  SET_TYPE(ulonglong, unsigned long long);
  SET_TYPE(longdouble, long double);
  SET_TYPE(pointer, char*);
  SET_TYPE(size_t, size_t);

  // used internally by "ffi" module for Buffer instantiation
  SET_TYPE(ffi_arg, ffi_arg);
  SET_TYPE(ffi_sarg, ffi_sarg);
  SET_TYPE(ffi_type, ffi_type);
  SET_TYPE(ffi_cif, ffi_cif);
#undef SET_TYPE
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "sizeof"), sizeofmap);
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "alignof"), alignofmap);

#define SET_ENUM_VALUE(_value) \
  target->ForceSet(FIXED_ONE_BYTE_STRING(env->isolate(), #_value), \
              Uint32::New(env->isolate(), static_cast<uint32_t>(_value)), \
              static_cast<v8::PropertyAttribute>(v8::ReadOnly|v8::DontDelete))
  // `ffi_status` enum values
  SET_ENUM_VALUE(FFI_OK);
  SET_ENUM_VALUE(FFI_BAD_TYPEDEF);
  SET_ENUM_VALUE(FFI_BAD_ABI);

  // `ffi_abi` enum values
  SET_ENUM_VALUE(FFI_DEFAULT_ABI);
  SET_ENUM_VALUE(FFI_FIRST_ABI);
  SET_ENUM_VALUE(FFI_LAST_ABI);
  /* ---- ARM processors ---------- */
#ifdef __arm__
  SET_ENUM_VALUE(FFI_SYSV);
  SET_ENUM_VALUE(FFI_VFP);
  /* ---- Intel x86 Win32 ---------- */
#elif defined(X86_WIN32)
  SET_ENUM_VALUE(FFI_SYSV);
  SET_ENUM_VALUE(FFI_STDCALL);
  SET_ENUM_VALUE(FFI_THISCALL);
  SET_ENUM_VALUE(FFI_FASTCALL);
  SET_ENUM_VALUE(FFI_MS_CDECL);
#elif defined(X86_WIN64)
  SET_ENUM_VALUE(FFI_WIN64);
#else
  /* ---- Intel x86 and AMD x86-64 - */
  SET_ENUM_VALUE(FFI_SYSV);
  /* Unix variants all use the same ABI for x86-64  */
  SET_ENUM_VALUE(FFI_UNIX64);
#endif
#undef SET_ENUM_VALUE

  Handle<Object> ftmap = Object::New(env->isolate());
#define SET_FFI_TYPE(_type) \
  ftmap->Set(FIXED_ONE_BYTE_STRING(env->isolate(), #_type), \
      Buffer::Use(env->isolate(), \
        reinterpret_cast<char*>(&ffi_type_##_type), 0))
  SET_FFI_TYPE(void);
  SET_FFI_TYPE(uint8);
  SET_FFI_TYPE(sint8);
  SET_FFI_TYPE(uint16);
  SET_FFI_TYPE(sint16);
  SET_FFI_TYPE(uint32);
  SET_FFI_TYPE(sint32);
  SET_FFI_TYPE(uint64);
  SET_FFI_TYPE(sint64);
  SET_FFI_TYPE(uchar);
  SET_FFI_TYPE(schar);
  SET_FFI_TYPE(ushort);
  SET_FFI_TYPE(sshort);
  SET_FFI_TYPE(uint);
  SET_FFI_TYPE(sint);
  SET_FFI_TYPE(float);
  SET_FFI_TYPE(double);
  SET_FFI_TYPE(pointer);
  SET_FFI_TYPE(ulong);
  SET_FFI_TYPE(slong);
  SET_FFI_TYPE(longdouble);
#undef SET_FFI_TYPE
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "type"), ftmap);

  env->SetMethod(target, "prepCif", PrepCif);
  //env->SetMethod(target, "prepCifVar", PrepCifVar);
  env->SetMethod(target, "call", Call);
  //env->SetMethod(target, "callAsync", CallAsync);

  // these static function pointers end up being used in the tests
  // (see "test/parallel/test-ffi.js")
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "abs"),
      Buffer::Use(env->isolate(), reinterpret_cast<char*>(abs), 0));
}

}  // namespace ffi
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(ffi, node::ffi::Initialize)
