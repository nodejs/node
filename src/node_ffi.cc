#if HAVE_FFI

#include "node_ffi.h"
#include <climits>
#include <cstring>
#include <limits>
#include <memory>
#include "base_object-inl.h"
#include "env-inl.h"
#include "ffi/data.h"
#include "ffi/types.h"
#include "node_errors.h"

namespace node {

using v8::Array;
using v8::ArrayBuffer;
using v8::BigInt;
using v8::Boolean;
using v8::Context;
using v8::DontDelete;
using v8::DontEnum;
using v8::External;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Global;
using v8::HandleScope;
using v8::Isolate;
using v8::Just;
using v8::Local;
using v8::LocalVector;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Null;
using v8::Object;
using v8::PropertyAttribute;
using v8::ReadOnly;
using v8::String;
using v8::TryCatch;
using v8::Value;
using v8::WeakCallbackInfo;
using v8::WeakCallbackType;

namespace ffi {

DynamicLibrary::DynamicLibrary(Environment* env, Local<Object> object)
    : BaseObject(env, object), lib_{}, handle_(nullptr), symbols_() {
  MakeWeak();
}

DynamicLibrary::~DynamicLibrary() {
  this->Close();
}

void DynamicLibrary::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("path", path_.capacity() + 1, "std::string");

  size_t symbols_size = 0;
  for (const auto& [name, ptr] : symbols_) {
    symbols_size += name.capacity() + 1;
    symbols_size += sizeof(ptr);
    symbols_size += sizeof(decltype(symbols_)::value_type);
  }

  tracker->TrackFieldWithSize(
      "symbols", symbols_size, "std::unordered_map<std::string, void*>");

  // FFIFunctionInfo instances and their sb_backing ArrayBuffers are
  // owned by V8 function wrappers and reachable only via weak references,
  // so they are deliberately not counted here.
}

void DynamicLibrary::Close() {
  for (auto& [name, fn] : functions_) {
    fn->closed = true;
    fn->ptr = nullptr;
  }

  // Closing the library invalidates all registered callbacks. Node.js does not
  // track or revoke callback pointers that have already been handed to native
  // code. If native code calls a callback pointer after `close()` or
  // `unregisterCallback()`, the behavior is undefined, not allowed, and
  // dangerous: it can crash the process, produce incorrect output, or corrupt
  // memory.

  if (handle_ != nullptr) {
    uv_dlclose(&lib_);
    handle_ = nullptr;
  }

  symbols_.clear();
  functions_.clear();
  callbacks_.clear();
}

Maybe<void*> DynamicLibrary::ResolveSymbol(Environment* env,
                                           const std::string& name) {
  if (handle_ == nullptr) {
    THROW_ERR_FFI_LIBRARY_CLOSED(env);
    return {};
  }

  auto existing = symbols_.find(name);
  void* ptr;

  if (existing != symbols_.end()) {
    ptr = existing->second;
  } else {
    if (uv_dlsym(&lib_, name.c_str(), &ptr) != 0) {
      THROW_ERR_FFI_CALL_FAILED(env, "dlsym failed: %s", uv_dlerror(&lib_));
      return {};
    }
  }

  return Just(ptr);
}

Maybe<DynamicLibrary::PreparedFunction> DynamicLibrary::PrepareFunction(
    Environment* env, const std::string& name, Local<Object> signature) {
  std::shared_ptr<FFIFunction> fn;
  auto existing = functions_.find(name);
  FunctionSignature parsed;

  if (!ParseFunctionSignature(env, name, signature).To(&parsed)) {
    return {};
  }
  auto [return_type, args, return_type_name, arg_type_names] =
      std::move(parsed);

  bool should_cache_symbol = false;
  bool should_cache_function = false;

  if (existing == functions_.end()) {
    void* ptr;

    if (!ResolveSymbol(env, name).To(&ptr)) {
      return {};
    }

    should_cache_symbol = symbols_.find(name) == symbols_.end();

    fn = std::make_shared<FFIFunction>(
        FFIFunction{.closed = false,
                    .ptr = ptr,
                    .cif = {},
                    .args = args,
                    .return_type = return_type,
                    .arg_type_names = std::move(arg_type_names),
                    .return_type_name = std::move(return_type_name)});

    ffi_status status = ffi_prep_cif(&fn->cif,
                                     FFI_DEFAULT_ABI,
                                     fn->args.size(),
                                     fn->return_type,
                                     fn->args.data());
    if (status != FFI_OK) {
      const char* msg = "ffi_prep_cif failed";
      switch (status) {
        case FFI_BAD_TYPEDEF:
          msg = "ffi_prep_cif failed: bad typedef";
          break;
        case FFI_BAD_ABI:
          msg = "ffi_prep_cif failed: bad ABI";
          break;
        default:
          msg = "ffi_prep_cif failed: unknown error";
          break;
      }

      THROW_ERR_FFI_CALL_FAILED(env, msg);
      return {};
    }

    should_cache_function = true;
  } else {
    fn = existing->second;

    if (!SignaturesMatch(*fn, return_type, args)) {
      THROW_ERR_INVALID_ARG_VALUE(
          env,
          "Function %s"
          " was already requested with a different signature",
          name);
      return {};
    }
  }

  return Just(PreparedFunction{fn, should_cache_symbol, should_cache_function});
}

void DynamicLibrary::CleanupFunctionInfo(
    const WeakCallbackInfo<FFIFunctionInfo>& data) {
  FFIFunctionInfo* info = data.GetParameter();
  info->fn.reset();
  info->self.Reset();
  delete info;
}

MaybeLocal<Function> DynamicLibrary::CreateFunction(
    Environment* env,
    const std::string& name,
    const std::shared_ptr<FFIFunction>& fn) {
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();

  // The info is held in a unique_ptr so early-return paths free it. Ownership
  // moves to the weak callback via `release()` once `SetWeak` succeeds.
  auto info = std::make_unique<FFIFunctionInfo>();
  info->fn = fn;

  DCHECK_EQ(fn->args.size(), fn->arg_type_names.size());

  bool use_sb = IsSBEligibleSignature(*fn);
  bool has_ptr_args = use_sb && SignatureHasPointerArgs(*fn);

  Local<External> data =
      External::New(isolate, info.get(), v8::kExternalPointerTypeTagDefault);

  MaybeLocal<Function> maybe_ret =
      Function::New(context,
                    use_sb ? DynamicLibrary::InvokeFunctionSB
                           : DynamicLibrary::InvokeFunction,
                    data);
  Local<Function> ret;
  if (!maybe_ret.ToLocal(&ret)) {
    return MaybeLocal<Function>();
  }

  Local<Value> name_str;
  if (!ToV8Value(env->context(), name, isolate).ToLocal(&name_str)) {
    return MaybeLocal<Function>();
  }
  ret->SetName(name_str.As<String>());

  if (!ret->Set(
              context,
              env->pointer_string(),
              BigInt::NewFromUnsigned(
                  isolate,
                  static_cast<uint64_t>(reinterpret_cast<uintptr_t>(fn->ptr))))
           .FromMaybe(false)) {
    return MaybeLocal<Function>();
  }

  // Internal properties are keyed by per-isolate Symbols (see
  // `env_properties.h`) to keep them out of string-key reflection, and the
  // `ReadOnly | DontEnum | DontDelete` attribute set blocks user code from
  // reading, modifying, or deleting them.
  PropertyAttribute internal_attrs =
      static_cast<PropertyAttribute>(ReadOnly | DontEnum | DontDelete);

  if (use_sb) {
    size_t sb_size = 8 * (fn->args.size() + 1);
    Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, sb_size);
    // The shared_ptr to the backing store keeps the memory alive while
    // FFIFunctionInfo still references it.
    info->sb_backing = ab->GetBackingStore();

    if (!ret->DefineOwnProperty(
                context, env->ffi_sb_shared_buffer_symbol(), ab, internal_attrs)
             .FromMaybe(false)) {
      return MaybeLocal<Function>();
    }

    // Signatures with pointer args also expose a slow-path invoker bound
    // to the same FFIFunctionInfo. The JS wrapper routes through it when a
    // pointer argument is anything other than a BigInt, null, or undefined
    // (strings, Buffers, ArrayBuffers, and ArrayBufferViews).
    if (has_ptr_args) {
      Local<Function> slow_fn;
      if (!Function::New(context, DynamicLibrary::InvokeFunction, data)
               .ToLocal(&slow_fn)) {
        return MaybeLocal<Function>();
      }
      if (!ret->DefineOwnProperty(context,
                                  env->ffi_sb_invoke_slow_symbol(),
                                  slow_fn,
                                  internal_attrs)
               .FromMaybe(false)) {
        return MaybeLocal<Function>();
      }
    }

    // Attach the original signature type names so the JS wrapper can
    // rebuild the signature from a raw function when the caller did not
    // pass parameters and result explicitly. The `lib.functions` accessor
    // path relies on this.
    Local<Value> params_arr;
    if (!ToV8Value(context, fn->arg_type_names, isolate).ToLocal(&params_arr)) {
      return MaybeLocal<Function>();
    }
    if (!ret->DefineOwnProperty(context,
                                env->ffi_sb_params_symbol(),
                                params_arr,
                                internal_attrs)
             .FromMaybe(false)) {
      return MaybeLocal<Function>();
    }

    Local<Value> result_name;
    if (!ToV8Value(context, fn->return_type_name, isolate)
             .ToLocal(&result_name)) {
      return MaybeLocal<Function>();
    }
    if (!ret->DefineOwnProperty(context,
                                env->ffi_sb_result_symbol(),
                                result_name,
                                internal_attrs)
             .FromMaybe(false)) {
      return MaybeLocal<Function>();
    }
  }

  info->self.Reset(isolate, ret);
  info->self.SetWeak(info.release(),
                     DynamicLibrary::CleanupFunctionInfo,
                     WeakCallbackType::kParameter);

  return ret;
}

void DynamicLibrary::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!args.IsConstructCall()) {
    return THROW_ERR_CONSTRUCT_CALL_REQUIRED(
        env,
        "Class constructor DynamicLibrary cannot be invoked without 'new'");
  }

  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFFI, "");

#ifndef _WIN32
  if (args.Length() < 1 || (!args[0]->IsString() && !args[0]->IsNull())) {
    THROW_ERR_INVALID_ARG_TYPE(env, "Library path must be a string or null");
    return;
  }
#else
  if (args.Length() < 1 || !args[0]->IsString()) {
    THROW_ERR_INVALID_ARG_TYPE(env, "Library path must be a string");
    return;
  }
#endif

  const char* library_path = nullptr;
  DynamicLibrary* lib = new DynamicLibrary(env, args.This());

  if (args[0]->IsString()) {
    Utf8Value filename(env->isolate(), args[0]);
    if (ThrowIfContainsNullBytes(env, filename, "Library path")) {
      return;
    }
    lib->path_ = filename.ToString();
    library_path = lib->path_.c_str();
  }

  // Open the library
  if (uv_dlopen(library_path, &lib->lib_) != 0) {
    THROW_ERR_FFI_CALL_FAILED(env, "dlopen failed: %s", uv_dlerror(&lib->lib_));
    return;
  }

  lib->handle_ = static_cast<void*>(lib->lib_.handle);
}

void DynamicLibrary::Close(const FunctionCallbackInfo<Value>& args) {
  DynamicLibrary* lib = Unwrap<DynamicLibrary>(args.This());
  // Closing a library from one of its active callbacks is unsupported and
  // dangerous. Callbacks must return before the owning library is closed.
  lib->Close();
}

void DynamicLibrary::InvokeFunction(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  FFIFunctionInfo* info = static_cast<FFIFunctionInfo*>(
      args.Data().As<External>()->Value(v8::kExternalPointerTypeTagDefault));
  FFIFunction* fn = info->fn.get();

  if (fn == nullptr || fn->closed || fn->ptr == nullptr) {
    THROW_ERR_FFI_LIBRARY_CLOSED(env);
    return;
  }

  // Convert arguments
  unsigned int expected_args = fn->args.size();
  unsigned int provided_args = args.Length();

  if (provided_args != expected_args) {
    THROW_ERR_INVALID_ARG_VALUE(env,
                                "Invalid argument count: expected %s, got %s",
                                expected_args,
                                provided_args);
    return;
  }

  std::vector<uint64_t> values(expected_args, 0);
  std::vector<void*> ffi_args(expected_args, nullptr);
  std::vector<std::string> strings;
  strings.reserve(expected_args);

  for (unsigned int i = 0; i < expected_args; i++) {
    FFIArgumentCategory res;

    if (!ToFFIArgument(env, i, fn->args[i], args[i], &values[i]).To(&res)) {
      return;
    }

    // The argument is a string, we need to copy
    if (res == FFIArgumentCategory::String) {
      Utf8Value str(env->isolate(), args[i]);

      if (*str == nullptr) {
        THROW_ERR_INVALID_ARG_TYPE(env, "Argument %s must be a string", i);
        return;
      }

      if (ThrowIfContainsNullBytes(env, str, "Argument " + std::to_string(i))) {
        return;
      }

      strings.push_back(*str);
      values[i] = reinterpret_cast<uint64_t>(strings.back().c_str());
      ffi_args[i] = &values[i];
    } else {
      ffi_args[i] = &values[i];
    }
  }

  void* result = nullptr;

  if (fn->return_type->type != FFI_TYPE_VOID) {
    result = Malloc(GetFFIReturnValueStorageSize(fn->return_type));
  }

  ffi_call(&fn->cif, FFI_FN(fn->ptr), result, ffi_args.data());

  // Return result back to Javascript
  ToJSReturnValue(env, args, fn->return_type, result);
  free(result);
}

void DynamicLibrary::InvokeFunctionSB(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  FFIFunctionInfo* info =
      static_cast<FFIFunctionInfo*>(args.Data().As<External>()->Value());
  FFIFunction* fn = info->fn.get();

  if (fn == nullptr || fn->closed || fn->ptr == nullptr) {
    THROW_ERR_FFI_LIBRARY_CLOSED(env);
    return;
  }

  // Arguments reach the native invoker through the shared buffer, not
  // through V8. The JS wrapper always calls the raw function as `rawFn()`
  // so any non-zero argument count indicates that user code reached the
  // raw SB function directly and is about to read stale buffer contents.
  if (args.Length() != 0) {
    THROW_ERR_INVALID_ARG_VALUE(
        env,
        "SB-invoked FFI functions receive arguments through the shared "
        "buffer, not as JavaScript arguments");
    return;
  }

  // A failure of either CHECK means the SB invoker ran against a function
  // that `CreateFunction` did not set up for the fast path, which is a
  // contract violation. They stay enabled in Release because each FFI call
  // is already dominated by `ffi_call` itself.
  CHECK(info->sb_backing);
  CHECK_EQ(info->sb_backing->ByteLength(), 8u * (info->fn->args.size() + 1));

  uint8_t* buffer = static_cast<uint8_t*>(info->sb_backing->Data());
  unsigned int nargs = fn->args.size();

  // Layout is 8 bytes per slot. The return value lives at offset 0 and
  // argument i lives at offset 8*(i+1).
  std::vector<uint64_t> values(nargs, 0);
  std::vector<void*> ffi_args(nargs, nullptr);

  for (unsigned int i = 0; i < nargs; i++) {
    ReadFFIArgFromBuffer(fn->args[i], buffer, 8 * (i + 1), &values[i]);
    ffi_args[i] = &values[i];
  }

  // The storage must cover both the ffi_arg width that libffi uses for
  // promoted small integer returns and the 8 bytes needed for non-promoted
  // SB-eligible returns like f64, i64, and u64. `sizeof(ffi_arg)` is only
  // 4 on 32-bit ARM, so take the max.
  constexpr size_t kSBResultStorageSize =
      sizeof(ffi_arg) > 8 ? sizeof(ffi_arg) : 8;
  alignas(8) uint8_t result_storage[kSBResultStorageSize] = {0};
  void* result = (fn->return_type != &ffi_type_void) ? result_storage : nullptr;

  ffi_call(&fn->cif, FFI_FN(fn->ptr), result, ffi_args.data());

  if (result != nullptr) {
    WriteFFIReturnToBuffer(fn->return_type, result, buffer, 0);
  }
}

// This is the function that will be called by libffi when a callback
// is invoked from a dlopen library. It converts the arguments to JavaScript
// values and calls the original JavaScript callback function.
// It also handles the return value and exceptions properly.
// Note that since this function is called from native code, it must not throw
// exceptions or return promises, as there is no defined way to propagate them
// back to the caller.
// If such cases occur, the process will be aborted to avoid undefined behavior.
void DynamicLibrary::InvokeCallback(ffi_cif* cif,
                                    void* ret,
                                    void** args,
                                    void* user_data) {
  FFICallback* cb = static_cast<FFICallback*>(user_data);

  // It is unsupported and dangerous for a callback to unregister itself or
  // close its owning library while executing. The current invocation must
  // return before teardown APIs are used.
  if (cb->owner->handle_ == nullptr || cb->ptr == nullptr) {
    if (ret != nullptr && cb->return_type->size > 0) {
      std::memset(ret, 0, GetFFIReturnValueStorageSize(cb->return_type));
    }
    return;
  }

  if (std::this_thread::get_id() != cb->thread_id) {
    FPrintF(stderr,
            "Callbacks can only be invoked on the system thread they were "
            "created on\n");
    ABORT();
  }

  Environment* env = cb->env;
  Isolate* isolate = env->isolate();

  HandleScope handle_scope(isolate);
  Local<Context> context = env->context();

  if (cb->fn.IsEmpty()) {
    if (ret != nullptr && cb->return_type->size > 0) {
      std::memset(ret, 0, GetFFIReturnValueStorageSize(cb->return_type));
    }
    return;
  }

  size_t expected_args = cb->args.size();
  LocalVector<Value> callback_args(isolate, expected_args);

  for (size_t i = 0; i < expected_args; i++) {
    if (args[i] == nullptr) {
      callback_args[i] = Null(isolate);
      continue;
    } else {
      callback_args[i] = ToJSArgument(isolate, cb->args[i], args[i]);
    }
  }

  TryCatch try_catch(isolate);
  Local<Function> callback = Local<Function>::New(isolate, cb->fn);
  MaybeLocal<Value> result = callback->Call(
      context, Undefined(isolate), expected_args, callback_args.data());

  // Handle exceptions by crashing (can't propagate across FFI boundary)
  if (try_catch.HasCaught()) {
    FPrintF(stderr, "Callbacks cannot throw an exception\n");
    ABORT();
  }

  Local<Value> result_val;
  if (!result.ToLocal(&result_val)) {
    if (try_catch.HasCaught()) {
      FPrintF(stderr, "Callbacks cannot return an exception\n");
      ABORT();
    }
    return;
  }

  if (result_val->IsPromise()) {
    FPrintF(stderr, "Callbacks cannot return promises\n");
    ABORT();
  }

  if (!ToFFIReturnValue(result_val, cb->return_type, ret)) {
    FPrintF(stderr, "Callback returned invalid value for declared FFI type\n");
    ABORT();
  }
}

void DynamicLibrary::GetPath(const FunctionCallbackInfo<Value>& args) {
  DynamicLibrary* lib = Unwrap<DynamicLibrary>(args.This());

  Local<Value> path;
  if (!ToV8Value(lib->env()->context(), lib->path_, args.GetIsolate())
           .ToLocal(&path)) {
    return;
  }

  args.GetReturnValue().Set(path);
}

void DynamicLibrary::GetFunction(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  if (args.Length() < 1 || !args[0]->IsString()) {
    THROW_ERR_INVALID_ARG_TYPE(env, "Function name must be a string");
    return;
  }

  if (args.Length() < 2 || !args[1]->IsObject() || args[1]->IsArray()) {
    THROW_ERR_INVALID_ARG_TYPE(env, "Function signature must be an object");
    return;
  }

  DynamicLibrary* lib = Unwrap<DynamicLibrary>(args.This());
  Utf8Value name(isolate, args[0]);
  if (ThrowIfContainsNullBytes(env, name, "Function name")) {
    return;
  }
  PreparedFunction prepared;

  Local<Object> signature = args[1].As<Object>();
  if (!lib->PrepareFunction(env, *name, signature).To(&prepared)) {
    return;
  }
  auto [fn, should_cache_symbol, should_cache_function] = prepared;

  if (should_cache_symbol) {
    lib->symbols_.emplace(*name, fn->ptr);
  }
  if (should_cache_function) {
    lib->functions_.emplace(*name, fn);
  }

  MaybeLocal<Function> maybe_ret = lib->CreateFunction(env, *name, fn);
  Local<Function> ret;
  if (!maybe_ret.ToLocal(&ret)) {
    return;
  }
  args.GetReturnValue().Set(ret);
}

void DynamicLibrary::GetFunctions(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  DynamicLibrary* lib = Unwrap<DynamicLibrary>(args.This());

  if (lib->handle_ == nullptr) {
    THROW_ERR_FFI_LIBRARY_CLOSED(env);
    return;
  }

  Local<Object> functions = Object::New(isolate);
  if (!functions->SetPrototypeV2(context, Null(isolate)).FromMaybe(false)) {
    return;
  }

  if (args.Length() > 0) {
    if (!args[0]->IsObject() || args[0]->IsArray()) {
      THROW_ERR_INVALID_ARG_TYPE(env, "Functions signatures must be an object");
      return;
    }

    Local<Object> signatures = args[0].As<Object>();
    Local<Array> keys;
    if (!signatures->GetOwnPropertyNames(context).ToLocal(&keys)) {
      return;
    }

    std::vector<ResolvedFunction> pending;
    pending.reserve(keys->Length());

    for (uint32_t i = 0; i < keys->Length(); i++) {
      Local<Value> key;
      Local<Value> signature;

      if (!keys->Get(context, i).ToLocal(&key)) {
        return;
      }

      Utf8Value name(isolate, key);
      if (ThrowIfContainsNullBytes(env, name, "Function name")) {
        return;
      }

      if (!signatures->Get(env->context(), key).ToLocal(&signature)) {
        return;
      }

      if (!signature->IsObject() || signature->IsArray()) {
        THROW_ERR_INVALID_ARG_TYPE(
            env, "Signature of function %s must be an object", name);
        return;
      }

      PreparedFunction prepared;

      Local<Object> signature_object = signature.As<Object>();
      if (!lib->PrepareFunction(env, *name, signature_object).To(&prepared)) {
        return;
      }
      auto [fn, should_cache_symbol, should_cache_function] = prepared;

      pending.push_back(ResolvedFunction{
          .name = *name,
          .fn = fn,
          .should_cache_symbol = should_cache_symbol,
          .should_cache_function = should_cache_function,
      });
    }

    for (const auto& item : pending) {
      if (item.should_cache_symbol) {
        lib->symbols_.emplace(item.name, item.fn->ptr);
      }
      if (item.should_cache_function) {
        lib->functions_.emplace(item.name, item.fn);
      }
    }

    for (const auto& item : pending) {
      MaybeLocal<Function> maybe_ret =
          lib->CreateFunction(env, item.name, item.fn);
      Local<Function> ret;
      if (!maybe_ret.ToLocal(&ret)) {
        return;
      }

      Local<Value> name_string;
      if (!ToV8Value(env->context(), item.name, env->isolate())
               .ToLocal(&name_string)) {
        return;
      }

      if (!functions->Set(context, name_string.As<String>(), ret)
               .FromMaybe(false)) {
        return;
      }
    }
  } else {
    for (const auto& entry : lib->functions_) {
      MaybeLocal<Function> maybe_fn =
          lib->CreateFunction(env, entry.first, entry.second);
      Local<Function> fn;
      if (!maybe_fn.ToLocal(&fn)) {
        return;
      }

      Local<Value> name_string;
      if (!ToV8Value(env->context(), entry.first, env->isolate())
               .ToLocal(&name_string)) {
        return;
      }

      if (!functions->Set(context, name_string.As<String>(), fn)
               .FromMaybe(false)) {
        return;
      }
    }
  }

  args.GetReturnValue().Set(functions);
}

void DynamicLibrary::GetSymbol(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  if (args.Length() < 1 || !args[0]->IsString()) {
    THROW_ERR_INVALID_ARG_TYPE(env, "Symbol name must be a string");
    return;
  }

  DynamicLibrary* lib = Unwrap<DynamicLibrary>(args.This());
  Utf8Value name(isolate, args[0]);
  if (ThrowIfContainsNullBytes(env, name, "Symbol name")) {
    return;
  }
  void* ptr;

  if (!lib->ResolveSymbol(env, *name).To(&ptr)) {
    return;
  }

  lib->symbols_.emplace(*name, ptr);

  args.GetReturnValue().Set(BigInt::NewFromUnsigned(
      isolate, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr))));
}

void DynamicLibrary::GetSymbols(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  DynamicLibrary* lib = Unwrap<DynamicLibrary>(args.This());

  if (lib->handle_ == nullptr) {
    THROW_ERR_FFI_LIBRARY_CLOSED(env);
    return;
  }

  Local<Object> symbols = Object::New(isolate);
  if (!symbols->SetPrototypeV2(context, Null(isolate)).FromMaybe(false)) {
    return;
  }
  for (const auto& entry : lib->symbols_) {
    Local<Value> symbol_key;
    if (!ToV8Value(env->context(), entry.first, env->isolate())
             .ToLocal(&symbol_key)) {
      return;
    }

    if (!symbols
             ->Set(context,
                   symbol_key.As<String>(),
                   BigInt::NewFromUnsigned(
                       isolate,
                       static_cast<uint64_t>(
                           reinterpret_cast<uintptr_t>(entry.second))))
             .FromMaybe(false)) {
      return;
    }
  }

  args.GetReturnValue().Set(symbols);
}

void DynamicLibrary::RegisterCallback(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  ffi_type* return_type = &ffi_type_void;
  std::vector<ffi_type*> callback_args;
  Local<Function> fn;

  if (args.Length() < 1) {
    THROW_ERR_INVALID_ARG_TYPE(
        env, "First argument must be a function or a signature object");
    return;
  }

  if (args[0]->IsFunction()) {
    fn = args[0].As<Function>();
  } else {
    if (!args[0]->IsObject() || args[0]->IsArray()) {
      THROW_ERR_INVALID_ARG_TYPE(
          env, "First argument must be a function or a signature object");
      return;
    }

    if (args.Length() < 2 || !args[1]->IsFunction()) {
      THROW_ERR_INVALID_ARG_TYPE(env, "Second argument must be a function");
      return;
    }

    FunctionSignature parsed;
    if (!ParseFunctionSignature(env, "<callback>", args[0].As<Object>())
             .To(&parsed)) {
      return;
    }

    return_type = parsed.return_type;
    callback_args = std::move(parsed.args);

    fn = args[1].As<Function>();
  }

  DynamicLibrary* lib = Unwrap<DynamicLibrary>(args.This());
  if (lib->handle_ == nullptr) {
    THROW_ERR_FFI_LIBRARY_CLOSED(env);
    return;
  }

  auto callback = new FFICallback{.owner = lib,
                                  .env = env,
                                  .thread_id = std::this_thread::get_id(),
                                  .fn = Global<Function>(isolate, fn),
                                  .closure = nullptr,
                                  .ptr = nullptr,
                                  .cif = {},
                                  .args = std::move(callback_args),
                                  .return_type = return_type};

  callback->closure = static_cast<ffi_closure*>(
      ffi_closure_alloc(sizeof(ffi_closure), &callback->ptr));

  if (callback->closure == nullptr) {
    THROW_ERR_FFI_CALL_FAILED(env, "ffi_closure_alloc failed");
    delete callback;
    return;
  }

  ffi_status status;
  status = ffi_prep_cif(&callback->cif,
                        FFI_DEFAULT_ABI,
                        callback->args.size(),
                        callback->return_type,
                        callback->args.data());
  if (status != FFI_OK) {
    const char* msg = "ffi_prep_cif failed";
    switch (status) {
      case FFI_BAD_TYPEDEF:
        msg = "ffi_prep_cif failed: bad typedef";
        break;
      case FFI_BAD_ABI:
        msg = "ffi_prep_cif failed: bad ABI";
        break;
      default:
        msg = "ffi_prep_cif failed: unknown error";
        break;
    }

    THROW_ERR_FFI_CALL_FAILED(env, msg);
    delete callback;
    return;
  }

  status = ffi_prep_closure_loc(callback->closure,
                                &callback->cif,
                                DynamicLibrary::InvokeCallback,
                                callback,
                                callback->ptr);
  if (status != FFI_OK) {
    const char* msg = "ffi_prep_closure_loc failed";
    switch (status) {
      case FFI_BAD_TYPEDEF:
        msg = "ffi_prep_closure_loc failed: bad typedef";
        break;
      case FFI_BAD_ABI:
        msg = "ffi_prep_closure_loc failed: bad ABI";
        break;
      default:
        msg = "ffi_prep_closure_loc failed: unknown error";
        break;
    }

    THROW_ERR_FFI_CALL_FAILED(env, msg);
    delete callback;
    return;
  }

  lib->callbacks_.emplace(callback->ptr, callback);
  args.GetReturnValue().Set(BigInt::NewFromUnsigned(
      isolate,
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(callback->ptr))));
}

void DynamicLibrary::UnregisterCallback(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  DynamicLibrary* lib = Unwrap<DynamicLibrary>(args.This());

  if (lib->handle_ == nullptr) {
    THROW_ERR_FFI_LIBRARY_CLOSED(env);
    return;
  }

  if (args.Length() < 1 || !args[0]->IsBigInt()) {
    THROW_ERR_INVALID_ARG_TYPE(env, "The first argument must be a bigint");
    return;
  }

  uintptr_t raw_ptr;
  if (!GetValidatedPointerAddress(env, args[0], "first argument")
           .To(&raw_ptr)) {
    return;
  }

  void* ptr = reinterpret_cast<void*>(raw_ptr);
  auto existing = lib->callbacks_.find(ptr);

  if (existing == lib->callbacks_.end()) {
    THROW_ERR_INVALID_ARG_VALUE(env, "Callback not found");
    return;
  }

  // This releases the callback trampoline immediately. If foreign code still
  // retains and invokes the pointer afterwards, the behavior is undefined, not
  // allowed, and dangerous: it can crash the process, produce incorrect
  // output, or corrupt memory. Unregistering a callback while it is currently
  // executing is also unsupported and dangerous.
  lib->callbacks_.erase(existing);
}

void DynamicLibrary::RefCallback(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  DynamicLibrary* lib = Unwrap<DynamicLibrary>(args.This());

  if (lib->handle_ == nullptr) {
    THROW_ERR_FFI_LIBRARY_CLOSED(env);
    return;
  }

  if (args.Length() < 1 || !args[0]->IsBigInt()) {
    THROW_ERR_INVALID_ARG_TYPE(env, "The first argument must be a bigint");
    return;
  }

  uintptr_t raw_ptr;
  if (!GetValidatedPointerAddress(env, args[0], "first argument")
           .To(&raw_ptr)) {
    return;
  }

  void* ptr = reinterpret_cast<void*>(raw_ptr);
  auto existing = lib->callbacks_.find(ptr);

  if (existing == lib->callbacks_.end()) {
    THROW_ERR_INVALID_ARG_VALUE(env, "Callback not found");
    return;
  }

  existing->second->fn.ClearWeak();
}

void DynamicLibrary::UnrefCallback(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  DynamicLibrary* lib = Unwrap<DynamicLibrary>(args.This());

  if (lib->handle_ == nullptr) {
    THROW_ERR_FFI_LIBRARY_CLOSED(env);
    return;
  }

  if (args.Length() < 1 || !args[0]->IsBigInt()) {
    THROW_ERR_INVALID_ARG_TYPE(env, "The first argument must be a bigint");
    return;
  }

  uintptr_t raw_ptr;
  if (!GetValidatedPointerAddress(env, args[0], "first argument")
           .To(&raw_ptr)) {
    return;
  }

  void* ptr = reinterpret_cast<void*>(raw_ptr);
  auto existing = lib->callbacks_.find(ptr);

  if (existing == lib->callbacks_.end()) {
    THROW_ERR_INVALID_ARG_VALUE(env, "Callback not found");
    return;
  }

  existing->second->fn.SetWeak();
}

Local<FunctionTemplate> DynamicLibrary::GetConstructorTemplate(
    Environment* env) {
  Local<FunctionTemplate> tmpl =
      env->ffi_dynamic_library_constructor_template();

  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    enum PropertyAttribute attributes =
        static_cast<PropertyAttribute>(ReadOnly | DontDelete);

    tmpl = NewFunctionTemplate(isolate, DynamicLibrary::New);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        DynamicLibrary::kInternalFieldCount);

    tmpl->InstanceTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "path"),
        FunctionTemplate::New(env->isolate(), DynamicLibrary::GetPath),
        Local<FunctionTemplate>(),
        attributes);

    tmpl->InstanceTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "symbols"),
        FunctionTemplate::New(env->isolate(), DynamicLibrary::GetSymbols),
        Local<FunctionTemplate>(),
        attributes);

    // `functions` lives on the prototype template rather than the instance
    // template so `lib/ffi.js` can replace it via `Object.defineProperty`
    // on the prototype. The attribute set omits `DontDelete` for the same
    // reason.
    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "functions"),
        FunctionTemplate::New(env->isolate(), DynamicLibrary::GetFunctions),
        Local<FunctionTemplate>(),
        static_cast<PropertyAttribute>(ReadOnly));

    SetProtoMethod(isolate, tmpl, "close", DynamicLibrary::Close);
    SetProtoMethod(isolate, tmpl, "getFunction", DynamicLibrary::GetFunction);
    SetProtoMethod(isolate, tmpl, "getFunctions", DynamicLibrary::GetFunctions);
    SetProtoMethod(isolate, tmpl, "getSymbol", DynamicLibrary::GetSymbol);
    SetProtoMethod(isolate, tmpl, "getSymbols", DynamicLibrary::GetSymbols);
    SetProtoMethod(
        isolate, tmpl, "registerCallback", DynamicLibrary::RegisterCallback);
    SetProtoMethod(isolate,
                   tmpl,
                   "unregisterCallback",
                   DynamicLibrary::UnregisterCallback);
    SetProtoMethod(isolate, tmpl, "refCallback", DynamicLibrary::RefCallback);
    SetProtoMethod(
        isolate, tmpl, "unrefCallback", DynamicLibrary::UnrefCallback);

    env->set_ffi_dynamic_library_constructor_template(tmpl);
  }

  return tmpl;
}

// Module initialization.
static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);

  // Create the DynamicLibrary template
  Local<FunctionTemplate> dl_tmpl = DynamicLibrary::GetConstructorTemplate(env);
  SetConstructorFunction(context, target, "DynamicLibrary", dl_tmpl);
  SetMethod(context, target, "toString", ToString);
  SetMethod(context, target, "toBuffer", ToBuffer);
  SetMethod(context, target, "toArrayBuffer", ToArrayBuffer);
  SetMethod(context, target, "exportBytes", ExportBytes);
  SetMethod(context, target, "getRawPointer", GetRawPointer);

  SetMethod(context, target, "getInt8", GetInt8);
  SetMethod(context, target, "getUint8", GetUint8);
  SetMethod(context, target, "getInt16", GetInt16);
  SetMethod(context, target, "getUint16", GetUint16);
  SetMethod(context, target, "getInt32", GetInt32);
  SetMethod(context, target, "getUint32", GetUint32);
  SetMethod(context, target, "getInt64", GetInt64);
  SetMethod(context, target, "getUint64", GetUint64);
  SetMethod(context, target, "getFloat32", GetFloat32);
  SetMethod(context, target, "getFloat64", GetFloat64);

  SetMethod(context, target, "setInt8", SetInt8);
  SetMethod(context, target, "setUint8", SetUint8);
  SetMethod(context, target, "setInt16", SetInt16);
  SetMethod(context, target, "setUint16", SetUint16);
  SetMethod(context, target, "setInt32", SetInt32);
  SetMethod(context, target, "setUint32", SetUint32);
  SetMethod(context, target, "setInt64", SetInt64);
  SetMethod(context, target, "setUint64", SetUint64);
  SetMethod(context, target, "setFloat32", SetFloat32);
  SetMethod(context, target, "setFloat64", SetFloat64);

  // ToFFIType maps `char` to sint8 or uint8 based on `CHAR_MIN < 0` at C++
  // build time. Exposing the same decision to JS lets the shared-buffer
  // wrapper's range check match `ToFFIArgument` on every platform.
  Isolate* isolate = env->isolate();
  target
      ->Set(context,
            FIXED_ONE_BYTE_STRING(isolate, "charIsSigned"),
            Boolean::New(isolate, CHAR_MIN < 0))
      .Check();

  // The shared-buffer fast path uses `uintptrMax` to reject pointer BigInts
  // that would otherwise be silently truncated by `ReadFFIArgFromBuffer`'s
  // `memcpy(..., type->size, ...)` on 32-bit platforms. The slow path
  // rejects the same values through `ToFFIArgument`.
  target
      ->Set(context,
            FIXED_ONE_BYTE_STRING(isolate, "uintptrMax"),
            v8::BigInt::NewFromUnsigned(
                isolate,
                static_cast<uint64_t>(std::numeric_limits<uintptr_t>::max())))
      .Check();

  // Per-isolate Symbols used by `lib/internal/ffi-shared-buffer.js` to key
  // shared-buffer internal state on raw FFI functions.
  target
      ->Set(context,
            FIXED_ONE_BYTE_STRING(isolate, "kSbSharedBuffer"),
            env->ffi_sb_shared_buffer_symbol())
      .Check();
  target
      ->Set(context,
            FIXED_ONE_BYTE_STRING(isolate, "kSbInvokeSlow"),
            env->ffi_sb_invoke_slow_symbol())
      .Check();
  target
      ->Set(context,
            FIXED_ONE_BYTE_STRING(isolate, "kSbParams"),
            env->ffi_sb_params_symbol())
      .Check();
  target
      ->Set(context,
            FIXED_ONE_BYTE_STRING(isolate, "kSbResult"),
            env->ffi_sb_result_symbol())
      .Check();
}

}  // namespace ffi
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(ffi, node::ffi::Initialize)

#endif  // HAVE_FFI
