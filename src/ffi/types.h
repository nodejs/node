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
                              const std::string& label);

bool ParseFunctionSignature(Environment* env,
                            const std::string& name,
                            v8::Local<v8::Object> signature,
                            ffi_type** return_type,
                            std::vector<ffi_type*>* args);

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

bool SignaturesMatch(const FFIFunction& fn,
                     ffi_type* return_type,
                     const std::vector<ffi_type*>& args);

}  // namespace node::ffi

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
