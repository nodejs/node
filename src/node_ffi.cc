#include "node_ffi.h"
#include <string>
#include "env-inl.h"
#include "ffi.h"
#include "node_binding.h"
#include "node_external_reference.h"
#include "util-inl.h"
#include "v8-array-buffer.h"
#include "v8-fast-api-calls.h"
#include "v8-function-callback.h"
#include "v8-primitive.h"
#include "v8-template.h"

using v8::Array;
using v8::ArrayBuffer;
using v8::BigInt;
using v8::Boolean;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::Number;
using v8::SharedArrayBuffer;

namespace node {
namespace ffi {

binding::DLib* FfiBindingData::GetLibrary(std::string fname) {
  auto result = libraries_.find(fname);
  if (result != libraries_.end()) {
    return result->second.get();
  }
  std::unique_ptr<binding::DLib> lib(
      new binding::DLib(fname.c_str(), binding::DLib::kDefaultFlags));
  if (!lib->Open()) return nullptr;
  binding::DLib* ptr = lib.get();
  libraries_.insert({fname, std::move(lib)});
  return ptr;
}

void ReturnPointer(const FunctionCallbackInfo<Value>& args, void* ptr) {
  args.GetReturnValue().Set(BigInt::NewFromUnsigned(
      args.GetIsolate(), reinterpret_cast<uint64_t>(ptr)));
}

void GetLibrary(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFfi, "");

  std::string fname = "";
  CHECK(args[0]->IsNull() || args[0]->IsString());
  if (args[0]->IsString()) {
    fname = *node::Utf8Value(env->isolate(), args[0]);
  }

  ReturnPointer(args,
                Realm::GetBindingData<FfiBindingData>(args)->GetLibrary(fname));
}

void GetSymbol(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFfi, "");

  CHECK(args[0]->IsBigInt());
  binding::DLib* lib =
      reinterpret_cast<binding::DLib*>(args[0].As<BigInt>()->Uint64Value());
  CHECK(args[1]->IsString());
  node::Utf8Value sym_name(env->isolate(), args[1]);
  void* symbol = lib->GetSymbolAddress(*sym_name);
  ReturnPointer(args, symbol);
}

void GetBufferPointer(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFfi, "");

  CHECK(args[0]->IsArrayBuffer() || args[0]->IsSharedArrayBuffer());
  void* data;
  if (args[0]->IsSharedArrayBuffer()) {
    data = args[0].As<SharedArrayBuffer>()->Data();
  } else {
    data = args[0].As<ArrayBuffer>()->Data();
  }

  ReturnPointer(args, data);
}

void SetCallBuffer(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsArrayBuffer());
  Local<ArrayBuffer> ab = args[0].As<ArrayBuffer>();

  void* call_buffer = ab->Data();
  Realm::GetBindingData<FfiBindingData>(args)->call_buffer = call_buffer;
  ReturnPointer(args, call_buffer);
}

FfiSignature::FfiSignature(Environment* env,
                           Local<Object> object,
                           Local<BigInt> fn,
                           Local<BigInt> ret_type,
                           Local<Array> arg_types)
    : BaseObject(env, object) {
  Local<Context> context = env->context();

  fn_ = reinterpret_cast<void (*)()>(fn->Uint64Value());
  uint32_t argc = arg_types->Length();
  argv_types_.reserve(argc);

  ffi_type* retTyp = reinterpret_cast<ffi_type*>(ret_type->Uint64Value());
  for (uint32_t i = 0; i < argc; i++) {
    Local<Value> val = arg_types->Get(context, i).ToLocalChecked();
    CHECK(val->IsBigInt());
    uint64_t arg = val.As<BigInt>()->Uint64Value();
    argv_types_.push_back(reinterpret_cast<ffi_type*>(arg));
  }

  ffi_prep_cif(&cif_, FFI_DEFAULT_ABI, argc, retTyp, argv_types_.data());
}

void FfiSignature::New(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Environment* env = Environment::GetCurrent(isolate);

  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFfi, "");

  CHECK(args.IsConstructCall());
  CHECK(args[0]->IsBigInt());
  CHECK(args[1]->IsBigInt());
  CHECK(args[2]->IsArray());

  FfiSignature* sig = new FfiSignature(env,
                                       args.This(),
                                       args[0].As<BigInt>(),
                                       args[1].As<BigInt>(),
                                       args[2].As<Array>());

  args.This()
      ->Set(env->context(),
            OneByteString(isolate, "pointer"),
            BigInt::NewFromUnsigned(isolate, reinterpret_cast<uint64_t>(sig)))
      .Check();
}

void MakeCall(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Environment* env = Environment::GetCurrent(isolate);
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFfi, "");

  char* call_buffer = reinterpret_cast<char*>(
      Realm::GetBindingData<FfiBindingData>(args)->call_buffer);
  FfiSignature* sig = *reinterpret_cast<FfiSignature**>(call_buffer);
  char* rvalue = call_buffer + sizeof(char*);
  char** avalues = reinterpret_cast<char**>(rvalue + 2 * sizeof(char*));
  ffi_call(&sig->cif_,
           sig->fn_,
           reinterpret_cast<void*>(rvalue),
           reinterpret_cast<void**>(avalues));
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Isolate* isolate = context->GetIsolate();
  Realm* realm = Realm::GetCurrent(context);
  FfiBindingData* const binding_data =
      realm->AddBindingData<FfiBindingData>(target);
  if (binding_data == nullptr) return;

  SetMethod(context, target, "setCallBuffer", SetCallBuffer);
  SetMethod(context, target, "getBufferPointer", GetBufferPointer);
  SetMethod(context, target, "makeCall", MakeCall);
  SetMethod(context, target, "getSymbol", GetSymbol);
  SetMethod(context, target, "getLibrary", GetLibrary);

  Local<FunctionTemplate> tmpl =
      NewFunctionTemplate(isolate, FfiSignature::New);
  tmpl->InstanceTemplate()->SetInternalFieldCount(
      FfiSignature::kInternalFieldCount);
  SetConstructorFunction(context, target, "FfiSignature", tmpl);

  Local<Object> types = Object::New(isolate);
#define NODE_FFI_SET_TYPE_CONSTANT(name, typ)                                  \
  types                                                                        \
      ->Set(context,                                                           \
            OneByteString(isolate, #name),                                     \
            BigInt::NewFromUnsigned(isolate, (uint64_t)&typ))                  \
      .Check();
  NODE_FFI_SET_TYPE_CONSTANT(void, ffi_type_void);
  NODE_FFI_SET_TYPE_CONSTANT(uint8, ffi_type_uint8);
  NODE_FFI_SET_TYPE_CONSTANT(int8, ffi_type_sint8);
  NODE_FFI_SET_TYPE_CONSTANT(uint16, ffi_type_uint16);
  NODE_FFI_SET_TYPE_CONSTANT(int16, ffi_type_sint16);
  NODE_FFI_SET_TYPE_CONSTANT(uint32, ffi_type_uint32);
  NODE_FFI_SET_TYPE_CONSTANT(int32, ffi_type_sint32);
  NODE_FFI_SET_TYPE_CONSTANT(uint64, ffi_type_uint64);
  NODE_FFI_SET_TYPE_CONSTANT(int64, ffi_type_sint64);
  NODE_FFI_SET_TYPE_CONSTANT(float, ffi_type_float);
  NODE_FFI_SET_TYPE_CONSTANT(double, ffi_type_double);
  NODE_FFI_SET_TYPE_CONSTANT(uchar, ffi_type_uchar);
  NODE_FFI_SET_TYPE_CONSTANT(char, ffi_type_schar);
  NODE_FFI_SET_TYPE_CONSTANT(ushort, ffi_type_ushort);
  NODE_FFI_SET_TYPE_CONSTANT(short, ffi_type_sshort);  // NOLINT(runtime/int)
  NODE_FFI_SET_TYPE_CONSTANT(uint, ffi_type_uint);
  NODE_FFI_SET_TYPE_CONSTANT(int, ffi_type_sint);
  NODE_FFI_SET_TYPE_CONSTANT(ulong, ffi_type_ulong);
  NODE_FFI_SET_TYPE_CONSTANT(long, ffi_type_slong);  // NOLINT(runtime/int)
  // NODE_FFI_SET_TYPE_CONSTANT(longdouble, ffi_type_longdouble);
  NODE_FFI_SET_TYPE_CONSTANT(pointer, ffi_type_pointer);
  target->Set(context, OneByteString(isolate, "types"), types).Check();

  Local<Object> sizes = Object::New(isolate);
#define NODE_FFI_SET_SIZE_CONSTANT(typ)                                        \
  sizes                                                                        \
      ->Set(context,                                                           \
            OneByteString(isolate, #typ),                                      \
            Number::New(isolate, sizeof(typ)))                                 \
      .Check();
  NODE_FFI_SET_SIZE_CONSTANT(char);
  NODE_FFI_SET_SIZE_CONSTANT(signed char);
  NODE_FFI_SET_SIZE_CONSTANT(unsigned char);
  NODE_FFI_SET_SIZE_CONSTANT(short);               // NOLINT(runtime/int)
  NODE_FFI_SET_SIZE_CONSTANT(short int);           // NOLINT(runtime/int)
  NODE_FFI_SET_SIZE_CONSTANT(signed short);        // NOLINT(runtime/int)
  NODE_FFI_SET_SIZE_CONSTANT(signed short int);    // NOLINT(runtime/int)
  NODE_FFI_SET_SIZE_CONSTANT(unsigned short);      // NOLINT(runtime/int)
  NODE_FFI_SET_SIZE_CONSTANT(unsigned short int);  // NOLINT(runtime/int)
  NODE_FFI_SET_SIZE_CONSTANT(int);
  NODE_FFI_SET_SIZE_CONSTANT(signed);
  NODE_FFI_SET_SIZE_CONSTANT(signed int);
  NODE_FFI_SET_SIZE_CONSTANT(unsigned);
  NODE_FFI_SET_SIZE_CONSTANT(unsigned int);
  NODE_FFI_SET_SIZE_CONSTANT(long);                    // NOLINT(runtime/int)
  NODE_FFI_SET_SIZE_CONSTANT(long int);                // NOLINT(runtime/int)
  NODE_FFI_SET_SIZE_CONSTANT(signed long);             // NOLINT(runtime/int)
  NODE_FFI_SET_SIZE_CONSTANT(signed long int);         // NOLINT(runtime/int)
  NODE_FFI_SET_SIZE_CONSTANT(unsigned long);           // NOLINT(runtime/int)
  NODE_FFI_SET_SIZE_CONSTANT(unsigned long int);       // NOLINT(runtime/int)
  NODE_FFI_SET_SIZE_CONSTANT(long long);               // NOLINT(runtime/int)
  NODE_FFI_SET_SIZE_CONSTANT(long long int);           // NOLINT(runtime/int)
  NODE_FFI_SET_SIZE_CONSTANT(signed long long);        // NOLINT(runtime/int)
  NODE_FFI_SET_SIZE_CONSTANT(signed long long int);    // NOLINT(runtime/int)
  NODE_FFI_SET_SIZE_CONSTANT(unsigned long long);      // NOLINT(runtime/int)
  NODE_FFI_SET_SIZE_CONSTANT(unsigned long long int);  // NOLINT(runtime/int)
  NODE_FFI_SET_SIZE_CONSTANT(float);
  NODE_FFI_SET_SIZE_CONSTANT(double);
  // NODE_FFI_SET_SIZE_CONSTANT(long double);
  NODE_FFI_SET_SIZE_CONSTANT(char*);
  NODE_FFI_SET_SIZE_CONSTANT(uint8_t);
  NODE_FFI_SET_SIZE_CONSTANT(int8_t);
  NODE_FFI_SET_SIZE_CONSTANT(uint16_t);
  NODE_FFI_SET_SIZE_CONSTANT(int16_t);
  NODE_FFI_SET_SIZE_CONSTANT(uint32_t);
  NODE_FFI_SET_SIZE_CONSTANT(int32_t);
  NODE_FFI_SET_SIZE_CONSTANT(uint64_t);
  NODE_FFI_SET_SIZE_CONSTANT(int64_t);
  target->Set(context, OneByteString(isolate, "sizes"), sizes).Check();

  char test_char = -1;
  Local<Boolean> charIsSigned = Boolean::New(isolate, test_char < 0);
  target->Set(context, OneByteString(isolate, "charIsSigned"), charIsSigned)
      .Check();
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(SetCallBuffer);
  registry->Register(GetBufferPointer);
  registry->Register(FfiSignature::New);
  registry->Register(MakeCall);
  registry->Register(GetSymbol);
  registry->Register(GetLibrary);
}

}  // namespace ffi
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(ffi, node::ffi::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(ffi, node::ffi::RegisterExternalReferences)
