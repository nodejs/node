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
using v8::Just;
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
                              std::string_view label) {
  if (value.length() != 0 &&
      std::memchr(*value, '\0', value.length()) != nullptr) {
    THROW_ERR_INVALID_ARG_VALUE(env, "%s must not contain null bytes", label);
    return true;
  }

  return false;
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

Maybe<FunctionSignature> ParseFunctionSignature(Environment* env,
                                                std::string_view name,
                                                Local<Object> signature) {
  Local<Context> context = env->context();
  Local<String> return_key = env->return_string();
  Local<String> arguments_key = env->arguments_string();

  bool has_return;
  bool has_arguments;

  if (!signature->Has(context, return_key).To(&has_return) ||
      !signature->Has(context, arguments_key).To(&has_arguments)) {
    return {};
  }

  ffi_type* return_type = &ffi_type_void;
  std::vector<ffi_type*> args;
  std::string return_type_name = "void";
  std::vector<std::string> arg_type_names;

  Isolate* isolate = env->isolate();
  if (has_return) {
    Local<Value> return_type_val;
    if (!signature->Get(context, return_key).ToLocal(&return_type_val)) {
      return {};
    }

    if (!return_type_val->IsString()) {
      THROW_ERR_INVALID_ARG_VALUE(
          env, "Return value type of function %s must be a string", name);
      return {};
    }

    Utf8Value return_type_str(isolate, return_type_val);
    if (ThrowIfContainsNullBytes(
            env,
            return_type_str,
            "Return value type of function " + std::string(name))) {
      return {};
    }
    if (!ToFFIType(env, return_type_str.ToStringView()).To(&return_type)) {
      return {};
    }
    return_type_name = return_type_str.ToString();
  }

  if (has_arguments) {
    Local<Value> arguments_val;
    if (!signature->Get(context, arguments_key).ToLocal(&arguments_val)) {
      return {};
    }

    if (!arguments_val->IsArray()) {
      THROW_ERR_INVALID_ARG_VALUE(
          env, "Arguments list of function %s must be an array", name);
      return {};
    }

    Local<Array> arguments_array = arguments_val.As<Array>();
    unsigned int argn = arguments_array->Length();
    args.reserve(argn);
    for (unsigned int i = 0; i < argn; i++) {
      Local<Value> arg;

      if (!arguments_array->Get(context, i).ToLocal(&arg)) {
        return {};
      }

      if (!arg->IsString()) {
        THROW_ERR_INVALID_ARG_VALUE(
            env, "Argument %u of function %s must be a string", i, name);
        return {};
      }

      Utf8Value arg_str(isolate, arg);
      if (ThrowIfContainsNullBytes(env,
                                   arg_str,
                                   "Argument " + std::to_string(i) +
                                       " of function " + std::string(name))) {
        return {};
      }
      ffi_type* arg_type;
      if (!ToFFIType(env, arg_str.ToStringView()).To(&arg_type)) {
        return {};
      }
      if (arg_type == &ffi_type_void) {
        THROW_ERR_INVALID_ARG_VALUE(
            env,
            "Argument %u of function %s must not be 'void'; "
            "use an empty array for no-argument functions",
            i,
            name);
        return {};
      }

      args.push_back(arg_type);
      arg_type_names.emplace_back(arg_str.ToString());
    }
  }

  return Just(FunctionSignature{return_type,
                                std::move(args),
                                std::move(return_type_name),
                                std::move(arg_type_names)});
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

namespace {

bool IsFastCallEligibleFFIType(ffi_type* type) {
  // Accept all numeric types, pointer, and void (void OK as return only).
  // Rejects struct types (not yet supported in fast-call path).
  return type == &ffi_type_void || type == &ffi_type_sint8 ||
         type == &ffi_type_uint8 || type == &ffi_type_sint16 ||
         type == &ffi_type_uint16 || type == &ffi_type_sint32 ||
         type == &ffi_type_uint32 || type == &ffi_type_sint64 ||
         type == &ffi_type_uint64 || type == &ffi_type_float ||
         type == &ffi_type_double || type == &ffi_type_pointer;
}

bool IsFunctionTypeName(const std::string& name) {
  return name == "function";
}

// Check if an FFI type occupies a floating-point register.
bool IsFFITypeFloat(ffi_type* type) {
  return type == &ffi_type_float || type == &ffi_type_double;
}

// Check if an FFI type name maps to a kBuffer (kV8Value) argument in the
// fast-call path, which consumes an extra GP register for the helper call.
bool IsBufferTypeName(const std::string& name) {
  return name == "buffer" || name == "arraybuffer";
}

}  // namespace

bool IsFastCallEligible(const FFIFunction& fn, const char** out_reason) {
  static const char* dummy = "";
  if (out_reason == nullptr) out_reason = &dummy;

    // Check that a platform stub emitter exists for the current ABI.
    // Stub emitters cover AArch64, x86_64 SysV, and Win64 x64. Other platforms
    // fall back to libffi.
#if !defined(__aarch64__) && !defined(_M_ARM64) && !defined(__x86_64__) &&     \
    !defined(_M_X64) && !defined(__powerpc64__) && !defined(__ppc64__) &&      \
    !defined(__PPC64__) && !defined(__loongarch64) &&                          \
    !(defined(__riscv) && __riscv_xlen == 64) && !defined(__s390x__)
  *out_reason = "no platform stub emitter";
  return false;
#endif

  // Check return type eligibility.
  if (!IsFastCallEligibleFFIType(fn.return_type)) {
    *out_reason = "unsupported return type";
    return false;
  }
  if (IsFunctionTypeName(fn.return_type_name)) {
    *out_reason = "return type is function";
    return false;
  }

  // V8's fast-call lowering caps the C-side arg count. With HasReceiver=kNo
  // there's no implicit receiver in the count, so this is the user-arg cap.
  // V8's hard limit is 8 args; signatures over that fall back to libffi.
  if (fn.args.size() > 8) {
    *out_reason = "argument count exceeds V8 fast-call cap";
    return false;
  }

  // Per-ABI register caps for arguments that must be passed in registers.
  // If an arg can't fit in a register, it goes on the stack — which the
  // current trampoline generators don't support.
  // `args` and `arg_type_names` are read in lockstep below. A malformed
  // FFIFunction with mismatched lengths would otherwise index out of bounds,
  // so reject it here rather than relying on callers to pre-validate.
  if (fn.args.size() != fn.arg_type_names.size()) {
    *out_reason = "argument type name count mismatch";
    return false;
  }

  size_t gp_count = 0;
  size_t fp_count = 0;
  bool has_buffer_arg = false;
  for (size_t i = 0; i < fn.args.size(); ++i) {
    ffi_type* t = fn.args[i];
    const std::string& name = fn.arg_type_names[i];

    if (!IsFastCallEligibleFFIType(t)) {
      *out_reason = "unsupported arg type";
      return false;
    }
    // `void` is fine as a return type but has no register slot, so it cannot
    // appear in `args`.
    if (t == &ffi_type_void) {
      *out_reason = "void cannot be an argument type";
      return false;
    }
    if (IsFunctionTypeName(name)) {
      *out_reason = "arg is function";
      return false;
    }

    // `buffer`/`arraybuffer` args arrive as kV8Value in the V8 fast-call
    // signature, consuming an extra GP register for the helper call.
    if (IsBufferTypeName(name)) {
      has_buffer_arg = true;
    }

    // Count register classes used by each argument.
    if (IsFFITypeFloat(t)) {
      fp_count++;
    } else {
      gp_count++;
    }
  }

  // Platform-specific register pressure limits.
#if defined(__aarch64__) || defined(_M_ARM64)
  // AArch64: 8 FP registers (v0-v7) + up to 7 GP registers per trampoline
  // constraint (the 8th GP slot is consumed by the helper call for buffer
  // args). Buffer args and float args can't coexist — the helper call would
  // clobber FP state.
  const size_t effective_gp = gp_count + (has_buffer_arg ? 1 : 0);
  if (has_buffer_arg && fp_count != 0) {
    *out_reason = "buffer and float args cannot coexist on AArch64";
    return false;
  }
  if (effective_gp > 7 || fp_count > 8) {
    *out_reason = "argument count exceeds AArch64 register limit";
    return false;
  }
#elif defined(_M_X64)
  // Win64 x64 uses positional integer/FP registers. The current emitter handles
  // only the register-only scalar subset: receiver plus up to three public
  // arguments. Buffer-shaped arguments require FastApiCallbackOptions and a C++
  // helper call, which is left to fallback until the Win64 emitter grows stack
  // and helper support.
  if (has_buffer_arg) {
    *out_reason = "buffer args are not yet supported on Win64 x64";
    return false;
  }
  if (fn.args.size() > 3) {
    *out_reason = "argument count exceeds Win64 x64 register-only limit";
    return false;
  }
  if (fp_count > 3 || gp_count > 3) {
    *out_reason = "argument count exceeds Win64 x64 register-only limit";
    return false;
  }
#elif defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__)
#if defined(_AIX) ||                                                           \
    !(defined(__LITTLE_ENDIAN__) ||                                            \
      (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
  *out_reason = "no PPC64BE fast-call trampoline emitter";
  return false;
#else
  // PPC64LE ELFv2: r3 is occupied by V8's receiver, leaving r4..r10 for
  // incoming user GP arguments. FP arguments use FPRs and are not shifted by
  // the receiver slot. The first PPC64LE emitter is scalar-only and
  // tail-branches to the target, so narrow return normalization and buffer
  // helper calls fall back.
  if (has_buffer_arg) {
    *out_reason = "buffer args are not yet supported on PPC64LE";
    return false;
  }
  if (fn.return_type == &ffi_type_sint8 || fn.return_type == &ffi_type_uint8 ||
      fn.return_type == &ffi_type_sint16 ||
      fn.return_type == &ffi_type_uint16) {
    *out_reason = "narrow returns are not yet supported on PPC64LE";
    return false;
  }
  if (gp_count > 7 || fp_count > 8) {
    *out_reason = "argument count exceeds PPC64LE register limit";
    return false;
  }
#endif
#elif defined(__loongarch64)
  // LoongArch64: a0 is occupied by V8's receiver, leaving a1..a7 for incoming
  // user GP arguments. FP arguments are already in fa0..fa7. The current
  // emitter is scalar-only and tail-branches to the target, so narrow returns
  // and buffer helper calls fall back.
  if (has_buffer_arg) {
    *out_reason = "buffer args are not yet supported on LoongArch64";
    return false;
  }
  if (fn.return_type == &ffi_type_sint8 || fn.return_type == &ffi_type_uint8 ||
      fn.return_type == &ffi_type_sint16 ||
      fn.return_type == &ffi_type_uint16) {
    *out_reason = "narrow returns are not yet supported on LoongArch64";
    return false;
  }
  if (gp_count > 7 || fp_count > 8) {
    *out_reason = "argument count exceeds LoongArch64 register limit";
    return false;
  }
#elif defined(__riscv) && __riscv_xlen == 64
  // RISC-V LP64D: a0 is occupied by V8's receiver, leaving a1..a7 for incoming
  // user GP arguments. FP arguments are already in fa0..fa7. The current
  // emitter is scalar-only and tail-branches to the target, so narrow returns
  // and buffer helper calls fall back.
  if (has_buffer_arg) {
    *out_reason = "buffer args are not yet supported on RISC-V 64";
    return false;
  }
  if (fn.return_type == &ffi_type_sint8 || fn.return_type == &ffi_type_uint8 ||
      fn.return_type == &ffi_type_sint16 ||
      fn.return_type == &ffi_type_uint16) {
    *out_reason = "narrow returns are not yet supported on RISC-V 64";
    return false;
  }
  if (gp_count > 7 || fp_count > 8) {
    *out_reason = "argument count exceeds RISC-V 64 register limit";
    return false;
  }
#elif defined(__s390x__)
  // Linux s390x: r2 is occupied by V8's receiver, leaving r3..r6 for incoming
  // user GP arguments. FP arguments are already in f0, f2, f4, and f6. The
  // current emitter is scalar-only and tail-branches to the target, so narrow
  // returns and buffer helper calls fall back.
  if (has_buffer_arg) {
    *out_reason = "buffer args are not yet supported on s390x";
    return false;
  }
  if (fn.return_type == &ffi_type_sint8 || fn.return_type == &ffi_type_uint8 ||
      fn.return_type == &ffi_type_sint16 ||
      fn.return_type == &ffi_type_uint16) {
    *out_reason = "narrow returns are not yet supported on s390x";
    return false;
  }
  if (gp_count > 4 || fp_count > 4) {
    *out_reason = "argument count exceeds s390x register limit";
    return false;
  }
#elif defined(__x86_64__)
  // x86_64 SysV: the V8 receiver occupies rdi, leaving rsi, rdx, rcx, r8, r9
  // (5 incoming GP slots); scalar signatures can load one more user GP arg
  // from the caller stack, for an effective cap of 6 GP. FP args use
  // xmm0-xmm7. Buffer args spill the whole incoming GP window through a C++
  // helper, so they cannot coexist with FP args and stay register-only
  // (incoming GP = gp + 1 must fit in the 5 incoming registers, i.e. <= 4 GP
  // when a buffer is present). These rules mirror the constraints enforced by
  // node_ffi_create_fast_trampoline in src/ffi/platforms/x64.cc.
  if (has_buffer_arg && fp_count != 0) {
    *out_reason = "buffer and float args cannot coexist on x86_64 SysV";
    return false;
  }
  const size_t incoming_gp = gp_count + (has_buffer_arg ? 1 : 0);
  const size_t max_incoming_gp = has_buffer_arg ? 5 : 6;
  if (incoming_gp > max_incoming_gp || fp_count > 8) {
    *out_reason = "argument count exceeds x86_64 SysV register limit";
    return false;
  }
#endif  // __x86_64__

  *out_reason = "";
  return true;
}

bool IsSBEligibleFFIType(ffi_type* type) {
  return type == &ffi_type_void || type == &ffi_type_sint8 ||
         type == &ffi_type_uint8 || type == &ffi_type_sint16 ||
         type == &ffi_type_uint16 || type == &ffi_type_sint32 ||
         type == &ffi_type_uint32 || type == &ffi_type_sint64 ||
         type == &ffi_type_uint64 || type == &ffi_type_float ||
         type == &ffi_type_double || type == &ffi_type_pointer;
}

bool IsSBEligibleSignature(const FFIFunction& fn) {
  // The JS wrapper writes and reads the shared buffer little-endian while
  // the C++ side uses memcpy in host order. On big-endian hosts these
  // disagree, so the fast path is disabled there.
  if constexpr (IsBigEndian()) {
    return false;
  }
  // Zero-argument functions gain nothing from the shared-buffer path
  // (no argument packing to skip) and measurably lose on tight native
  // calls like `uv_os_getpid` due to the wrapper's fixed overhead.
  if (fn.args.empty()) return false;
  if (!IsSBEligibleFFIType(fn.return_type)) return false;
  for (ffi_type* arg : fn.args) {
    if (!IsSBEligibleFFIType(arg)) return false;
  }
  return true;
}

bool SignatureHasPointerArgs(const FFIFunction& fn) {
  for (ffi_type* arg : fn.args) {
    if (arg == &ffi_type_pointer) return true;
  }
  return false;
}

void ReadFFIArgFromBuffer(ffi_type* type,
                          const uint8_t* buffer,
                          size_t offset,
                          void* out) {
  CHECK(IsSBEligibleFFIType(type));
  CHECK_LE(type->size, sizeof(uint64_t));
  // memcpy avoids the strict-aliasing violation that a direct typed load
  // from the raw uint8_t buffer would incur.
  const uint8_t* src = buffer + offset;
  std::memcpy(out, src, type->size);
}

void WriteFFIReturnToBuffer(ffi_type* type,
                            const void* result,
                            uint8_t* buffer,
                            size_t offset) {
  CHECK(IsSBEligibleFFIType(type));
  uint8_t* dst = buffer + offset;
  std::memset(dst, 0, 8);

  if (type == &ffi_type_void) {
    return;
  }

  // libffi promotes small integer return values to ffi_arg size, so these
  // branches read as ffi_arg or ffi_sarg and then truncate back down.
  if (type == &ffi_type_sint8) {
    int8_t tmp = static_cast<int8_t>(*static_cast<const ffi_sarg*>(result));
    std::memcpy(dst, &tmp, sizeof(tmp));
    return;
  }
  if (type == &ffi_type_uint8) {
    uint8_t tmp = static_cast<uint8_t>(*static_cast<const ffi_arg*>(result));
    std::memcpy(dst, &tmp, sizeof(tmp));
    return;
  }
  if (type == &ffi_type_sint16) {
    int16_t tmp = static_cast<int16_t>(*static_cast<const ffi_sarg*>(result));
    std::memcpy(dst, &tmp, sizeof(tmp));
    return;
  }
  if (type == &ffi_type_uint16) {
    uint16_t tmp = static_cast<uint16_t>(*static_cast<const ffi_arg*>(result));
    std::memcpy(dst, &tmp, sizeof(tmp));
    return;
  }
  if (type == &ffi_type_sint32) {
    int32_t tmp = static_cast<int32_t>(*static_cast<const ffi_sarg*>(result));
    std::memcpy(dst, &tmp, sizeof(tmp));
    return;
  }
  if (type == &ffi_type_uint32) {
    uint32_t tmp = static_cast<uint32_t>(*static_cast<const ffi_arg*>(result));
    std::memcpy(dst, &tmp, sizeof(tmp));
    return;
  }

  // Remaining SB-eligible types (sint64, uint64, float, double, pointer)
  // are not promoted by libffi and can be copied as-is.
  std::memcpy(dst, result, type->size);
}

v8::Maybe<ffi_type*> ToFFIType(Environment* env, std::string_view type_str) {
  if (type_str == "void") {
    return Just(&ffi_type_void);
  } else if (type_str == "i8" || type_str == "int8") {
    return Just(&ffi_type_sint8);
  } else if (type_str == "u8" || type_str == "uint8" || type_str == "bool") {
    return Just(&ffi_type_uint8);
  } else if (type_str == "char") {
    return Just(CHAR_MIN < 0 ? &ffi_type_sint8 : &ffi_type_uint8);
  } else if (type_str == "i16" || type_str == "int16") {
    return Just(&ffi_type_sint16);
  } else if (type_str == "u16" || type_str == "uint16") {
    return Just(&ffi_type_uint16);
  } else if (type_str == "i32" || type_str == "int32") {
    return Just(&ffi_type_sint32);
  } else if (type_str == "u32" || type_str == "uint32") {
    return Just(&ffi_type_uint32);
  } else if (type_str == "i64" || type_str == "int64") {
    return Just(&ffi_type_sint64);
  } else if (type_str == "u64" || type_str == "uint64") {
    return Just(&ffi_type_uint64);
  } else if (type_str == "f32" || type_str == "float" ||
             type_str == "float32") {
    return Just(&ffi_type_float);
  } else if (type_str == "f64" || type_str == "double" ||
             type_str == "float64") {
    return Just(&ffi_type_double);
  } else if (type_str == "buffer" || type_str == "arraybuffer" ||
             type_str == "string" || type_str == "str" ||
             type_str == "pointer" || type_str == "ptr" ||
             type_str == "function") {
    return Just(&ffi_type_pointer);
  } else {
    THROW_ERR_INVALID_ARG_VALUE(env, "Unsupported FFI type: %s", type_str);
    return {};
  }
}

// The JS fast path in lib/internal/ffi-shared-buffer.js mirrors the
// validation below. `writeNumericArg` matches the numeric branches and
// `writePointerArg` matches the pointer-BigInt branch. Error codes and
// messages must stay identical across all three sites.
Maybe<FFIArgumentCategory> ToFFIArgument(Environment* env,
                                         unsigned int index,
                                         ffi_type* type,
                                         Local<Value> arg,
                                         void* ret) {
  Local<Context> context = env->context();

  if (type == &ffi_type_void) {
    return Just(FFIArgumentCategory::Regular);
  } else if (type == &ffi_type_sint8) {
    int64_t value;
    if (!GetValidatedSignedInt(env, arg, INT8_MIN, INT8_MAX, "int8")
             .To(&value)) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be an int8", index);
      return {};
    }

    *static_cast<int8_t*>(ret) = static_cast<int8_t>(value);
  } else if (type == &ffi_type_uint8) {
    uint64_t value;
    if (!GetValidatedUnsignedInt(env, arg, UINT8_MAX, "uint8").To(&value)) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be a uint8", index);
      return {};
    }

    *static_cast<uint8_t*>(ret) = static_cast<uint8_t>(value);
  } else if (type == &ffi_type_sint16) {
    int64_t value;
    if (!GetValidatedSignedInt(env, arg, INT16_MIN, INT16_MAX, "int16")
             .To(&value)) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be an int16", index);
      return {};
    }

    *static_cast<int16_t*>(ret) = static_cast<int16_t>(value);
  } else if (type == &ffi_type_uint16) {
    uint64_t value;
    if (!GetValidatedUnsignedInt(env, arg, UINT16_MAX, "uint16").To(&value)) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be a uint16", index);
      return {};
    }

    *static_cast<uint16_t*>(ret) = static_cast<uint16_t>(value);
  } else if (type == &ffi_type_sint32) {
    if (!arg->IsInt32()) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be an int32", index);
      return {};
    }

    *static_cast<int32_t*>(ret) = arg->Int32Value(context).FromJust();
  } else if (type == &ffi_type_uint32) {
    if (!arg->IsUint32()) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be a uint32", index);
      return {};
    }

    *static_cast<uint32_t*>(ret) = arg->Uint32Value(context).FromJust();
  } else if (type == &ffi_type_sint64) {
    if (!arg->IsBigInt()) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be an int64", index);
      return {};
    }

    bool lossless;
    *static_cast<int64_t*>(ret) = arg.As<BigInt>()->Int64Value(&lossless);
    if (!lossless) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be an int64", index);
      return {};
    }
  } else if (type == &ffi_type_uint64) {
    if (!arg->IsBigInt()) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be a uint64", index);
      return {};
    }

    bool lossless;
    *static_cast<uint64_t*>(ret) = arg.As<BigInt>()->Uint64Value(&lossless);
    if (!lossless) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be a uint64", index);
      return {};
    }
  } else if (type == &ffi_type_float) {
    if (!arg->IsNumber()) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be a float", index);
      return {};
    }

    *static_cast<float*>(ret) =
        static_cast<float>(arg->NumberValue(context).FromJust());
  } else if (type == &ffi_type_double) {
    if (!arg->IsNumber()) {
      THROW_ERR_INVALID_ARG_VALUE(env, "Argument %u must be a double", index);
      return {};
    }

    *static_cast<double*>(ret) = arg->NumberValue(context).FromJust();
  } else if (type == &ffi_type_pointer) {
    if (arg->IsNullOrUndefined()) {
      *static_cast<uint64_t*>(ret) = reinterpret_cast<uint64_t>(nullptr);
    } else if (arg->IsString()) {
      // String arguments are handled in Invoke so the UTF-8 copy can be
      // freed after the call.
      return Just(FFIArgumentCategory::String);
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
        return {};
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
        return {};
      }

      *static_cast<uint64_t*>(ret) = reinterpret_cast<uint64_t>(store->Data());
    } else if (arg->IsBigInt()) {
      bool lossless;
      uint64_t pointer = arg.As<BigInt>()->Uint64Value(&lossless);
      if (!lossless || pointer > static_cast<uint64_t>(
                                     std::numeric_limits<uintptr_t>::max())) {
        THROW_ERR_INVALID_ARG_VALUE(
            env, "Argument %u must be a non-negative pointer bigint", index);
        return {};
      }

      *static_cast<uint64_t*>(ret) = pointer;
    } else {
      THROW_ERR_INVALID_ARG_VALUE(
          env,
          "Argument %u must be a buffer, an ArrayBuffer, a string, or a bigint",
          index);
      return {};
    }
  } else {
    THROW_ERR_INVALID_ARG_VALUE(
        env, "Unsupported FFI type for argument %u", index);
    return {};
  }

  return Just(FFIArgumentCategory::Regular);
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
    // Pointers surface as BigInt. Callers decode them further with the
    // ffi helpers.
    ret = BigInt::NewFromUnsigned(
        isolate, reinterpret_cast<uint64_t>(*static_cast<void**>(data)));
  } else {
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

    *static_cast<ffi_sarg*>(ret) = static_cast<ffi_sarg>(value);
  } else if (type == &ffi_type_uint32) {
    uint64_t value;

    if (!GetStrictUnsignedInteger(result, UINT32_MAX, &value)) {
      return false;
    }

    *static_cast<ffi_arg*>(ret) = static_cast<ffi_arg>(value);
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
      // Strings, Buffers, and ArrayBuffers are not accepted as pointer
      // return values from a JS callback. The slot is zeroed before the
      // false return so the caller sees a defined null pointer.
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
