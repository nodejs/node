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
  std::string return_type_name;
  std::vector<std::string> arg_type_names;
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

#ifdef HAVE_FFI_FASTCALL
// Returns true if `fn` can be invoked via the V8 fast-call path. On
// false, `*out_reason` is set to a static string describing why
// (never null after this returns; callers may pass nullptr to ignore).
//
// Eligibility = all of: every arg type and the return type are
// numeric-or-pointer, no `function`-typed args/return, GP arg count
// within ABI cap, FP arg count within ABI cap. Adds an AArch32-only
// rejection of i64/u64 args (kGPPair handling not in v1).
bool IsFastCallEligible(const FFIFunction& fn, const char** out_reason);
#endif

}  // namespace node::ffi

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
