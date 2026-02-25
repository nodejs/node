#if HAVE_FFI

#include "../base_object-inl.h"
#include "../env-inl.h"
#include "../node_errors.h"
#include "../permission/permission.h"
#include "node_ffi.h"

#include <cstring>

namespace node {

using v8::Array;
using v8::BigInt;
using v8::Context;
using v8::DontDelete;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::LocalVector;
using v8::MaybeLocal;
using v8::Number;
using v8::Object;
using v8::PropertyAttribute;
using v8::ReadOnly;
using v8::String;
using v8::TryCatch;
using v8::Undefined;
using v8::Value;

namespace ffi {

// UnsafeCallback implementation
Local<FunctionTemplate> UnsafeCallback::GetConstructorTemplate(
    Environment* env) {
  Local<FunctionTemplate> tmpl =
      env->ffi_unsafe_callback_constructor_template();

  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, UnsafeCallback::New);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        UnsafeCallback::kInternalFieldCount);
    SetProtoMethod(isolate, tmpl, "close", UnsafeCallback::Close);
    SetProtoMethod(isolate, tmpl, "ref", UnsafeCallback::Ref);
    SetProtoMethod(isolate, tmpl, "unref", UnsafeCallback::Unref);
    SetMethod(isolate, tmpl, "threadSafe", UnsafeCallback::ThreadSafe);
    env->set_ffi_unsafe_callback_constructor_template(tmpl);
  }

  return tmpl;
}

UnsafeCallback::UnsafeCallback(Environment* env,
                               Local<Object> object,
                               Local<Object> definition,
                               Local<Function> callback,
                               const std::string& return_type,
                               const std::vector<std::string>& param_types)
    : BaseObject(env, object),
      definition_(env->isolate(), definition),
      callback_(env->isolate(), callback),
      pointer_(),
      closure_(nullptr),
      code_(nullptr),
      return_type_(FFIType::ToFFI(return_type)),
      cif_prepared_(false),
      ref_count_(0),
      closed_(false) {
  // Convert param types
  for (const auto& param_type : param_types) {
    auto ffi_type = FFIType::ToFFI(param_type);
    if (ffi_type != nullptr) {
      param_types_.push_back(ffi_type);
    }
  }

  // Allocate closure
  closure_ =
      static_cast<ffi_closure*>(ffi_closure_alloc(sizeof(ffi_closure), &code_));

  if (closure_ != nullptr) {
    // Prepare CIF
    ffi_status cif_status = ffi_prep_cif(&cif_,
                                         FFI_DEFAULT_ABI,
                                         param_types_.size(),
                                         return_type_,
                                         param_types_.data());
    if (cif_status == FFI_OK) {
      // Prepare closure
      ffi_status closure_status =
          ffi_prep_closure_loc(closure_, &cif_, CallbackWrapper, this, code_);
      if (closure_status == FFI_OK) {
        cif_prepared_ = true;
      }
    }
  }

  Local<Context> context = env->context();
  Local<Object> pointer_object = CreatePointerObject(env, code_);
  pointer_.Reset(env->isolate(), pointer_object);

  const PropertyAttribute attributes =
      static_cast<PropertyAttribute>(ReadOnly | DontDelete);
  object
      ->DefineOwnProperty(
          context,
          String::NewFromUtf8Literal(env->isolate(), "definition"),
          definition,
          attributes)
      .Check();
  object
      ->DefineOwnProperty(
          context,
          String::NewFromUtf8Literal(env->isolate(), "callback"),
          callback,
          attributes)
      .Check();
  object
      ->DefineOwnProperty(context,
                          String::NewFromUtf8Literal(env->isolate(), "pointer"),
                          pointer_object,
                          attributes)
      .Check();

  object->SetInternalField(kCallbackFunction, callback);
  MakeWeak();
}

UnsafeCallback::~UnsafeCallback() {
  if (closure_ != nullptr) {
    ffi_closure_free(closure_);
  }
}

void UnsafeCallback::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("definition", definition_);
  tracker->TrackField("callback", callback_);
  tracker->TrackField("pointer", pointer_);
}

void UnsafeCallback::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFFI, "");
  if (args.Length() < 2) {
    env->ThrowTypeError("Expected definition and callback arguments");
    return;
  }

  if (!args[0]->IsObject()) {
    env->ThrowTypeError("First argument must be a definition object");
    return;
  }

  if (!args[1]->IsFunction()) {
    env->ThrowTypeError("Second argument must be a function");
    return;
  }

  Local<Object> definition = args[0].As<Object>();
  Local<Context> context = env->context();

  MaybeLocal<Value> parameters_val = definition->Get(
      context, String::NewFromUtf8Literal(env->isolate(), "parameters"));
  if (parameters_val.IsEmpty() || !parameters_val.ToLocalChecked()->IsArray()) {
    env->ThrowTypeError("definition.parameters must be an array");
    return;
  }

  std::vector<std::string> param_types;
  Local<Array> param_types_array = parameters_val.ToLocalChecked().As<Array>();
  param_types.reserve(param_types_array->Length());

  for (uint32_t i = 0; i < param_types_array->Length(); i++) {
    MaybeLocal<Value> val = param_types_array->Get(context, i);
    if (!val.IsEmpty() && val.ToLocalChecked()->IsString()) {
      node::Utf8Value type_str(env->isolate(), val.ToLocalChecked());
      param_types.push_back(std::string(*type_str));
    } else {
      env->ThrowTypeError("definition.parameters must contain only strings");
      return;
    }
  }

  MaybeLocal<Value> result_val = definition->Get(
      context, String::NewFromUtf8Literal(env->isolate(), "result"));
  if (result_val.IsEmpty() || !result_val.ToLocalChecked()->IsString()) {
    env->ThrowTypeError("definition.result must be a string");
    return;
  }

  node::Utf8Value return_type_str(env->isolate(), result_val.ToLocalChecked());
  std::string return_type = std::string(*return_type_str);

  Local<Function> callback = args[1].As<Function>();

  UnsafeCallback* cb = new UnsafeCallback(
      env, args.This(), definition, callback, return_type, param_types);

  if (!cb->cif_prepared_ || cb->code_ == nullptr) {
    env->ThrowError("Failed to initialize FFI callback closure");
    return;
  }
}

void UnsafeCallback::Close(const FunctionCallbackInfo<Value>& args) {
  UnsafeCallback* cb = Unwrap<UnsafeCallback>(args.This());
  if (cb->closed_) {
    return;
  }

  cb->callback_.Reset();
  cb->definition_.Reset();
  cb->pointer_.Reset();
  if (cb->closure_ != nullptr) {
    ffi_closure_free(cb->closure_);
    cb->closure_ = nullptr;
  }
  cb->code_ = nullptr;
  cb->cif_prepared_ = false;
  cb->ref_count_ = 0;
  cb->closed_ = true;

  if (!cb->IsWeakOrDetached()) {
    cb->MakeWeak();
  }
}

void UnsafeCallback::Ref(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  UnsafeCallback* cb = Unwrap<UnsafeCallback>(args.This());

  if (cb->closed_) {
    env->ThrowError("UnsafeCallback is closed");
    return;
  }

  args.GetReturnValue().Set(cb->RefCallback());
}

void UnsafeCallback::Unref(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  UnsafeCallback* cb = Unwrap<UnsafeCallback>(args.This());

  if (cb->closed_) {
    env->ThrowError("UnsafeCallback is closed");
    return;
  }

  args.GetReturnValue().Set(cb->UnrefCallback());
}

void UnsafeCallback::ThreadSafe(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() < 2) {
    env->ThrowTypeError("Expected definition and callback arguments");
    return;
  }

  if (!args.This()->IsFunction()) {
    env->ThrowTypeError(
        "UnsafeCallback.threadSafe must be called on the constructor");
    return;
  }

  Local<Function> ctor = args.This().As<Function>();
  Local<Value> argv[2] = {args[0], args[1]};
  Local<Object> instance;
  if (!ctor->NewInstance(env->context(), 2, argv).ToLocal(&instance)) {
    return;
  }

  UnsafeCallback* cb = Unwrap<UnsafeCallback>(instance);
  if (cb != nullptr) {
    cb->RefCallback();
  }

  args.GetReturnValue().Set(instance);
}

uint32_t UnsafeCallback::RefCallback() {
  if (ref_count_ == 0 && IsWeakOrDetached()) {
    ClearWeak();
  }
  ref_count_ += 1;
  return ref_count_;
}

uint32_t UnsafeCallback::UnrefCallback() {
  if (ref_count_ > 0) {
    ref_count_ -= 1;
  }
  if (ref_count_ == 0 && !IsWeakOrDetached()) {
    MakeWeak();
  }
  return ref_count_;
}

void UnsafeCallback::CallbackWrapper(ffi_cif* cif,
                                     void* ret,
                                     void** args,
                                     void* user_data) {
  UnsafeCallback* cb = static_cast<UnsafeCallback*>(user_data);
  // Initialize return value buffer conservatively using libffi type size.
  if (ret != nullptr && cif->rtype != nullptr && cif->rtype->size > 0) {
    memset(ret, 0, cif->rtype->size);
  }

  if (cb == nullptr || cb->env() == nullptr) {
    return;
  }

  Environment* env = cb->env();
  Isolate* isolate = env->isolate();

  if (isolate == nullptr) {
    return;
  }

  HandleScope handle_scope(isolate);
  Local<Context> context = env->context();
  Context::Scope context_scope(context);

  // Check if callback is still valid
  if (cb->callback_.IsEmpty()) {
    return;
  }

  Local<Function> callback = Local<Function>::New(isolate, cb->callback_);

  // Build JavaScript arguments from FFI arguments
  size_t argc = cb->param_types_.size();
  LocalVector<Value> js_args(isolate, argc);

  for (size_t i = 0; i < argc; i++) {
    if (args[i] == nullptr) {
      js_args[i] = Undefined(isolate);
      continue;
    }

    if (cb->param_types_[i] == &ffi_type_sint8) {
      int8_t val = *static_cast<int8_t*>(args[i]);
      js_args[i] = Integer::New(isolate, static_cast<int32_t>(val));
    } else if (cb->param_types_[i] == &ffi_type_uint8) {
      uint8_t val = *static_cast<uint8_t*>(args[i]);
      js_args[i] =
          Integer::NewFromUnsigned(isolate, static_cast<uint32_t>(val));
    } else if (cb->param_types_[i] == &ffi_type_sint16) {
      int16_t val = *static_cast<int16_t*>(args[i]);
      js_args[i] = Integer::New(isolate, static_cast<int32_t>(val));
    } else if (cb->param_types_[i] == &ffi_type_uint16) {
      uint16_t val = *static_cast<uint16_t*>(args[i]);
      js_args[i] =
          Integer::NewFromUnsigned(isolate, static_cast<uint32_t>(val));
    } else if (cb->param_types_[i] == &ffi_type_sint32) {
      int32_t val = *static_cast<int32_t*>(args[i]);
      js_args[i] = Integer::New(isolate, val);
    } else if (cb->param_types_[i] == &ffi_type_uint32) {
      uint32_t val = *static_cast<uint32_t*>(args[i]);
      js_args[i] = Integer::NewFromUnsigned(isolate, val);
    } else if (cb->param_types_[i] == &ffi_type_sint64) {
      int64_t val = *static_cast<int64_t*>(args[i]);
      js_args[i] = BigInt::New(isolate, val);
    } else if (cb->param_types_[i] == &ffi_type_uint64) {
      uint64_t val = *static_cast<uint64_t*>(args[i]);
      js_args[i] = BigInt::NewFromUnsigned(isolate, val);
    } else if (cb->param_types_[i] == &ffi_type_pointer) {
      void* ptr = *static_cast<void**>(args[i]);
      uint64_t ptr_val = reinterpret_cast<uint64_t>(ptr);
      js_args[i] = BigInt::NewFromUnsigned(isolate, ptr_val);
    } else if (cb->param_types_[i] == &ffi_type_float) {
      float val = *static_cast<float*>(args[i]);
      js_args[i] = Number::New(isolate, static_cast<double>(val));
    } else if (cb->param_types_[i] == &ffi_type_double) {
      double val = *static_cast<double*>(args[i]);
      js_args[i] = Number::New(isolate, val);
    } else {
      // Unsupported type, pass undefined
      js_args[i] = Undefined(isolate);
    }
  }

  // Call the JavaScript callback
  TryCatch try_catch(isolate);
  MaybeLocal<Value> result = callback->Call(
      context, Undefined(isolate), argc, argc > 0 ? js_args.data() : nullptr);

  // Handle exceptions by logging (can't propagate across FFI boundary)
  if (try_catch.HasCaught()) {
    try_catch.ReThrow();
    return;
  }

  // Convert and store return value
  if (ret == nullptr || cb->return_type_ == &ffi_type_void) {
    return;  // No return value to store
  }

  if (result.IsEmpty()) {
    return;  // Callback returned nothing
  }

  Local<Value> ret_val = result.ToLocalChecked();

  if (cb->return_type_ == &ffi_type_sint32) {
    MaybeLocal<v8::Int32> i32_val = ret_val->ToInt32(context);
    if (!i32_val.IsEmpty()) {
      *static_cast<int32_t*>(ret) = i32_val.ToLocalChecked()->Value();
    }
  } else if (cb->return_type_ == &ffi_type_uint32) {
    MaybeLocal<v8::Uint32> u32_val = ret_val->ToUint32(context);
    if (!u32_val.IsEmpty()) {
      *static_cast<uint32_t*>(ret) = u32_val.ToLocalChecked()->Value();
    }
  } else if (cb->return_type_ == &ffi_type_sint8) {
    MaybeLocal<v8::Int32> i32_val = ret_val->ToInt32(context);
    if (!i32_val.IsEmpty()) {
      *static_cast<int8_t*>(ret) =
          static_cast<int8_t>(i32_val.ToLocalChecked()->Value());
    }
  } else if (cb->return_type_ == &ffi_type_uint8) {
    MaybeLocal<v8::Uint32> u32_val = ret_val->ToUint32(context);
    if (!u32_val.IsEmpty()) {
      *static_cast<uint8_t*>(ret) =
          static_cast<uint8_t>(u32_val.ToLocalChecked()->Value());
    }
  } else if (cb->return_type_ == &ffi_type_sint16) {
    MaybeLocal<v8::Int32> i32_val = ret_val->ToInt32(context);
    if (!i32_val.IsEmpty()) {
      *static_cast<int16_t*>(ret) =
          static_cast<int16_t>(i32_val.ToLocalChecked()->Value());
    }
  } else if (cb->return_type_ == &ffi_type_uint16) {
    MaybeLocal<v8::Uint32> u32_val = ret_val->ToUint32(context);
    if (!u32_val.IsEmpty()) {
      *static_cast<uint16_t*>(ret) =
          static_cast<uint16_t>(u32_val.ToLocalChecked()->Value());
    }
  } else if (cb->return_type_ == &ffi_type_sint64) {
    MaybeLocal<BigInt> bi = ret_val->ToBigInt(context);
    if (!bi.IsEmpty()) {
      *static_cast<int64_t*>(ret) = bi.ToLocalChecked()->Int64Value();
    }
  } else if (cb->return_type_ == &ffi_type_uint64) {
    MaybeLocal<BigInt> bi = ret_val->ToBigInt(context);
    if (!bi.IsEmpty()) {
      *static_cast<uint64_t*>(ret) = bi.ToLocalChecked()->Uint64Value();
    }
  } else if (cb->return_type_ == &ffi_type_pointer) {
    uint64_t ptr_val = 0;
    // Try to extract pointer address from PointerObject first
    if (!GetPointerAddress(env, ret_val, &ptr_val)) {
      // Fall back to BigInt conversion
      MaybeLocal<BigInt> bi = ret_val->ToBigInt(context);
      if (!bi.IsEmpty()) {
        ptr_val = bi.ToLocalChecked()->Uint64Value();
      }
    }
    *static_cast<void**>(ret) = reinterpret_cast<void*>(ptr_val);
  } else if (cb->return_type_ == &ffi_type_float) {
    MaybeLocal<Number> num = ret_val->ToNumber(context);
    if (!num.IsEmpty()) {
      float val = static_cast<float>(num.ToLocalChecked()->Value());
      *static_cast<float*>(ret) = val;
    }
  } else if (cb->return_type_ == &ffi_type_double) {
    MaybeLocal<Number> num = ret_val->ToNumber(context);
    if (!num.IsEmpty()) {
      *static_cast<double*>(ret) = num.ToLocalChecked()->Value();
    }
  }
}

}  // namespace ffi
}  // namespace node

#endif  // HAVE_FFI
