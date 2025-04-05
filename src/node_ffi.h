#ifndef SRC_NODE_FFI_H_
#define SRC_NODE_FFI_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#include "env.h"
#include "ffi.h"

namespace node::ffi {
using binding::DLib;
using permission::PermissionScope;
using v8::Array;
using v8::ArrayBuffer;
using v8::BackingStore;
using v8::BigInt;
using v8::Boolean;
using v8::Context;
using v8::External;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Int32;
using v8::Isolate;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::Persistent;
using v8::Uint32;
using v8::Value;

void* readAddress(Local<Value> value);
int64_t readInt64(Local<Value> value);
uint64_t readUInt64(Local<Value> value);
double readDouble(Local<Value> value);
std::string readString(Isolate* isolate, Local<Value> value);
void ffiCallback(ffi_cif* cif, void* ret, ffi_raw* args, void* hint);

class FFILibrary : public DLib {
 public:
  explicit FFILibrary(const char* path);
  ~FFILibrary();
};

class FFIDefinition {
 public:
  static constexpr auto ABI = FFI_DEFAULT_ABI;

  explicit FFIDefinition(const char* defStr);
  void readValue(int i, Local<Value> input, ffi_raw* output) const;
  Local<Value> wrapValue(int i, Isolate* isolate, const ffi_raw* input) const;

  ffi_cif cif{};
  std::unique_ptr<ffi_type*[]> types;
};

class FFIInvoker : public FFIDefinition {
 public:
  explicit FFIInvoker(const char* defStr, const void* address);
  void setParam(int i, Local<Value> value);
  Local<Value> doInvoke(Isolate* isolate);

  void (*invoker)(){};
  std::unique_ptr<ffi_raw[]> datas;
};

class FFICallback : public FFIDefinition {
 public:
  static constexpr auto RCS = sizeof(ffi_raw_closure);

  explicit FFICallback(const char* defStr);
  void setCallback(Isolate* isolate, Local<Value> value);
  void doCallback(ffi_raw* result, int argc, const ffi_raw* args) const;
  ~FFICallback();

  ffi_raw_closure* frc{};
  void* address{};
  Persistent<Function> callback;
};
}  // namespace node::ffi

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_NODE_FFI_H_
