#if HAVE_FFI

#include "types.h"
#include "base_object-inl.h"
#include "data.h"
#include "ffi.h"
#include "node_errors.h"
#include "node_ffi.h"
#include "v8.h"

#include <climits>
#include <cmath>
#include <cstring>
#include <limits>

using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::BackingStore;
using v8::BigInt;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Maybe;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;

namespace node {

namespace ffi {

bool ThrowIfContainsNullBytes(Environment* env,
                              const Utf8Value& value,
                              const std::string& label) {
  if (value.length() != 0 &&
      std::memchr(*value, '\0', value.length()) != nullptr) {
    THROW_ERR_INVALID_ARG_VALUE(
        env, "%s must not contain null bytes", label.c_str());
    return true;
  }

  return false;
}

bool HasProperty(Local<Context> context,
                 Local<Object> object,
                 Local<String> key,
                 bool* out) {
  Maybe<bool> has = object->Has(context, key);
  if (has.IsNothing()) {
    return false;
  }

  *out = has.FromJust();
  return true;
}

bool GetStrictSignedInteger(Local<Value> value,
                            int64_t min,
                            int64_t max,
                            int64_t* out) {
  if (!value->IsNumber()) {
    return false;
  }

  double number = value.As<Number>()->Value();
  if (!std::isfinite(number) || std::floor(number) != number || number < min ||
      number > max) {
    return false;
  }

  *out = static_cast<int64_t>(number);
  return true;
}

bool GetStrictUnsignedInteger(Local<Value> value, uint64_t max, uint64_t* out) {
  if (!value->IsNumber()) {
    return false;
  }

  double number = value.As<Number>()->Value();
  if (!std::isfinite(number) || std::floor(number) != number || number < 0 ||
      number > static_cast<double>(max)) {
    return false;
  }

  *out = static_cast<uint64_t>(number);
  return true;
}

bool ParseFunctionSignature(Environment* env,
                            const std::string& name,
                            Local<Object> signature,
                            ffi_type** return_type,
                            std::vector<ffi_type*>* args) {
  Local<Context> context = env->context();
  Local<String> returns_key = env->returns_string();
  Local<String> return_key = env->return_string();
  Local<String> result_key = env->result_string();
  Local<String> parameters_key = env->parameters_string();
  Local<String> arguments_key = env->arguments_string();

  bool has_returns;
  bool has_return;
  bool has_result;
  bool has_parameters;
  bool has_arguments;

  if (!HasProperty(context, signature, returns_key, &has_returns) ||
      !HasProperty(context, signature, return_key, &has_return) ||
      !HasProperty(context, signature, result_key, &has_result) ||
      !HasProperty(context, signature, parameters_key, &has_parameters) ||
      !HasProperty(context, signature, arguments_key, &has_arguments)) {
    return false;
  }

  if (has_returns + has_return + has_result > 1) {
    std::string msg = "Function signature of " + name +
                      " must have either 'returns', 'return' or 'result' "
                      "property";
    THROW_ERR_INVALID_ARG_VALUE(env, msg);
    return false;
  }

  if (has_arguments && has_parameters) {
    std::string msg = "Function signature of " + name +
                      " must have either 'parameters' or 'arguments' "
                      "property";
    THROW_ERR_INVALID_ARG_VALUE(env, msg);
    return false;
  }

  *return_type = &ffi_type_void;
  args->clear();

  Isolate* isolate = env->isolate();
  if (has_returns || has_return || has_result) {
    Local<String> return_type_key;
    if (has_returns) {
      return_type_key = returns_key;
    } else if (has_return) {
      return_type_key = return_key;
    } else {
      return_type_key = result_key;
    }

    Local<Value> return_type_val;
    if (!signature->Get(context, return_type_key).ToLocal(&return_type_val)) {
      return false;
    }

    if (!return_type_val->IsString()) {
      std::string msg =
          "Return value type of function " + name + " must be a string";
      THROW_ERR_INVALID_ARG_VALUE(env, msg);
      return false;
    }

    Utf8Value return_type_str(isolate, return_type_val);
    if (ThrowIfContainsNullBytes(
            env, return_type_str, "Return value type of function " + name)) {
      return false;
    }
    if (!ToFFIType(env, *return_type_str, return_type)) {
      return false;
    }
  }

  if (has_arguments || has_parameters) {
    Local<Value> arguments_val;
    if (!signature->Get(context, has_arguments ? arguments_key : parameters_key)
             .ToLocal(&arguments_val)) {
      return false;
    }

    if (!arguments_val->IsArray()) {
      std::string msg =
          "Arguments list of function " + name + " must be an array";
      THROW_ERR_INVALID_ARG_VALUE(env, msg);
      return false;
    }

    Local<Array> arguments_array = arguments_val.As<Array>();
    unsigned int argn = arguments_array->Length();
    args->reserve(argn);
    for (unsigned int i = 0; i < argn; i++) {
      Local<Value> arg;

      if (!arguments_array->Get(context, i).ToLocal(&arg)) {
        return false;
      }

      if (!arg->IsString()) {
        std::string msg = "Argument " + std::to_string(i) + " of function " +
                          name + " must be a string";
        THROW_ERR_INVALID_ARG_VALUE(env, msg.c_str());
        return false;
      }

      Utf8Value arg_str(isolate, arg);
      if (ThrowIfContainsNullBytes(
              env,
              arg_str,
              "Argument " + std::to_string(i) + " of function " + name)) {
        return false;
      }
      ffi_type* arg_type;
      if (!ToFFIType(env, *arg_str, &arg_type)) {
        return false;
      }

      args->push_back(arg_type);
    }
  }

  return true;
}

bool SignaturesMatch(const FFIFunction& fn,
                     ffi_type* return_type,
                     const std::vector<ffi_type*>& args) {
  if (fn.return_type != return_type || fn.args.size() != args.size()) {
    return false;
  }

  for (size_t i = 0; i < args.size(); i++) {
    if (fn.args[i] != args[i]) {
      return false;
    }
  }

  return true;
}

bool ToFFIType(Environment* env, const std::string& type_str, ffi_type** ret) {
  if (ret == nullptr) {
    THROW_ERR_INVALID_ARG_VALUE(env, "ret must not be null");
    return false;
  }

  if (type_str == "void") {
    *ret = &ffi_type_void;
  } else if (type_str == "i8" || type_str == "int8") {
    *ret = &ffi_type_sint8;
  } else if (type_str == "u8" || type_str == "uint8" || type_str == "bool") {
    *ret = &ffi_type_uint8;
  } else if (type_str == "char") {
    *ret = CHAR_MIN < 0 ? &ffi_type_sint8 : &ffi_type_uint8;
  } else if (type_str == "i16" || type_str == "int16") {
    *ret = &ffi_type_sint16;
  } else if (type_str == "u16" || type_str == "uint16") {
    *ret = &ffi_type_uint16;
  } else if (type_str == "i32" || type_str == "int32") {
    *ret = &ffi_type_sint32;
  } else if (type_str == "u32" || type_str == "uint32") {
    *ret = &ffi_type_uint32;
  } else if (type_str == "i64" || type_str == "int64") {
    *ret = &ffi_type_sint64;
  } else if (type_str == "u64" || type_str == "uint64") {
    *ret = &ffi_type_uint64;
  } else if (type_str == "f32" || type_str == "float") {
    *ret = &ffi_type_float;
  } else if (type_str == "f64" || type_str == "double") {
    *ret = &ffi_type_double;
  } else if (type_str == "buffer" || type_str == "arraybuffer" ||
             type_str == "string" || type_str == "str" ||
             type_str == "pointer" || type_str == "ptr" ||
             type_str == "function") {
    *ret = &ffi_type_pointer;
  } else {
    std::string msg = std::string("Unsupported FFI type: ") + type_str;
    THROW_ERR_INVALID_ARG_VALUE(env, msg);
    return false;
  }

  return true;
}

uint8_t ToFFIArgument(Environment* env,
                      unsigned int index,
                      ffi_type* type,
                      Local<Value> arg,
                      void* ret) {
  Local<Context> context = env->context();

  if (type == &ffi_type_void) {
    return 1;
  } else if (type == &ffi_type_sint8) {
    int64_t value;
    if (!GetValidatedSignedInt(env, arg, INT8_MIN, INT8_MAX, "int8", &value)) {
      if (env->isolate()->IsExecutionTerminating()) return 0;
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be an int8", index);
      return 0;
    }

    *static_cast<int8_t*>(ret) = static_cast<int8_t>(value);
  } else if (type == &ffi_type_uint8) {
    uint64_t value;
    if (!GetValidatedUnsignedInt(env, arg, UINT8_MAX, "uint8", &value)) {
      if (env->isolate()->IsExecutionTerminating()) return 0;
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be a uint8", index);
      return 0;
    }

    *static_cast<uint8_t*>(ret) = static_cast<uint8_t>(value);
  } else if (type == &ffi_type_sint16) {
    int64_t value;
    if (!GetValidatedSignedInt(
            env, arg, INT16_MIN, INT16_MAX, "int16", &value)) {
      if (env->isolate()->IsExecutionTerminating()) return 0;
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be an int16", index);
      return 0;
    }

    *static_cast<int16_t*>(ret) = static_cast<int16_t>(value);
  } else if (type == &ffi_type_uint16) {
    uint64_t value;
    if (!GetValidatedUnsignedInt(env, arg, UINT16_MAX, "uint16", &value)) {
      if (env->isolate()->IsExecutionTerminating()) return 0;
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be a uint16", index);
      return 0;
    }

    *static_cast<uint16_t*>(ret) = static_cast<uint16_t>(value);
  } else if (type == &ffi_type_sint32) {
    if (!arg->IsInt32()) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be an int32", index);
      return 0;
    }

    *static_cast<int32_t*>(ret) = arg->Int32Value(context).FromJust();
  } else if (type == &ffi_type_uint32) {
    if (!arg->IsUint32()) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be a uint32", index);
      return 0;
    }

    *static_cast<uint32_t*>(ret) = arg->Uint32Value(context).FromJust();
  } else if (type == &ffi_type_sint64) {
    if (!arg->IsBigInt()) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be an int64", index);
      return 0;
    }

    bool lossless;
    *static_cast<int64_t*>(ret) = arg.As<BigInt>()->Int64Value(&lossless);
    if (!lossless) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be an int64", index);
      return 0;
    }
  } else if (type == &ffi_type_uint64) {
    if (!arg->IsBigInt()) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be a uint64", index);
      return 0;
    }

    bool lossless;
    *static_cast<uint64_t*>(ret) = arg.As<BigInt>()->Uint64Value(&lossless);
    if (!lossless) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be a uint64", index);
      return 0;
    }
  } else if (type == &ffi_type_float) {
    if (!arg->IsNumber()) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be a float", index);
      return 0;
    }

    *static_cast<float*>(ret) =
        static_cast<float>(arg->NumberValue(context).FromJust());
  } else if (type == &ffi_type_double) {
    if (!arg->IsNumber()) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be a double", index);
      return 0;
    }

    *static_cast<double*>(ret) = arg->NumberValue(context).FromJust();
  } else if (type == &ffi_type_pointer) {
    if (arg->IsNullOrUndefined()) {
      *static_cast<uint64_t*>(ret) = reinterpret_cast<uint64_t>(nullptr);
    } else if (arg->IsString()) {
      // This will handled in Invoke so that we can free the copied string after
      // the call
      return 2;
    } else if (arg->IsArrayBufferView()) {
      // Pointer-like ArrayBufferView arguments borrow backing-store memory
      // without pinning. Resizing, transferring, detaching, or otherwise
      // invalidating that backing store during the active FFI call is
      // unsupported and dangerous.
      Local<ArrayBufferView> view = arg.As<ArrayBufferView>();
      std::shared_ptr<BackingStore> store = view->Buffer()->GetBackingStore();

      if (!store) {
        THROW_ERR_INVALID_ARG_VALUE(
            env,
            "Invalid ArrayBufferView backing store for argument %u",
            index);
        return 0;
      }

      void* data = store->Data();
      size_t offset = view->ByteOffset();

      *static_cast<uint64_t*>(ret) =
          reinterpret_cast<uint64_t>(static_cast<char*>(data) + offset);
    } else if (arg->IsArrayBuffer()) {
      // Pointer-like ArrayBuffer arguments borrow backing-store memory without
      // pinning. Resizing, transferring, detaching, or otherwise invalidating
      // that backing store during the active FFI call is unsupported and
      // dangerous.
      Local<ArrayBuffer> buffer = arg.As<ArrayBuffer>();
      std::shared_ptr<BackingStore> store = buffer->GetBackingStore();

      if (!store) {
        THROW_ERR_INVALID_ARG_VALUE(
            env, "Invalid ArrayBuffer backing store for argument %u", index);
        return 0;
      }

      *static_cast<uint64_t*>(ret) = reinterpret_cast<uint64_t>(store->Data());
    } else if (arg->IsBigInt()) {
      bool lossless;
      uint64_t pointer = arg.As<BigInt>()->Uint64Value(&lossless);
      if (!lossless || pointer > static_cast<uint64_t>(
                                     std::numeric_limits<uintptr_t>::max())) {
        THROW_ERR_INVALID_ARG_VALUE(
            env, "Argument %u must be a non-negative pointer bigint", index);
        return 0;
      }

      *static_cast<uint64_t*>(ret) = pointer;
    } else {
      THROW_ERR_INVALID_ARG_VALUE(
          env,
          "Argument %u must be a buffer, an ArrayBuffer, a string, or a bigint",
          index);
      return 0;
    }
  } else {
    THROW_ERR_INVALID_ARG_VALUE(
        env, "Unsupported FFI type for argument %u", index);
    return 0;
  }

  return 1;
}

Local<Value> ToJSArgument(Isolate* isolate, ffi_type* type, void* data) {
  Local<Value> ret;

  if (type == &ffi_type_sint8) {
    ret = Integer::New(isolate, *static_cast<int8_t*>(data));
  } else if (type == &ffi_type_uint8) {
    ret = Integer::NewFromUnsigned(isolate, *static_cast<uint8_t*>(data));
  } else if (type == &ffi_type_sint16) {
    ret = Integer::New(isolate, *static_cast<int16_t*>(data));
  } else if (type == &ffi_type_uint16) {
    ret = Integer::NewFromUnsigned(isolate, *static_cast<uint16_t*>(data));
  } else if (type == &ffi_type_sint32) {
    ret = Integer::New(isolate, *static_cast<int32_t*>(data));
  } else if (type == &ffi_type_uint32) {
    ret = Integer::NewFromUnsigned(isolate, *static_cast<uint32_t*>(data));
  } else if (type == &ffi_type_sint64) {
    ret = BigInt::New(isolate, *static_cast<int64_t*>(data));
  } else if (type == &ffi_type_uint64) {
    ret = BigInt::NewFromUnsigned(isolate, *static_cast<uint64_t*>(data));
  } else if (type == &ffi_type_float) {
    ret = Number::New(isolate, *static_cast<float*>(data));
  } else if (type == &ffi_type_double) {
    ret = Number::New(isolate, *static_cast<double*>(data));
  } else if (type == &ffi_type_pointer) {
    // All others are treated as pointer (and thus bigint), the user will use
    // other helpers to convert
    ret = BigInt::NewFromUnsigned(
        isolate, reinterpret_cast<uint64_t>(*static_cast<void**>(data)));
  } else {
    // For anything else, return undefined to avoid problems
    ret = Undefined(isolate);
  }

  return ret;
}

size_t GetFFIReturnValueStorageSize(ffi_type* type) {
  if (type == &ffi_type_sint8 || type == &ffi_type_uint8 ||
      type == &ffi_type_sint16 || type == &ffi_type_uint16 ||
      type == &ffi_type_sint32 || type == &ffi_type_uint32) {
    return sizeof(ffi_arg);
  }

  return type->size;
}

bool ToJSReturnValue(Environment* env,
                     const FunctionCallbackInfo<Value>& args,
                     ffi_type* type,
                     void* result) {
  if (type == &ffi_type_void) {
    args.GetReturnValue().SetUndefined();
  } else if (type == &ffi_type_sint8) {
    args.GetReturnValue().Set(static_cast<int32_t>(
        static_cast<int8_t>(*static_cast<const ffi_sarg*>(result))));
  } else if (type == &ffi_type_uint8) {
    args.GetReturnValue().Set(static_cast<uint32_t>(
        static_cast<uint8_t>(*static_cast<const ffi_arg*>(result))));
  } else if (type == &ffi_type_sint16) {
    args.GetReturnValue().Set(static_cast<int32_t>(
        static_cast<int16_t>(*static_cast<const ffi_sarg*>(result))));
  } else if (type == &ffi_type_uint16) {
    args.GetReturnValue().Set(static_cast<uint32_t>(
        static_cast<uint16_t>(*static_cast<const ffi_arg*>(result))));
  } else if (type == &ffi_type_sint32) {
    args.GetReturnValue().Set(
        static_cast<int32_t>(*static_cast<const ffi_sarg*>(result)));
  } else if (type == &ffi_type_uint32) {
    args.GetReturnValue().Set(
        static_cast<uint32_t>(*static_cast<const ffi_arg*>(result)));
  } else if (type == &ffi_type_sint64) {
    args.GetReturnValue().Set(
        BigInt::New(env->isolate(), *static_cast<const int64_t*>(result)));
  } else if (type == &ffi_type_uint64) {
    args.GetReturnValue().Set(BigInt::NewFromUnsigned(
        env->isolate(), *static_cast<const uint64_t*>(result)));
  } else if (type == &ffi_type_float) {
    args.GetReturnValue().Set(*static_cast<const float*>(result));
  } else if (type == &ffi_type_double) {
    args.GetReturnValue().Set(*static_cast<const double*>(result));
  } else if (type == &ffi_type_pointer) {
    auto ptr = *static_cast<void* const*>(result);
    args.GetReturnValue().Set(BigInt::NewFromUnsigned(
        env->isolate(),
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr))));
  }

  return true;
}

bool ToFFIReturnValue(Local<Value> result, ffi_type* type, void* ret) {
  if (type != &ffi_type_void && ret != nullptr) {
    std::memset(ret, 0, GetFFIReturnValueStorageSize(type));
  }

  if (type == &ffi_type_sint8) {
    int64_t value;

    if (!GetStrictSignedInteger(result, INT8_MIN, INT8_MAX, &value)) {
      return false;
    }

    *static_cast<ffi_sarg*>(ret) = static_cast<ffi_sarg>(value);
  } else if (type == &ffi_type_uint8) {
    uint64_t value;

    if (!GetStrictUnsignedInteger(result, UINT8_MAX, &value)) {
      return false;
    }

    *static_cast<ffi_arg*>(ret) = static_cast<ffi_arg>(value);
  } else if (type == &ffi_type_sint16) {
    int64_t value;

    if (!GetStrictSignedInteger(result, INT16_MIN, INT16_MAX, &value)) {
      return false;
    }

    *static_cast<ffi_sarg*>(ret) = static_cast<ffi_sarg>(value);
  } else if (type == &ffi_type_uint16) {
    uint64_t value;

    if (!GetStrictUnsignedInteger(result, UINT16_MAX, &value)) {
      return false;
    }

    *static_cast<ffi_arg*>(ret) = static_cast<ffi_arg>(value);
  } else if (type == &ffi_type_sint32) {
    int64_t value;

    if (!GetStrictSignedInteger(result, INT32_MIN, INT32_MAX, &value)) {
      return false;
    }

    *static_cast<int32_t*>(ret) = static_cast<int32_t>(value);
  } else if (type == &ffi_type_uint32) {
    uint64_t value;

    if (!GetStrictUnsignedInteger(result, UINT32_MAX, &value)) {
      return false;
    }

    *static_cast<uint32_t*>(ret) = static_cast<uint32_t>(value);
  } else if (type == &ffi_type_sint64) {
    bool lossless;

    if (!result->IsBigInt()) {
      return false;
    }

    *static_cast<int64_t*>(ret) = result.As<BigInt>()->Int64Value(&lossless);
    if (!lossless) {
      return false;
    }
  } else if (type == &ffi_type_uint64) {
    bool lossless;

    if (!result->IsBigInt()) {
      return false;
    }

    *static_cast<uint64_t*>(ret) = result.As<BigInt>()->Uint64Value(&lossless);
    if (!lossless) {
      return false;
    }
  } else if (type == &ffi_type_float) {
    if (!result->IsNumber()) {
      return false;
    }

    *static_cast<float*>(ret) =
        static_cast<float>(result.As<Number>()->Value());
  } else if (type == &ffi_type_double) {
    if (!result->IsNumber()) {
      return false;
    }

    *static_cast<double*>(ret) = result.As<Number>()->Value();
  } else if (type == &ffi_type_pointer) {
    bool lossless;

    if (result->IsNullOrUndefined()) {
      *static_cast<uint64_t*>(ret) = reinterpret_cast<uint64_t>(nullptr);
    } else if (result->IsBigInt()) {
      uint64_t pointer = result.As<BigInt>()->Uint64Value(&lossless);
      if (!lossless || pointer > static_cast<uint64_t>(
                                     std::numeric_limits<uintptr_t>::max())) {
        return false;
      }

      *static_cast<uint64_t*>(ret) = pointer;
    } else {
      // Note that strings, buffer or ArrayBuffer are ignored
      *static_cast<uint64_t*>(ret) = reinterpret_cast<uint64_t>(nullptr);
      return false;
    }
  } else if (type != &ffi_type_void) {
    return false;
  }

  return true;
}

}  // namespace ffi

}  // namespace node

#endif  // HAVE_FFI
