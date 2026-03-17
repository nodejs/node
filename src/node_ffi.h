#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "ffi.h"
#include "util.h"
#include "uv.h"

#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Global;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;
using v8::WeakCallbackInfo;

namespace node::ffi {

class DynamicLibrary;
struct FFIFunction;

bool IsFFINarrowSignedInteger(ffi_type* type);

bool IsFFINarrowUnsignedInteger(ffi_type* type);

bool IsFFINarrowInteger(ffi_type* type);

bool HasProperty(Local<Context> context,
                 Local<Object> object,
                 Local<String> key,
                 bool* out);

bool GetStrictSignedInteger(Local<Value> value,
                            int64_t min,
                            int64_t max,
                            int64_t* out);

bool GetStrictUnsignedInteger(Local<Value> value, uint64_t max, uint64_t* out);

bool ParseFunctionSignature(Environment* env,
                            const std::string& name,
                            Local<Object> signature,
                            ffi_type** return_type,
                            std::vector<ffi_type*>* args);

bool SignaturesMatch(const FFIFunction& fn,
                     ffi_type* return_type,
                     const std::vector<ffi_type*>& args);

bool ToFFIType(Environment* env, const std::string& type_str, ffi_type** ret);

uint8_t ToFFIArgument(Environment* env,
                      unsigned int index,
                      ffi_type* type,
                      Local<Value> arg,
                      void* ret);

Local<Value> ToJSArgument(Isolate* isolate, ffi_type* type, void* data);

bool ToJSReturnValue(Environment* env,
                     const FunctionCallbackInfo<Value>& args,
                     ffi_type* type,
                     void* result);

bool ToFFIReturnValue(Local<Value> result, ffi_type* type, void* ret);

size_t GetFFIReturnValueStorageSize(ffi_type* type);

bool GetValidatedSize(Environment* env,
                      Local<Value> value,
                      const char* label,
                      size_t* out);

bool GetValidatedPointerAddress(Environment* env,
                                Local<Value> value,
                                const char* label,
                                uintptr_t* out);

bool GetValidatedSignedInt(Environment* env,
                           Local<Value> value,
                           int64_t min,
                           int64_t max,
                           const char* type_name,
                           int64_t* out);

bool GetValidatedUnsignedInt(Environment* env,
                             Local<Value> value,
                             uint64_t max,
                             const char* type_name,
                             uint64_t* out);

bool GetValidatedPointerAndOffset(Environment* env,
                                  const FunctionCallbackInfo<Value>& args,
                                  uint8_t** ptr,
                                  size_t* offset);

bool GetValidatedPointerValueAndOffset(Environment* env,
                                       const FunctionCallbackInfo<Value>& args,
                                       uint8_t** ptr,
                                       Local<Value>* value,
                                       size_t* offset);

template <typename T>
void GetValue(const FunctionCallbackInfo<Value>& args);

template <typename T>
void SetValue(const FunctionCallbackInfo<Value>& args);

struct FFIFunction {
  bool closed;

  void* ptr;
  ffi_cif cif;
  std::vector<ffi_type*> args;
  ffi_type* return_type;
};

struct FFIFunctionInfo {
  std::shared_ptr<FFIFunction> fn;
  Global<Function> self;
};

struct FFICallback {
  DynamicLibrary* owner;
  Environment* env;
  std::thread::id thread_id;
  Global<Function> fn;
  ffi_closure* closure;

  void* ptr;
  ffi_cif cif;
  std::vector<ffi_type*> args;
  ffi_type* return_type;

  ~FFICallback() {
    if (!fn.IsEmpty()) {
      fn.Reset();
    }

    if (closure != nullptr) {
      ffi_closure_free(closure);
      closure = nullptr;
    }
  }
};

class DynamicLibrary : public BaseObject {
 public:
  DynamicLibrary(Environment* env, Local<Object> object);
  ~DynamicLibrary() override;

  void MemoryInfo(MemoryTracker* tracker) const override;

  static Local<FunctionTemplate> GetConstructorTemplate(Environment* env);
  static void New(const FunctionCallbackInfo<Value>& args);
  static void Close(const FunctionCallbackInfo<Value>& args);
  static void InvokeFunction(const FunctionCallbackInfo<Value>& args);
  static void InvokeCallback(ffi_cif* cif,
                             void* ret,
                             void** args,
                             void* user_data);

  static void GetPath(const FunctionCallbackInfo<Value>& args);
  static void GetFunction(const FunctionCallbackInfo<Value>& args);
  static void GetFunctions(const FunctionCallbackInfo<Value>& args);
  static void GetSymbol(const FunctionCallbackInfo<Value>& args);
  static void GetSymbols(const FunctionCallbackInfo<Value>& args);

  static void RegisterCallback(const FunctionCallbackInfo<Value>& args);
  static void UnregisterCallback(const FunctionCallbackInfo<Value>& args);
  static void RefCallback(const FunctionCallbackInfo<Value>& args);
  static void UnrefCallback(const FunctionCallbackInfo<Value>& args);

  SET_MEMORY_INFO_NAME(DynamicLibrary)
  SET_SELF_SIZE(DynamicLibrary)

 private:
  void Close();
  bool FindOrCreateSymbol(Environment* env,
                          const std::string& name,
                          void** ptr);
  bool FindOrCreateFunction(Environment* env,
                            const std::string& name,
                            Local<Object> signature,
                            std::shared_ptr<FFIFunction>* ret);
  Local<Function> CreateFunction(Environment* env,
                                 const std::string& name,
                                 const std::shared_ptr<FFIFunction>& fn);
  static void CleanupFunctionInfo(
      const WeakCallbackInfo<FFIFunctionInfo>& data);

  uv_lib_t lib_;
  void* handle_;
  std::string path_;
  std::unordered_map<std::string, void*> symbols_;
  std::unordered_map<std::string, std::shared_ptr<FFIFunction>> functions_;
  std::unordered_map<void*, std::unique_ptr<FFICallback>> callbacks_;
};

void GetInt8(const FunctionCallbackInfo<Value>& args);
void GetUint8(const FunctionCallbackInfo<Value>& args);
void GetInt16(const FunctionCallbackInfo<Value>& args);
void GetUint16(const FunctionCallbackInfo<Value>& args);
void GetInt32(const FunctionCallbackInfo<Value>& args);
void GetUint32(const FunctionCallbackInfo<Value>& args);
void GetInt64(const FunctionCallbackInfo<Value>& args);
void GetUint64(const FunctionCallbackInfo<Value>& args);
void GetFloat32(const FunctionCallbackInfo<Value>& args);
void GetFloat64(const FunctionCallbackInfo<Value>& args);

void SetInt8(const FunctionCallbackInfo<Value>& args);
void SetUint8(const FunctionCallbackInfo<Value>& args);
void SetInt16(const FunctionCallbackInfo<Value>& args);
void SetUint16(const FunctionCallbackInfo<Value>& args);
void SetInt32(const FunctionCallbackInfo<Value>& args);
void SetUint32(const FunctionCallbackInfo<Value>& args);
void SetInt64(const FunctionCallbackInfo<Value>& args);
void SetUint64(const FunctionCallbackInfo<Value>& args);
void SetFloat32(const FunctionCallbackInfo<Value>& args);
void SetFloat64(const FunctionCallbackInfo<Value>& args);

void ToString(const FunctionCallbackInfo<Value>& args);
void ToBuffer(const FunctionCallbackInfo<Value>& args);
void ToArrayBuffer(const FunctionCallbackInfo<Value>& args);

}  // namespace node::ffi

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
