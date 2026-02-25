#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "ffi.h"
#include "node_mem.h"
#include "util.h"

#include <dlfcn.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace node::ffi {

// Type information for FFI type conversion
struct FFIType {
  static ffi_type* ToFFI(const std::string& type_name);
  static bool IsValidType(const std::string& type_name);
};

v8::Local<v8::Object> CreatePointerObject(Environment* env, void* address);
bool IsPointerObject(Environment* env, v8::Local<v8::Value> value);
bool GetPointerAddress(Environment* env,
                       v8::Local<v8::Value> value,
                       uint64_t* address);
void FFIFunctionWrapper(const v8::FunctionCallbackInfo<v8::Value>& args);

class DynamicLibrary : public BaseObject {
 public:
  enum InternalFields {
    kInternalFieldCount = BaseObject::kInternalFieldCount,
  };

  DynamicLibrary(Environment* env, v8::Local<v8::Object> object);
  ~DynamicLibrary() override;

  void MemoryInfo(MemoryTracker* tracker) const override;

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetSymbol(const v8::FunctionCallbackInfo<v8::Value>& args);

  static BaseObjectPtr<DynamicLibrary> Create(Environment* env,
                                              v8::Local<v8::Value> path,
                                              v8::Local<v8::Value> symbols);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);

  void* GetSymbolAddress(const std::string& symbol);

  SET_MEMORY_INFO_NAME(DynamicLibrary)
  SET_SELF_SIZE(DynamicLibrary)

 private:
  void* GetSymbolPointer(const std::string& name) {
    return dlsym(handle_, name.c_str());
  }

  void* handle_;
  std::string filename_;
};

void Dlopen(const v8::FunctionCallbackInfo<v8::Value>& args);

class UnsafeFnPointer : public BaseObject {
 public:
  enum InternalFields {
    kInternalFieldCount = BaseObject::kInternalFieldCount,
  };

  UnsafeFnPointer(
      Environment* env,
      v8::Local<v8::Object> object,
      void* pointer,
      ffi_type* return_type,
      std::vector<ffi_type*> param_types,
      size_t return_struct_size,
      std::vector<std::unique_ptr<ffi_type>> owned_struct_types,
      std::vector<std::unique_ptr<ffi_type*[]>> owned_struct_elements,
      bool nonblocking = false);
  ~UnsafeFnPointer() override;

  void MemoryInfo(MemoryTracker* tracker) const override;

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Call(const v8::FunctionCallbackInfo<v8::Value>& args);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);

  v8::MaybeLocal<v8::Value> InvokeFunction(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  SET_MEMORY_INFO_NAME(UnsafeFnPointer)
  SET_SELF_SIZE(UnsafeFnPointer)

 private:
  void* pointer_;
  ffi_cif cif_;
  ffi_type* return_type_;
  std::vector<ffi_type*> param_types_;
  size_t return_struct_size_;
  std::vector<std::unique_ptr<ffi_type>> owned_struct_types_;
  std::vector<std::unique_ptr<ffi_type*[]>> owned_struct_elements_;
  bool nonblocking_;
  bool cif_prepared_;
};

class UnsafeCallback : public BaseObject {
 public:
  enum InternalFields {
    kCallbackFunction = BaseObject::kInternalFieldCount,
    kInternalFieldCount,
  };

  UnsafeCallback(Environment* env,
                 v8::Local<v8::Object> object,
                 v8::Local<v8::Object> definition,
                 v8::Local<v8::Function> callback,
                 const std::string& return_type,
                 const std::vector<std::string>& param_types);
  ~UnsafeCallback() override;

  void MemoryInfo(MemoryTracker* tracker) const override;

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Ref(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Unref(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ThreadSafe(const v8::FunctionCallbackInfo<v8::Value>& args);
  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);

  void* GetPointer() const { return code_; }

  SET_MEMORY_INFO_NAME(UnsafeCallback)
  SET_SELF_SIZE(UnsafeCallback)

 private:
  uint32_t RefCallback();
  uint32_t UnrefCallback();

  static void CallbackWrapper(ffi_cif* cif,
                              void* ret,
                              void** args,
                              void* user_data);

  v8::Global<v8::Object> definition_;
  v8::Global<v8::Function> callback_;
  v8::Global<v8::Object> pointer_;
  ffi_cif cif_;
  ffi_closure* closure_;
  void* code_;
  ffi_type* return_type_;
  std::vector<ffi_type*> param_types_;
  bool cif_prepared_;
  uint32_t ref_count_;
  bool closed_;
};

class UnsafePointer : public BaseObject {
 public:
  enum InternalFields {
    kInternalFieldCount = BaseObject::kInternalFieldCount,
  };

  UnsafePointer(Environment* env, v8::Local<v8::Object> object, void* ptr);
  ~UnsafePointer() override;

  void MemoryInfo(MemoryTracker* tracker) const override;

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Static methods.
  static void Create(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Equals(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Offset(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetValue(const v8::FunctionCallbackInfo<v8::Value>& args);

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);

  void* GetPointer() const { return pointer_; }

  SET_MEMORY_INFO_NAME(UnsafePointer)
  SET_SELF_SIZE(UnsafePointer)

 private:
  void* pointer_;
};

class UnsafePointerView : public BaseObject {
 public:
  enum InternalFields {
    kInternalFieldCount = BaseObject::kInternalFieldCount,
  };

  UnsafePointerView(Environment* env,
                    v8::Local<v8::Object> object,
                    void* pointer);
  ~UnsafePointerView() override;

  void MemoryInfo(MemoryTracker* tracker) const override;

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Instance methods
  static void GetInt8(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetUint8(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetInt16(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetUint16(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetInt32(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetUint32(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetBigInt64(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetBigUint64(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetFloat32(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetFloat64(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void SetInt8(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetUint8(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetInt16(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetUint16(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetInt32(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetUint32(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetBigInt64(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetBigUint64(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetFloat32(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetFloat64(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void GetBool(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetCString(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetPointer(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CopyInto(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetArrayBuffer(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Static methods
  static void StaticGetCString(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void StaticCopyInto(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void StaticGetArrayBuffer(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);

  void* GetPointer() const { return pointer_; }

  SET_MEMORY_INFO_NAME(UnsafePointerView)
  SET_SELF_SIZE(UnsafePointerView)

 private:
  void* pointer_;
};

}  // namespace node::ffi

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
