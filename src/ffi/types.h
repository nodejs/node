#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "ffi.h"

#include <cstdint>
#include <string>
#include <vector>

namespace node::ffi {

struct FFIFunction;

bool ThrowIfContainsNullBytes(Environment* env,
                              const Utf8Value& value,
                              std::string_view label);

struct FunctionSignature {
  ffi_type* return_type;
  std::vector<ffi_type*> args;
};
v8::Maybe<FunctionSignature> ParseFunctionSignature(
    Environment* env, std::string_view name, v8::Local<v8::Object> signature);

v8::Maybe<ffi_type*> ToFFIType(Environment* env, std::string_view type_str);

enum class FFIArgumentCategory {
  Regular,
  String,
};
v8::Maybe<FFIArgumentCategory> ToFFIArgument(Environment* env,
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

bool SignaturesMatch(const FFIFunction& fn,
                     ffi_type* return_type,
                     const std::vector<ffi_type*>& args);

}  // namespace node::ffi

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
