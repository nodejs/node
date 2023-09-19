#ifndef SRC_NODE_FFI_H_
#define SRC_NODE_FFI_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object-inl.h"
#include "base_object_types.h"
#include "env.h"
#include "ffi.h"
#include "node_binding.h"
#include "v8-function-callback.h"
#include "v8.h"

using v8::Array;
using v8::BigInt;
using v8::Context;
using v8::Local;
using v8::Object;
using v8::Value;

namespace node {
namespace ffi {

class FfiSignature : public BaseObject {
 public:
  FfiSignature(Environment* env,
               Local<Object> object,
               Local<BigInt> fn,
               Local<BigInt> ret_type,
               Local<Array> arg_types);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  SET_SELF_SIZE(FfiSignature)
  SET_MEMORY_INFO_NAME(FfiSignature)
  SET_NO_MEMORY_INFO()

  ffi_cif cif_;
  std::vector<ffi_type*> argv_types_;
  void (*fn_)();
};

class FfiBindingData : public BaseObject {
 public:
  FfiBindingData(Realm* realm, Local<Object> wrap) : BaseObject(realm, wrap) {}

  binding::DLib * GetLibrary(std::string fname);

  SET_BINDING_ID(ffi_binding_data)

  void* call_buffer;

  SET_SELF_SIZE(FfiBindingData)
  SET_MEMORY_INFO_NAME(FfiBindingData)
  SET_NO_MEMORY_INFO()

 private:
  std::map<std::string, binding::DLib*> libraries_;
};

void MakeCall(const v8::FunctionCallbackInfo<Value>& args);
void AddSignature(const v8::FunctionCallbackInfo<Value>& args);
void SetCallBuffer(const v8::FunctionCallbackInfo<Value>& args);
void GetLibrary(const v8::FunctionCallbackInfo<Value>& args);
void GetSymbol(const v8::FunctionCallbackInfo<Value>& args);

void Initialize(v8::Local<v8::Object> target,
                v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context,
                void* priv);
}  // namespace ffi
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_FFI_H_
