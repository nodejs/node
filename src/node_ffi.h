#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "ffi.h"
#include "uv.h"

#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef HAVE_FFI_FASTCALL
// cfunction_info.h includes node_ffi.h, so it cannot be included here
// (cycle); a forward declaration suffices for the unique_ptr member.
namespace node::ffi::fastcall {
struct CFunctionInfoBundle;
}  // namespace node::ffi::fastcall
#endif

namespace node::ffi {

class DynamicLibrary;
struct FFIFunction;

struct FFIFunction {
  bool closed;

  void* ptr;
  ffi_cif cif;
  std::vector<ffi_type*> args;
  ffi_type* return_type;
  std::vector<std::string> arg_type_names;
  std::string return_type_name;
};

#ifdef HAVE_FFI_FASTCALL
// Owns the per-function fast-call state for a single FFI registration.
// Two resources are owned: the slow-path libffi `v8::Global<v8::Function>`,
// and the `CFunctionInfoBundle` (which itself owns the heap-allocated
// `v8::CFunctionInfo` + `v8::CTypeInfo[]`).
//
// `FFIFunctionInfo::fast` is null for ineligible signatures (libffi-only
// path); a non-null `fast` always represents a fully-constructed
// fast-call state.
struct FastCallState {
  v8::Global<v8::Function> slow_invoke;
  std::unique_ptr<node::ffi::fastcall::CFunctionInfoBundle> cfun_bundle;

  FastCallState() = delete;

  FastCallState(v8::Isolate* isolate,
                v8::Local<v8::Function> slow_fn,
                std::unique_ptr<fastcall::CFunctionInfoBundle> bndl);
  ~FastCallState();
  FastCallState(const FastCallState&) = delete;
  FastCallState& operator=(const FastCallState&) = delete;
};
#endif

struct FFIFunctionInfo {
  std::shared_ptr<FFIFunction> fn;
  v8::Global<v8::Function> self;

#ifdef HAVE_FFI_FASTCALL
  std::unique_ptr<FastCallState> fast;  // null when ineligible
#endif
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

struct ResolvedFunction {
  std::string name;
  std::shared_ptr<FFIFunction> fn;
  bool should_cache_symbol;
  bool should_cache_function;
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

#ifdef HAVE_FFI_FASTCALL
 public:
  // Returns the per-library "alive" ArrayBuffer. The first byte is 0
  // while the library is open and 1 after Close() runs. Wrappers in
  // lib/internal/ffi-fastcall.js use this to throw ERR_FFI_LIBRARY_CLOSED
  // before the V8 fast-call path enters a stub pointing at unmapped
  // library memory.
  v8::Local<v8::ArrayBuffer> AliveBuffer(v8::Isolate* isolate);
#endif

 private:
  void Close();
  v8::Maybe<void*> ResolveSymbol(Environment* env, const std::string& name);
  struct PreparedFunction {
    std::shared_ptr<FFIFunction> fn;
    bool should_cache_symbol;
    bool should_cache_function;
  };
  v8::Maybe<PreparedFunction> PrepareFunction(Environment* env,
                                              const std::string& name,
                                              v8::Local<v8::Object> signature);
  v8::MaybeLocal<v8::Function> CreateFunction(
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
#ifdef HAVE_FFI_FASTCALL
  std::shared_ptr<v8::BackingStore> alive_backing_;
#endif
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
void ExportBytes(const v8::FunctionCallbackInfo<v8::Value>& args);
void GetRawPointer(const v8::FunctionCallbackInfo<v8::Value>& args);

}  // namespace node::ffi

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
