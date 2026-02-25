#if HAVE_FFI

#include "../base_object-inl.h"
#include "../env-inl.h"
#include "../node_errors.h"
#include "../permission/permission.h"
#include "node_ffi.h"

namespace node {

using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::BackingStore;
using v8::BigInt;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Uint8Array;
using v8::Value;

namespace ffi {

namespace {

bool ParseFFIType(
    Environment* env,
    Local<Value> type_val,
    ffi_type** out,
    std::vector<std::unique_ptr<ffi_type>>* owned_struct_types,
    std::vector<std::unique_ptr<ffi_type*[]>>* owned_struct_elements) {
  if (type_val->IsString()) {
    node::Utf8Value type_str(env->isolate(), type_val);
    ffi_type* ffi = FFIType::ToFFI(std::string(*type_str));
    if (ffi == nullptr) {
      env->ThrowTypeError("Unsupported FFI type");
      return false;
    }
    *out = ffi;
    return true;
  }

  if (!type_val->IsObject()) {
    env->ThrowTypeError("FFI type must be a string or struct descriptor");
    return false;
  }

  Local<Object> type_obj = type_val.As<Object>();
  Local<Value> struct_val;
  if (!type_obj
           ->Get(env->context(),
                 String::NewFromUtf8Literal(env->isolate(), "struct"))
           .ToLocal(&struct_val) ||
      !struct_val->IsArray()) {
    env->ThrowTypeError(
        "Struct descriptor must be an object with a 'struct' array");
    return false;
  }

  Local<Array> struct_fields = struct_val.As<Array>();
  const uint32_t field_count = struct_fields->Length();
  if (field_count == 0) {
    env->ThrowTypeError("Struct descriptor must contain at least one field");
    return false;
  }

  auto struct_type = std::make_unique<ffi_type>();
  struct_type->size = 0;
  struct_type->alignment = 0;
  struct_type->type = FFI_TYPE_STRUCT;

  auto elements = std::make_unique<ffi_type*[]>(field_count + 1);
  for (uint32_t i = 0; i < field_count; i++) {
    Local<Value> field_val;
    if (!struct_fields->Get(env->context(), i).ToLocal(&field_val)) {
      return false;
    }

    ffi_type* field_type = nullptr;
    if (!ParseFFIType(env,
                      field_val,
                      &field_type,
                      owned_struct_types,
                      owned_struct_elements)) {
      return false;
    }
    elements[i] = field_type;
  }
  elements[field_count] = nullptr;
  struct_type->elements = elements.get();

  ffi_get_struct_offsets(FFI_DEFAULT_ABI, struct_type.get(), nullptr);

  *out = struct_type.get();
  owned_struct_elements->push_back(std::move(elements));
  owned_struct_types->push_back(std::move(struct_type));
  return true;
}

}  // namespace

Local<FunctionTemplate> UnsafeFnPointer::GetConstructorTemplate(
    Environment* env) {
  Local<FunctionTemplate> tmpl =
      env->ffi_unsafe_fn_pointer_constructor_template();

  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, UnsafeFnPointer::New);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        UnsafeFnPointer::kInternalFieldCount);
    SetProtoMethod(isolate, tmpl, "call", UnsafeFnPointer::Call);
    // Make instances callable as functions.
    tmpl->InstanceTemplate()->SetCallAsFunctionHandler(UnsafeFnPointer::Call);

    env->set_ffi_unsafe_fn_pointer_constructor_template(tmpl);
  }
  return tmpl;
}

UnsafeFnPointer::UnsafeFnPointer(
    Environment* env,
    Local<Object> object,
    void* pointer,
    ffi_type* return_type,
    std::vector<ffi_type*> param_types,
    size_t return_struct_size,
    std::vector<std::unique_ptr<ffi_type>> owned_struct_types,
    std::vector<std::unique_ptr<ffi_type*[]>> owned_struct_elements,
    bool nonblocking)
    : BaseObject(env, object),
      pointer_(pointer),
      return_type_(return_type),
      param_types_(std::move(param_types)),
      return_struct_size_(return_struct_size),
      owned_struct_types_(std::move(owned_struct_types)),
      owned_struct_elements_(std::move(owned_struct_elements)),
      nonblocking_(nonblocking),
      cif_prepared_(false) {
  // Prepare the CIF
  if (ffi_prep_cif(&cif_,
                   FFI_DEFAULT_ABI,
                   param_types_.size(),
                   return_type_,
                   param_types_.empty() ? nullptr : param_types_.data()) ==
      FFI_OK) {
    cif_prepared_ = true;
  }

  MakeWeak();
}

UnsafeFnPointer::~UnsafeFnPointer() {}

void UnsafeFnPointer::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("param_types", param_types_.size() * 8);
  tracker->TrackFieldWithSize("owned_struct_types",
                              owned_struct_types_.size() * sizeof(ffi_type));
}

void UnsafeFnPointer::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  THROW_IF_INSUFFICIENT_PERMISSIONS(env, permission::PermissionScope::kFFI, "");
  if (args.Length() < 2) {
    env->ThrowTypeError("Expected pointer and definition arguments");
    return;
  }

  // Parse pointer argument (first arg - can be PointerObject or BigInt)
  uint64_t pointer = 0;
  if (args[0]->IsObject()) {
    // Try to get the pointer address using GetPointerAddress helper
    if (!GetPointerAddress(env, args[0], &pointer)) {
      return;
    }
  } else if (args[0]->IsBigInt()) {
    auto big = args[0]->ToBigInt(env->context());
    if (big.IsEmpty()) {
      env->ThrowTypeError("Invalid function pointer");
      return;
    }
    pointer = big.ToLocalChecked()->Uint64Value();
  } else {
    env->ThrowTypeError("First argument must be a PointerObject or BigInt");
    return;
  }

  // Parse definition object (second arg)
  if (!args[1]->IsObject()) {
    env->ThrowTypeError("Second argument must be a definition object");
    return;
  }

  Local<Object> definition = args[1].As<Object>();
  Local<Context> context = env->context();

  // Extract 'parameters' field (required)
  MaybeLocal<Value> parameters_val = definition->Get(
      context, String::NewFromUtf8Literal(env->isolate(), "parameters"));
  if (parameters_val.IsEmpty() || !parameters_val.ToLocalChecked()->IsArray()) {
    env->ThrowTypeError("definition.parameters must be an array");
    return;
  }
  Local<Array> param_types_array = parameters_val.ToLocalChecked().As<Array>();
  std::vector<ffi_type*> param_types;
  param_types.reserve(param_types_array->Length());

  std::vector<std::unique_ptr<ffi_type>> owned_struct_types;
  std::vector<std::unique_ptr<ffi_type*[]>> owned_struct_elements;

  for (uint32_t i = 0; i < param_types_array->Length(); i++) {
    MaybeLocal<Value> val = param_types_array->Get(context, i);
    if (val.IsEmpty()) {
      env->ThrowTypeError(
          "definition.parameters contains an invalid type entry");
      return;
    }

    ffi_type* param_type = nullptr;
    if (!ParseFFIType(env,
                      val.ToLocalChecked(),
                      &param_type,
                      &owned_struct_types,
                      &owned_struct_elements)) {
      return;
    }
    param_types.push_back(param_type);
  }

  // Extract 'result' field (required)
  MaybeLocal<Value> result_val = definition->Get(
      context, String::NewFromUtf8Literal(env->isolate(), "result"));
  if (result_val.IsEmpty()) {
    env->ThrowTypeError("definition.result is required");
    return;
  }

  ffi_type* return_type = nullptr;
  if (!ParseFFIType(env,
                    result_val.ToLocalChecked(),
                    &return_type,
                    &owned_struct_types,
                    &owned_struct_elements)) {
    return;
  }
  size_t return_struct_size =
      return_type->type == FFI_TYPE_STRUCT ? return_type->size : 0;

  // Extract 'nonblocking' field (optional, defaults to false)
  bool nonblocking = false;
  MaybeLocal<Value> nonblocking_val = definition->Get(
      context, String::NewFromUtf8Literal(env->isolate(), "nonblocking"));
  if (!nonblocking_val.IsEmpty() &&
      nonblocking_val.ToLocalChecked()->IsBoolean()) {
    nonblocking =
        nonblocking_val.ToLocalChecked()->BooleanValue(env->isolate());
  }

  // Note: 'optional' field is handled at dlopen level, not here
  // Note: 'callback' field would be for UnsafeCallback, not UnsafeFnPointer

  new UnsafeFnPointer(env,
                      args.This(),
                      reinterpret_cast<void*>(pointer),
                      return_type,
                      param_types,
                      return_struct_size,
                      std::move(owned_struct_types),
                      std::move(owned_struct_elements),
                      nonblocking);
}

void UnsafeFnPointer::Call(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  UnsafeFnPointer* fn = Unwrap<UnsafeFnPointer>(args.This());

  if (!fn->cif_prepared_) {
    env->ThrowError("Function interface not properly prepared");
    return;
  }

  // Convert JavaScript arguments to C values
  std::vector<void*> ffi_args(fn->param_types_.size());
  std::vector<uint64_t> arg_storage(fn->param_types_.size());

  for (size_t i = 0; i < fn->param_types_.size(); i++) {
    if (i >= static_cast<size_t>(args.Length())) {
      env->ThrowTypeError("Not enough arguments");
      return;
    }

    Local<Value> arg = args[i];
    if (fn->param_types_[i] == &ffi_type_sint8) {
      int32_t val = arg->Int32Value(env->context()).FromJust();
      arg_storage[i] = static_cast<uint64_t>(static_cast<int8_t>(val));
      ffi_args[i] = &arg_storage[i];
    } else if (fn->param_types_[i] == &ffi_type_uint8) {
      uint32_t val = arg->Uint32Value(env->context()).FromJust();
      arg_storage[i] = static_cast<uint8_t>(val);
      ffi_args[i] = &arg_storage[i];
    } else if (fn->param_types_[i] == &ffi_type_sint16) {
      int32_t val = arg->Int32Value(env->context()).FromJust();
      arg_storage[i] = static_cast<uint64_t>(static_cast<int16_t>(val));
      ffi_args[i] = &arg_storage[i];
    } else if (fn->param_types_[i] == &ffi_type_uint16) {
      uint32_t val = arg->Uint32Value(env->context()).FromJust();
      arg_storage[i] = static_cast<uint16_t>(val);
      ffi_args[i] = &arg_storage[i];
    } else if (fn->param_types_[i] == &ffi_type_sint32) {
      arg_storage[i] = arg->Int32Value(env->context()).FromJust();
      ffi_args[i] = &arg_storage[i];
    } else if (fn->param_types_[i] == &ffi_type_uint32) {
      arg_storage[i] = arg->Uint32Value(env->context()).FromJust();
      ffi_args[i] = &arg_storage[i];
    } else if (fn->param_types_[i] == &ffi_type_sint64) {
      if (!arg->IsBigInt()) {
        env->ThrowTypeError("Expected BigInt for int64 argument");
        return;
      }
      auto big = arg->ToBigInt(env->context());
      if (big.IsEmpty()) {
        env->ThrowTypeError("Invalid BigInt for int64 argument");
        return;
      }
      arg_storage[i] =
          static_cast<uint64_t>(big.ToLocalChecked()->Int64Value());
      ffi_args[i] = &arg_storage[i];
    } else if (fn->param_types_[i] == &ffi_type_uint64) {
      if (!arg->IsBigInt()) {
        env->ThrowTypeError("Expected BigInt for uint64 argument");
        return;
      }
      auto big = arg->ToBigInt(env->context());
      if (big.IsEmpty()) {
        env->ThrowTypeError("Invalid BigInt for uint64 argument");
        return;
      }
      arg_storage[i] = big.ToLocalChecked()->Uint64Value();
      ffi_args[i] = &arg_storage[i];
    } else if (fn->param_types_[i] == &ffi_type_pointer) {
      if (arg->IsObject()) {
        uint64_t pointer_address = 0;

        if (arg->IsArrayBufferView()) {
          Local<ArrayBufferView> view = arg.As<ArrayBufferView>();
          std::shared_ptr<BackingStore> store =
              view->Buffer()->GetBackingStore();
          if (!store) {
            env->ThrowTypeError("Invalid ArrayBufferView backing store");
            return;
          }
          void* data = store->Data();
          size_t offset = view->ByteOffset();
          arg_storage[i] =
              reinterpret_cast<uint64_t>(static_cast<char*>(data) + offset);
          ffi_args[i] = &arg_storage[i];
        } else if (arg->IsArrayBuffer()) {
          Local<ArrayBuffer> buffer = arg.As<ArrayBuffer>();
          std::shared_ptr<BackingStore> store = buffer->GetBackingStore();
          if (!store) {
            env->ThrowTypeError("Invalid ArrayBuffer backing store");
            return;
          }
          arg_storage[i] = reinterpret_cast<uint64_t>(store->Data());
          ffi_args[i] = &arg_storage[i];
        } else {
          if (!GetPointerAddress(env, arg, &pointer_address)) {
            return;
          }
          arg_storage[i] = pointer_address;
          ffi_args[i] = &arg_storage[i];
        }
      } else if (arg->IsBigInt()) {
        auto big = arg->ToBigInt(env->context());
        if (big.IsEmpty()) {
          env->ThrowTypeError("Invalid BigInt for pointer argument");
          return;
        }
        arg_storage[i] = big.ToLocalChecked()->Uint64Value();
        ffi_args[i] = &arg_storage[i];
      } else {
        env->ThrowTypeError("Pointer argument must be a PointerObject, BigInt, "
                            "ArrayBuffer, or TypedArray");
        return;
      }
    } else if (fn->param_types_[i]->type == FFI_TYPE_STRUCT) {
      void* struct_ptr = nullptr;
      size_t byte_length = 0;

      if (arg->IsArrayBufferView()) {
        Local<ArrayBufferView> view = arg.As<ArrayBufferView>();
        std::shared_ptr<BackingStore> store = view->Buffer()->GetBackingStore();
        if (!store) {
          env->ThrowTypeError("Invalid ArrayBufferView backing store");
          return;
        }
        struct_ptr = static_cast<char*>(store->Data()) + view->ByteOffset();
        byte_length = view->ByteLength();
      } else if (arg->IsArrayBuffer()) {
        Local<ArrayBuffer> buffer = arg.As<ArrayBuffer>();
        std::shared_ptr<BackingStore> store = buffer->GetBackingStore();
        if (!store) {
          env->ThrowTypeError("Invalid ArrayBuffer backing store");
          return;
        }
        struct_ptr = store->Data();
        byte_length = buffer->ByteLength();
      } else {
        env->ThrowTypeError(
            "Struct argument must be an ArrayBuffer or TypedArray");
        return;
      }

      if (byte_length < fn->param_types_[i]->size) {
        env->ThrowRangeError(
            "Struct argument buffer is smaller than struct size");
        return;
      }

      ffi_args[i] = struct_ptr;
    } else if (fn->param_types_[i] == &ffi_type_float) {
      float f = static_cast<float>(arg->NumberValue(env->context()).FromJust());
      *reinterpret_cast<float*>(&arg_storage[i]) = f;
      ffi_args[i] = &arg_storage[i];
    } else if (fn->param_types_[i] == &ffi_type_double) {
      double d = arg->NumberValue(env->context()).FromJust();
      *reinterpret_cast<double*>(&arg_storage[i]) = d;
      ffi_args[i] = &arg_storage[i];
    } else {
      env->ThrowTypeError("Unsupported argument type");
      return;
    }
  }

  if (fn->return_type_->type == FFI_TYPE_STRUCT) {
    std::unique_ptr<BackingStore> return_store =
        ArrayBuffer::NewBackingStore(env->isolate(), fn->return_struct_size_);
    ffi_call(
        &fn->cif_, FFI_FN(fn->pointer_), return_store->Data(), ffi_args.data());

    Local<ArrayBuffer> return_ab =
        ArrayBuffer::New(env->isolate(), std::move(return_store));
    Local<Uint8Array> return_view =
        Uint8Array::New(return_ab, 0, fn->return_struct_size_);
    args.GetReturnValue().Set(return_view);
    return;
  }

  // Call the function
  uint64_t return_value = 0;
  ffi_call(&fn->cif_, FFI_FN(fn->pointer_), &return_value, ffi_args.data());

  // Convert return value to JavaScript
  if (fn->return_type_ == &ffi_type_void) {
    args.GetReturnValue().SetUndefined();
  } else if (fn->return_type_ == &ffi_type_sint8) {
    args.GetReturnValue().Set(
        static_cast<int32_t>(static_cast<int8_t>(return_value)));
  } else if (fn->return_type_ == &ffi_type_uint8) {
    args.GetReturnValue().Set(
        static_cast<uint32_t>(static_cast<uint8_t>(return_value)));
  } else if (fn->return_type_ == &ffi_type_sint16) {
    args.GetReturnValue().Set(
        static_cast<int32_t>(static_cast<int16_t>(return_value)));
  } else if (fn->return_type_ == &ffi_type_uint16) {
    args.GetReturnValue().Set(
        static_cast<uint32_t>(static_cast<uint16_t>(return_value)));
  } else if (fn->return_type_ == &ffi_type_sint32) {
    args.GetReturnValue().Set(static_cast<int32_t>(return_value));
  } else if (fn->return_type_ == &ffi_type_uint32) {
    args.GetReturnValue().Set(static_cast<uint32_t>(return_value));
  } else if (fn->return_type_ == &ffi_type_sint64) {
    args.GetReturnValue().Set(
        BigInt::NewFromUnsigned(env->isolate(), return_value));
  } else if (fn->return_type_ == &ffi_type_uint64) {
    args.GetReturnValue().Set(
        BigInt::NewFromUnsigned(env->isolate(), return_value));
  } else if (fn->return_type_ == &ffi_type_pointer) {
    void* ptr = reinterpret_cast<void*>(return_value);
    args.GetReturnValue().Set(CreatePointerObject(env, ptr));
  } else if (fn->return_type_ == &ffi_type_float) {
    args.GetReturnValue().Set(*reinterpret_cast<float*>(&return_value));
  } else if (fn->return_type_ == &ffi_type_double) {
    args.GetReturnValue().Set(*reinterpret_cast<double*>(&return_value));
  }
}

MaybeLocal<Value> UnsafeFnPointer::InvokeFunction(
    const FunctionCallbackInfo<Value>& args) {
  // Delegate to Call
  Call(args);
  return MaybeLocal<Value>();
}

}  // namespace ffi
}  // namespace node

#endif  // HAVE_FFI
