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

// Returns true if `fn` can be invoked via the V8 fast-call path. On
// false, `*out_reason` is set to a static string describing why
// (never null after this returns; callers may pass nullptr to ignore).
//
// Eligibility checks: every arg type and the return type are
// numeric-or-pointer, no `function`-typed args/return, arg count
// within V8 fast-call cap (8), and register-passed arg counts within
// per-ABI limits. Trampoline emitters currently exist for AArch64
// (≤ 7 GP + ≤ 8 FP), x86_64 SysV (≤ 6 GP + ≤ 8 FP; buffer args cap GP at 5 and
// cannot coexist with FP args), Win64 x64 (≤ 3 register-only scalar args), and
// PPC64LE ELFv2, LoongArch64, and RISC-V 64 (≤ 7 GP + ≤ 8 FP scalar args, no
// narrow returns), and s390x (≤ 4 GP + ≤ 4 FP scalar args, no narrow returns).
// Platforms without an emitter are reported ineligible so the caller falls back
// to libffi.
bool IsFastCallEligible(const FFIFunction& fn, const char** out_reason);

// True if the FFI type can be read from / written to a raw byte buffer
// without needing V8 operations (conversion, allocation, etc.).
bool IsSBEligibleFFIType(ffi_type* type);

// True if the signature's return type and all argument types are SB-eligible.
bool IsSBEligibleSignature(const FFIFunction& fn);

// True if any argument is pointer-typed. For these, the JS wrapper must
// do a runtime type check to decide fast vs. slow path per call.
bool SignatureHasPointerArgs(const FFIFunction& fn);

// Read a value of the given FFI type from buffer at the given byte offset
// into the output pointer (sized as uint64_t to hold any numeric type).
void ReadFFIArgFromBuffer(ffi_type* type,
                          const uint8_t* buffer,
                          size_t offset,
                          void* out);

// Write a return value of the given FFI type from the ffi_call result
// into the buffer at the given byte offset.
void WriteFFIReturnToBuffer(ffi_type* type,
                            const void* result,
                            uint8_t* buffer,
                            size_t offset);

}  // namespace node::ffi

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
