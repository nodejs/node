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

namespace node::ffi {

class DynamicLibrary;
struct FFIFunction;

bool IsFFINarrowSignedInteger(ffi_type* type);

bool IsFFINarrowUnsignedInteger(ffi_type* type);

bool IsFFINarrowInteger(ffi_type* type);

bool HasProperty(v8::Local<v8::Context> context,
                 v8::Local<v8::Object> object,
                 v8::Local<v8::String> key,
                 bool* out);

bool GetStrictSignedInteger(v8::Local<v8::Value> value,
                            int64_t min,
                            int64_t max,
                            int64_t* out);

bool GetStrictUnsignedInteger(v8::Local<v8::Value> value,
                              uint64_t max,
                              uint64_t* out);

bool ParseFunctionSignature(Environment* env,
                            const std::string& name,
                            v8::Local<v8::Object> signature,
                            ffi_type** return_type,
                            std::vector<ffi_type*>* args);

bool SignaturesMatch(const FFIFunction& fn,
                     ffi_type* return_type,
                     const std::vector<ffi_type*>& args);

bool ToFFIType(Environment* env, const std::string& type_str, ffi_type** ret);

uint8_t ToFFIArgument(Environment* env,
                      unsigned int index,
                      ffi_type* type,
                      v8::Local<v8::Value> arg,
                      void* ret);

v8::Local<v8::Value> ToJSArgument(v8::Isolate* isolate,
                                  ffi_type* type,
                                  void* data);

bool ToJSReturnValue(Environment* env,
                     const v8::FunctionCallbackInfo<v8::Value>& args,
                     ffi_type* type,
                     void* result);

bool ToFFIReturnValue(v8::Local<v8::Value> result, ffi_type* type, void* ret);

size_t GetFFIReturnValueStorageSize(ffi_type* type);

bool GetValidatedSize(Environment* env,
                      v8::Local<v8::Value> value,
                      const char* label,
                      size_t* out);

bool GetValidatedPointerAddress(Environment* env,
                                v8::Local<v8::Value> value,
                                const char* label,
                                uintptr_t* out);

bool GetValidatedSignedInt(Environment* env,
                           v8::Local<v8::Value> value,
                           int64_t min,
                           int64_t max,
                           const char* type_name,
                           int64_t* out);

bool GetValidatedUnsignedInt(Environment* env,
                             v8::Local<v8::Value> value,
                             uint64_t max,
                             const char* type_name,
                             uint64_t* out);

bool GetValidatedPointerAndOffset(
    Environment* env,
    const v8::FunctionCallbackInfo<v8::Value>& args,
    uint8_t** ptr,
    size_t* offset);

bool GetValidatedPointerValueAndOffset(
    Environment* env,
    const v8::FunctionCallbackInfo<v8::Value>& args,
    uint8_t** ptr,
    v8::Local<v8::Value>* value,
    size_t* offset);

template <typename T>
void GetValue(const v8::FunctionCallbackInfo<v8::Value>& args);

template <typename T>
void SetValue(const v8::FunctionCallbackInfo<v8::Value>& args);

struct FFIFunction {
  bool closed;

  void* ptr;
  ffi_cif cif;
  std::vector<ffi_type*> args;
  ffi_type* return_type;
};

struct FFIFunctionInfo {
  std::shared_ptr<FFIFunction> fn;
  v8::Global<v8::Function> self;
};

struct FFICallback {
  DynamicLibrary* owner;
  Environment* env;
  std::thread::id thread_id;
  v8::Global<v8::Function> fn;
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
  DynamicLibrary(Environment* env, v8::Local<v8::Object> object);
  ~DynamicLibrary() override;

  void MemoryInfo(MemoryTracker* tracker) const override;

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void InvokeFunction(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void InvokeCallback(ffi_cif* cif,
                             void* ret,
                             void** args,
                             void* user_data);

  static void GetPath(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetFunction(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetFunctions(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetSymbol(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetSymbols(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void RegisterCallback(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void UnregisterCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RefCallback(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void UnrefCallback(const v8::FunctionCallbackInfo<v8::Value>& args);

  SET_MEMORY_INFO_NAME(DynamicLibrary)
  SET_SELF_SIZE(DynamicLibrary)

 private:
  void Close();
  bool FindOrCreateSymbol(Environment* env,
                          const std::string& name,
                          void** ptr);
  bool FindOrCreateFunction(Environment* env,
                            const std::string& name,
                            v8::Local<v8::Object> signature,
                            std::shared_ptr<FFIFunction>* ret);
  v8::Local<v8::Function> CreateFunction(
      Environment* env,
      const std::string& name,
      const std::shared_ptr<FFIFunction>& fn);
  static void CleanupFunctionInfo(
      const v8::WeakCallbackInfo<FFIFunctionInfo>& data);

  uv_lib_t lib_;
  void* handle_;
  std::string path_;
  std::unordered_map<std::string, void*> symbols_;
  std::unordered_map<std::string, std::shared_ptr<FFIFunction>> functions_;
  std::unordered_map<void*, std::unique_ptr<FFICallback>> callbacks_;
};

void GetInt8(const v8::FunctionCallbackInfo<v8::Value>& args);
void GetUint8(const v8::FunctionCallbackInfo<v8::Value>& args);
void GetInt16(const v8::FunctionCallbackInfo<v8::Value>& args);
void GetUint16(const v8::FunctionCallbackInfo<v8::Value>& args);
void GetInt32(const v8::FunctionCallbackInfo<v8::Value>& args);
void GetUint32(const v8::FunctionCallbackInfo<v8::Value>& args);
void GetInt64(const v8::FunctionCallbackInfo<v8::Value>& args);
void GetUint64(const v8::FunctionCallbackInfo<v8::Value>& args);
void GetFloat32(const v8::FunctionCallbackInfo<v8::Value>& args);
void GetFloat64(const v8::FunctionCallbackInfo<v8::Value>& args);

void SetInt8(const v8::FunctionCallbackInfo<v8::Value>& args);
void SetUint8(const v8::FunctionCallbackInfo<v8::Value>& args);
void SetInt16(const v8::FunctionCallbackInfo<v8::Value>& args);
void SetUint16(const v8::FunctionCallbackInfo<v8::Value>& args);
void SetInt32(const v8::FunctionCallbackInfo<v8::Value>& args);
void SetUint32(const v8::FunctionCallbackInfo<v8::Value>& args);
void SetInt64(const v8::FunctionCallbackInfo<v8::Value>& args);
void SetUint64(const v8::FunctionCallbackInfo<v8::Value>& args);
void SetFloat32(const v8::FunctionCallbackInfo<v8::Value>& args);
void SetFloat64(const v8::FunctionCallbackInfo<v8::Value>& args);

void ToString(const v8::FunctionCallbackInfo<v8::Value>& args);
void ToBuffer(const v8::FunctionCallbackInfo<v8::Value>& args);
void ToArrayBuffer(const v8::FunctionCallbackInfo<v8::Value>& args);

}  // namespace node::ffi

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
