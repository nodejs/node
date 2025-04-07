#ifndef SRC_NODE_FFI_H_
#define SRC_NODE_FFI_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#include "env.h"
#include "ffi.h"

namespace node::ffi {
void* readAddress(v8::Local<v8::Value> value);
int64_t readInt64(v8::Local<v8::Value> value);
uint64_t readUInt64(v8::Local<v8::Value> value);
double readDouble(v8::Local<v8::Value> value);
std::string readString(v8::Isolate* isolate, v8::Local<v8::Value> value);
void ffiCallback(ffi_cif* cif, void* ret, ffi_raw* args, void* hint);

class FFILibrary : public binding::DLib {
 public:
  explicit FFILibrary(const char* path);
  ~FFILibrary();
};

class FFIDefinition {
 public:
  static constexpr auto ABI = FFI_DEFAULT_ABI;

  explicit FFIDefinition(const char* defStr);
  void readValue(int i, v8::Local<v8::Value> input, ffi_raw* output) const;
  v8::Local<v8::Value> wrapValue(int i,
                                 v8::Isolate* isolate,
                                 const ffi_raw* input) const;

  ffi_cif cif{};
  std::unique_ptr<ffi_type*[]> types;
};

class FFIInvoker : public FFIDefinition {
 public:
  explicit FFIInvoker(const char* defStr, const void* address);
  void setParam(int i, v8::Local<v8::Value> value);
  v8::Local<v8::Value> doInvoke(v8::Isolate* isolate);

  void (*invoker)(){};
  std::unique_ptr<ffi_raw[]> datas;
};

class FFICallback : public FFIDefinition {
 public:
  static constexpr auto RCS = sizeof(ffi_raw_closure);

  explicit FFICallback(const char* defStr);
  void setCallback(v8::Isolate* isolate, v8::Local<v8::Value> value);
  void doCallback(ffi_raw* result, int argc, const ffi_raw* args) const;
  ~FFICallback();

  ffi_raw_closure* frc{};
  void* address{};
  v8::Persistent<v8::Function> callback;
};
}  // namespace node::ffi

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_NODE_FFI_H_
