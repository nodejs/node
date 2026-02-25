#if HAVE_FFI

#include "../base_object-inl.h"
#include "../env-inl.h"
#include "../node_errors.h"
#include "../permission/permission.h"
#include "node_ffi.h"

namespace node {

using v8::Array;
using v8::BigInt;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::Local;
using v8::LocalVector;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Object;
using v8::String;
using v8::Undefined;
using v8::Value;

namespace ffi {

DynamicLibrary::DynamicLibrary(Environment* env, Local<Object> object)
    : BaseObject(env, object), handle_(nullptr) {
  MakeWeak();
}

DynamicLibrary::~DynamicLibrary() {
  if (handle_ != nullptr) {
    dlclose(handle_);
  }
}

void DynamicLibrary::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("filename", filename_.size());
}

Local<FunctionTemplate> DynamicLibrary::GetConstructorTemplate(
    Environment* env) {
  Local<FunctionTemplate> tmpl =
      env->ffi_dynamic_library_constructor_template();

  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, DynamicLibrary::New);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        DynamicLibrary::kInternalFieldCount);
    SetProtoMethod(isolate, tmpl, "close", DynamicLibrary::Close);
    SetProtoMethod(isolate, tmpl, "getSymbol", DynamicLibrary::GetSymbol);

    env->set_ffi_dynamic_library_constructor_template(tmpl);
  }

  return tmpl;
}

BaseObjectPtr<DynamicLibrary> DynamicLibrary::Create(Environment* env,
                                                     Local<Value> path,
                                                     Local<Value> symbols_val) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env)
           ->InstanceTemplate()
           ->NewInstance(env->context())
           .ToLocal(&obj)) {
    return nullptr;
  }

  if (!path->IsString()) {
    THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                               "The \"path\" argument must be a string.");
    return nullptr;
  }

  BaseObjectPtr<DynamicLibrary> lib = MakeBaseObject<DynamicLibrary>(env, obj);

  node::Utf8Value filename(env->isolate(), path);
  lib->filename_ = std::string(*filename);

  int flags = RTLD_LAZY;
  lib->handle_ = dlopen(*filename, flags);
  if (lib->handle_ == nullptr) {
    env->ThrowError(dlerror());
    return nullptr;
  }

  // Process symbols
  if (symbols_val->IsObject()) {
    Local<Object> symbols = symbols_val.As<Object>();
    Local<Array> keys;
    if (!symbols->GetOwnPropertyNames(env->context()).ToLocal(&keys)) {
      return nullptr;
    }

    Local<Object> symbols_obj = Object::New(env->isolate());

    for (uint32_t i = 0; i < keys->Length(); i++) {
      Local<Value> key_val;
      if (!keys->Get(env->context(), i).ToLocal(&key_val)) {
        return nullptr;
      }
      Local<String> key = key_val.As<String>();

      Local<Value> sig_val;
      if (!symbols->Get(env->context(), key).ToLocal(&sig_val)) {
        return nullptr;
      }

      if (!sig_val->IsObject()) {
        THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                                  "The \"signature\" argument must be an object.");
        return nullptr;
      }

      Local<Object> sig = sig_val.As<Object>();

      // Extract parameters array
      Local<Value> params_val;
      if (!sig->Get(env->context(),
                    String::NewFromUtf8Literal(env->isolate(), "parameters"))
               .ToLocal(&params_val)) {
        return nullptr;
      }

      if (!params_val->IsArray()) {
        THROW_ERR_INVALID_ARG_TYPE(env->isolate(),
                                  "The \"parameters\" argument must be an array.");
        return nullptr;
      }

      // Extract result type
      Local<Value> result_val;
      if (!sig->Get(env->context(),
                    String::NewFromUtf8Literal(env->isolate(), "result"))
               .ToLocal(&result_val)) {
        return nullptr;
      }

      // Get symbol pointer
      node::Utf8Value symbol_name(env->isolate(), key);
      void* symbol_ptr = lib->GetSymbolPointer(*symbol_name);
      if (!symbol_ptr) {
        env->ThrowError(dlerror());
        return nullptr;
      }

      // Create UnsafeFnPointer for this symbol
      Local<FunctionTemplate> fn_tmpl =
          UnsafeFnPointer::GetConstructorTemplate(env);
      Local<Object> fn_obj;
      if (!fn_tmpl->InstanceTemplate()
               ->NewInstance(env->context())
               .ToLocal(&fn_obj)) {
        return nullptr;
      }

      // Create definition object with parameters and result
      Local<Object> definition = Object::New(env->isolate());
      if (definition
              ->Set(env->context(),
                    String::NewFromUtf8Literal(env->isolate(), "parameters"),
                    params_val)
              .IsNothing()) {
        return nullptr;
      }
      if (definition
              ->Set(env->context(),
                    String::NewFromUtf8Literal(env->isolate(), "result"),
                    result_val)
              .IsNothing()) {
        return nullptr;
      }

      Local<Value> fn_argv[] = {
          BigInt::NewFromUnsigned(env->isolate(),
                                  reinterpret_cast<uint64_t>(symbol_ptr)),
          definition};

      Local<Value> fn_result;
      if (!fn_tmpl->GetFunction(env->context())
               .ToLocalChecked()
               ->CallAsConstructor(env->context(), 2, fn_argv)
               .ToLocal(&fn_result)) {
        return nullptr;
      }

      // Create a real JavaScript function that wraps the UnsafeFnPointer
      Local<Array> params_array = params_val.As<Array>();
      int param_count = static_cast<int>(params_array->Length());
      Local<Function> wrapper_fn;
      if (!Function::New(
               env->context(), FFIFunctionWrapper, fn_result, param_count)
               .ToLocal(&wrapper_fn)) {
        return nullptr;
      }

      // Set the function name
      Local<String> name_string =
          String::NewFromUtf8(
              env->isolate(), *symbol_name, NewStringType::kInternalized)
              .ToLocalChecked();
      wrapper_fn->SetName(name_string);

      // Add the wrapper function to symbols object
      if (symbols_obj->Set(env->context(), key, wrapper_fn).IsNothing()) {
        return nullptr;
      }
    }

    // Attach symbols to library object
    if (obj->Set(env->context(),
                 String::NewFromUtf8Literal(env->isolate(), "symbols"),
                 symbols_obj)
            .IsNothing()) {
      return nullptr;
    }
  } else {
    // No symbols provided, create empty symbols object
    Local<Object> symbols_obj = Object::New(env->isolate());
    if (obj->Set(env->context(),
                 String::NewFromUtf8Literal(env->isolate(), "symbols"),
                 symbols_obj)
            .IsNothing()) {
      return nullptr;
    }
  }

  return lib;
}

void DynamicLibrary::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFFI, "");
  if (args.Length() < 1 || !args[0]->IsString()) {
    env->ThrowTypeError("Library path must be a string");
    return;
  }

  DynamicLibrary* lib = new DynamicLibrary(env, args.This());

  node::Utf8Value filename(env->isolate(), args[0]);
  lib->filename_ = std::string(*filename);

  int flags = RTLD_LAZY;
  if (args.Length() >= 2 && args[1]->IsObject()) {
    Local<Object> options = args[1].As<Object>();
    MaybeLocal<Value> global = options->Get(
        env->context(), String::NewFromUtf8Literal(env->isolate(), "global"));
    if (!global.IsEmpty() && global.ToLocalChecked()->IsTrue()) {
      flags |= RTLD_GLOBAL;
    }
  }

  lib->handle_ = dlopen(*filename, flags);
  if (lib->handle_ == nullptr) {
    env->ThrowError(dlerror());
    return;
  }
}

void DynamicLibrary::Close(const FunctionCallbackInfo<Value>& args) {
  DynamicLibrary* lib = Unwrap<DynamicLibrary>(args.This());

  if (lib->handle_ != nullptr) {
    dlclose(lib->handle_);
    lib->handle_ = nullptr;
  }
}

void DynamicLibrary::GetSymbol(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  DynamicLibrary* lib = Unwrap<DynamicLibrary>(args.This());

  if (lib->handle_ == nullptr) {
    env->ThrowError("Library not opened");
    return;
  }

  if (args.Length() < 1 || !args[0]->IsString()) {
    env->ThrowTypeError("Symbol name must be a string");
    return;
  }

  node::Utf8Value symbol_name(env->isolate(), args[0]);
  void* symbol_ptr = dlsym(lib->handle_, *symbol_name);

  if (symbol_ptr == nullptr) {
    env->ThrowError(dlerror());
    return;
  }

  // Return an opaque PointerObject
  args.GetReturnValue().Set(CreatePointerObject(env, symbol_ptr));
}

// Static wrapper function for FFI symbols
void FFIFunctionWrapper(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Value> data = args.Data();

  if (!data->IsObject()) {
    env->ThrowError("Invalid FFI function data");
    return;
  }

  Local<Object> fn_obj = data.As<Object>();
  UnsafeFnPointer* fn_ptr = BaseObject::Unwrap<UnsafeFnPointer>(fn_obj);

  if (!fn_ptr) {
    env->ThrowError("Invalid FFI function pointer");
    return;
  }

  // Delegate to UnsafeFnPointer::Call with the original arguments
  // We need to create a new args object that uses fn_obj as This()
  const int argc = args.Length();
  LocalVector<Value> argv(env->isolate(), argc);
  for (int i = 0; i < argc; i++) {
    argv[i] = args[i];
  }

  // Call the UnsafeFnPointer's Call method
  Local<Value> result;
  if (fn_ptr->object()
          ->CallAsFunction(
              env->context(), fn_obj, argc, argc > 0 ? argv.data() : nullptr)
          .ToLocal(&result)) {
    args.GetReturnValue().Set(result);
  }
}

void Dlopen(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFFI, "");

  if (args.Length() < 1) {
    env->ThrowTypeError("Library path must be provided");
    return;
  }

  Local<Value> path = args[0];
  Local<Value> symbols =
      args.Length() > 1 ? args[1] : Undefined(env->isolate());

  BaseObjectPtr<DynamicLibrary> lib =
      DynamicLibrary::Create(env, path, symbols);
  if (!lib) return;

  args.GetReturnValue().Set(lib->object());
}

}  // namespace ffi
}  // namespace node

#endif  // HAVE_FFI
