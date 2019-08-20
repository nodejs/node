// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <type_traits>

#include "src/wasm/wasm-interpreter.h"

#include "src/base/overflowing-math.h"
#include "src/codegen/assembler-inl.h"
#include "src/compiler/wasm-compiler.h"
#include "src/numbers/conversions.h"
#include "src/objects/objects-inl.h"
#include "src/utils/boxed-float.h"
#include "src/utils/identity-map.h"
#include "src/utils/utils.h"
#include "src/wasm/decoder.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/memory-tracing.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/wasm-arguments.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-external-refs.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/zone/accounting-allocator.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace wasm {

using base::ReadLittleEndianValue;
using base::ReadUnalignedValue;
using base::WriteLittleEndianValue;
using base::WriteUnalignedValue;

#define TRACE(...)                                        \
  do {                                                    \
    if (FLAG_trace_wasm_interpreter) PrintF(__VA_ARGS__); \
  } while (false)

#if V8_TARGET_BIG_ENDIAN
#define LANE(i, type) ((sizeof(type.val) / sizeof(type.val[0])) - (i)-1)
#else
#define LANE(i, type) (i)
#endif

#define FOREACH_INTERNAL_OPCODE(V) V(Breakpoint, 0xFF)

#define WASM_CTYPES(V) \
  V(I32, int32_t) V(I64, int64_t) V(F32, float) V(F64, double) V(S128, Simd128)

#define FOREACH_SIMPLE_BINOP(V) \
  V(I32Add, uint32_t, +)        \
  V(I32Sub, uint32_t, -)        \
  V(I32Mul, uint32_t, *)        \
  V(I32And, uint32_t, &)        \
  V(I32Ior, uint32_t, |)        \
  V(I32Xor, uint32_t, ^)        \
  V(I32Eq, uint32_t, ==)        \
  V(I32Ne, uint32_t, !=)        \
  V(I32LtU, uint32_t, <)        \
  V(I32LeU, uint32_t, <=)       \
  V(I32GtU, uint32_t, >)        \
  V(I32GeU, uint32_t, >=)       \
  V(I32LtS, int32_t, <)         \
  V(I32LeS, int32_t, <=)        \
  V(I32GtS, int32_t, >)         \
  V(I32GeS, int32_t, >=)        \
  V(I64Add, uint64_t, +)        \
  V(I64Sub, uint64_t, -)        \
  V(I64Mul, uint64_t, *)        \
  V(I64And, uint64_t, &)        \
  V(I64Ior, uint64_t, |)        \
  V(I64Xor, uint64_t, ^)        \
  V(I64Eq, uint64_t, ==)        \
  V(I64Ne, uint64_t, !=)        \
  V(I64LtU, uint64_t, <)        \
  V(I64LeU, uint64_t, <=)       \
  V(I64GtU, uint64_t, >)        \
  V(I64GeU, uint64_t, >=)       \
  V(I64LtS, int64_t, <)         \
  V(I64LeS, int64_t, <=)        \
  V(I64GtS, int64_t, >)         \
  V(I64GeS, int64_t, >=)        \
  V(F32Add, float, +)           \
  V(F32Sub, float, -)           \
  V(F32Eq, float, ==)           \
  V(F32Ne, float, !=)           \
  V(F32Lt, float, <)            \
  V(F32Le, float, <=)           \
  V(F32Gt, float, >)            \
  V(F32Ge, float, >=)           \
  V(F64Add, double, +)          \
  V(F64Sub, double, -)          \
  V(F64Eq, double, ==)          \
  V(F64Ne, double, !=)          \
  V(F64Lt, double, <)           \
  V(F64Le, double, <=)          \
  V(F64Gt, double, >)           \
  V(F64Ge, double, >=)          \
  V(F32Mul, float, *)           \
  V(F64Mul, double, *)          \
  V(F32Div, float, /)           \
  V(F64Div, double, /)

#define FOREACH_OTHER_BINOP(V) \
  V(I32DivS, int32_t)          \
  V(I32DivU, uint32_t)         \
  V(I32RemS, int32_t)          \
  V(I32RemU, uint32_t)         \
  V(I32Shl, uint32_t)          \
  V(I32ShrU, uint32_t)         \
  V(I32ShrS, int32_t)          \
  V(I64DivS, int64_t)          \
  V(I64DivU, uint64_t)         \
  V(I64RemS, int64_t)          \
  V(I64RemU, uint64_t)         \
  V(I64Shl, uint64_t)          \
  V(I64ShrU, uint64_t)         \
  V(I64ShrS, int64_t)          \
  V(I32Ror, int32_t)           \
  V(I32Rol, int32_t)           \
  V(I64Ror, int64_t)           \
  V(I64Rol, int64_t)           \
  V(F32Min, float)             \
  V(F32Max, float)             \
  V(F64Min, double)            \
  V(F64Max, double)            \
  V(I32AsmjsDivS, int32_t)     \
  V(I32AsmjsDivU, uint32_t)    \
  V(I32AsmjsRemS, int32_t)     \
  V(I32AsmjsRemU, uint32_t)    \
  V(F32CopySign, Float32)      \
  V(F64CopySign, Float64)

#define FOREACH_I32CONV_FLOATOP(V)   \
  V(I32SConvertF32, int32_t, float)  \
  V(I32SConvertF64, int32_t, double) \
  V(I32UConvertF32, uint32_t, float) \
  V(I32UConvertF64, uint32_t, double)

#define FOREACH_OTHER_UNOP(V)    \
  V(I32Clz, uint32_t)            \
  V(I32Ctz, uint32_t)            \
  V(I32Popcnt, uint32_t)         \
  V(I32Eqz, uint32_t)            \
  V(I64Clz, uint64_t)            \
  V(I64Ctz, uint64_t)            \
  V(I64Popcnt, uint64_t)         \
  V(I64Eqz, uint64_t)            \
  V(F32Abs, Float32)             \
  V(F32Neg, Float32)             \
  V(F32Ceil, float)              \
  V(F32Floor, float)             \
  V(F32Trunc, float)             \
  V(F32NearestInt, float)        \
  V(F64Abs, Float64)             \
  V(F64Neg, Float64)             \
  V(F64Ceil, double)             \
  V(F64Floor, double)            \
  V(F64Trunc, double)            \
  V(F64NearestInt, double)       \
  V(I32ConvertI64, int64_t)      \
  V(I64SConvertF32, float)       \
  V(I64SConvertF64, double)      \
  V(I64UConvertF32, float)       \
  V(I64UConvertF64, double)      \
  V(I64SConvertI32, int32_t)     \
  V(I64UConvertI32, uint32_t)    \
  V(F32SConvertI32, int32_t)     \
  V(F32UConvertI32, uint32_t)    \
  V(F32SConvertI64, int64_t)     \
  V(F32UConvertI64, uint64_t)    \
  V(F32ConvertF64, double)       \
  V(F32ReinterpretI32, int32_t)  \
  V(F64SConvertI32, int32_t)     \
  V(F64UConvertI32, uint32_t)    \
  V(F64SConvertI64, int64_t)     \
  V(F64UConvertI64, uint64_t)    \
  V(F64ConvertF32, float)        \
  V(F64ReinterpretI64, int64_t)  \
  V(I32AsmjsSConvertF32, float)  \
  V(I32AsmjsUConvertF32, float)  \
  V(I32AsmjsSConvertF64, double) \
  V(I32AsmjsUConvertF64, double) \
  V(F32Sqrt, float)              \
  V(F64Sqrt, double)

namespace {

constexpr uint32_t kFloat32SignBitMask = uint32_t{1} << 31;
constexpr uint64_t kFloat64SignBitMask = uint64_t{1} << 63;

inline int32_t ExecuteI32DivS(int32_t a, int32_t b, TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapDivByZero;
    return 0;
  }
  if (b == -1 && a == std::numeric_limits<int32_t>::min()) {
    *trap = kTrapDivUnrepresentable;
    return 0;
  }
  return a / b;
}

inline uint32_t ExecuteI32DivU(uint32_t a, uint32_t b, TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapDivByZero;
    return 0;
  }
  return a / b;
}

inline int32_t ExecuteI32RemS(int32_t a, int32_t b, TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapRemByZero;
    return 0;
  }
  if (b == -1) return 0;
  return a % b;
}

inline uint32_t ExecuteI32RemU(uint32_t a, uint32_t b, TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapRemByZero;
    return 0;
  }
  return a % b;
}

inline uint32_t ExecuteI32Shl(uint32_t a, uint32_t b, TrapReason* trap) {
  return a << (b & 0x1F);
}

inline uint32_t ExecuteI32ShrU(uint32_t a, uint32_t b, TrapReason* trap) {
  return a >> (b & 0x1F);
}

inline int32_t ExecuteI32ShrS(int32_t a, int32_t b, TrapReason* trap) {
  return a >> (b & 0x1F);
}

inline int64_t ExecuteI64DivS(int64_t a, int64_t b, TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapDivByZero;
    return 0;
  }
  if (b == -1 && a == std::numeric_limits<int64_t>::min()) {
    *trap = kTrapDivUnrepresentable;
    return 0;
  }
  return a / b;
}

inline uint64_t ExecuteI64DivU(uint64_t a, uint64_t b, TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapDivByZero;
    return 0;
  }
  return a / b;
}

inline int64_t ExecuteI64RemS(int64_t a, int64_t b, TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapRemByZero;
    return 0;
  }
  if (b == -1) return 0;
  return a % b;
}

inline uint64_t ExecuteI64RemU(uint64_t a, uint64_t b, TrapReason* trap) {
  if (b == 0) {
    *trap = kTrapRemByZero;
    return 0;
  }
  return a % b;
}

inline uint64_t ExecuteI64Shl(uint64_t a, uint64_t b, TrapReason* trap) {
  return a << (b & 0x3F);
}

inline uint64_t ExecuteI64ShrU(uint64_t a, uint64_t b, TrapReason* trap) {
  return a >> (b & 0x3F);
}

inline int64_t ExecuteI64ShrS(int64_t a, int64_t b, TrapReason* trap) {
  return a >> (b & 0x3F);
}

inline uint32_t ExecuteI32Ror(uint32_t a, uint32_t b, TrapReason* trap) {
  return (a >> (b & 0x1F)) | (a << ((32 - b) & 0x1F));
}

inline uint32_t ExecuteI32Rol(uint32_t a, uint32_t b, TrapReason* trap) {
  return (a << (b & 0x1F)) | (a >> ((32 - b) & 0x1F));
}

inline uint64_t ExecuteI64Ror(uint64_t a, uint64_t b, TrapReason* trap) {
  return (a >> (b & 0x3F)) | (a << ((64 - b) & 0x3F));
}

inline uint64_t ExecuteI64Rol(uint64_t a, uint64_t b, TrapReason* trap) {
  return (a << (b & 0x3F)) | (a >> ((64 - b) & 0x3F));
}

inline float ExecuteF32Min(float a, float b, TrapReason* trap) {
  return JSMin(a, b);
}

inline float ExecuteF32Max(float a, float b, TrapReason* trap) {
  return JSMax(a, b);
}

inline Float32 ExecuteF32CopySign(Float32 a, Float32 b, TrapReason* trap) {
  return Float32::FromBits((a.get_bits() & ~kFloat32SignBitMask) |
                           (b.get_bits() & kFloat32SignBitMask));
}

inline double ExecuteF64Min(double a, double b, TrapReason* trap) {
  return JSMin(a, b);
}

inline double ExecuteF64Max(double a, double b, TrapReason* trap) {
  return JSMax(a, b);
}

inline Float64 ExecuteF64CopySign(Float64 a, Float64 b, TrapReason* trap) {
  return Float64::FromBits((a.get_bits() & ~kFloat64SignBitMask) |
                           (b.get_bits() & kFloat64SignBitMask));
}

inline int32_t ExecuteI32AsmjsDivS(int32_t a, int32_t b, TrapReason* trap) {
  if (b == 0) return 0;
  if (b == -1 && a == std::numeric_limits<int32_t>::min()) {
    return std::numeric_limits<int32_t>::min();
  }
  return a / b;
}

inline uint32_t ExecuteI32AsmjsDivU(uint32_t a, uint32_t b, TrapReason* trap) {
  if (b == 0) return 0;
  return a / b;
}

inline int32_t ExecuteI32AsmjsRemS(int32_t a, int32_t b, TrapReason* trap) {
  if (b == 0) return 0;
  if (b == -1) return 0;
  return a % b;
}

inline uint32_t ExecuteI32AsmjsRemU(uint32_t a, uint32_t b, TrapReason* trap) {
  if (b == 0) return 0;
  return a % b;
}

inline int32_t ExecuteI32AsmjsSConvertF32(float a, TrapReason* trap) {
  return DoubleToInt32(a);
}

inline uint32_t ExecuteI32AsmjsUConvertF32(float a, TrapReason* trap) {
  return DoubleToUint32(a);
}

inline int32_t ExecuteI32AsmjsSConvertF64(double a, TrapReason* trap) {
  return DoubleToInt32(a);
}

inline uint32_t ExecuteI32AsmjsUConvertF64(double a, TrapReason* trap) {
  return DoubleToUint32(a);
}

int32_t ExecuteI32Clz(uint32_t val, TrapReason* trap) {
  return base::bits::CountLeadingZeros(val);
}

uint32_t ExecuteI32Ctz(uint32_t val, TrapReason* trap) {
  return base::bits::CountTrailingZeros(val);
}

uint32_t ExecuteI32Popcnt(uint32_t val, TrapReason* trap) {
  return base::bits::CountPopulation(val);
}

inline uint32_t ExecuteI32Eqz(uint32_t val, TrapReason* trap) {
  return val == 0 ? 1 : 0;
}

int64_t ExecuteI64Clz(uint64_t val, TrapReason* trap) {
  return base::bits::CountLeadingZeros(val);
}

inline uint64_t ExecuteI64Ctz(uint64_t val, TrapReason* trap) {
  return base::bits::CountTrailingZeros(val);
}

inline int64_t ExecuteI64Popcnt(uint64_t val, TrapReason* trap) {
  return base::bits::CountPopulation(val);
}

inline int32_t ExecuteI64Eqz(uint64_t val, TrapReason* trap) {
  return val == 0 ? 1 : 0;
}

inline Float32 ExecuteF32Abs(Float32 a, TrapReason* trap) {
  return Float32::FromBits(a.get_bits() & ~kFloat32SignBitMask);
}

inline Float32 ExecuteF32Neg(Float32 a, TrapReason* trap) {
  return Float32::FromBits(a.get_bits() ^ kFloat32SignBitMask);
}

inline float ExecuteF32Ceil(float a, TrapReason* trap) { return ceilf(a); }

inline float ExecuteF32Floor(float a, TrapReason* trap) { return floorf(a); }

inline float ExecuteF32Trunc(float a, TrapReason* trap) { return truncf(a); }

inline float ExecuteF32NearestInt(float a, TrapReason* trap) {
  return nearbyintf(a);
}

inline float ExecuteF32Sqrt(float a, TrapReason* trap) {
  float result = sqrtf(a);
  return result;
}

inline Float64 ExecuteF64Abs(Float64 a, TrapReason* trap) {
  return Float64::FromBits(a.get_bits() & ~kFloat64SignBitMask);
}

inline Float64 ExecuteF64Neg(Float64 a, TrapReason* trap) {
  return Float64::FromBits(a.get_bits() ^ kFloat64SignBitMask);
}

inline double ExecuteF64Ceil(double a, TrapReason* trap) { return ceil(a); }

inline double ExecuteF64Floor(double a, TrapReason* trap) { return floor(a); }

inline double ExecuteF64Trunc(double a, TrapReason* trap) { return trunc(a); }

inline double ExecuteF64NearestInt(double a, TrapReason* trap) {
  return nearbyint(a);
}

inline double ExecuteF64Sqrt(double a, TrapReason* trap) { return sqrt(a); }

template <typename int_type, typename float_type>
int_type ExecuteConvert(float_type a, TrapReason* trap) {
  if (is_inbounds<int_type>(a)) {
    return static_cast<int_type>(a);
  }
  *trap = kTrapFloatUnrepresentable;
  return 0;
}

template <typename int_type, typename float_type>
int_type ExecuteConvertSaturate(float_type a) {
  TrapReason base_trap = kTrapCount;
  int32_t val = ExecuteConvert<int_type>(a, &base_trap);
  if (base_trap == kTrapCount) {
    return val;
  }
  return std::isnan(a) ? 0
                       : (a < static_cast<float_type>(0.0)
                              ? std::numeric_limits<int_type>::min()
                              : std::numeric_limits<int_type>::max());
}

template <typename dst_type, typename src_type, void (*fn)(Address)>
inline dst_type CallExternalIntToFloatFunction(src_type input) {
  uint8_t data[std::max(sizeof(dst_type), sizeof(src_type))];
  Address data_addr = reinterpret_cast<Address>(data);
  WriteUnalignedValue<src_type>(data_addr, input);
  fn(data_addr);
  return ReadUnalignedValue<dst_type>(data_addr);
}

template <typename dst_type, typename src_type, int32_t (*fn)(Address)>
inline dst_type CallExternalFloatToIntFunction(src_type input,
                                               TrapReason* trap) {
  uint8_t data[std::max(sizeof(dst_type), sizeof(src_type))];
  Address data_addr = reinterpret_cast<Address>(data);
  WriteUnalignedValue<src_type>(data_addr, input);
  if (!fn(data_addr)) *trap = kTrapFloatUnrepresentable;
  return ReadUnalignedValue<dst_type>(data_addr);
}

inline uint32_t ExecuteI32ConvertI64(int64_t a, TrapReason* trap) {
  return static_cast<uint32_t>(a & 0xFFFFFFFF);
}

int64_t ExecuteI64SConvertF32(float a, TrapReason* trap) {
  return CallExternalFloatToIntFunction<int64_t, float,
                                        float32_to_int64_wrapper>(a, trap);
}

int64_t ExecuteI64SConvertSatF32(float a) {
  TrapReason base_trap = kTrapCount;
  int64_t val = ExecuteI64SConvertF32(a, &base_trap);
  if (base_trap == kTrapCount) {
    return val;
  }
  return std::isnan(a) ? 0
                       : (a < 0.0 ? std::numeric_limits<int64_t>::min()
                                  : std::numeric_limits<int64_t>::max());
}

int64_t ExecuteI64SConvertF64(double a, TrapReason* trap) {
  return CallExternalFloatToIntFunction<int64_t, double,
                                        float64_to_int64_wrapper>(a, trap);
}

int64_t ExecuteI64SConvertSatF64(double a) {
  TrapReason base_trap = kTrapCount;
  int64_t val = ExecuteI64SConvertF64(a, &base_trap);
  if (base_trap == kTrapCount) {
    return val;
  }
  return std::isnan(a) ? 0
                       : (a < 0.0 ? std::numeric_limits<int64_t>::min()
                                  : std::numeric_limits<int64_t>::max());
}

uint64_t ExecuteI64UConvertF32(float a, TrapReason* trap) {
  return CallExternalFloatToIntFunction<uint64_t, float,
                                        float32_to_uint64_wrapper>(a, trap);
}

uint64_t ExecuteI64UConvertSatF32(float a) {
  TrapReason base_trap = kTrapCount;
  uint64_t val = ExecuteI64UConvertF32(a, &base_trap);
  if (base_trap == kTrapCount) {
    return val;
  }
  return std::isnan(a) ? 0
                       : (a < 0.0 ? std::numeric_limits<uint64_t>::min()
                                  : std::numeric_limits<uint64_t>::max());
}

uint64_t ExecuteI64UConvertF64(double a, TrapReason* trap) {
  return CallExternalFloatToIntFunction<uint64_t, double,
                                        float64_to_uint64_wrapper>(a, trap);
}

uint64_t ExecuteI64UConvertSatF64(double a) {
  TrapReason base_trap = kTrapCount;
  int64_t val = ExecuteI64UConvertF64(a, &base_trap);
  if (base_trap == kTrapCount) {
    return val;
  }
  return std::isnan(a) ? 0
                       : (a < 0.0 ? std::numeric_limits<uint64_t>::min()
                                  : std::numeric_limits<uint64_t>::max());
}

inline int64_t ExecuteI64SConvertI32(int32_t a, TrapReason* trap) {
  return static_cast<int64_t>(a);
}

inline int64_t ExecuteI64UConvertI32(uint32_t a, TrapReason* trap) {
  return static_cast<uint64_t>(a);
}

inline float ExecuteF32SConvertI32(int32_t a, TrapReason* trap) {
  return static_cast<float>(a);
}

inline float ExecuteF32UConvertI32(uint32_t a, TrapReason* trap) {
  return static_cast<float>(a);
}

inline float ExecuteF32SConvertI64(int64_t a, TrapReason* trap) {
  return static_cast<float>(a);
}

inline float ExecuteF32UConvertI64(uint64_t a, TrapReason* trap) {
  return CallExternalIntToFloatFunction<float, uint64_t,
                                        uint64_to_float32_wrapper>(a);
}

inline float ExecuteF32ConvertF64(double a, TrapReason* trap) {
  return DoubleToFloat32(a);
}

inline Float32 ExecuteF32ReinterpretI32(int32_t a, TrapReason* trap) {
  return Float32::FromBits(a);
}

inline double ExecuteF64SConvertI32(int32_t a, TrapReason* trap) {
  return static_cast<double>(a);
}

inline double ExecuteF64UConvertI32(uint32_t a, TrapReason* trap) {
  return static_cast<double>(a);
}

inline double ExecuteF64SConvertI64(int64_t a, TrapReason* trap) {
  return static_cast<double>(a);
}

inline double ExecuteF64UConvertI64(uint64_t a, TrapReason* trap) {
  return CallExternalIntToFloatFunction<double, uint64_t,
                                        uint64_to_float64_wrapper>(a);
}

inline double ExecuteF64ConvertF32(float a, TrapReason* trap) {
  return static_cast<double>(a);
}

inline Float64 ExecuteF64ReinterpretI64(int64_t a, TrapReason* trap) {
  return Float64::FromBits(a);
}

inline int32_t ExecuteI32ReinterpretF32(WasmValue a) {
  return a.to_f32_boxed().get_bits();
}

inline int64_t ExecuteI64ReinterpretF64(WasmValue a) {
  return a.to_f64_boxed().get_bits();
}

enum InternalOpcode {
#define DECL_INTERNAL_ENUM(name, value) kInternal##name = value,
  FOREACH_INTERNAL_OPCODE(DECL_INTERNAL_ENUM)
#undef DECL_INTERNAL_ENUM
};

const char* OpcodeName(uint32_t val) {
  switch (val) {
#define DECL_INTERNAL_CASE(name, value) \
  case kInternal##name:                 \
    return "Internal" #name;
    FOREACH_INTERNAL_OPCODE(DECL_INTERNAL_CASE)
#undef DECL_INTERNAL_CASE
  }
  return WasmOpcodes::OpcodeName(static_cast<WasmOpcode>(val));
}

constexpr uint32_t kCatchInArity = 1;

}  // namespace

class SideTable;

// Code and metadata needed to execute a function.
struct InterpreterCode {
  const WasmFunction* function;  // wasm function
  BodyLocalDecls locals;         // local declarations
  const byte* orig_start;        // start of original code
  const byte* orig_end;          // end of original code
  byte* start;                   // start of (maybe altered) code
  byte* end;                     // end of (maybe altered) code
  SideTable* side_table;         // precomputed side table for control flow.

  const byte* at(pc_t pc) { return start + pc; }
};

// A helper class to compute the control transfers for each bytecode offset.
// Control transfers allow Br, BrIf, BrTable, If, Else, and End bytecodes to
// be directly executed without the need to dynamically track blocks.
class SideTable : public ZoneObject {
 public:
  ControlTransferMap map_;
  uint32_t max_stack_height_ = 0;

  SideTable(Zone* zone, const WasmModule* module, InterpreterCode* code)
      : map_(zone) {
    // Create a zone for all temporary objects.
    Zone control_transfer_zone(zone->allocator(), ZONE_NAME);

    // Represents a control flow label.
    class CLabel : public ZoneObject {
      explicit CLabel(Zone* zone, uint32_t target_stack_height, uint32_t arity)
          : target_stack_height(target_stack_height),
            arity(arity),
            refs(zone) {}

     public:
      struct Ref {
        const byte* from_pc;
        const uint32_t stack_height;
      };
      const byte* target = nullptr;
      uint32_t target_stack_height;
      // Arity when branching to this label.
      const uint32_t arity;
      ZoneVector<Ref> refs;

      static CLabel* New(Zone* zone, uint32_t stack_height, uint32_t arity) {
        return new (zone) CLabel(zone, stack_height, arity);
      }

      // Bind this label to the given PC.
      void Bind(const byte* pc) {
        DCHECK_NULL(target);
        target = pc;
      }

      // Reference this label from the given location.
      void Ref(const byte* from_pc, uint32_t stack_height) {
        // Target being bound before a reference means this is a loop.
        DCHECK_IMPLIES(target, *target == kExprLoop);
        refs.push_back({from_pc, stack_height});
      }

      void Finish(ControlTransferMap* map, const byte* start) {
        DCHECK_NOT_NULL(target);
        for (auto ref : refs) {
          size_t offset = static_cast<size_t>(ref.from_pc - start);
          auto pcdiff = static_cast<pcdiff_t>(target - ref.from_pc);
          DCHECK_GE(ref.stack_height, target_stack_height);
          spdiff_t spdiff =
              static_cast<spdiff_t>(ref.stack_height - target_stack_height);
          TRACE("control transfer @%zu: Δpc %d, stack %u->%u = -%u\n", offset,
                pcdiff, ref.stack_height, target_stack_height, spdiff);
          ControlTransferEntry& entry = (*map)[offset];
          entry.pc_diff = pcdiff;
          entry.sp_diff = spdiff;
          entry.target_arity = arity;
        }
      }
    };

    // An entry in the control stack.
    struct Control {
      const byte* pc;
      CLabel* end_label;
      CLabel* else_label;
      // Arity (number of values on the stack) when exiting this control
      // structure via |end|.
      uint32_t exit_arity;
      // Track whether this block was already left, i.e. all further
      // instructions are unreachable.
      bool unreachable = false;

      Control(const byte* pc, CLabel* end_label, CLabel* else_label,
              uint32_t exit_arity)
          : pc(pc),
            end_label(end_label),
            else_label(else_label),
            exit_arity(exit_arity) {}
      Control(const byte* pc, CLabel* end_label, uint32_t exit_arity)
          : Control(pc, end_label, nullptr, exit_arity) {}

      void Finish(ControlTransferMap* map, const byte* start) {
        end_label->Finish(map, start);
        if (else_label) else_label->Finish(map, start);
      }
    };

    // Compute the ControlTransfer map.
    // This algorithm maintains a stack of control constructs similar to the
    // AST decoder. The {control_stack} allows matching {br,br_if,br_table}
    // bytecodes with their target, as well as determining whether the current
    // bytecodes are within the true or false block of an else.
    ZoneVector<Control> control_stack(&control_transfer_zone);
    // It also maintains a stack of all nested {try} blocks to resolve local
    // handler targets for potentially throwing operations. These exceptional
    // control transfers are treated just like other branches in the resulting
    // map. This stack contains indices into the above control stack.
    ZoneVector<size_t> exception_stack(zone);
    uint32_t stack_height = 0;
    uint32_t func_arity =
        static_cast<uint32_t>(code->function->sig->return_count());
    CLabel* func_label =
        CLabel::New(&control_transfer_zone, stack_height, func_arity);
    control_stack.emplace_back(code->orig_start, func_label, func_arity);
    auto control_parent = [&]() -> Control& {
      DCHECK_LE(2, control_stack.size());
      return control_stack[control_stack.size() - 2];
    };
    auto copy_unreachable = [&] {
      control_stack.back().unreachable = control_parent().unreachable;
    };
    for (BytecodeIterator i(code->orig_start, code->orig_end, &code->locals);
         i.has_next(); i.next()) {
      WasmOpcode opcode = i.current();
      uint32_t exceptional_stack_height = 0;
      if (WasmOpcodes::IsPrefixOpcode(opcode)) opcode = i.prefixed_opcode();
      bool unreachable = control_stack.back().unreachable;
      if (unreachable) {
        TRACE("@%u: %s (is unreachable)\n", i.pc_offset(),
              WasmOpcodes::OpcodeName(opcode));
      } else {
        auto stack_effect =
            StackEffect(module, code->function->sig, i.pc(), i.end());
        TRACE("@%u: %s (sp %d - %d + %d)\n", i.pc_offset(),
              WasmOpcodes::OpcodeName(opcode), stack_height, stack_effect.first,
              stack_effect.second);
        DCHECK_GE(stack_height, stack_effect.first);
        DCHECK_GE(kMaxUInt32, static_cast<uint64_t>(stack_height) -
                                  stack_effect.first + stack_effect.second);
        exceptional_stack_height = stack_height - stack_effect.first;
        stack_height = stack_height - stack_effect.first + stack_effect.second;
        if (stack_height > max_stack_height_) max_stack_height_ = stack_height;
      }
      if (!exception_stack.empty() && WasmOpcodes::IsThrowingOpcode(opcode)) {
        // Record exceptional control flow from potentially throwing opcodes to
        // the local handler if one is present. The stack height at the throw
        // point is assumed to have popped all operands and not pushed any yet.
        DCHECK_GE(control_stack.size() - 1, exception_stack.back());
        const Control* c = &control_stack[exception_stack.back()];
        if (!unreachable) c->else_label->Ref(i.pc(), exceptional_stack_height);
        if (exceptional_stack_height + kCatchInArity > max_stack_height_) {
          max_stack_height_ = exceptional_stack_height + kCatchInArity;
        }
        TRACE("handler @%u: %s -> try @%u\n", i.pc_offset(), OpcodeName(opcode),
              static_cast<uint32_t>(c->pc - code->start));
      }
      switch (opcode) {
        case kExprBlock:
        case kExprLoop: {
          bool is_loop = opcode == kExprLoop;
          BlockTypeImmediate<Decoder::kNoValidate> imm(kAllWasmFeatures, &i,
                                                       i.pc());
          if (imm.type == kWasmBottom) {
            imm.sig = module->signatures[imm.sig_index];
          }
          TRACE("control @%u: %s, arity %d->%d\n", i.pc_offset(),
                is_loop ? "Loop" : "Block", imm.in_arity(), imm.out_arity());
          CLabel* label =
              CLabel::New(&control_transfer_zone, stack_height,
                          is_loop ? imm.in_arity() : imm.out_arity());
          control_stack.emplace_back(i.pc(), label, imm.out_arity());
          copy_unreachable();
          if (is_loop) label->Bind(i.pc());
          break;
        }
        case kExprIf: {
          BlockTypeImmediate<Decoder::kNoValidate> imm(kAllWasmFeatures, &i,
                                                       i.pc());
          if (imm.type == kWasmBottom) {
            imm.sig = module->signatures[imm.sig_index];
          }
          TRACE("control @%u: If, arity %d->%d\n", i.pc_offset(),
                imm.in_arity(), imm.out_arity());
          CLabel* end_label = CLabel::New(&control_transfer_zone, stack_height,
                                          imm.out_arity());
          CLabel* else_label =
              CLabel::New(&control_transfer_zone, stack_height, 0);
          control_stack.emplace_back(i.pc(), end_label, else_label,
                                     imm.out_arity());
          copy_unreachable();
          if (!unreachable) else_label->Ref(i.pc(), stack_height);
          break;
        }
        case kExprElse: {
          Control* c = &control_stack.back();
          copy_unreachable();
          TRACE("control @%u: Else\n", i.pc_offset());
          if (!control_parent().unreachable) {
            c->end_label->Ref(i.pc(), stack_height);
          }
          DCHECK_NOT_NULL(c->else_label);
          c->else_label->Bind(i.pc() + 1);
          c->else_label->Finish(&map_, code->orig_start);
          c->else_label = nullptr;
          DCHECK_GE(stack_height, c->end_label->target_stack_height);
          stack_height = c->end_label->target_stack_height;
          break;
        }
        case kExprTry: {
          BlockTypeImmediate<Decoder::kNoValidate> imm(kAllWasmFeatures, &i,
                                                       i.pc());
          if (imm.type == kWasmBottom) {
            imm.sig = module->signatures[imm.sig_index];
          }
          TRACE("control @%u: Try, arity %d->%d\n", i.pc_offset(),
                imm.in_arity(), imm.out_arity());
          CLabel* end_label = CLabel::New(&control_transfer_zone, stack_height,
                                          imm.out_arity());
          CLabel* catch_label =
              CLabel::New(&control_transfer_zone, stack_height, kCatchInArity);
          control_stack.emplace_back(i.pc(), end_label, catch_label,
                                     imm.out_arity());
          exception_stack.push_back(control_stack.size() - 1);
          copy_unreachable();
          break;
        }
        case kExprCatch: {
          DCHECK_EQ(control_stack.size() - 1, exception_stack.back());
          Control* c = &control_stack.back();
          exception_stack.pop_back();
          copy_unreachable();
          TRACE("control @%u: Catch\n", i.pc_offset());
          if (!control_parent().unreachable) {
            c->end_label->Ref(i.pc(), stack_height);
          }
          DCHECK_NOT_NULL(c->else_label);
          c->else_label->Bind(i.pc() + 1);
          c->else_label->Finish(&map_, code->orig_start);
          c->else_label = nullptr;
          DCHECK_GE(stack_height, c->end_label->target_stack_height);
          stack_height = c->end_label->target_stack_height + kCatchInArity;
          break;
        }
        case kExprBrOnExn: {
          BranchOnExceptionImmediate<Decoder::kNoValidate> imm(&i, i.pc());
          uint32_t depth = imm.depth.depth;  // Extracted for convenience.
          imm.index.exception = &module->exceptions[imm.index.index];
          DCHECK_EQ(0, imm.index.exception->sig->return_count());
          size_t params = imm.index.exception->sig->parameter_count();
          // Taken branches pop the exception and push the encoded values.
          uint32_t height = stack_height - 1 + static_cast<uint32_t>(params);
          TRACE("control @%u: BrOnExn[depth=%u]\n", i.pc_offset(), depth);
          Control* c = &control_stack[control_stack.size() - depth - 1];
          if (!unreachable) c->end_label->Ref(i.pc(), height);
          break;
        }
        case kExprEnd: {
          Control* c = &control_stack.back();
          TRACE("control @%u: End\n", i.pc_offset());
          // Only loops have bound labels.
          DCHECK_IMPLIES(c->end_label->target, *c->pc == kExprLoop);
          if (!c->end_label->target) {
            if (c->else_label) c->else_label->Bind(i.pc());
            c->end_label->Bind(i.pc() + 1);
          }
          c->Finish(&map_, code->orig_start);
          DCHECK_GE(stack_height, c->end_label->target_stack_height);
          stack_height = c->end_label->target_stack_height + c->exit_arity;
          control_stack.pop_back();
          break;
        }
        case kExprBr: {
          BranchDepthImmediate<Decoder::kNoValidate> imm(&i, i.pc());
          TRACE("control @%u: Br[depth=%u]\n", i.pc_offset(), imm.depth);
          Control* c = &control_stack[control_stack.size() - imm.depth - 1];
          if (!unreachable) c->end_label->Ref(i.pc(), stack_height);
          break;
        }
        case kExprBrIf: {
          BranchDepthImmediate<Decoder::kNoValidate> imm(&i, i.pc());
          TRACE("control @%u: BrIf[depth=%u]\n", i.pc_offset(), imm.depth);
          Control* c = &control_stack[control_stack.size() - imm.depth - 1];
          if (!unreachable) c->end_label->Ref(i.pc(), stack_height);
          break;
        }
        case kExprBrTable: {
          BranchTableImmediate<Decoder::kNoValidate> imm(&i, i.pc());
          BranchTableIterator<Decoder::kNoValidate> iterator(&i, imm);
          TRACE("control @%u: BrTable[count=%u]\n", i.pc_offset(),
                imm.table_count);
          if (!unreachable) {
            while (iterator.has_next()) {
              uint32_t j = iterator.cur_index();
              uint32_t target = iterator.next();
              Control* c = &control_stack[control_stack.size() - target - 1];
              c->end_label->Ref(i.pc() + j, stack_height);
            }
          }
          break;
        }
        default:
          break;
      }
      if (WasmOpcodes::IsUnconditionalJump(opcode)) {
        control_stack.back().unreachable = true;
      }
    }
    DCHECK_EQ(0, control_stack.size());
    DCHECK_EQ(func_arity, stack_height);
  }

  bool HasEntryAt(pc_t from) {
    auto result = map_.find(from);
    return result != map_.end();
  }

  ControlTransferEntry& Lookup(pc_t from) {
    auto result = map_.find(from);
    DCHECK(result != map_.end());
    return result->second;
  }
};

// The main storage for interpreter code. It maps {WasmFunction} to the
// metadata needed to execute each function.
class CodeMap {
  Zone* zone_;
  const WasmModule* module_;
  ZoneVector<InterpreterCode> interpreter_code_;

 public:
  CodeMap(const WasmModule* module, const uint8_t* module_start, Zone* zone)
      : zone_(zone), module_(module), interpreter_code_(zone) {
    if (module == nullptr) return;
    interpreter_code_.reserve(module->functions.size());
    for (const WasmFunction& function : module->functions) {
      if (function.imported) {
        DCHECK(!function.code.is_set());
        AddFunction(&function, nullptr, nullptr);
      } else {
        AddFunction(&function, module_start + function.code.offset(),
                    module_start + function.code.end_offset());
      }
    }
  }

  const WasmModule* module() const { return module_; }

  InterpreterCode* GetCode(const WasmFunction* function) {
    InterpreterCode* code = GetCode(function->func_index);
    DCHECK_EQ(function, code->function);
    return code;
  }

  InterpreterCode* GetCode(uint32_t function_index) {
    DCHECK_LT(function_index, interpreter_code_.size());
    return Preprocess(&interpreter_code_[function_index]);
  }

  InterpreterCode* Preprocess(InterpreterCode* code) {
    DCHECK_EQ(code->function->imported, code->start == nullptr);
    if (!code->side_table && code->start) {
      // Compute the control targets map and the local declarations.
      code->side_table = new (zone_) SideTable(zone_, module_, code);
    }
    return code;
  }

  void AddFunction(const WasmFunction* function, const byte* code_start,
                   const byte* code_end) {
    InterpreterCode code = {
        function, BodyLocalDecls(zone_),         code_start,
        code_end, const_cast<byte*>(code_start), const_cast<byte*>(code_end),
        nullptr};

    DCHECK_EQ(interpreter_code_.size(), function->func_index);
    interpreter_code_.push_back(code);
  }

  void SetFunctionCode(const WasmFunction* function, const byte* start,
                       const byte* end) {
    DCHECK_LT(function->func_index, interpreter_code_.size());
    InterpreterCode* code = &interpreter_code_[function->func_index];
    DCHECK_EQ(function, code->function);
    code->orig_start = start;
    code->orig_end = end;
    code->start = const_cast<byte*>(start);
    code->end = const_cast<byte*>(end);
    code->side_table = nullptr;
    Preprocess(code);
  }
};

namespace {

struct ExternalCallResult {
  enum Type {
    // The function should be executed inside this interpreter.
    INTERNAL,
    // For indirect calls: Table or function does not exist.
    INVALID_FUNC,
    // For indirect calls: Signature does not match expected signature.
    SIGNATURE_MISMATCH,
    // The function was executed and returned normally.
    EXTERNAL_RETURNED,
    // The function was executed, threw an exception, and the stack was unwound.
    EXTERNAL_UNWOUND,
    // The function was executed and threw an exception that was locally caught.
    EXTERNAL_CAUGHT
  };
  Type type;
  // If type is INTERNAL, this field holds the function to call internally.
  InterpreterCode* interpreter_code;

  ExternalCallResult(Type type) : type(type) {  // NOLINT
    DCHECK_NE(INTERNAL, type);
  }
  ExternalCallResult(Type type, InterpreterCode* code)
      : type(type), interpreter_code(code) {
    DCHECK_EQ(INTERNAL, type);
  }
};

// Like a static_cast from src to dst, but specialized for boxed floats.
template <typename dst, typename src>
struct converter {
  dst operator()(src val) const { return static_cast<dst>(val); }
};
template <>
struct converter<Float64, uint64_t> {
  Float64 operator()(uint64_t val) const { return Float64::FromBits(val); }
};
template <>
struct converter<Float32, uint32_t> {
  Float32 operator()(uint32_t val) const { return Float32::FromBits(val); }
};
template <>
struct converter<uint64_t, Float64> {
  uint64_t operator()(Float64 val) const { return val.get_bits(); }
};
template <>
struct converter<uint32_t, Float32> {
  uint32_t operator()(Float32 val) const { return val.get_bits(); }
};

template <typename T>
V8_INLINE bool has_nondeterminism(T val) {
  static_assert(!std::is_floating_point<T>::value, "missing specialization");
  return false;
}
template <>
V8_INLINE bool has_nondeterminism<float>(float val) {
  return std::isnan(val);
}
template <>
V8_INLINE bool has_nondeterminism<double>(double val) {
  return std::isnan(val);
}

}  // namespace

// Responsible for executing code directly.
class ThreadImpl {
  struct Activation {
    uint32_t fp;
    sp_t sp;
    Activation(uint32_t fp, sp_t sp) : fp(fp), sp(sp) {}
  };

 public:
  ThreadImpl(Zone* zone, CodeMap* codemap,
             Handle<WasmInstanceObject> instance_object,
             Handle<Cell> reference_stack_cell)
      : codemap_(codemap),
        isolate_(instance_object->GetIsolate()),
        instance_object_(instance_object),
        reference_stack_cell_(reference_stack_cell),
        frames_(zone),
        activations_(zone) {}

  //==========================================================================
  // Implementation of public interface for WasmInterpreter::Thread.
  //==========================================================================

  WasmInterpreter::State state() { return state_; }

  void InitFrame(const WasmFunction* function, WasmValue* args) {
    DCHECK_EQ(current_activation().fp, frames_.size());
    InterpreterCode* code = codemap()->GetCode(function);
    size_t num_params = function->sig->parameter_count();
    EnsureStackSpace(num_params);
    Push(args, num_params);
    PushFrame(code);
  }

  WasmInterpreter::State Run(int num_steps = -1) {
    DCHECK(state_ == WasmInterpreter::STOPPED ||
           state_ == WasmInterpreter::PAUSED);
    DCHECK(num_steps == -1 || num_steps > 0);
    if (num_steps == -1) {
      TRACE("  => Run()\n");
    } else if (num_steps == 1) {
      TRACE("  => Step()\n");
    } else {
      TRACE("  => Run(%d)\n", num_steps);
    }
    state_ = WasmInterpreter::RUNNING;
    Execute(frames_.back().code, frames_.back().pc, num_steps);
    // If state_ is STOPPED, the current activation must be fully unwound.
    DCHECK_IMPLIES(state_ == WasmInterpreter::STOPPED,
                   current_activation().fp == frames_.size());
    return state_;
  }

  void Pause() { UNIMPLEMENTED(); }

  void Reset() {
    TRACE("----- RESET -----\n");
    ResetStack(0);
    frames_.clear();
    state_ = WasmInterpreter::STOPPED;
    trap_reason_ = kTrapCount;
    possible_nondeterminism_ = false;
  }

  int GetFrameCount() {
    DCHECK_GE(kMaxInt, frames_.size());
    return static_cast<int>(frames_.size());
  }

  WasmValue GetReturnValue(uint32_t index) {
    if (state_ == WasmInterpreter::TRAPPED) return WasmValue(0xDEADBEEF);
    DCHECK_EQ(WasmInterpreter::FINISHED, state_);
    Activation act = current_activation();
    // Current activation must be finished.
    DCHECK_EQ(act.fp, frames_.size());
    return GetStackValue(act.sp + index);
  }

  WasmValue GetStackValue(sp_t index) {
    DCHECK_GT(StackHeight(), index);
    return stack_[index].ExtractValue(this, index);
  }

  void SetStackValue(sp_t index, WasmValue value) {
    DCHECK_GT(StackHeight(), index);
    stack_[index] = StackValue(value, this, index);
  }

  TrapReason GetTrapReason() { return trap_reason_; }

  pc_t GetBreakpointPc() { return break_pc_; }

  bool PossibleNondeterminism() { return possible_nondeterminism_; }

  uint64_t NumInterpretedCalls() { return num_interpreted_calls_; }

  void AddBreakFlags(uint8_t flags) { break_flags_ |= flags; }

  void ClearBreakFlags() { break_flags_ = WasmInterpreter::BreakFlag::None; }

  Handle<Cell> reference_stack_cell() const { return reference_stack_cell_; }

  uint32_t NumActivations() {
    return static_cast<uint32_t>(activations_.size());
  }

  uint32_t StartActivation() {
    TRACE("----- START ACTIVATION %zu -----\n", activations_.size());
    // If you use activations, use them consistently:
    DCHECK_IMPLIES(activations_.empty(), frames_.empty());
    DCHECK_IMPLIES(activations_.empty(), StackHeight() == 0);
    uint32_t activation_id = static_cast<uint32_t>(activations_.size());
    activations_.emplace_back(static_cast<uint32_t>(frames_.size()),
                              StackHeight());
    state_ = WasmInterpreter::STOPPED;
    return activation_id;
  }

  void FinishActivation(uint32_t id) {
    TRACE("----- FINISH ACTIVATION %zu -----\n", activations_.size() - 1);
    DCHECK_LT(0, activations_.size());
    DCHECK_EQ(activations_.size() - 1, id);
    // Stack height must match the start of this activation (otherwise unwind
    // first).
    DCHECK_EQ(activations_.back().fp, frames_.size());
    DCHECK_LE(activations_.back().sp, StackHeight());
    ResetStack(activations_.back().sp);
    activations_.pop_back();
  }

  uint32_t ActivationFrameBase(uint32_t id) {
    DCHECK_GT(activations_.size(), id);
    return activations_[id].fp;
  }

  WasmInterpreter::Thread::ExceptionHandlingResult RaiseException(
      Isolate* isolate, Handle<Object> exception) {
    DCHECK_EQ(WasmInterpreter::TRAPPED, state_);
    isolate->Throw(*exception);  // Will check that none is pending.
    if (HandleException(isolate) == WasmInterpreter::Thread::UNWOUND) {
      DCHECK_EQ(WasmInterpreter::STOPPED, state_);
      return WasmInterpreter::Thread::UNWOUND;
    }
    state_ = WasmInterpreter::PAUSED;
    return WasmInterpreter::Thread::HANDLED;
  }

  uint32_t GetGlobalCount() {
    return static_cast<uint32_t>(module()->globals.size());
  }

  WasmValue GetGlobalValue(uint32_t index) {
    const WasmGlobal* global = &module()->globals[index];
    switch (global->type) {
#define CASE_TYPE(wasm, ctype)                                         \
  case kWasm##wasm: {                                                  \
    byte* ptr = GetGlobalPtr(global);                                  \
    return WasmValue(                                                  \
        ReadLittleEndianValue<ctype>(reinterpret_cast<Address>(ptr))); \
    break;                                                             \
  }
      WASM_CTYPES(CASE_TYPE)
#undef CASE_TYPE
      case kWasmAnyRef:
      case kWasmFuncRef:
      case kWasmExnRef: {
        HandleScope handle_scope(isolate_);  // Avoid leaking handles.
        Handle<FixedArray> global_buffer;    // The buffer of the global.
        uint32_t global_index = 0;           // The index into the buffer.
        GetGlobalBufferAndIndex(global, &global_buffer, &global_index);
        Handle<Object> value(global_buffer->get(global_index), isolate_);
        return WasmValue(handle_scope.CloseAndEscape(value));
      }
      default:
        UNREACHABLE();
    }
  }

 private:
  // Handle a thrown exception. Returns whether the exception was handled inside
  // the current activation. Unwinds the interpreted stack accordingly.
  WasmInterpreter::Thread::ExceptionHandlingResult HandleException(
      Isolate* isolate) {
    DCHECK(isolate->has_pending_exception());
    DCHECK_LT(0, activations_.size());
    Activation& act = activations_.back();
    while (frames_.size() > act.fp) {
      Frame& frame = frames_.back();
      InterpreterCode* code = frame.code;
      if (code->side_table->HasEntryAt(frame.pc)) {
        TRACE("----- HANDLE -----\n");
        Push(WasmValue(handle(isolate->pending_exception(), isolate)));
        isolate->clear_pending_exception();
        frame.pc += JumpToHandlerDelta(code, frame.pc);
        TRACE("  => handler #%zu (#%u @%zu)\n", frames_.size() - 1,
              code->function->func_index, frame.pc);
        return WasmInterpreter::Thread::HANDLED;
      }
      TRACE("  => drop frame #%zu (#%u @%zu)\n", frames_.size() - 1,
            code->function->func_index, frame.pc);
      ResetStack(frame.sp);
      frames_.pop_back();
    }
    TRACE("----- UNWIND -----\n");
    DCHECK_EQ(act.fp, frames_.size());
    DCHECK_EQ(act.sp, StackHeight());
    state_ = WasmInterpreter::STOPPED;
    return WasmInterpreter::Thread::UNWOUND;
  }

  // Entries on the stack of functions being evaluated.
  struct Frame {
    InterpreterCode* code;
    pc_t pc;
    sp_t sp;

    // Limit of parameters.
    sp_t plimit() { return sp + code->function->sig->parameter_count(); }
    // Limit of locals.
    sp_t llimit() { return plimit() + code->locals.type_list.size(); }
  };

  // Safety wrapper for values on the operand stack represented as {WasmValue}.
  // Most values are stored directly on the stack, only reference values are
  // kept in a separate on-heap reference stack to make the GC trace them.
  // TODO(mstarzinger): Optimize simple stack operations (like "get_local",
  // "set_local", and "tee_local") so that they don't require a handle scope.
  // TODO(mstarzinger): Consider optimizing activations that use no reference
  // values to avoid allocating the reference stack entirely.
  class StackValue {
   public:
    StackValue() = default;  // Only needed for resizing the stack.
    StackValue(WasmValue v, ThreadImpl* thread, sp_t index) : value_(v) {
      if (IsReferenceValue()) {
        value_ = WasmValue(Handle<Object>::null());
        int ref_index = static_cast<int>(index);
        thread->reference_stack().set(ref_index, *v.to_anyref());
      }
    }

    WasmValue ExtractValue(ThreadImpl* thread, sp_t index) {
      if (!IsReferenceValue()) return value_;
      DCHECK(value_.to_anyref().is_null());
      int ref_index = static_cast<int>(index);
      Isolate* isolate = thread->isolate_;
      Handle<Object> ref(thread->reference_stack().get(ref_index), isolate);
      DCHECK(!ref->IsTheHole(isolate));
      return WasmValue(ref);
    }

    bool IsReferenceValue() const { return value_.type() == kWasmAnyRef; }

    void ClearValue(ThreadImpl* thread, sp_t index) {
      if (!IsReferenceValue()) return;
      int ref_index = static_cast<int>(index);
      Isolate* isolate = thread->isolate_;
      thread->reference_stack().set_the_hole(isolate, ref_index);
    }

    static void ClearValues(ThreadImpl* thread, sp_t index, int count) {
      int ref_index = static_cast<int>(index);
      thread->reference_stack().FillWithHoles(ref_index, ref_index + count);
    }

    static bool IsClearedValue(ThreadImpl* thread, sp_t index) {
      int ref_index = static_cast<int>(index);
      Isolate* isolate = thread->isolate_;
      return thread->reference_stack().is_the_hole(isolate, ref_index);
    }

   private:
    WasmValue value_;
  };

  friend class InterpretedFrameImpl;

  CodeMap* codemap_;
  Isolate* isolate_;
  Handle<WasmInstanceObject> instance_object_;
  std::unique_ptr<StackValue[]> stack_;
  StackValue* stack_limit_ = nullptr;  // End of allocated stack space.
  StackValue* sp_ = nullptr;           // Current stack pointer.
  // The reference stack is pointed to by a {Cell} to be able to replace the
  // underlying {FixedArray} when growing the stack. This avoids having to
  // recreate or update the global handle keeping this object alive.
  Handle<Cell> reference_stack_cell_;  // References are on an on-heap stack.
  ZoneVector<Frame> frames_;
  WasmInterpreter::State state_ = WasmInterpreter::STOPPED;
  pc_t break_pc_ = kInvalidPc;
  TrapReason trap_reason_ = kTrapCount;
  bool possible_nondeterminism_ = false;
  uint8_t break_flags_ = 0;  // a combination of WasmInterpreter::BreakFlag
  uint64_t num_interpreted_calls_ = 0;
  // Store the stack height of each activation (for unwind and frame
  // inspection).
  ZoneVector<Activation> activations_;

  CodeMap* codemap() const { return codemap_; }
  const WasmModule* module() const { return codemap_->module(); }
  FixedArray reference_stack() const {
    return FixedArray::cast(reference_stack_cell_->value());
  }

  void DoTrap(TrapReason trap, pc_t pc) {
    TRACE("TRAP: %s\n", WasmOpcodes::TrapReasonMessage(trap));
    state_ = WasmInterpreter::TRAPPED;
    trap_reason_ = trap;
    CommitPc(pc);
  }

  // Check if there is room for a function's activation.
  void EnsureStackSpaceForCall(InterpreterCode* code) {
    EnsureStackSpace(code->side_table->max_stack_height_ +
                     code->locals.type_list.size());
    DCHECK_GE(StackHeight(), code->function->sig->parameter_count());
  }

  // Push a frame with arguments already on the stack.
  void PushFrame(InterpreterCode* code) {
    DCHECK_NOT_NULL(code);
    DCHECK_NOT_NULL(code->side_table);
    EnsureStackSpaceForCall(code);

    ++num_interpreted_calls_;
    size_t arity = code->function->sig->parameter_count();
    // The parameters will overlap the arguments already on the stack.
    DCHECK_GE(StackHeight(), arity);

    frames_.push_back({code, 0, StackHeight() - arity});
    frames_.back().pc = InitLocals(code);
    TRACE("  => PushFrame #%zu (#%u @%zu)\n", frames_.size() - 1,
          code->function->func_index, frames_.back().pc);
  }

  pc_t InitLocals(InterpreterCode* code) {
    for (auto p : code->locals.type_list) {
      WasmValue val;
      switch (p) {
#define CASE_TYPE(wasm, ctype) \
  case kWasm##wasm:            \
    val = WasmValue(ctype{});  \
    break;
        WASM_CTYPES(CASE_TYPE)
#undef CASE_TYPE
        case kWasmAnyRef:
        case kWasmFuncRef:
        case kWasmExnRef: {
          val = WasmValue(isolate_->factory()->null_value());
          break;
        }
        default:
          UNREACHABLE();
          break;
      }
      Push(val);
    }
    return code->locals.encoded_size;
  }

  void CommitPc(pc_t pc) {
    DCHECK(!frames_.empty());
    frames_.back().pc = pc;
  }

  bool SkipBreakpoint(InterpreterCode* code, pc_t pc) {
    if (pc == break_pc_) {
      // Skip the previously hit breakpoint when resuming.
      break_pc_ = kInvalidPc;
      return true;
    }
    return false;
  }

  void ReloadFromFrameOnException(Decoder* decoder, InterpreterCode** code,
                                  pc_t* pc, pc_t* limit) {
    Frame* top = &frames_.back();
    *code = top->code;
    *pc = top->pc;
    *limit = top->code->end - top->code->start;
    decoder->Reset(top->code->start, top->code->end);
  }

  int LookupTargetDelta(InterpreterCode* code, pc_t pc) {
    return static_cast<int>(code->side_table->Lookup(pc).pc_diff);
  }

  int JumpToHandlerDelta(InterpreterCode* code, pc_t pc) {
    ControlTransferEntry& control_transfer_entry = code->side_table->Lookup(pc);
    DoStackTransfer(control_transfer_entry.sp_diff + kCatchInArity,
                    control_transfer_entry.target_arity);
    return control_transfer_entry.pc_diff;
  }

  int DoBreak(InterpreterCode* code, pc_t pc, size_t depth) {
    ControlTransferEntry& control_transfer_entry = code->side_table->Lookup(pc);
    DoStackTransfer(control_transfer_entry.sp_diff,
                    control_transfer_entry.target_arity);
    return control_transfer_entry.pc_diff;
  }

  pc_t ReturnPc(Decoder* decoder, InterpreterCode* code, pc_t pc) {
    switch (code->orig_start[pc]) {
      case kExprCallFunction: {
        CallFunctionImmediate<Decoder::kNoValidate> imm(decoder, code->at(pc));
        return pc + 1 + imm.length;
      }
      case kExprCallIndirect: {
        CallIndirectImmediate<Decoder::kNoValidate> imm(kAllWasmFeatures,
                                                        decoder, code->at(pc));
        return pc + 1 + imm.length;
      }
      default:
        UNREACHABLE();
    }
  }

  bool DoReturn(Decoder* decoder, InterpreterCode** code, pc_t* pc, pc_t* limit,
                size_t arity) {
    DCHECK_GT(frames_.size(), 0);
    spdiff_t sp_diff = static_cast<spdiff_t>(StackHeight() - frames_.back().sp);
    frames_.pop_back();
    if (frames_.size() == current_activation().fp) {
      // A return from the last frame terminates the execution.
      state_ = WasmInterpreter::FINISHED;
      DoStackTransfer(sp_diff, arity);
      TRACE("  => finish\n");
      return false;
    } else {
      // Return to caller frame.
      Frame* top = &frames_.back();
      *code = top->code;
      decoder->Reset((*code)->start, (*code)->end);
      *pc = ReturnPc(decoder, *code, top->pc);
      *limit = top->code->end - top->code->start;
      TRACE("  => Return to #%zu (#%u @%zu)\n", frames_.size() - 1,
            (*code)->function->func_index, *pc);
      DoStackTransfer(sp_diff, arity);
      return true;
    }
  }

  // Returns true if the call was successful, false if the stack check failed
  // and the current activation was fully unwound.
  bool DoCall(Decoder* decoder, InterpreterCode* target, pc_t* pc,
              pc_t* limit) V8_WARN_UNUSED_RESULT {
    frames_.back().pc = *pc;
    PushFrame(target);
    if (!DoStackCheck()) return false;
    *pc = frames_.back().pc;
    *limit = target->end - target->start;
    decoder->Reset(target->start, target->end);
    return true;
  }

  // Returns true if the tail call was successful, false if the stack check
  // failed.
  bool DoReturnCall(Decoder* decoder, InterpreterCode* target, pc_t* pc,
                    pc_t* limit) V8_WARN_UNUSED_RESULT {
    DCHECK_NOT_NULL(target);
    DCHECK_NOT_NULL(target->side_table);
    EnsureStackSpaceForCall(target);

    ++num_interpreted_calls_;

    Frame* top = &frames_.back();

    // Drop everything except current parameters.
    spdiff_t sp_diff = static_cast<spdiff_t>(StackHeight() - top->sp);
    size_t arity = target->function->sig->parameter_count();

    DoStackTransfer(sp_diff, arity);

    *limit = target->end - target->start;
    decoder->Reset(target->start, target->end);

    // Rebuild current frame to look like a call to callee.
    top->code = target;
    top->pc = 0;
    top->sp = StackHeight() - arity;
    top->pc = InitLocals(target);

    *pc = top->pc;

    TRACE("  => ReturnCall #%zu (#%u @%zu)\n", frames_.size() - 1,
          target->function->func_index, top->pc);

    return true;
  }

  // Copies {arity} values on the top of the stack down the stack while also
  // dropping {sp_diff} many stack values in total from the stack.
  void DoStackTransfer(spdiff_t sp_diff, size_t arity) {
    // before: |---------------| pop_count | arity |
    //         ^ 0             ^ dest      ^ src   ^ StackHeight()
    //                         ^----< sp_diff >----^
    //
    // after:  |---------------| arity |
    //         ^ 0                     ^ StackHeight()
    sp_t stack_height = StackHeight();
    sp_t dest = stack_height - sp_diff;
    sp_t src = stack_height - arity;
    DCHECK_LE(dest, stack_height);
    DCHECK_LE(dest, src);
    if (arity && (dest != src)) {
      StackValue* stack = stack_.get();
      memmove(stack + dest, stack + src, arity * sizeof(StackValue));
      // Also move elements on the reference stack accordingly.
      reference_stack().MoveElements(
          isolate_, static_cast<int>(dest), static_cast<int>(src),
          static_cast<int>(arity), UPDATE_WRITE_BARRIER);
    }
    ResetStack(dest + arity);
  }

  inline Address EffectiveAddress(uint32_t index) {
    // Compute the effective address of the access, making sure to condition
    // the index even in the in-bounds case.
    return reinterpret_cast<Address>(instance_object_->memory_start()) +
           (index & instance_object_->memory_mask());
  }

  template <typename mtype>
  inline Address BoundsCheckMem(uint32_t offset, uint32_t index) {
    uint32_t effective_index = offset + index;
    if (effective_index < index) {
      return kNullAddress;  // wraparound => oob
    }
    if (!IsInBounds(effective_index, sizeof(mtype),
                    instance_object_->memory_size())) {
      return kNullAddress;  // oob
    }
    return EffectiveAddress(effective_index);
  }

  inline bool BoundsCheckMemRange(uint32_t index, uint32_t* size,
                                  Address* out_address) {
    bool ok = ClampToBounds(
        index, size, static_cast<uint32_t>(instance_object_->memory_size()));
    *out_address = EffectiveAddress(index);
    return ok;
  }

  template <typename ctype, typename mtype>
  bool ExecuteLoad(Decoder* decoder, InterpreterCode* code, pc_t pc,
                   int* const len, MachineRepresentation rep) {
    MemoryAccessImmediate<Decoder::kNoValidate> imm(decoder, code->at(pc),
                                                    sizeof(ctype));
    uint32_t index = Pop().to<uint32_t>();
    Address addr = BoundsCheckMem<mtype>(imm.offset, index);
    if (!addr) {
      DoTrap(kTrapMemOutOfBounds, pc);
      return false;
    }
    WasmValue result(
        converter<ctype, mtype>{}(ReadLittleEndianValue<mtype>(addr)));

    Push(result);
    *len = 1 + imm.length;

    if (FLAG_trace_wasm_memory) {
      MemoryTracingInfo info(imm.offset + index, false, rep);
      TraceMemoryOperation(ExecutionTier::kInterpreter, &info,
                           code->function->func_index, static_cast<int>(pc),
                           instance_object_->memory_start());
    }

    return true;
  }

  template <typename ctype, typename mtype>
  bool ExecuteStore(Decoder* decoder, InterpreterCode* code, pc_t pc,
                    int* const len, MachineRepresentation rep) {
    MemoryAccessImmediate<Decoder::kNoValidate> imm(decoder, code->at(pc),
                                                    sizeof(ctype));
    ctype val = Pop().to<ctype>();

    uint32_t index = Pop().to<uint32_t>();
    Address addr = BoundsCheckMem<mtype>(imm.offset, index);
    if (!addr) {
      DoTrap(kTrapMemOutOfBounds, pc);
      return false;
    }
    WriteLittleEndianValue<mtype>(addr, converter<mtype, ctype>{}(val));
    *len = 1 + imm.length;

    if (FLAG_trace_wasm_memory) {
      MemoryTracingInfo info(imm.offset + index, true, rep);
      TraceMemoryOperation(ExecutionTier::kInterpreter, &info,
                           code->function->func_index, static_cast<int>(pc),
                           instance_object_->memory_start());
    }

    return true;
  }

  bool CheckDataSegmentIsPassiveAndNotDropped(uint32_t index, pc_t pc) {
    DCHECK_LT(index, module()->num_declared_data_segments);
    if (instance_object_->dropped_data_segments()[index]) {
      DoTrap(kTrapDataSegmentDropped, pc);
      return false;
    }
    return true;
  }

  bool CheckElemSegmentIsPassiveAndNotDropped(uint32_t index, pc_t pc) {
    DCHECK_LT(index, module()->elem_segments.size());
    if (instance_object_->dropped_elem_segments()[index]) {
      DoTrap(kTrapElemSegmentDropped, pc);
      return false;
    }
    return true;
  }

  template <typename type, typename op_type>
  bool ExtractAtomicOpParams(Decoder* decoder, InterpreterCode* code,
                             Address* address, pc_t pc, int* const len,
                             type* val = nullptr, type* val2 = nullptr) {
    MemoryAccessImmediate<Decoder::kNoValidate> imm(decoder, code->at(pc + 1),
                                                    sizeof(type));
    if (val2) *val2 = static_cast<type>(Pop().to<op_type>());
    if (val) *val = static_cast<type>(Pop().to<op_type>());
    uint32_t index = Pop().to<uint32_t>();
    *address = BoundsCheckMem<type>(imm.offset, index);
    if (!address) {
      DoTrap(kTrapMemOutOfBounds, pc);
      return false;
    }
    *len = 2 + imm.length;
    return true;
  }

  bool ExecuteNumericOp(WasmOpcode opcode, Decoder* decoder,
                        InterpreterCode* code, pc_t pc, int* const len) {
    switch (opcode) {
      case kExprI32SConvertSatF32:
        Push(WasmValue(ExecuteConvertSaturate<int32_t>(Pop().to<float>())));
        return true;
      case kExprI32UConvertSatF32:
        Push(WasmValue(ExecuteConvertSaturate<uint32_t>(Pop().to<float>())));
        return true;
      case kExprI32SConvertSatF64:
        Push(WasmValue(ExecuteConvertSaturate<int32_t>(Pop().to<double>())));
        return true;
      case kExprI32UConvertSatF64:
        Push(WasmValue(ExecuteConvertSaturate<uint32_t>(Pop().to<double>())));
        return true;
      case kExprI64SConvertSatF32:
        Push(WasmValue(ExecuteI64SConvertSatF32(Pop().to<float>())));
        return true;
      case kExprI64UConvertSatF32:
        Push(WasmValue(ExecuteI64UConvertSatF32(Pop().to<float>())));
        return true;
      case kExprI64SConvertSatF64:
        Push(WasmValue(ExecuteI64SConvertSatF64(Pop().to<double>())));
        return true;
      case kExprI64UConvertSatF64:
        Push(WasmValue(ExecuteI64UConvertSatF64(Pop().to<double>())));
        return true;
      case kExprMemoryInit: {
        MemoryInitImmediate<Decoder::kNoValidate> imm(decoder, code->at(pc));
        DCHECK_LT(imm.data_segment_index, module()->num_declared_data_segments);
        *len += imm.length;
        if (!CheckDataSegmentIsPassiveAndNotDropped(imm.data_segment_index,
                                                    pc)) {
          return false;
        }
        auto size = Pop().to<uint32_t>();
        auto src = Pop().to<uint32_t>();
        auto dst = Pop().to<uint32_t>();
        if (size == 0) {
          return true;
        }
        Address dst_addr;
        bool ok = BoundsCheckMemRange(dst, &size, &dst_addr);
        auto src_max =
            instance_object_->data_segment_sizes()[imm.data_segment_index];
        // Use & instead of && so the clamp is not short-circuited.
        ok &= ClampToBounds(src, &size, src_max);
        Address src_addr =
            instance_object_->data_segment_starts()[imm.data_segment_index] +
            src;
        memory_copy_wrapper(dst_addr, src_addr, size);
        if (!ok) DoTrap(kTrapMemOutOfBounds, pc);
        return ok;
      }
      case kExprDataDrop: {
        DataDropImmediate<Decoder::kNoValidate> imm(decoder, code->at(pc));
        *len += imm.length;
        if (!CheckDataSegmentIsPassiveAndNotDropped(imm.index, pc)) {
          return false;
        }
        instance_object_->dropped_data_segments()[imm.index] = 1;
        return true;
      }
      case kExprMemoryCopy: {
        MemoryCopyImmediate<Decoder::kNoValidate> imm(decoder, code->at(pc));
        *len += imm.length;
        auto size = Pop().to<uint32_t>();
        auto src = Pop().to<uint32_t>();
        auto dst = Pop().to<uint32_t>();
        if (size == 0) {
          return true;
        }
        Address dst_addr;
        bool copy_backward = src < dst;
        bool ok = BoundsCheckMemRange(dst, &size, &dst_addr);
        // Trap without copying any bytes if we are copying backward and the
        // copy is partially out-of-bounds. We only need to check that the dst
        // region is out-of-bounds, because we know that {src < dst}, so the src
        // region is always out of bounds if the dst region is.
        if (ok || !copy_backward) {
          Address src_addr;
          // Use & instead of && so the bounds check is not short-circuited.
          ok &= BoundsCheckMemRange(src, &size, &src_addr);
          memory_copy_wrapper(dst_addr, src_addr, size);
        }
        if (!ok) DoTrap(kTrapMemOutOfBounds, pc);
        return ok;
      }
      case kExprMemoryFill: {
        MemoryIndexImmediate<Decoder::kNoValidate> imm(decoder,
                                                       code->at(pc + 1));
        *len += imm.length;
        auto size = Pop().to<uint32_t>();
        auto value = Pop().to<uint32_t>();
        auto dst = Pop().to<uint32_t>();
        if (size == 0) {
          return true;
        }
        Address dst_addr;
        bool ok = BoundsCheckMemRange(dst, &size, &dst_addr);
        memory_fill_wrapper(dst_addr, value, size);
        if (!ok) DoTrap(kTrapMemOutOfBounds, pc);
        return ok;
      }
      case kExprTableInit: {
        TableInitImmediate<Decoder::kNoValidate> imm(decoder, code->at(pc));
        *len += imm.length;
        if (!CheckElemSegmentIsPassiveAndNotDropped(imm.elem_segment_index,
                                                    pc)) {
          return false;
        }
        auto size = Pop().to<uint32_t>();
        auto src = Pop().to<uint32_t>();
        auto dst = Pop().to<uint32_t>();
        HandleScope scope(isolate_);  // Avoid leaking handles.
        bool ok = WasmInstanceObject::InitTableEntries(
            instance_object_->GetIsolate(), instance_object_, imm.table.index,
            imm.elem_segment_index, dst, src, size);
        if (!ok) DoTrap(kTrapTableOutOfBounds, pc);
        return ok;
      }
      case kExprElemDrop: {
        ElemDropImmediate<Decoder::kNoValidate> imm(decoder, code->at(pc));
        *len += imm.length;
        if (!CheckElemSegmentIsPassiveAndNotDropped(imm.index, pc)) {
          return false;
        }
        instance_object_->dropped_elem_segments()[imm.index] = 1;
        return true;
      }
      case kExprTableCopy: {
        TableCopyImmediate<Decoder::kNoValidate> imm(decoder, code->at(pc));
        auto size = Pop().to<uint32_t>();
        auto src = Pop().to<uint32_t>();
        auto dst = Pop().to<uint32_t>();
        HandleScope handle_scope(isolate_);  // Avoid leaking handles.
        bool ok = WasmInstanceObject::CopyTableEntries(
            isolate_, instance_object_, imm.table_dst.index,
            imm.table_src.index, dst, src, size);
        if (!ok) DoTrap(kTrapTableOutOfBounds, pc);
        *len += imm.length;
        return ok;
      }
      case kExprTableGrow: {
        TableIndexImmediate<Decoder::kNoValidate> imm(decoder,
                                                      code->at(pc + 1));
        HandleScope handle_scope(isolate_);
        auto table = handle(
            WasmTableObject::cast(instance_object_->tables().get(imm.index)),
            isolate_);
        auto delta = Pop().to<uint32_t>();
        auto value = Pop().to_anyref();
        int32_t result = WasmTableObject::Grow(isolate_, table, delta, value);
        Push(WasmValue(result));
        *len += imm.length;
        return true;
      }
      case kExprTableSize: {
        TableIndexImmediate<Decoder::kNoValidate> imm(decoder,
                                                      code->at(pc + 1));
        HandleScope handle_scope(isolate_);
        auto table = handle(
            WasmTableObject::cast(instance_object_->tables().get(imm.index)),
            isolate_);
        uint32_t table_size = table->current_length();
        Push(WasmValue(table_size));
        *len += imm.length;
        return true;
      }
      case kExprTableFill: {
        TableIndexImmediate<Decoder::kNoValidate> imm(decoder,
                                                      code->at(pc + 1));
        HandleScope handle_scope(isolate_);
        auto count = Pop().to<uint32_t>();
        auto value = Pop().to_anyref();
        auto start = Pop().to<uint32_t>();

        auto table = handle(
            WasmTableObject::cast(instance_object_->tables().get(imm.index)),
            isolate_);
        uint32_t table_size = table->current_length();
        if (start > table_size) {
          DoTrap(kTrapTableOutOfBounds, pc);
          return false;
        }

        // Even when table.fill goes out-of-bounds, as many entries as possible
        // are put into the table. Only afterwards we trap.
        uint32_t fill_count = std::min(count, table_size - start);
        WasmTableObject::Fill(isolate_, table, start, value, fill_count);

        if (fill_count < count) {
          DoTrap(kTrapTableOutOfBounds, pc);
          return false;
        }
        *len += imm.length;
        return true;
      }
      default:
        FATAL("Unknown or unimplemented opcode #%d:%s", code->start[pc],
              OpcodeName(code->start[pc]));
        UNREACHABLE();
    }
    return false;
  }

  template <typename type, typename op_type, typename func>
  op_type ExecuteAtomicBinopBE(type val, Address addr, func op) {
    type old_val;
    type new_val;
    old_val = ReadUnalignedValue<type>(addr);
    do {
      new_val =
          ByteReverse(static_cast<type>(op(ByteReverse<type>(old_val), val)));
    } while (!(std::atomic_compare_exchange_strong(
        reinterpret_cast<std::atomic<type>*>(addr), &old_val, new_val)));
    return static_cast<op_type>(ByteReverse<type>(old_val));
  }

  template <typename type>
  type AdjustByteOrder(type param) {
#if V8_TARGET_BIG_ENDIAN
    return ByteReverse(param);
#else
    return param;
#endif
  }

  bool ExecuteAtomicOp(WasmOpcode opcode, Decoder* decoder,
                       InterpreterCode* code, pc_t pc, int* const len) {
#if V8_TARGET_BIG_ENDIAN
    constexpr bool kBigEndian = true;
#else
    constexpr bool kBigEndian = false;
#endif
    WasmValue result;
    switch (opcode) {
#define ATOMIC_BINOP_CASE(name, type, op_type, operation, op)                \
  case kExpr##name: {                                                        \
    type val;                                                                \
    Address addr;                                                            \
    op_type result;                                                          \
    if (!ExtractAtomicOpParams<type, op_type>(decoder, code, &addr, pc, len, \
                                              &val)) {                       \
      return false;                                                          \
    }                                                                        \
    static_assert(sizeof(std::atomic<type>) == sizeof(type),                 \
                  "Size mismatch for types std::atomic<" #type               \
                  ">, and " #type);                                          \
    if (kBigEndian) {                                                        \
      auto oplambda = [](type a, type b) { return a op b; };                 \
      result = ExecuteAtomicBinopBE<type, op_type>(val, addr, oplambda);     \
    } else {                                                                 \
      result = static_cast<op_type>(                                         \
          std::operation(reinterpret_cast<std::atomic<type>*>(addr), val));  \
    }                                                                        \
    Push(WasmValue(result));                                                 \
    break;                                                                   \
  }
      ATOMIC_BINOP_CASE(I32AtomicAdd, uint32_t, uint32_t, atomic_fetch_add, +);
      ATOMIC_BINOP_CASE(I32AtomicAdd8U, uint8_t, uint32_t, atomic_fetch_add, +);
      ATOMIC_BINOP_CASE(I32AtomicAdd16U, uint16_t, uint32_t, atomic_fetch_add,
                        +);
      ATOMIC_BINOP_CASE(I32AtomicSub, uint32_t, uint32_t, atomic_fetch_sub, -);
      ATOMIC_BINOP_CASE(I32AtomicSub8U, uint8_t, uint32_t, atomic_fetch_sub, -);
      ATOMIC_BINOP_CASE(I32AtomicSub16U, uint16_t, uint32_t, atomic_fetch_sub,
                        -);
      ATOMIC_BINOP_CASE(I32AtomicAnd, uint32_t, uint32_t, atomic_fetch_and, &);
      ATOMIC_BINOP_CASE(I32AtomicAnd8U, uint8_t, uint32_t, atomic_fetch_and, &);
      ATOMIC_BINOP_CASE(I32AtomicAnd16U, uint16_t, uint32_t,
                        atomic_fetch_and, &);
      ATOMIC_BINOP_CASE(I32AtomicOr, uint32_t, uint32_t, atomic_fetch_or, |);
      ATOMIC_BINOP_CASE(I32AtomicOr8U, uint8_t, uint32_t, atomic_fetch_or, |);
      ATOMIC_BINOP_CASE(I32AtomicOr16U, uint16_t, uint32_t, atomic_fetch_or, |);
      ATOMIC_BINOP_CASE(I32AtomicXor, uint32_t, uint32_t, atomic_fetch_xor, ^);
      ATOMIC_BINOP_CASE(I32AtomicXor8U, uint8_t, uint32_t, atomic_fetch_xor, ^);
      ATOMIC_BINOP_CASE(I32AtomicXor16U, uint16_t, uint32_t, atomic_fetch_xor,
                        ^);
      ATOMIC_BINOP_CASE(I32AtomicExchange, uint32_t, uint32_t, atomic_exchange,
                        =);
      ATOMIC_BINOP_CASE(I32AtomicExchange8U, uint8_t, uint32_t, atomic_exchange,
                        =);
      ATOMIC_BINOP_CASE(I32AtomicExchange16U, uint16_t, uint32_t,
                        atomic_exchange, =);
      ATOMIC_BINOP_CASE(I64AtomicAdd, uint64_t, uint64_t, atomic_fetch_add, +);
      ATOMIC_BINOP_CASE(I64AtomicAdd8U, uint8_t, uint64_t, atomic_fetch_add, +);
      ATOMIC_BINOP_CASE(I64AtomicAdd16U, uint16_t, uint64_t, atomic_fetch_add,
                        +);
      ATOMIC_BINOP_CASE(I64AtomicAdd32U, uint32_t, uint64_t, atomic_fetch_add,
                        +);
      ATOMIC_BINOP_CASE(I64AtomicSub, uint64_t, uint64_t, atomic_fetch_sub, -);
      ATOMIC_BINOP_CASE(I64AtomicSub8U, uint8_t, uint64_t, atomic_fetch_sub, -);
      ATOMIC_BINOP_CASE(I64AtomicSub16U, uint16_t, uint64_t, atomic_fetch_sub,
                        -);
      ATOMIC_BINOP_CASE(I64AtomicSub32U, uint32_t, uint64_t, atomic_fetch_sub,
                        -);
      ATOMIC_BINOP_CASE(I64AtomicAnd, uint64_t, uint64_t, atomic_fetch_and, &);
      ATOMIC_BINOP_CASE(I64AtomicAnd8U, uint8_t, uint64_t, atomic_fetch_and, &);
      ATOMIC_BINOP_CASE(I64AtomicAnd16U, uint16_t, uint64_t,
                        atomic_fetch_and, &);
      ATOMIC_BINOP_CASE(I64AtomicAnd32U, uint32_t, uint64_t,
                        atomic_fetch_and, &);
      ATOMIC_BINOP_CASE(I64AtomicOr, uint64_t, uint64_t, atomic_fetch_or, |);
      ATOMIC_BINOP_CASE(I64AtomicOr8U, uint8_t, uint64_t, atomic_fetch_or, |);
      ATOMIC_BINOP_CASE(I64AtomicOr16U, uint16_t, uint64_t, atomic_fetch_or, |);
      ATOMIC_BINOP_CASE(I64AtomicOr32U, uint32_t, uint64_t, atomic_fetch_or, |);
      ATOMIC_BINOP_CASE(I64AtomicXor, uint64_t, uint64_t, atomic_fetch_xor, ^);
      ATOMIC_BINOP_CASE(I64AtomicXor8U, uint8_t, uint64_t, atomic_fetch_xor, ^);
      ATOMIC_BINOP_CASE(I64AtomicXor16U, uint16_t, uint64_t, atomic_fetch_xor,
                        ^);
      ATOMIC_BINOP_CASE(I64AtomicXor32U, uint32_t, uint64_t, atomic_fetch_xor,
                        ^);
      ATOMIC_BINOP_CASE(I64AtomicExchange, uint64_t, uint64_t, atomic_exchange,
                        =);
      ATOMIC_BINOP_CASE(I64AtomicExchange8U, uint8_t, uint64_t, atomic_exchange,
                        =);
      ATOMIC_BINOP_CASE(I64AtomicExchange16U, uint16_t, uint64_t,
                        atomic_exchange, =);
      ATOMIC_BINOP_CASE(I64AtomicExchange32U, uint32_t, uint64_t,
                        atomic_exchange, =);
#undef ATOMIC_BINOP_CASE
#define ATOMIC_COMPARE_EXCHANGE_CASE(name, type, op_type)                    \
  case kExpr##name: {                                                        \
    type old_val;                                                            \
    type new_val;                                                            \
    Address addr;                                                            \
    if (!ExtractAtomicOpParams<type, op_type>(decoder, code, &addr, pc, len, \
                                              &old_val, &new_val)) {         \
      return false;                                                          \
    }                                                                        \
    static_assert(sizeof(std::atomic<type>) == sizeof(type),                 \
                  "Size mismatch for types std::atomic<" #type               \
                  ">, and " #type);                                          \
    old_val = AdjustByteOrder<type>(old_val);                                \
    new_val = AdjustByteOrder<type>(new_val);                                \
    std::atomic_compare_exchange_strong(                                     \
        reinterpret_cast<std::atomic<type>*>(addr), &old_val, new_val);      \
    Push(WasmValue(static_cast<op_type>(AdjustByteOrder<type>(old_val))));   \
    break;                                                                   \
  }
      ATOMIC_COMPARE_EXCHANGE_CASE(I32AtomicCompareExchange, uint32_t,
                                   uint32_t);
      ATOMIC_COMPARE_EXCHANGE_CASE(I32AtomicCompareExchange8U, uint8_t,
                                   uint32_t);
      ATOMIC_COMPARE_EXCHANGE_CASE(I32AtomicCompareExchange16U, uint16_t,
                                   uint32_t);
      ATOMIC_COMPARE_EXCHANGE_CASE(I64AtomicCompareExchange, uint64_t,
                                   uint64_t);
      ATOMIC_COMPARE_EXCHANGE_CASE(I64AtomicCompareExchange8U, uint8_t,
                                   uint64_t);
      ATOMIC_COMPARE_EXCHANGE_CASE(I64AtomicCompareExchange16U, uint16_t,
                                   uint64_t);
      ATOMIC_COMPARE_EXCHANGE_CASE(I64AtomicCompareExchange32U, uint32_t,
                                   uint64_t);
#undef ATOMIC_COMPARE_EXCHANGE_CASE
#define ATOMIC_LOAD_CASE(name, type, op_type, operation)                \
  case kExpr##name: {                                                   \
    Address addr;                                                       \
    if (!ExtractAtomicOpParams<type, op_type>(decoder, code, &addr, pc, \
                                              len)) {                   \
      return false;                                                     \
    }                                                                   \
    static_assert(sizeof(std::atomic<type>) == sizeof(type),            \
                  "Size mismatch for types std::atomic<" #type          \
                  ">, and " #type);                                     \
    result = WasmValue(static_cast<op_type>(AdjustByteOrder<type>(      \
        std::operation(reinterpret_cast<std::atomic<type>*>(addr)))));  \
    Push(result);                                                       \
    break;                                                              \
  }
      ATOMIC_LOAD_CASE(I32AtomicLoad, uint32_t, uint32_t, atomic_load);
      ATOMIC_LOAD_CASE(I32AtomicLoad8U, uint8_t, uint32_t, atomic_load);
      ATOMIC_LOAD_CASE(I32AtomicLoad16U, uint16_t, uint32_t, atomic_load);
      ATOMIC_LOAD_CASE(I64AtomicLoad, uint64_t, uint64_t, atomic_load);
      ATOMIC_LOAD_CASE(I64AtomicLoad8U, uint8_t, uint64_t, atomic_load);
      ATOMIC_LOAD_CASE(I64AtomicLoad16U, uint16_t, uint64_t, atomic_load);
      ATOMIC_LOAD_CASE(I64AtomicLoad32U, uint32_t, uint64_t, atomic_load);
#undef ATOMIC_LOAD_CASE
#define ATOMIC_STORE_CASE(name, type, op_type, operation)                    \
  case kExpr##name: {                                                        \
    type val;                                                                \
    Address addr;                                                            \
    if (!ExtractAtomicOpParams<type, op_type>(decoder, code, &addr, pc, len, \
                                              &val)) {                       \
      return false;                                                          \
    }                                                                        \
    static_assert(sizeof(std::atomic<type>) == sizeof(type),                 \
                  "Size mismatch for types std::atomic<" #type               \
                  ">, and " #type);                                          \
    std::operation(reinterpret_cast<std::atomic<type>*>(addr),               \
                   AdjustByteOrder<type>(val));                              \
    break;                                                                   \
  }
      ATOMIC_STORE_CASE(I32AtomicStore, uint32_t, uint32_t, atomic_store);
      ATOMIC_STORE_CASE(I32AtomicStore8U, uint8_t, uint32_t, atomic_store);
      ATOMIC_STORE_CASE(I32AtomicStore16U, uint16_t, uint32_t, atomic_store);
      ATOMIC_STORE_CASE(I64AtomicStore, uint64_t, uint64_t, atomic_store);
      ATOMIC_STORE_CASE(I64AtomicStore8U, uint8_t, uint64_t, atomic_store);
      ATOMIC_STORE_CASE(I64AtomicStore16U, uint16_t, uint64_t, atomic_store);
      ATOMIC_STORE_CASE(I64AtomicStore32U, uint32_t, uint64_t, atomic_store);
#undef ATOMIC_STORE_CASE
      case kExprAtomicFence:
        std::atomic_thread_fence(std::memory_order_seq_cst);
        *len += 2;
        break;
      default:
        UNREACHABLE();
        return false;
    }
    return true;
  }

  byte* GetGlobalPtr(const WasmGlobal* global) {
    DCHECK(!ValueTypes::IsReferenceType(global->type));
    if (global->mutability && global->imported) {
      return reinterpret_cast<byte*>(
          instance_object_->imported_mutable_globals()[global->index]);
    } else {
      return instance_object_->globals_start() + global->offset;
    }
  }

  void GetGlobalBufferAndIndex(const WasmGlobal* global,
                               Handle<FixedArray>* buffer, uint32_t* index) {
    DCHECK(ValueTypes::IsReferenceType(global->type));
    if (global->mutability && global->imported) {
      *buffer =
          handle(FixedArray::cast(
                     instance_object_->imported_mutable_globals_buffers().get(
                         global->index)),
                 isolate_);
      Address idx = instance_object_->imported_mutable_globals()[global->index];
      DCHECK_LE(idx, std::numeric_limits<uint32_t>::max());
      *index = static_cast<uint32_t>(idx);
    } else {
      *buffer = handle(instance_object_->tagged_globals_buffer(), isolate_);
      *index = global->offset;
    }
  }

  bool ExecuteSimdOp(WasmOpcode opcode, Decoder* decoder, InterpreterCode* code,
                     pc_t pc, int* const len) {
    switch (opcode) {
#define SPLAT_CASE(format, sType, valType, num) \
  case kExpr##format##Splat: {                  \
    WasmValue val = Pop();                      \
    valType v = val.to<valType>();              \
    sType s;                                    \
    for (int i = 0; i < num; i++) s.val[i] = v; \
    Push(WasmValue(Simd128(s)));                \
    return true;                                \
  }
      SPLAT_CASE(F64x2, float2, double, 2)
      SPLAT_CASE(F32x4, float4, float, 4)
      SPLAT_CASE(I64x2, int2, int64_t, 2)
      SPLAT_CASE(I32x4, int4, int32_t, 4)
      SPLAT_CASE(I16x8, int8, int32_t, 8)
      SPLAT_CASE(I8x16, int16, int32_t, 16)
#undef SPLAT_CASE
#define EXTRACT_LANE_CASE(format, name)                                 \
  case kExpr##format##ExtractLane: {                                    \
    SimdLaneImmediate<Decoder::kNoValidate> imm(decoder, code->at(pc)); \
    *len += 1;                                                          \
    WasmValue val = Pop();                                              \
    Simd128 s = val.to_s128();                                          \
    auto ss = s.to_##name();                                            \
    Push(WasmValue(ss.val[LANE(imm.lane, ss)]));                        \
    return true;                                                        \
  }
      EXTRACT_LANE_CASE(F64x2, f64x2)
      EXTRACT_LANE_CASE(F32x4, f32x4)
      EXTRACT_LANE_CASE(I64x2, i64x2)
      EXTRACT_LANE_CASE(I32x4, i32x4)
      EXTRACT_LANE_CASE(I16x8, i16x8)
      EXTRACT_LANE_CASE(I8x16, i8x16)
#undef EXTRACT_LANE_CASE
#define BINOP_CASE(op, name, stype, count, expr) \
  case kExpr##op: {                              \
    WasmValue v2 = Pop();                        \
    WasmValue v1 = Pop();                        \
    stype s1 = v1.to_s128().to_##name();         \
    stype s2 = v2.to_s128().to_##name();         \
    stype res;                                   \
    for (size_t i = 0; i < count; ++i) {         \
      auto a = s1.val[LANE(i, s1)];              \
      auto b = s2.val[LANE(i, s1)];              \
      res.val[LANE(i, s1)] = expr;               \
    }                                            \
    Push(WasmValue(Simd128(res)));               \
    return true;                                 \
  }
      BINOP_CASE(F32x4Add, f32x4, float4, 4, a + b)
      BINOP_CASE(F32x4Sub, f32x4, float4, 4, a - b)
      BINOP_CASE(F32x4Mul, f32x4, float4, 4, a * b)
      BINOP_CASE(F32x4Min, f32x4, float4, 4, a < b ? a : b)
      BINOP_CASE(F32x4Max, f32x4, float4, 4, a > b ? a : b)
      BINOP_CASE(I64x2Add, i64x2, int2, 2, base::AddWithWraparound(a, b))
      BINOP_CASE(I64x2Sub, i64x2, int2, 2, base::SubWithWraparound(a, b))
      BINOP_CASE(I64x2Mul, i64x2, int2, 2, base::MulWithWraparound(a, b))
      BINOP_CASE(I32x4Add, i32x4, int4, 4, base::AddWithWraparound(a, b))
      BINOP_CASE(I32x4Sub, i32x4, int4, 4, base::SubWithWraparound(a, b))
      BINOP_CASE(I32x4Mul, i32x4, int4, 4, base::MulWithWraparound(a, b))
      BINOP_CASE(I32x4MinS, i32x4, int4, 4, a < b ? a : b)
      BINOP_CASE(I32x4MinU, i32x4, int4, 4,
                 static_cast<uint32_t>(a) < static_cast<uint32_t>(b) ? a : b)
      BINOP_CASE(I32x4MaxS, i32x4, int4, 4, a > b ? a : b)
      BINOP_CASE(I32x4MaxU, i32x4, int4, 4,
                 static_cast<uint32_t>(a) > static_cast<uint32_t>(b) ? a : b)
      BINOP_CASE(S128And, i32x4, int4, 4, a & b)
      BINOP_CASE(S128Or, i32x4, int4, 4, a | b)
      BINOP_CASE(S128Xor, i32x4, int4, 4, a ^ b)
      BINOP_CASE(I16x8Add, i16x8, int8, 8, base::AddWithWraparound(a, b))
      BINOP_CASE(I16x8Sub, i16x8, int8, 8, base::SubWithWraparound(a, b))
      BINOP_CASE(I16x8Mul, i16x8, int8, 8, base::MulWithWraparound(a, b))
      BINOP_CASE(I16x8MinS, i16x8, int8, 8, a < b ? a : b)
      BINOP_CASE(I16x8MinU, i16x8, int8, 8,
                 static_cast<uint16_t>(a) < static_cast<uint16_t>(b) ? a : b)
      BINOP_CASE(I16x8MaxS, i16x8, int8, 8, a > b ? a : b)
      BINOP_CASE(I16x8MaxU, i16x8, int8, 8,
                 static_cast<uint16_t>(a) > static_cast<uint16_t>(b) ? a : b)
      BINOP_CASE(I16x8AddSaturateS, i16x8, int8, 8, SaturateAdd<int16_t>(a, b))
      BINOP_CASE(I16x8AddSaturateU, i16x8, int8, 8, SaturateAdd<uint16_t>(a, b))
      BINOP_CASE(I16x8SubSaturateS, i16x8, int8, 8, SaturateSub<int16_t>(a, b))
      BINOP_CASE(I16x8SubSaturateU, i16x8, int8, 8, SaturateSub<uint16_t>(a, b))
      BINOP_CASE(I8x16Add, i8x16, int16, 16, base::AddWithWraparound(a, b))
      BINOP_CASE(I8x16Sub, i8x16, int16, 16, base::SubWithWraparound(a, b))
      BINOP_CASE(I8x16Mul, i8x16, int16, 16, base::MulWithWraparound(a, b))
      BINOP_CASE(I8x16MinS, i8x16, int16, 16, a < b ? a : b)
      BINOP_CASE(I8x16MinU, i8x16, int16, 16,
                 static_cast<uint8_t>(a) < static_cast<uint8_t>(b) ? a : b)
      BINOP_CASE(I8x16MaxS, i8x16, int16, 16, a > b ? a : b)
      BINOP_CASE(I8x16MaxU, i8x16, int16, 16,
                 static_cast<uint8_t>(a) > static_cast<uint8_t>(b) ? a : b)
      BINOP_CASE(I8x16AddSaturateS, i8x16, int16, 16, SaturateAdd<int8_t>(a, b))
      BINOP_CASE(I8x16AddSaturateU, i8x16, int16, 16,
                 SaturateAdd<uint8_t>(a, b))
      BINOP_CASE(I8x16SubSaturateS, i8x16, int16, 16, SaturateSub<int8_t>(a, b))
      BINOP_CASE(I8x16SubSaturateU, i8x16, int16, 16,
                 SaturateSub<uint8_t>(a, b))
#undef BINOP_CASE
#define UNOP_CASE(op, name, stype, count, expr) \
  case kExpr##op: {                             \
    WasmValue v = Pop();                        \
    stype s = v.to_s128().to_##name();          \
    stype res;                                  \
    for (size_t i = 0; i < count; ++i) {        \
      auto a = s.val[i];                        \
      res.val[i] = expr;                        \
    }                                           \
    Push(WasmValue(Simd128(res)));              \
    return true;                                \
  }
      UNOP_CASE(F64x2Abs, f64x2, float2, 2, std::abs(a))
      UNOP_CASE(F64x2Neg, f64x2, float2, 2, -a)
      UNOP_CASE(F32x4Abs, f32x4, float4, 4, std::abs(a))
      UNOP_CASE(F32x4Neg, f32x4, float4, 4, -a)
      UNOP_CASE(F32x4RecipApprox, f32x4, float4, 4, base::Recip(a))
      UNOP_CASE(F32x4RecipSqrtApprox, f32x4, float4, 4, base::RecipSqrt(a))
      UNOP_CASE(I64x2Neg, i64x2, int2, 2, base::NegateWithWraparound(a))
      UNOP_CASE(I32x4Neg, i32x4, int4, 4, base::NegateWithWraparound(a))
      UNOP_CASE(S128Not, i32x4, int4, 4, ~a)
      UNOP_CASE(I16x8Neg, i16x8, int8, 8, base::NegateWithWraparound(a))
      UNOP_CASE(I8x16Neg, i8x16, int16, 16, base::NegateWithWraparound(a))
#undef UNOP_CASE
#define CMPOP_CASE(op, name, stype, out_stype, count, expr) \
  case kExpr##op: {                                         \
    WasmValue v2 = Pop();                                   \
    WasmValue v1 = Pop();                                   \
    stype s1 = v1.to_s128().to_##name();                    \
    stype s2 = v2.to_s128().to_##name();                    \
    out_stype res;                                          \
    for (size_t i = 0; i < count; ++i) {                    \
      auto a = s1.val[i];                                   \
      auto b = s2.val[i];                                   \
      res.val[i] = expr ? -1 : 0;                           \
    }                                                       \
    Push(WasmValue(Simd128(res)));                          \
    return true;                                            \
  }
      CMPOP_CASE(F64x2Eq, f64x2, float2, int2, 2, a == b)
      CMPOP_CASE(F64x2Ne, f64x2, float2, int2, 2, a != b)
      CMPOP_CASE(F64x2Gt, f64x2, float2, int2, 2, a > b)
      CMPOP_CASE(F64x2Ge, f64x2, float2, int2, 2, a >= b)
      CMPOP_CASE(F64x2Lt, f64x2, float2, int2, 2, a < b)
      CMPOP_CASE(F64x2Le, f64x2, float2, int2, 2, a <= b)
      CMPOP_CASE(F32x4Eq, f32x4, float4, int4, 4, a == b)
      CMPOP_CASE(F32x4Ne, f32x4, float4, int4, 4, a != b)
      CMPOP_CASE(F32x4Gt, f32x4, float4, int4, 4, a > b)
      CMPOP_CASE(F32x4Ge, f32x4, float4, int4, 4, a >= b)
      CMPOP_CASE(F32x4Lt, f32x4, float4, int4, 4, a < b)
      CMPOP_CASE(F32x4Le, f32x4, float4, int4, 4, a <= b)
      CMPOP_CASE(I64x2Eq, i64x2, int2, int2, 2, a == b)
      CMPOP_CASE(I64x2Ne, i64x2, int2, int2, 2, a != b)
      CMPOP_CASE(I64x2GtS, i64x2, int2, int2, 2, a > b)
      CMPOP_CASE(I64x2GeS, i64x2, int2, int2, 2, a >= b)
      CMPOP_CASE(I64x2LtS, i64x2, int2, int2, 2, a < b)
      CMPOP_CASE(I64x2LeS, i64x2, int2, int2, 2, a <= b)
      CMPOP_CASE(I64x2GtU, i64x2, int2, int2, 2,
                 static_cast<uint64_t>(a) > static_cast<uint64_t>(b))
      CMPOP_CASE(I64x2GeU, i64x2, int2, int2, 2,
                 static_cast<uint64_t>(a) >= static_cast<uint64_t>(b))
      CMPOP_CASE(I64x2LtU, i64x2, int2, int2, 2,
                 static_cast<uint64_t>(a) < static_cast<uint64_t>(b))
      CMPOP_CASE(I64x2LeU, i64x2, int2, int2, 2,
                 static_cast<uint64_t>(a) <= static_cast<uint64_t>(b))
      CMPOP_CASE(I32x4Eq, i32x4, int4, int4, 4, a == b)
      CMPOP_CASE(I32x4Ne, i32x4, int4, int4, 4, a != b)
      CMPOP_CASE(I32x4GtS, i32x4, int4, int4, 4, a > b)
      CMPOP_CASE(I32x4GeS, i32x4, int4, int4, 4, a >= b)
      CMPOP_CASE(I32x4LtS, i32x4, int4, int4, 4, a < b)
      CMPOP_CASE(I32x4LeS, i32x4, int4, int4, 4, a <= b)
      CMPOP_CASE(I32x4GtU, i32x4, int4, int4, 4,
                 static_cast<uint32_t>(a) > static_cast<uint32_t>(b))
      CMPOP_CASE(I32x4GeU, i32x4, int4, int4, 4,
                 static_cast<uint32_t>(a) >= static_cast<uint32_t>(b))
      CMPOP_CASE(I32x4LtU, i32x4, int4, int4, 4,
                 static_cast<uint32_t>(a) < static_cast<uint32_t>(b))
      CMPOP_CASE(I32x4LeU, i32x4, int4, int4, 4,
                 static_cast<uint32_t>(a) <= static_cast<uint32_t>(b))
      CMPOP_CASE(I16x8Eq, i16x8, int8, int8, 8, a == b)
      CMPOP_CASE(I16x8Ne, i16x8, int8, int8, 8, a != b)
      CMPOP_CASE(I16x8GtS, i16x8, int8, int8, 8, a > b)
      CMPOP_CASE(I16x8GeS, i16x8, int8, int8, 8, a >= b)
      CMPOP_CASE(I16x8LtS, i16x8, int8, int8, 8, a < b)
      CMPOP_CASE(I16x8LeS, i16x8, int8, int8, 8, a <= b)
      CMPOP_CASE(I16x8GtU, i16x8, int8, int8, 8,
                 static_cast<uint16_t>(a) > static_cast<uint16_t>(b))
      CMPOP_CASE(I16x8GeU, i16x8, int8, int8, 8,
                 static_cast<uint16_t>(a) >= static_cast<uint16_t>(b))
      CMPOP_CASE(I16x8LtU, i16x8, int8, int8, 8,
                 static_cast<uint16_t>(a) < static_cast<uint16_t>(b))
      CMPOP_CASE(I16x8LeU, i16x8, int8, int8, 8,
                 static_cast<uint16_t>(a) <= static_cast<uint16_t>(b))
      CMPOP_CASE(I8x16Eq, i8x16, int16, int16, 16, a == b)
      CMPOP_CASE(I8x16Ne, i8x16, int16, int16, 16, a != b)
      CMPOP_CASE(I8x16GtS, i8x16, int16, int16, 16, a > b)
      CMPOP_CASE(I8x16GeS, i8x16, int16, int16, 16, a >= b)
      CMPOP_CASE(I8x16LtS, i8x16, int16, int16, 16, a < b)
      CMPOP_CASE(I8x16LeS, i8x16, int16, int16, 16, a <= b)
      CMPOP_CASE(I8x16GtU, i8x16, int16, int16, 16,
                 static_cast<uint8_t>(a) > static_cast<uint8_t>(b))
      CMPOP_CASE(I8x16GeU, i8x16, int16, int16, 16,
                 static_cast<uint8_t>(a) >= static_cast<uint8_t>(b))
      CMPOP_CASE(I8x16LtU, i8x16, int16, int16, 16,
                 static_cast<uint8_t>(a) < static_cast<uint8_t>(b))
      CMPOP_CASE(I8x16LeU, i8x16, int16, int16, 16,
                 static_cast<uint8_t>(a) <= static_cast<uint8_t>(b))
#undef CMPOP_CASE
#define REPLACE_LANE_CASE(format, name, stype, ctype)                   \
  case kExpr##format##ReplaceLane: {                                    \
    SimdLaneImmediate<Decoder::kNoValidate> imm(decoder, code->at(pc)); \
    *len += 1;                                                          \
    WasmValue new_val = Pop();                                          \
    WasmValue simd_val = Pop();                                         \
    stype s = simd_val.to_s128().to_##name();                           \
    s.val[LANE(imm.lane, s)] = new_val.to<ctype>();                     \
    Push(WasmValue(Simd128(s)));                                        \
    return true;                                                        \
  }
      REPLACE_LANE_CASE(F64x2, f64x2, float2, double)
      REPLACE_LANE_CASE(F32x4, f32x4, float4, float)
      REPLACE_LANE_CASE(I64x2, i64x2, int2, int64_t)
      REPLACE_LANE_CASE(I32x4, i32x4, int4, int32_t)
      REPLACE_LANE_CASE(I16x8, i16x8, int8, int32_t)
      REPLACE_LANE_CASE(I8x16, i8x16, int16, int32_t)
#undef REPLACE_LANE_CASE
      case kExprS128LoadMem:
        return ExecuteLoad<Simd128, Simd128>(decoder, code, pc, len,
                                             MachineRepresentation::kSimd128);
      case kExprS128StoreMem:
        return ExecuteStore<Simd128, Simd128>(decoder, code, pc, len,
                                              MachineRepresentation::kSimd128);
#define SHIFT_CASE(op, name, stype, count, expr)                         \
  case kExpr##op: {                                                      \
    SimdShiftImmediate<Decoder::kNoValidate> imm(decoder, code->at(pc)); \
    *len += 1;                                                           \
    WasmValue v = Pop();                                                 \
    stype s = v.to_s128().to_##name();                                   \
    stype res;                                                           \
    for (size_t i = 0; i < count; ++i) {                                 \
      auto a = s.val[i];                                                 \
      res.val[i] = expr;                                                 \
    }                                                                    \
    Push(WasmValue(Simd128(res)));                                       \
    return true;                                                         \
  }
        SHIFT_CASE(I64x2Shl, i64x2, int2, 2,
                   static_cast<uint64_t>(a) << imm.shift)
        SHIFT_CASE(I64x2ShrS, i64x2, int2, 2, a >> imm.shift)
        SHIFT_CASE(I64x2ShrU, i64x2, int2, 2,
                   static_cast<uint64_t>(a) >> imm.shift)
        SHIFT_CASE(I32x4Shl, i32x4, int4, 4,
                   static_cast<uint32_t>(a) << imm.shift)
        SHIFT_CASE(I32x4ShrS, i32x4, int4, 4, a >> imm.shift)
        SHIFT_CASE(I32x4ShrU, i32x4, int4, 4,
                   static_cast<uint32_t>(a) >> imm.shift)
        SHIFT_CASE(I16x8Shl, i16x8, int8, 8,
                   static_cast<uint16_t>(a) << imm.shift)
        SHIFT_CASE(I16x8ShrS, i16x8, int8, 8, a >> imm.shift)
        SHIFT_CASE(I16x8ShrU, i16x8, int8, 8,
                   static_cast<uint16_t>(a) >> imm.shift)
        SHIFT_CASE(I8x16Shl, i8x16, int16, 16,
                   static_cast<uint8_t>(a) << imm.shift)
        SHIFT_CASE(I8x16ShrS, i8x16, int16, 16, a >> imm.shift)
        SHIFT_CASE(I8x16ShrU, i8x16, int16, 16,
                   static_cast<uint8_t>(a) >> imm.shift)
#undef SHIFT_CASE
#define CONVERT_CASE(op, src_type, name, dst_type, count, start_index, ctype, \
                     expr)                                                    \
  case kExpr##op: {                                                           \
    WasmValue v = Pop();                                                      \
    src_type s = v.to_s128().to_##name();                                     \
    dst_type res;                                                             \
    for (size_t i = 0; i < count; ++i) {                                      \
      ctype a = s.val[LANE(start_index + i, s)];                              \
      res.val[LANE(i, res)] = expr;                                           \
    }                                                                         \
    Push(WasmValue(Simd128(res)));                                            \
    return true;                                                              \
  }
        CONVERT_CASE(F32x4SConvertI32x4, int4, i32x4, float4, 4, 0, int32_t,
                     static_cast<float>(a))
        CONVERT_CASE(F32x4UConvertI32x4, int4, i32x4, float4, 4, 0, uint32_t,
                     static_cast<float>(a))
        CONVERT_CASE(I32x4SConvertF32x4, float4, f32x4, int4, 4, 0, double,
                     std::isnan(a) ? 0
                                   : a<kMinInt ? kMinInt : a> kMaxInt
                                         ? kMaxInt
                                         : static_cast<int32_t>(a))
        CONVERT_CASE(I32x4UConvertF32x4, float4, f32x4, int4, 4, 0, double,
                     std::isnan(a)
                         ? 0
                         : a<0 ? 0 : a> kMaxUInt32 ? kMaxUInt32
                                                   : static_cast<uint32_t>(a))
        CONVERT_CASE(I32x4SConvertI16x8High, int8, i16x8, int4, 4, 4, int16_t,
                     a)
        CONVERT_CASE(I32x4UConvertI16x8High, int8, i16x8, int4, 4, 4, uint16_t,
                     a)
        CONVERT_CASE(I32x4SConvertI16x8Low, int8, i16x8, int4, 4, 0, int16_t, a)
        CONVERT_CASE(I32x4UConvertI16x8Low, int8, i16x8, int4, 4, 0, uint16_t,
                     a)
        CONVERT_CASE(I16x8SConvertI8x16High, int16, i8x16, int8, 8, 8, int8_t,
                     a)
        CONVERT_CASE(I16x8UConvertI8x16High, int16, i8x16, int8, 8, 8, uint8_t,
                     a)
        CONVERT_CASE(I16x8SConvertI8x16Low, int16, i8x16, int8, 8, 0, int8_t, a)
        CONVERT_CASE(I16x8UConvertI8x16Low, int16, i8x16, int8, 8, 0, uint8_t,
                     a)
#undef CONVERT_CASE
#define PACK_CASE(op, src_type, name, dst_type, count, ctype, dst_ctype,   \
                  is_unsigned)                                             \
  case kExpr##op: {                                                        \
    WasmValue v2 = Pop();                                                  \
    WasmValue v1 = Pop();                                                  \
    src_type s1 = v1.to_s128().to_##name();                                \
    src_type s2 = v2.to_s128().to_##name();                                \
    dst_type res;                                                          \
    int64_t min = std::numeric_limits<ctype>::min();                       \
    int64_t max = std::numeric_limits<ctype>::max();                       \
    for (size_t i = 0; i < count; ++i) {                                   \
      int32_t v = i < count / 2 ? s1.val[LANE(i, s1)]                      \
                                : s2.val[LANE(i - count / 2, s2)];         \
      int64_t a = is_unsigned ? static_cast<int64_t>(v & 0xFFFFFFFFu) : v; \
      res.val[LANE(i, res)] =                                              \
          static_cast<dst_ctype>(std::max(min, std::min(max, a)));         \
    }                                                                      \
    Push(WasmValue(Simd128(res)));                                         \
    return true;                                                           \
  }
        PACK_CASE(I16x8SConvertI32x4, int4, i32x4, int8, 8, int16_t, int16_t,
                  false)
        PACK_CASE(I16x8UConvertI32x4, int4, i32x4, int8, 8, uint16_t, int16_t,
                  true)
        PACK_CASE(I8x16SConvertI16x8, int8, i16x8, int16, 16, int8_t, int8_t,
                  false)
        PACK_CASE(I8x16UConvertI16x8, int8, i16x8, int16, 16, uint8_t, int8_t,
                  true)
#undef PACK_CASE
      case kExprS128Select: {
        int4 bool_val = Pop().to_s128().to_i32x4();
        int4 v2 = Pop().to_s128().to_i32x4();
        int4 v1 = Pop().to_s128().to_i32x4();
        int4 res;
        for (size_t i = 0; i < 4; ++i) {
          res.val[i] = v2.val[i] ^ ((v1.val[i] ^ v2.val[i]) & bool_val.val[i]);
        }
        Push(WasmValue(Simd128(res)));
        return true;
      }
#define ADD_HORIZ_CASE(op, name, stype, count)                   \
  case kExpr##op: {                                              \
    WasmValue v2 = Pop();                                        \
    WasmValue v1 = Pop();                                        \
    stype s1 = v1.to_s128().to_##name();                         \
    stype s2 = v2.to_s128().to_##name();                         \
    stype res;                                                   \
    for (size_t i = 0; i < count / 2; ++i) {                     \
      res.val[LANE(i, s1)] =                                     \
          s1.val[LANE(i * 2, s1)] + s1.val[LANE(i * 2 + 1, s1)]; \
      res.val[LANE(i + count / 2, s1)] =                         \
          s2.val[LANE(i * 2, s1)] + s2.val[LANE(i * 2 + 1, s1)]; \
    }                                                            \
    Push(WasmValue(Simd128(res)));                               \
    return true;                                                 \
  }
        ADD_HORIZ_CASE(I32x4AddHoriz, i32x4, int4, 4)
        ADD_HORIZ_CASE(F32x4AddHoriz, f32x4, float4, 4)
        ADD_HORIZ_CASE(I16x8AddHoriz, i16x8, int8, 8)
#undef ADD_HORIZ_CASE
      case kExprS8x16Shuffle: {
        Simd8x16ShuffleImmediate<Decoder::kNoValidate> imm(decoder,
                                                           code->at(pc));
        *len += 16;
        int16 v2 = Pop().to_s128().to_i8x16();
        int16 v1 = Pop().to_s128().to_i8x16();
        int16 res;
        for (size_t i = 0; i < kSimd128Size; ++i) {
          int lane = imm.shuffle[i];
          res.val[LANE(i, v1)] = lane < kSimd128Size
                                     ? v1.val[LANE(lane, v1)]
                                     : v2.val[LANE(lane - kSimd128Size, v1)];
        }
        Push(WasmValue(Simd128(res)));
        return true;
      }
      case kExprS1x2AnyTrue:
      case kExprS1x4AnyTrue:
      case kExprS1x8AnyTrue:
      case kExprS1x16AnyTrue: {
        int4 s = Pop().to_s128().to_i32x4();
        bool res = s.val[0] | s.val[1] | s.val[2] | s.val[3];
        Push(WasmValue((res)));
        return true;
      }
#define REDUCTION_CASE(op, name, stype, count, operation) \
  case kExpr##op: {                                       \
    stype s = Pop().to_s128().to_##name();                \
    bool res = true;                                      \
    for (size_t i = 0; i < count; ++i) {                  \
      res = res & static_cast<bool>(s.val[i]);            \
    }                                                     \
    Push(WasmValue(res));                                 \
    return true;                                          \
  }
        REDUCTION_CASE(S1x2AllTrue, i64x2, int2, 2, &)
        REDUCTION_CASE(S1x4AllTrue, i32x4, int4, 4, &)
        REDUCTION_CASE(S1x8AllTrue, i16x8, int8, 8, &)
        REDUCTION_CASE(S1x16AllTrue, i8x16, int16, 16, &)
#undef REDUCTION_CASE
      default:
        return false;
    }
  }

  // Check if our control stack (frames_) exceeds the limit. Trigger stack
  // overflow if it does, and unwinding the current frame.
  // Returns true if execution can continue, false if the current activation was
  // fully unwound.
  // Do call this function immediately *after* pushing a new frame. The pc of
  // the top frame will be reset to 0 if the stack check fails.
  bool DoStackCheck() V8_WARN_UNUSED_RESULT {
    // The goal of this stack check is not to prevent actual stack overflows,
    // but to simulate stack overflows during the execution of compiled code.
    // That is why this function uses FLAG_stack_size, even though the value
    // stack actually lies in zone memory.
    const size_t stack_size_limit = FLAG_stack_size * KB;
    // Sum up the value stack size and the control stack size.
    const size_t current_stack_size = (sp_ - stack_.get()) * sizeof(*sp_) +
                                      frames_.size() * sizeof(frames_[0]);
    if (V8_LIKELY(current_stack_size <= stack_size_limit)) {
      return true;
    }
    // The pc of the top frame is initialized to the first instruction. We reset
    // it to 0 here such that we report the same position as in compiled code.
    frames_.back().pc = 0;
    isolate_->StackOverflow();
    return HandleException(isolate_) == WasmInterpreter::Thread::HANDLED;
  }

  void EncodeI32ExceptionValue(Handle<FixedArray> encoded_values,
                               uint32_t* encoded_index, uint32_t value) {
    encoded_values->set((*encoded_index)++, Smi::FromInt(value >> 16));
    encoded_values->set((*encoded_index)++, Smi::FromInt(value & 0xffff));
  }

  void EncodeI64ExceptionValue(Handle<FixedArray> encoded_values,
                               uint32_t* encoded_index, uint64_t value) {
    EncodeI32ExceptionValue(encoded_values, encoded_index,
                            static_cast<uint32_t>(value >> 32));
    EncodeI32ExceptionValue(encoded_values, encoded_index,
                            static_cast<uint32_t>(value));
  }

  // Allocate, initialize and throw a new exception. The exception values are
  // being popped off the operand stack. Returns true if the exception is being
  // handled locally by the interpreter, false otherwise (interpreter exits).
  bool DoThrowException(const WasmException* exception,
                        uint32_t index) V8_WARN_UNUSED_RESULT {
    HandleScope handle_scope(isolate_);  // Avoid leaking handles.
    Handle<WasmExceptionTag> exception_tag(
        WasmExceptionTag::cast(instance_object_->exceptions_table().get(index)),
        isolate_);
    uint32_t encoded_size = WasmExceptionPackage::GetEncodedSize(exception);
    Handle<Object> exception_object =
        WasmExceptionPackage::New(isolate_, exception_tag, encoded_size);
    Handle<FixedArray> encoded_values = Handle<FixedArray>::cast(
        WasmExceptionPackage::GetExceptionValues(isolate_, exception_object));
    // Encode the exception values on the operand stack into the exception
    // package allocated above. This encoding has to be in sync with other
    // backends so that exceptions can be passed between them.
    const WasmExceptionSig* sig = exception->sig;
    uint32_t encoded_index = 0;
    sp_t base_index = StackHeight() - sig->parameter_count();
    for (size_t i = 0; i < sig->parameter_count(); ++i) {
      WasmValue value = GetStackValue(base_index + i);
      switch (sig->GetParam(i)) {
        case kWasmI32: {
          uint32_t u32 = value.to_u32();
          EncodeI32ExceptionValue(encoded_values, &encoded_index, u32);
          break;
        }
        case kWasmF32: {
          uint32_t f32 = value.to_f32_boxed().get_bits();
          EncodeI32ExceptionValue(encoded_values, &encoded_index, f32);
          break;
        }
        case kWasmI64: {
          uint64_t u64 = value.to_u64();
          EncodeI64ExceptionValue(encoded_values, &encoded_index, u64);
          break;
        }
        case kWasmF64: {
          uint64_t f64 = value.to_f64_boxed().get_bits();
          EncodeI64ExceptionValue(encoded_values, &encoded_index, f64);
          break;
        }
        case kWasmS128: {
          int4 s128 = value.to_s128().to_i32x4();
          EncodeI32ExceptionValue(encoded_values, &encoded_index, s128.val[0]);
          EncodeI32ExceptionValue(encoded_values, &encoded_index, s128.val[1]);
          EncodeI32ExceptionValue(encoded_values, &encoded_index, s128.val[2]);
          EncodeI32ExceptionValue(encoded_values, &encoded_index, s128.val[3]);
          break;
        }
        case kWasmAnyRef:
        case kWasmFuncRef:
        case kWasmExnRef: {
          Handle<Object> anyref = value.to_anyref();
          encoded_values->set(encoded_index++, *anyref);
          break;
        }
        default:
          UNREACHABLE();
      }
    }
    DCHECK_EQ(encoded_size, encoded_index);
    Drop(static_cast<int>(sig->parameter_count()));
    // Now that the exception is ready, set it as pending.
    isolate_->Throw(*exception_object);
    return HandleException(isolate_) == WasmInterpreter::Thread::HANDLED;
  }

  // Throw a given existing exception. Returns true if the exception is being
  // handled locally by the interpreter, false otherwise (interpreter exits).
  bool DoRethrowException(WasmValue exception) {
    isolate_->ReThrow(*exception.to_anyref());
    return HandleException(isolate_) == WasmInterpreter::Thread::HANDLED;
  }

  // Determines whether the given exception has a tag matching the expected tag
  // for the given index within the exception table of the current instance.
  bool MatchingExceptionTag(Handle<Object> exception_object, uint32_t index) {
    Handle<Object> caught_tag =
        WasmExceptionPackage::GetExceptionTag(isolate_, exception_object);
    Handle<Object> expected_tag =
        handle(instance_object_->exceptions_table().get(index), isolate_);
    DCHECK(expected_tag->IsWasmExceptionTag());
    return expected_tag.is_identical_to(caught_tag);
  }

  void DecodeI32ExceptionValue(Handle<FixedArray> encoded_values,
                               uint32_t* encoded_index, uint32_t* value) {
    uint32_t msb = Smi::cast(encoded_values->get((*encoded_index)++)).value();
    uint32_t lsb = Smi::cast(encoded_values->get((*encoded_index)++)).value();
    *value = (msb << 16) | (lsb & 0xffff);
  }

  void DecodeI64ExceptionValue(Handle<FixedArray> encoded_values,
                               uint32_t* encoded_index, uint64_t* value) {
    uint32_t lsb = 0, msb = 0;
    DecodeI32ExceptionValue(encoded_values, encoded_index, &msb);
    DecodeI32ExceptionValue(encoded_values, encoded_index, &lsb);
    *value = (static_cast<uint64_t>(msb) << 32) | static_cast<uint64_t>(lsb);
  }

  // Unpack the values encoded in the given exception. The exception values are
  // pushed onto the operand stack. Callers must perform a tag check to ensure
  // the encoded values match the expected signature of the exception.
  void DoUnpackException(const WasmException* exception,
                         Handle<Object> exception_object) {
    Handle<FixedArray> encoded_values = Handle<FixedArray>::cast(
        WasmExceptionPackage::GetExceptionValues(isolate_, exception_object));
    // Decode the exception values from the given exception package and push
    // them onto the operand stack. This encoding has to be in sync with other
    // backends so that exceptions can be passed between them.
    const WasmExceptionSig* sig = exception->sig;
    uint32_t encoded_index = 0;
    for (size_t i = 0; i < sig->parameter_count(); ++i) {
      WasmValue value;
      switch (sig->GetParam(i)) {
        case kWasmI32: {
          uint32_t u32 = 0;
          DecodeI32ExceptionValue(encoded_values, &encoded_index, &u32);
          value = WasmValue(u32);
          break;
        }
        case kWasmF32: {
          uint32_t f32_bits = 0;
          DecodeI32ExceptionValue(encoded_values, &encoded_index, &f32_bits);
          value = WasmValue(Float32::FromBits(f32_bits));
          break;
        }
        case kWasmI64: {
          uint64_t u64 = 0;
          DecodeI64ExceptionValue(encoded_values, &encoded_index, &u64);
          value = WasmValue(u64);
          break;
        }
        case kWasmF64: {
          uint64_t f64_bits = 0;
          DecodeI64ExceptionValue(encoded_values, &encoded_index, &f64_bits);
          value = WasmValue(Float64::FromBits(f64_bits));
          break;
        }
        case kWasmS128: {
          int4 s128 = {0, 0, 0, 0};
          uint32_t* vals = reinterpret_cast<uint32_t*>(s128.val);
          DecodeI32ExceptionValue(encoded_values, &encoded_index, &vals[0]);
          DecodeI32ExceptionValue(encoded_values, &encoded_index, &vals[1]);
          DecodeI32ExceptionValue(encoded_values, &encoded_index, &vals[2]);
          DecodeI32ExceptionValue(encoded_values, &encoded_index, &vals[3]);
          value = WasmValue(Simd128(s128));
          break;
        }
        case kWasmAnyRef:
        case kWasmFuncRef:
        case kWasmExnRef: {
          Handle<Object> anyref(encoded_values->get(encoded_index++), isolate_);
          value = WasmValue(anyref);
          break;
        }
        default:
          UNREACHABLE();
      }
      Push(value);
    }
    DCHECK_EQ(WasmExceptionPackage::GetEncodedSize(exception), encoded_index);
  }

  void Execute(InterpreterCode* code, pc_t pc, int max) {
    DCHECK_NOT_NULL(code->side_table);
    DCHECK(!frames_.empty());
    // There must be enough space on the stack to hold the arguments, locals,
    // and the value stack.
    DCHECK_LE(code->function->sig->parameter_count() +
                  code->locals.type_list.size() +
                  code->side_table->max_stack_height_,
              stack_limit_ - stack_.get() - frames_.back().sp);
    // Seal the surrounding {HandleScope} to ensure that all cases within the
    // interpreter switch below which deal with handles open their own scope.
    // This avoids leaking / accumulating handles in the surrounding scope.
    SealHandleScope shs(isolate_);

    Decoder decoder(code->start, code->end);
    pc_t limit = code->end - code->start;
    bool hit_break = false;

    while (true) {
#define PAUSE_IF_BREAK_FLAG(flag)                                     \
  if (V8_UNLIKELY(break_flags_ & WasmInterpreter::BreakFlag::flag)) { \
    hit_break = true;                                                 \
    max = 0;                                                          \
  }

      DCHECK_GT(limit, pc);
      DCHECK_NOT_NULL(code->start);

      // Do first check for a breakpoint, in order to set hit_break correctly.
      const char* skip = "        ";
      int len = 1;
      byte orig = code->start[pc];
      WasmOpcode opcode = static_cast<WasmOpcode>(orig);
      if (WasmOpcodes::IsPrefixOpcode(opcode)) {
        opcode = static_cast<WasmOpcode>(opcode << 8 | code->start[pc + 1]);
      }
      if (V8_UNLIKELY(orig == kInternalBreakpoint)) {
        orig = code->orig_start[pc];
        if (WasmOpcodes::IsPrefixOpcode(static_cast<WasmOpcode>(orig))) {
          opcode =
              static_cast<WasmOpcode>(orig << 8 | code->orig_start[pc + 1]);
        }
        if (SkipBreakpoint(code, pc)) {
          // skip breakpoint by switching on original code.
          skip = "[skip]  ";
        } else {
          TRACE("@%-3zu: [break] %-24s:", pc, WasmOpcodes::OpcodeName(opcode));
          TraceValueStack();
          TRACE("\n");
          hit_break = true;
          break;
        }
      }

      // If max is 0, break. If max is positive (a limit is set), decrement it.
      if (max == 0) break;
      if (max > 0) --max;

      USE(skip);
      TRACE("@%-3zu: %s%-24s:", pc, skip, WasmOpcodes::OpcodeName(opcode));
      TraceValueStack();
      TRACE("\n");

#ifdef DEBUG
      // Compute the stack effect of this opcode, and verify later that the
      // stack was modified accordingly.
      std::pair<uint32_t, uint32_t> stack_effect =
          StackEffect(codemap_->module(), frames_.back().code->function->sig,
                      code->orig_start + pc, code->orig_end);
      sp_t expected_new_stack_height =
          StackHeight() - stack_effect.first + stack_effect.second;
#endif

      switch (orig) {
        case kExprNop:
          break;
        case kExprBlock:
        case kExprLoop:
        case kExprTry: {
          BlockTypeImmediate<Decoder::kNoValidate> imm(kAllWasmFeatures,
                                                       &decoder, code->at(pc));
          len = 1 + imm.length;
          break;
        }
        case kExprIf: {
          BlockTypeImmediate<Decoder::kNoValidate> imm(kAllWasmFeatures,
                                                       &decoder, code->at(pc));
          WasmValue cond = Pop();
          bool is_true = cond.to<uint32_t>() != 0;
          if (is_true) {
            // fall through to the true block.
            len = 1 + imm.length;
            TRACE("  true => fallthrough\n");
          } else {
            len = LookupTargetDelta(code, pc);
            TRACE("  false => @%zu\n", pc + len);
          }
          break;
        }
        case kExprElse:
        case kExprCatch: {
          len = LookupTargetDelta(code, pc);
          TRACE("  end => @%zu\n", pc + len);
          break;
        }
        case kExprThrow: {
          ExceptionIndexImmediate<Decoder::kNoValidate> imm(&decoder,
                                                            code->at(pc));
          CommitPc(pc);  // Needed for local unwinding.
          const WasmException* exception = &module()->exceptions[imm.index];
          if (!DoThrowException(exception, imm.index)) return;
          ReloadFromFrameOnException(&decoder, &code, &pc, &limit);
          continue;  // Do not bump pc.
        }
        case kExprRethrow: {
          HandleScope handle_scope(isolate_);  // Avoid leaking handles.
          WasmValue ex = Pop();
          CommitPc(pc);  // Needed for local unwinding.
          if (!DoRethrowException(ex)) return;
          ReloadFromFrameOnException(&decoder, &code, &pc, &limit);
          continue;  // Do not bump pc.
        }
        case kExprBrOnExn: {
          BranchOnExceptionImmediate<Decoder::kNoValidate> imm(&decoder,
                                                               code->at(pc));
          HandleScope handle_scope(isolate_);  // Avoid leaking handles.
          WasmValue ex = Pop();
          Handle<Object> exception = ex.to_anyref();
          if (MatchingExceptionTag(exception, imm.index.index)) {
            imm.index.exception = &module()->exceptions[imm.index.index];
            DoUnpackException(imm.index.exception, exception);
            len = DoBreak(code, pc, imm.depth.depth);
            TRACE("  match => @%zu\n", pc + len);
          } else {
            Push(ex);  // Exception remains on stack.
            TRACE("  false => fallthrough\n");
            len = 1 + imm.length;
          }
          break;
        }
        case kExprSelectWithType: {
          SelectTypeImmediate<Decoder::kNoValidate> imm(&decoder, code->at(pc));
          len = 1 + imm.length;
          V8_FALLTHROUGH;
        }
        case kExprSelect: {
          WasmValue cond = Pop();
          WasmValue fval = Pop();
          WasmValue tval = Pop();
          Push(cond.to<int32_t>() != 0 ? tval : fval);
          break;
        }
        case kExprBr: {
          BranchDepthImmediate<Decoder::kNoValidate> imm(&decoder,
                                                         code->at(pc));
          len = DoBreak(code, pc, imm.depth);
          TRACE("  br => @%zu\n", pc + len);
          break;
        }
        case kExprBrIf: {
          BranchDepthImmediate<Decoder::kNoValidate> imm(&decoder,
                                                         code->at(pc));
          WasmValue cond = Pop();
          bool is_true = cond.to<uint32_t>() != 0;
          if (is_true) {
            len = DoBreak(code, pc, imm.depth);
            TRACE("  br_if => @%zu\n", pc + len);
          } else {
            TRACE("  false => fallthrough\n");
            len = 1 + imm.length;
          }
          break;
        }
        case kExprBrTable: {
          BranchTableImmediate<Decoder::kNoValidate> imm(&decoder,
                                                         code->at(pc));
          BranchTableIterator<Decoder::kNoValidate> iterator(&decoder, imm);
          uint32_t key = Pop().to<uint32_t>();
          uint32_t depth = 0;
          if (key >= imm.table_count) key = imm.table_count;
          for (uint32_t i = 0; i <= key; i++) {
            DCHECK(iterator.has_next());
            depth = iterator.next();
          }
          len = key + DoBreak(code, pc + key, static_cast<size_t>(depth));
          TRACE("  br[%u] => @%zu\n", key, pc + key + len);
          break;
        }
        case kExprReturn: {
          size_t arity = code->function->sig->return_count();
          if (!DoReturn(&decoder, &code, &pc, &limit, arity)) return;
          PAUSE_IF_BREAK_FLAG(AfterReturn);
          continue;  // Do not bump pc.
        }
        case kExprUnreachable: {
          return DoTrap(kTrapUnreachable, pc);
        }
        case kExprEnd: {
          break;
        }
        case kExprI32Const: {
          ImmI32Immediate<Decoder::kNoValidate> imm(&decoder, code->at(pc));
          Push(WasmValue(imm.value));
          len = 1 + imm.length;
          break;
        }
        case kExprI64Const: {
          ImmI64Immediate<Decoder::kNoValidate> imm(&decoder, code->at(pc));
          Push(WasmValue(imm.value));
          len = 1 + imm.length;
          break;
        }
        case kExprF32Const: {
          ImmF32Immediate<Decoder::kNoValidate> imm(&decoder, code->at(pc));
          Push(WasmValue(imm.value));
          len = 1 + imm.length;
          break;
        }
        case kExprF64Const: {
          ImmF64Immediate<Decoder::kNoValidate> imm(&decoder, code->at(pc));
          Push(WasmValue(imm.value));
          len = 1 + imm.length;
          break;
        }
        case kExprRefNull: {
          Push(WasmValue(isolate_->factory()->null_value()));
          break;
        }
        case kExprRefFunc: {
          FunctionIndexImmediate<Decoder::kNoValidate> imm(&decoder,
                                                           code->at(pc));
          HandleScope handle_scope(isolate_);  // Avoid leaking handles.

          Handle<WasmExportedFunction> function =
              WasmInstanceObject::GetOrCreateWasmExportedFunction(
                  isolate_, instance_object_, imm.index);
          Push(WasmValue(function));
          len = 1 + imm.length;
          break;
        }
        case kExprGetLocal: {
          LocalIndexImmediate<Decoder::kNoValidate> imm(&decoder, code->at(pc));
          HandleScope handle_scope(isolate_);  // Avoid leaking handles.
          Push(GetStackValue(frames_.back().sp + imm.index));
          len = 1 + imm.length;
          break;
        }
        case kExprSetLocal: {
          LocalIndexImmediate<Decoder::kNoValidate> imm(&decoder, code->at(pc));
          HandleScope handle_scope(isolate_);  // Avoid leaking handles.
          WasmValue val = Pop();
          SetStackValue(frames_.back().sp + imm.index, val);
          len = 1 + imm.length;
          break;
        }
        case kExprTeeLocal: {
          LocalIndexImmediate<Decoder::kNoValidate> imm(&decoder, code->at(pc));
          HandleScope handle_scope(isolate_);  // Avoid leaking handles.
          WasmValue val = Pop();
          SetStackValue(frames_.back().sp + imm.index, val);
          Push(val);
          len = 1 + imm.length;
          break;
        }
        case kExprDrop: {
          Drop();
          break;
        }
        case kExprCallFunction: {
          CallFunctionImmediate<Decoder::kNoValidate> imm(&decoder,
                                                          code->at(pc));
          InterpreterCode* target = codemap()->GetCode(imm.index);
          if (target->function->imported) {
            CommitPc(pc);
            ExternalCallResult result =
                CallImportedFunction(target->function->func_index);
            switch (result.type) {
              case ExternalCallResult::INTERNAL:
                // The import is a function of this instance. Call it directly.
                DCHECK(!result.interpreter_code->function->imported);
                break;
              case ExternalCallResult::INVALID_FUNC:
              case ExternalCallResult::SIGNATURE_MISMATCH:
                // Direct calls are checked statically.
                UNREACHABLE();
              case ExternalCallResult::EXTERNAL_RETURNED:
                PAUSE_IF_BREAK_FLAG(AfterCall);
                len = 1 + imm.length;
                break;
              case ExternalCallResult::EXTERNAL_UNWOUND:
                return;
              case ExternalCallResult::EXTERNAL_CAUGHT:
                ReloadFromFrameOnException(&decoder, &code, &pc, &limit);
                continue;  // Do not bump pc.
            }
            if (result.type != ExternalCallResult::INTERNAL) break;
          }
          // Execute an internal call.
          if (!DoCall(&decoder, target, &pc, &limit)) return;
          code = target;
          PAUSE_IF_BREAK_FLAG(AfterCall);
          continue;  // Do not bump pc.
        } break;

        case kExprCallIndirect: {
          CallIndirectImmediate<Decoder::kNoValidate> imm(
              kAllWasmFeatures, &decoder, code->at(pc));
          uint32_t entry_index = Pop().to<uint32_t>();
          CommitPc(pc);  // TODO(wasm): Be more disciplined about committing PC.
          ExternalCallResult result =
              CallIndirectFunction(imm.table_index, entry_index, imm.sig_index);
          switch (result.type) {
            case ExternalCallResult::INTERNAL:
              // The import is a function of this instance. Call it directly.
              if (!DoCall(&decoder, result.interpreter_code, &pc, &limit))
                return;
              code = result.interpreter_code;
              PAUSE_IF_BREAK_FLAG(AfterCall);
              continue;  // Do not bump pc.
            case ExternalCallResult::INVALID_FUNC:
              return DoTrap(kTrapFuncInvalid, pc);
            case ExternalCallResult::SIGNATURE_MISMATCH:
              return DoTrap(kTrapFuncSigMismatch, pc);
            case ExternalCallResult::EXTERNAL_RETURNED:
              PAUSE_IF_BREAK_FLAG(AfterCall);
              len = 1 + imm.length;
              break;
            case ExternalCallResult::EXTERNAL_UNWOUND:
              return;
            case ExternalCallResult::EXTERNAL_CAUGHT:
              ReloadFromFrameOnException(&decoder, &code, &pc, &limit);
              continue;  // Do not bump pc.
          }
        } break;

        case kExprReturnCall: {
          CallFunctionImmediate<Decoder::kNoValidate> imm(&decoder,
                                                          code->at(pc));
          InterpreterCode* target = codemap()->GetCode(imm.index);

          if (!target->function->imported) {
            // Enter internal found function.
            if (!DoReturnCall(&decoder, target, &pc, &limit)) return;
            code = target;
            PAUSE_IF_BREAK_FLAG(AfterCall);

            continue;  // Do not bump pc.
          }
          // Function is imported.
          CommitPc(pc);
          ExternalCallResult result =
              CallImportedFunction(target->function->func_index);
          switch (result.type) {
            case ExternalCallResult::INTERNAL:
              // Cannot import internal functions.
            case ExternalCallResult::INVALID_FUNC:
            case ExternalCallResult::SIGNATURE_MISMATCH:
              // Direct calls are checked statically.
              UNREACHABLE();
            case ExternalCallResult::EXTERNAL_RETURNED:
              len = 1 + imm.length;
              break;
            case ExternalCallResult::EXTERNAL_UNWOUND:
              return;
            case ExternalCallResult::EXTERNAL_CAUGHT:
              ReloadFromFrameOnException(&decoder, &code, &pc, &limit);
              continue;
          }
          size_t arity = code->function->sig->return_count();
          if (!DoReturn(&decoder, &code, &pc, &limit, arity)) return;
          PAUSE_IF_BREAK_FLAG(AfterReturn);
          continue;
        } break;

        case kExprReturnCallIndirect: {
          CallIndirectImmediate<Decoder::kNoValidate> imm(
              kAllWasmFeatures, &decoder, code->at(pc));
          uint32_t entry_index = Pop().to<uint32_t>();
          CommitPc(pc);  // TODO(wasm): Be more disciplined about committing PC.

          // TODO(wasm): Calling functions needs some refactoring to avoid
          // multi-exit code like this.
          ExternalCallResult result =
              CallIndirectFunction(imm.table_index, entry_index, imm.sig_index);
          switch (result.type) {
            case ExternalCallResult::INTERNAL: {
              InterpreterCode* target = result.interpreter_code;

              DCHECK(!target->function->imported);

              // The function belongs to this instance. Enter it directly.
              if (!DoReturnCall(&decoder, target, &pc, &limit)) return;
              code = result.interpreter_code;
              PAUSE_IF_BREAK_FLAG(AfterCall);
              continue;  // Do not bump pc.
            }
            case ExternalCallResult::INVALID_FUNC:
              return DoTrap(kTrapFuncInvalid, pc);
            case ExternalCallResult::SIGNATURE_MISMATCH:
              return DoTrap(kTrapFuncSigMismatch, pc);
            case ExternalCallResult::EXTERNAL_RETURNED: {
              len = 1 + imm.length;

              size_t arity = code->function->sig->return_count();
              if (!DoReturn(&decoder, &code, &pc, &limit, arity)) return;
              PAUSE_IF_BREAK_FLAG(AfterCall);
              break;
            }
            case ExternalCallResult::EXTERNAL_UNWOUND:
              return;

            case ExternalCallResult::EXTERNAL_CAUGHT:
              ReloadFromFrameOnException(&decoder, &code, &pc, &limit);
              break;
          }
        } break;

        case kExprGetGlobal: {
          GlobalIndexImmediate<Decoder::kNoValidate> imm(&decoder,
                                                         code->at(pc));
          HandleScope handle_scope(isolate_);
          Push(GetGlobalValue(imm.index));
          len = 1 + imm.length;
          break;
        }
        case kExprSetGlobal: {
          GlobalIndexImmediate<Decoder::kNoValidate> imm(&decoder,
                                                         code->at(pc));
          const WasmGlobal* global = &module()->globals[imm.index];
          switch (global->type) {
#define CASE_TYPE(wasm, ctype)                                    \
  case kWasm##wasm: {                                             \
    byte* ptr = GetGlobalPtr(global);                             \
    WriteLittleEndianValue<ctype>(reinterpret_cast<Address>(ptr), \
                                  Pop().to<ctype>());             \
    break;                                                        \
  }
            WASM_CTYPES(CASE_TYPE)
#undef CASE_TYPE
            case kWasmAnyRef:
            case kWasmFuncRef:
            case kWasmExnRef: {
              HandleScope handle_scope(isolate_);  // Avoid leaking handles.
              Handle<FixedArray> global_buffer;    // The buffer of the global.
              uint32_t global_index = 0;           // The index into the buffer.
              GetGlobalBufferAndIndex(global, &global_buffer, &global_index);
              global_buffer->set(global_index, *Pop().to_anyref());
              break;
            }
            default:
              UNREACHABLE();
          }
          len = 1 + imm.length;
          break;
        }
        case kExprTableGet: {
          TableIndexImmediate<Decoder::kNoValidate> imm(&decoder, code->at(pc));
          HandleScope handle_scope(isolate_);
          auto table = handle(
              WasmTableObject::cast(instance_object_->tables().get(imm.index)),
              isolate_);
          uint32_t table_size = table->current_length();
          uint32_t entry_index = Pop().to<uint32_t>();
          if (entry_index >= table_size) {
            return DoTrap(kTrapTableOutOfBounds, pc);
          }
          Handle<Object> value =
              WasmTableObject::Get(isolate_, table, entry_index);
          Push(WasmValue(value));
          len = 1 + imm.length;
          break;
        }
        case kExprTableSet: {
          TableIndexImmediate<Decoder::kNoValidate> imm(&decoder, code->at(pc));
          HandleScope handle_scope(isolate_);
          auto table = handle(
              WasmTableObject::cast(instance_object_->tables().get(imm.index)),
              isolate_);
          uint32_t table_size = table->current_length();
          Handle<Object> value = Pop().to_anyref();
          uint32_t entry_index = Pop().to<uint32_t>();
          if (entry_index >= table_size) {
            return DoTrap(kTrapTableOutOfBounds, pc);
          }
          WasmTableObject::Set(isolate_, table, entry_index, value);
          len = 1 + imm.length;
          break;
        }
#define LOAD_CASE(name, ctype, mtype, rep)                      \
  case kExpr##name: {                                           \
    if (!ExecuteLoad<ctype, mtype>(&decoder, code, pc, &len,    \
                                   MachineRepresentation::rep)) \
      return;                                                   \
    break;                                                      \
  }

          LOAD_CASE(I32LoadMem8S, int32_t, int8_t, kWord8);
          LOAD_CASE(I32LoadMem8U, int32_t, uint8_t, kWord8);
          LOAD_CASE(I32LoadMem16S, int32_t, int16_t, kWord16);
          LOAD_CASE(I32LoadMem16U, int32_t, uint16_t, kWord16);
          LOAD_CASE(I64LoadMem8S, int64_t, int8_t, kWord8);
          LOAD_CASE(I64LoadMem8U, int64_t, uint8_t, kWord16);
          LOAD_CASE(I64LoadMem16S, int64_t, int16_t, kWord16);
          LOAD_CASE(I64LoadMem16U, int64_t, uint16_t, kWord16);
          LOAD_CASE(I64LoadMem32S, int64_t, int32_t, kWord32);
          LOAD_CASE(I64LoadMem32U, int64_t, uint32_t, kWord32);
          LOAD_CASE(I32LoadMem, int32_t, int32_t, kWord32);
          LOAD_CASE(I64LoadMem, int64_t, int64_t, kWord64);
          LOAD_CASE(F32LoadMem, Float32, uint32_t, kFloat32);
          LOAD_CASE(F64LoadMem, Float64, uint64_t, kFloat64);
#undef LOAD_CASE

#define STORE_CASE(name, ctype, mtype, rep)                      \
  case kExpr##name: {                                            \
    if (!ExecuteStore<ctype, mtype>(&decoder, code, pc, &len,    \
                                    MachineRepresentation::rep)) \
      return;                                                    \
    break;                                                       \
  }

          STORE_CASE(I32StoreMem8, int32_t, int8_t, kWord8);
          STORE_CASE(I32StoreMem16, int32_t, int16_t, kWord16);
          STORE_CASE(I64StoreMem8, int64_t, int8_t, kWord8);
          STORE_CASE(I64StoreMem16, int64_t, int16_t, kWord16);
          STORE_CASE(I64StoreMem32, int64_t, int32_t, kWord32);
          STORE_CASE(I32StoreMem, int32_t, int32_t, kWord32);
          STORE_CASE(I64StoreMem, int64_t, int64_t, kWord64);
          STORE_CASE(F32StoreMem, Float32, uint32_t, kFloat32);
          STORE_CASE(F64StoreMem, Float64, uint64_t, kFloat64);
#undef STORE_CASE

#define ASMJS_LOAD_CASE(name, ctype, mtype, defval)                 \
  case kExpr##name: {                                               \
    uint32_t index = Pop().to<uint32_t>();                          \
    ctype result;                                                   \
    Address addr = BoundsCheckMem<mtype>(0, index);                 \
    if (!addr) {                                                    \
      result = defval;                                              \
    } else {                                                        \
      /* TODO(titzer): alignment for asmjs load mem? */             \
      result = static_cast<ctype>(*reinterpret_cast<mtype*>(addr)); \
    }                                                               \
    Push(WasmValue(result));                                        \
    break;                                                          \
  }
          ASMJS_LOAD_CASE(I32AsmjsLoadMem8S, int32_t, int8_t, 0);
          ASMJS_LOAD_CASE(I32AsmjsLoadMem8U, int32_t, uint8_t, 0);
          ASMJS_LOAD_CASE(I32AsmjsLoadMem16S, int32_t, int16_t, 0);
          ASMJS_LOAD_CASE(I32AsmjsLoadMem16U, int32_t, uint16_t, 0);
          ASMJS_LOAD_CASE(I32AsmjsLoadMem, int32_t, int32_t, 0);
          ASMJS_LOAD_CASE(F32AsmjsLoadMem, float, float,
                          std::numeric_limits<float>::quiet_NaN());
          ASMJS_LOAD_CASE(F64AsmjsLoadMem, double, double,
                          std::numeric_limits<double>::quiet_NaN());
#undef ASMJS_LOAD_CASE

#define ASMJS_STORE_CASE(name, ctype, mtype)                                   \
  case kExpr##name: {                                                          \
    WasmValue val = Pop();                                                     \
    uint32_t index = Pop().to<uint32_t>();                                     \
    Address addr = BoundsCheckMem<mtype>(0, index);                            \
    if (addr) {                                                                \
      *(reinterpret_cast<mtype*>(addr)) = static_cast<mtype>(val.to<ctype>()); \
    }                                                                          \
    Push(val);                                                                 \
    break;                                                                     \
  }

          ASMJS_STORE_CASE(I32AsmjsStoreMem8, int32_t, int8_t);
          ASMJS_STORE_CASE(I32AsmjsStoreMem16, int32_t, int16_t);
          ASMJS_STORE_CASE(I32AsmjsStoreMem, int32_t, int32_t);
          ASMJS_STORE_CASE(F32AsmjsStoreMem, float, float);
          ASMJS_STORE_CASE(F64AsmjsStoreMem, double, double);
#undef ASMJS_STORE_CASE
        case kExprMemoryGrow: {
          MemoryIndexImmediate<Decoder::kNoValidate> imm(&decoder,
                                                         code->at(pc));
          uint32_t delta_pages = Pop().to<uint32_t>();
          HandleScope handle_scope(isolate_);  // Avoid leaking handles.
          Handle<WasmMemoryObject> memory(instance_object_->memory_object(),
                                          isolate_);
          int32_t result =
              WasmMemoryObject::Grow(isolate_, memory, delta_pages);
          Push(WasmValue(result));
          len = 1 + imm.length;
          // Treat one grow_memory instruction like 1000 other instructions,
          // because it is a really expensive operation.
          if (max > 0) max = std::max(0, max - 1000);
          break;
        }
        case kExprMemorySize: {
          MemoryIndexImmediate<Decoder::kNoValidate> imm(&decoder,
                                                         code->at(pc));
          Push(WasmValue(static_cast<uint32_t>(instance_object_->memory_size() /
                                               kWasmPageSize)));
          len = 1 + imm.length;
          break;
        }
        // We need to treat kExprI32ReinterpretF32 and kExprI64ReinterpretF64
        // specially to guarantee that the quiet bit of a NaN is preserved on
        // ia32 by the reinterpret casts.
        case kExprI32ReinterpretF32: {
          WasmValue val = Pop();
          Push(WasmValue(ExecuteI32ReinterpretF32(val)));
          break;
        }
        case kExprI64ReinterpretF64: {
          WasmValue val = Pop();
          Push(WasmValue(ExecuteI64ReinterpretF64(val)));
          break;
        }
#define SIGN_EXTENSION_CASE(name, wtype, ntype)        \
  case kExpr##name: {                                  \
    ntype val = static_cast<ntype>(Pop().to<wtype>()); \
    Push(WasmValue(static_cast<wtype>(val)));          \
    break;                                             \
  }
          SIGN_EXTENSION_CASE(I32SExtendI8, int32_t, int8_t);
          SIGN_EXTENSION_CASE(I32SExtendI16, int32_t, int16_t);
          SIGN_EXTENSION_CASE(I64SExtendI8, int64_t, int8_t);
          SIGN_EXTENSION_CASE(I64SExtendI16, int64_t, int16_t);
          SIGN_EXTENSION_CASE(I64SExtendI32, int64_t, int32_t);
#undef SIGN_EXTENSION_CASE
        case kExprRefIsNull: {
          HandleScope handle_scope(isolate_);  // Avoid leaking handles.
          uint32_t result = Pop().to_anyref()->IsNull() ? 1 : 0;
          Push(WasmValue(result));
          break;
        }
        case kNumericPrefix: {
          ++len;
          if (!ExecuteNumericOp(opcode, &decoder, code, pc, &len)) return;
          break;
        }
        case kAtomicPrefix: {
          if (!ExecuteAtomicOp(opcode, &decoder, code, pc, &len)) return;
          break;
        }
        case kSimdPrefix: {
          ++len;
          if (!ExecuteSimdOp(opcode, &decoder, code, pc, &len)) return;
          break;
        }

#define EXECUTE_SIMPLE_BINOP(name, ctype, op)               \
  case kExpr##name: {                                       \
    WasmValue rval = Pop();                                 \
    WasmValue lval = Pop();                                 \
    auto result = lval.to<ctype>() op rval.to<ctype>();     \
    possible_nondeterminism_ |= has_nondeterminism(result); \
    Push(WasmValue(result));                                \
    break;                                                  \
  }
          FOREACH_SIMPLE_BINOP(EXECUTE_SIMPLE_BINOP)
#undef EXECUTE_SIMPLE_BINOP

#define EXECUTE_OTHER_BINOP(name, ctype)                    \
  case kExpr##name: {                                       \
    TrapReason trap = kTrapCount;                           \
    ctype rval = Pop().to<ctype>();                         \
    ctype lval = Pop().to<ctype>();                         \
    auto result = Execute##name(lval, rval, &trap);         \
    possible_nondeterminism_ |= has_nondeterminism(result); \
    if (trap != kTrapCount) return DoTrap(trap, pc);        \
    Push(WasmValue(result));                                \
    break;                                                  \
  }
          FOREACH_OTHER_BINOP(EXECUTE_OTHER_BINOP)
#undef EXECUTE_OTHER_BINOP

#define EXECUTE_UNOP(name, ctype, exec_fn)                  \
  case kExpr##name: {                                       \
    TrapReason trap = kTrapCount;                           \
    ctype val = Pop().to<ctype>();                          \
    auto result = exec_fn(val, &trap);                      \
    possible_nondeterminism_ |= has_nondeterminism(result); \
    if (trap != kTrapCount) return DoTrap(trap, pc);        \
    Push(WasmValue(result));                                \
    break;                                                  \
  }

#define EXECUTE_OTHER_UNOP(name, ctype) EXECUTE_UNOP(name, ctype, Execute##name)
          FOREACH_OTHER_UNOP(EXECUTE_OTHER_UNOP)
#undef EXECUTE_OTHER_UNOP

#define EXECUTE_I32CONV_FLOATOP(name, out_type, in_type) \
  EXECUTE_UNOP(name, in_type, ExecuteConvert<out_type>)
          FOREACH_I32CONV_FLOATOP(EXECUTE_I32CONV_FLOATOP)
#undef EXECUTE_I32CONV_FLOATOP
#undef EXECUTE_UNOP

        default:
          FATAL("Unknown or unimplemented opcode #%d:%s", code->start[pc],
                OpcodeName(code->start[pc]));
          UNREACHABLE();
      }

#ifdef DEBUG
      if (!WasmOpcodes::IsControlOpcode(opcode)) {
        DCHECK_EQ(expected_new_stack_height, StackHeight());
      }
#endif

      pc += len;
      if (pc == limit) {
        // Fell off end of code; do an implicit return.
        TRACE("@%-3zu: ImplicitReturn\n", pc);
        size_t arity = code->function->sig->return_count();
        DCHECK_EQ(StackHeight() - arity, frames_.back().llimit());
        if (!DoReturn(&decoder, &code, &pc, &limit, arity)) return;
        PAUSE_IF_BREAK_FLAG(AfterReturn);
      }
#undef PAUSE_IF_BREAK_FLAG
    }

    state_ = WasmInterpreter::PAUSED;
    break_pc_ = hit_break ? pc : kInvalidPc;
    CommitPc(pc);
  }

  WasmValue Pop() {
    DCHECK_GT(frames_.size(), 0);
    DCHECK_GT(StackHeight(), frames_.back().llimit());  // can't pop into locals
    StackValue stack_value = *--sp_;
    // Note that {StackHeight} depends on the current {sp} value, hence this
    // operation is split into two statements to ensure proper evaluation order.
    WasmValue val = stack_value.ExtractValue(this, StackHeight());
    stack_value.ClearValue(this, StackHeight());
    return val;
  }

  void Drop(int n = 1) {
    DCHECK_GE(StackHeight(), n);
    DCHECK_GT(frames_.size(), 0);
    // Check that we don't pop into locals.
    DCHECK_GE(StackHeight() - n, frames_.back().llimit());
    StackValue::ClearValues(this, StackHeight() - n, n);
    sp_ -= n;
  }

  WasmValue PopArity(size_t arity) {
    if (arity == 0) return WasmValue();
    CHECK_EQ(1, arity);
    return Pop();
  }

  void Push(WasmValue val) {
    DCHECK_NE(kWasmStmt, val.type());
    DCHECK_LE(1, stack_limit_ - sp_);
    DCHECK(StackValue::IsClearedValue(this, StackHeight()));
    StackValue stack_value(val, this, StackHeight());
    // Note that {StackHeight} depends on the current {sp} value, hence this
    // operation is split into two statements to ensure proper evaluation order.
    *sp_++ = stack_value;
  }

  void Push(WasmValue* vals, size_t arity) {
    DCHECK_LE(arity, stack_limit_ - sp_);
    for (WasmValue *val = vals, *end = vals + arity; val != end; ++val) {
      DCHECK_NE(kWasmStmt, val->type());
      Push(*val);
    }
  }

  void ResetStack(sp_t new_height) {
    DCHECK_LE(new_height, StackHeight());  // Only allowed to shrink.
    int count = static_cast<int>(StackHeight() - new_height);
    StackValue::ClearValues(this, new_height, count);
    sp_ = stack_.get() + new_height;
  }

  void EnsureStackSpace(size_t size) {
    if (V8_LIKELY(static_cast<size_t>(stack_limit_ - sp_) >= size)) return;
    size_t old_size = stack_limit_ - stack_.get();
    size_t requested_size =
        base::bits::RoundUpToPowerOfTwo64((sp_ - stack_.get()) + size);
    size_t new_size = Max(size_t{8}, Max(2 * old_size, requested_size));
    std::unique_ptr<StackValue[]> new_stack(new StackValue[new_size]);
    if (old_size > 0) {
      memcpy(new_stack.get(), stack_.get(), old_size * sizeof(*sp_));
    }
    sp_ = new_stack.get() + (sp_ - stack_.get());
    stack_ = std::move(new_stack);
    stack_limit_ = stack_.get() + new_size;
    // Also resize the reference stack to the same size.
    int grow_by = static_cast<int>(new_size - old_size);
    HandleScope handle_scope(isolate_);  // Avoid leaking handles.
    Handle<FixedArray> old_ref_stack(reference_stack(), isolate_);
    Handle<FixedArray> new_ref_stack =
        isolate_->factory()->CopyFixedArrayAndGrow(old_ref_stack, grow_by);
    new_ref_stack->FillWithHoles(static_cast<int>(old_size),
                                 static_cast<int>(new_size));
    reference_stack_cell_->set_value(*new_ref_stack);
  }

  sp_t StackHeight() { return sp_ - stack_.get(); }

  void TraceValueStack() {
#ifdef DEBUG
    if (!FLAG_trace_wasm_interpreter) return;
    HandleScope handle_scope(isolate_);  // Avoid leaking handles.
    Frame* top = frames_.size() > 0 ? &frames_.back() : nullptr;
    sp_t sp = top ? top->sp : 0;
    sp_t plimit = top ? top->plimit() : 0;
    sp_t llimit = top ? top->llimit() : 0;
    for (size_t i = sp; i < StackHeight(); ++i) {
      if (i < plimit)
        PrintF(" p%zu:", i);
      else if (i < llimit)
        PrintF(" l%zu:", i);
      else
        PrintF(" s%zu:", i);
      WasmValue val = GetStackValue(i);
      switch (val.type()) {
        case kWasmI32:
          PrintF("i32:%d", val.to<int32_t>());
          break;
        case kWasmI64:
          PrintF("i64:%" PRId64 "", val.to<int64_t>());
          break;
        case kWasmF32:
          PrintF("f32:%f", val.to<float>());
          break;
        case kWasmF64:
          PrintF("f64:%lf", val.to<double>());
          break;
        case kWasmS128: {
          // This defaults to tracing all S128 values as i32x4 values for now,
          // when there is more state to know what type of values are on the
          // stack, the right format should be printed here.
          int4 s = val.to_s128().to_i32x4();
          PrintF("i32x4:%d,%d,%d,%d", s.val[0], s.val[1], s.val[2], s.val[3]);
          break;
        }
        case kWasmAnyRef: {
          Handle<Object> ref = val.to_anyref();
          if (ref->IsNull()) {
            PrintF("ref:null");
          } else {
            PrintF("ref:0x%" V8PRIxPTR, ref->ptr());
          }
          break;
        }
        case kWasmStmt:
          PrintF("void");
          break;
        default:
          UNREACHABLE();
          break;
      }
    }
#endif  // DEBUG
  }

  ExternalCallResult TryHandleException(Isolate* isolate) {
    DCHECK(isolate->has_pending_exception());  // Assume exceptional return.
    if (HandleException(isolate) == WasmInterpreter::Thread::UNWOUND) {
      return {ExternalCallResult::EXTERNAL_UNWOUND};
    }
    return {ExternalCallResult::EXTERNAL_CAUGHT};
  }

  ExternalCallResult CallExternalWasmFunction(Isolate* isolate,
                                              Handle<Object> object_ref,
                                              const WasmCode* code,
                                              FunctionSig* sig) {
    int num_args = static_cast<int>(sig->parameter_count());
    WasmFeatures enabled_features = WasmFeaturesFromIsolate(isolate);

    if (code->kind() == WasmCode::kWasmToJsWrapper &&
        !IsJSCompatibleSignature(sig, enabled_features.bigint)) {
      Drop(num_args);  // Pop arguments before throwing.
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kWasmTrapTypeError));
      return TryHandleException(isolate);
    }

    Handle<WasmDebugInfo> debug_info(instance_object_->debug_info(), isolate);
    Handle<Code> wasm_entry = WasmDebugInfo::GetCWasmEntry(debug_info, sig);

    TRACE("  => Calling external wasm function\n");

    // Copy the arguments to one buffer.
    CWasmArgumentsPacker packer(CWasmArgumentsPacker::TotalSize(sig));
    sp_t base_index = StackHeight() - num_args;
    for (int i = 0; i < num_args; ++i) {
      WasmValue arg = GetStackValue(base_index + i);
      switch (sig->GetParam(i)) {
        case kWasmI32:
          packer.Push(arg.to<uint32_t>());
          break;
        case kWasmI64:
          packer.Push(arg.to<uint64_t>());
          break;
        case kWasmF32:
          packer.Push(arg.to<float>());
          break;
        case kWasmF64:
          packer.Push(arg.to<double>());
          break;
        case kWasmAnyRef:
        case kWasmFuncRef:
        case kWasmExnRef:
          packer.Push(arg.to_anyref()->ptr());
          break;
        default:
          UNIMPLEMENTED();
      }
    }

    Address call_target = code->instruction_start();
    Execution::CallWasm(isolate, wasm_entry, call_target, object_ref,
                        packer.argv());
    TRACE("  => External wasm function returned%s\n",
          isolate->has_pending_exception() ? " with exception" : "");

    // Pop arguments off the stack.
    Drop(num_args);

    if (isolate->has_pending_exception()) {
      return TryHandleException(isolate);
    }

    // Push return values.
    packer.Reset();
    for (size_t i = 0; i < sig->return_count(); i++) {
      switch (sig->GetReturn(i)) {
        case kWasmI32:
          Push(WasmValue(packer.Pop<uint32_t>()));
          break;
        case kWasmI64:
          Push(WasmValue(packer.Pop<uint64_t>()));
          break;
        case kWasmF32:
          Push(WasmValue(packer.Pop<float>()));
          break;
        case kWasmF64:
          Push(WasmValue(packer.Pop<double>()));
          break;
        case kWasmAnyRef:
        case kWasmFuncRef:
        case kWasmExnRef: {
          Handle<Object> ref(Object(packer.Pop<Address>()), isolate);
          Push(WasmValue(ref));
          break;
        }
        default:
          UNIMPLEMENTED();
      }
    }
    return {ExternalCallResult::EXTERNAL_RETURNED};
  }

  static WasmCode* GetTargetCode(Isolate* isolate, Address target) {
    WasmCodeManager* code_manager = isolate->wasm_engine()->code_manager();
    NativeModule* native_module = code_manager->LookupNativeModule(target);
    if (native_module->is_jump_table_slot(target)) {
      uint32_t func_index =
          native_module->GetFunctionIndexFromJumpTableSlot(target);

      if (!native_module->HasCode(func_index)) {
        bool success = CompileLazy(isolate, native_module, func_index);
        if (!success) {
          DCHECK(isolate->has_pending_exception());
          return nullptr;
        }
      }

      return native_module->GetCode(func_index);
    }
    WasmCode* code = native_module->Lookup(target);
    DCHECK_EQ(code->instruction_start(), target);
    return code;
  }

  ExternalCallResult CallImportedFunction(uint32_t function_index) {
    DCHECK_GT(module()->num_imported_functions, function_index);
    HandleScope handle_scope(isolate_);  // Avoid leaking handles.

    ImportedFunctionEntry entry(instance_object_, function_index);
    Handle<Object> object_ref(entry.object_ref(), isolate_);
    WasmCode* code = GetTargetCode(isolate_, entry.target());

    // In case a function's body is invalid and the function is lazily validated
    // and compiled we may get an exception.
    if (code == nullptr) return TryHandleException(isolate_);

    FunctionSig* sig = module()->functions[function_index].sig;
    return CallExternalWasmFunction(isolate_, object_ref, code, sig);
  }

  ExternalCallResult CallIndirectFunction(uint32_t table_index,
                                          uint32_t entry_index,
                                          uint32_t sig_index) {
    HandleScope handle_scope(isolate_);  // Avoid leaking handles.
    uint32_t expected_sig_id = module()->signature_ids[sig_index];
    DCHECK_EQ(expected_sig_id,
              module()->signature_map.Find(*module()->signatures[sig_index]));
    // Bounds check against table size.
    if (entry_index >=
        static_cast<uint32_t>(WasmInstanceObject::IndirectFunctionTableSize(
            isolate_, instance_object_, table_index))) {
      return {ExternalCallResult::INVALID_FUNC};
    }

    IndirectFunctionTableEntry entry(instance_object_, table_index,
                                     entry_index);
    // Signature check.
    if (entry.sig_id() != static_cast<int32_t>(expected_sig_id)) {
      return {ExternalCallResult::SIGNATURE_MISMATCH};
    }

    FunctionSig* signature = module()->signatures[sig_index];
    Handle<Object> object_ref = handle(entry.object_ref(), isolate_);
    WasmCode* code = GetTargetCode(isolate_, entry.target());

    // In case a function's body is invalid and the function is lazily validated
    // and compiled we may get an exception.
    if (code == nullptr) return TryHandleException(isolate_);

    if (!object_ref->IsWasmInstanceObject() || /* call to an import */
        !instance_object_.is_identical_to(object_ref) /* cross-instance */) {
      return CallExternalWasmFunction(isolate_, object_ref, code, signature);
    }

    DCHECK(code->kind() == WasmCode::kInterpreterEntry ||
           code->kind() == WasmCode::kFunction);
    return {ExternalCallResult::INTERNAL, codemap()->GetCode(code->index())};
  }

  inline Activation current_activation() {
    return activations_.empty() ? Activation(0, 0) : activations_.back();
  }
};

class InterpretedFrameImpl {
 public:
  InterpretedFrameImpl(ThreadImpl* thread, int index)
      : thread_(thread), index_(index) {
    DCHECK_LE(0, index);
  }

  const WasmFunction* function() const { return frame()->code->function; }

  int pc() const {
    DCHECK_LE(0, frame()->pc);
    DCHECK_GE(kMaxInt, frame()->pc);
    return static_cast<int>(frame()->pc);
  }

  int GetParameterCount() const {
    DCHECK_GE(kMaxInt, function()->sig->parameter_count());
    return static_cast<int>(function()->sig->parameter_count());
  }

  int GetLocalCount() const {
    size_t num_locals = function()->sig->parameter_count() +
                        frame()->code->locals.type_list.size();
    DCHECK_GE(kMaxInt, num_locals);
    return static_cast<int>(num_locals);
  }

  int GetStackHeight() const {
    bool is_top_frame =
        static_cast<size_t>(index_) + 1 == thread_->frames_.size();
    size_t stack_limit =
        is_top_frame ? thread_->StackHeight() : thread_->frames_[index_ + 1].sp;
    DCHECK_LE(frame()->sp, stack_limit);
    size_t frame_size = stack_limit - frame()->sp;
    DCHECK_LE(GetLocalCount(), frame_size);
    return static_cast<int>(frame_size) - GetLocalCount();
  }

  WasmValue GetLocalValue(int index) const {
    DCHECK_LE(0, index);
    DCHECK_GT(GetLocalCount(), index);
    return thread_->GetStackValue(static_cast<int>(frame()->sp) + index);
  }

  WasmValue GetStackValue(int index) const {
    DCHECK_LE(0, index);
    // Index must be within the number of stack values of this frame.
    DCHECK_GT(GetStackHeight(), index);
    return thread_->GetStackValue(static_cast<int>(frame()->sp) +
                                  GetLocalCount() + index);
  }

 private:
  ThreadImpl* thread_;
  int index_;

  ThreadImpl::Frame* frame() const {
    DCHECK_GT(thread_->frames_.size(), index_);
    return &thread_->frames_[index_];
  }
};

namespace {

// Converters between WasmInterpreter::Thread and WasmInterpreter::ThreadImpl.
// Thread* is the public interface, without knowledge of the object layout.
// This cast is potentially risky, but as long as we always cast it back before
// accessing any data, it should be fine. UBSan is not complaining.
WasmInterpreter::Thread* ToThread(ThreadImpl* impl) {
  return reinterpret_cast<WasmInterpreter::Thread*>(impl);
}
ThreadImpl* ToImpl(WasmInterpreter::Thread* thread) {
  return reinterpret_cast<ThreadImpl*>(thread);
}

// Same conversion for InterpretedFrame and InterpretedFrameImpl.
InterpretedFrame* ToFrame(InterpretedFrameImpl* impl) {
  return reinterpret_cast<InterpretedFrame*>(impl);
}
const InterpretedFrameImpl* ToImpl(const InterpretedFrame* frame) {
  return reinterpret_cast<const InterpretedFrameImpl*>(frame);
}

}  // namespace

//============================================================================
// Implementation of the pimpl idiom for WasmInterpreter::Thread.
// Instead of placing a pointer to the ThreadImpl inside of the Thread object,
// we just reinterpret_cast them. ThreadImpls are only allocated inside this
// translation unit anyway.
//============================================================================
WasmInterpreter::State WasmInterpreter::Thread::state() {
  return ToImpl(this)->state();
}
void WasmInterpreter::Thread::InitFrame(const WasmFunction* function,
                                        WasmValue* args) {
  ToImpl(this)->InitFrame(function, args);
}
WasmInterpreter::State WasmInterpreter::Thread::Run(int num_steps) {
  return ToImpl(this)->Run(num_steps);
}
void WasmInterpreter::Thread::Pause() { return ToImpl(this)->Pause(); }
void WasmInterpreter::Thread::Reset() { return ToImpl(this)->Reset(); }
WasmInterpreter::Thread::ExceptionHandlingResult
WasmInterpreter::Thread::RaiseException(Isolate* isolate,
                                        Handle<Object> exception) {
  return ToImpl(this)->RaiseException(isolate, exception);
}
pc_t WasmInterpreter::Thread::GetBreakpointPc() {
  return ToImpl(this)->GetBreakpointPc();
}
int WasmInterpreter::Thread::GetFrameCount() {
  return ToImpl(this)->GetFrameCount();
}
WasmInterpreter::FramePtr WasmInterpreter::Thread::GetFrame(int index) {
  DCHECK_LE(0, index);
  DCHECK_GT(GetFrameCount(), index);
  return FramePtr(ToFrame(new InterpretedFrameImpl(ToImpl(this), index)));
}
WasmValue WasmInterpreter::Thread::GetReturnValue(int index) {
  return ToImpl(this)->GetReturnValue(index);
}
TrapReason WasmInterpreter::Thread::GetTrapReason() {
  return ToImpl(this)->GetTrapReason();
}
uint32_t WasmInterpreter::Thread::GetGlobalCount() {
  return ToImpl(this)->GetGlobalCount();
}
WasmValue WasmInterpreter::Thread::GetGlobalValue(uint32_t index) {
  return ToImpl(this)->GetGlobalValue(index);
}
bool WasmInterpreter::Thread::PossibleNondeterminism() {
  return ToImpl(this)->PossibleNondeterminism();
}
uint64_t WasmInterpreter::Thread::NumInterpretedCalls() {
  return ToImpl(this)->NumInterpretedCalls();
}
void WasmInterpreter::Thread::AddBreakFlags(uint8_t flags) {
  ToImpl(this)->AddBreakFlags(flags);
}
void WasmInterpreter::Thread::ClearBreakFlags() {
  ToImpl(this)->ClearBreakFlags();
}
uint32_t WasmInterpreter::Thread::NumActivations() {
  return ToImpl(this)->NumActivations();
}
uint32_t WasmInterpreter::Thread::StartActivation() {
  return ToImpl(this)->StartActivation();
}
void WasmInterpreter::Thread::FinishActivation(uint32_t id) {
  ToImpl(this)->FinishActivation(id);
}
uint32_t WasmInterpreter::Thread::ActivationFrameBase(uint32_t id) {
  return ToImpl(this)->ActivationFrameBase(id);
}

//============================================================================
// The implementation details of the interpreter.
//============================================================================
class WasmInterpreterInternals : public ZoneObject {
 public:
  // Create a copy of the module bytes for the interpreter, since the passed
  // pointer might be invalidated after constructing the interpreter.
  const ZoneVector<uint8_t> module_bytes_;
  CodeMap codemap_;
  ZoneVector<ThreadImpl> threads_;

  WasmInterpreterInternals(Zone* zone, const WasmModule* module,
                           const ModuleWireBytes& wire_bytes,
                           Handle<WasmInstanceObject> instance_object)
      : module_bytes_(wire_bytes.start(), wire_bytes.end(), zone),
        codemap_(module, module_bytes_.data(), zone),
        threads_(zone) {
    Isolate* isolate = instance_object->GetIsolate();
    Handle<Cell> reference_stack = isolate->global_handles()->Create(
        *isolate->factory()->NewCell(isolate->factory()->empty_fixed_array()));
    threads_.emplace_back(zone, &codemap_, instance_object, reference_stack);
  }

  ~WasmInterpreterInternals() {
    DCHECK_EQ(1, threads_.size());
    GlobalHandles::Destroy(threads_[0].reference_stack_cell().location());
  }
};

namespace {
void NopFinalizer(const v8::WeakCallbackInfo<void>& data) {
  Address* global_handle_location =
      reinterpret_cast<Address*>(data.GetParameter());
  GlobalHandles::Destroy(global_handle_location);
}

Handle<WasmInstanceObject> MakeWeak(
    Isolate* isolate, Handle<WasmInstanceObject> instance_object) {
  Handle<WasmInstanceObject> weak_instance =
      isolate->global_handles()->Create<WasmInstanceObject>(*instance_object);
  Address* global_handle_location = weak_instance.location();
  GlobalHandles::MakeWeak(global_handle_location, global_handle_location,
                          &NopFinalizer, v8::WeakCallbackType::kParameter);
  return weak_instance;
}
}  // namespace

//============================================================================
// Implementation of the public interface of the interpreter.
//============================================================================
WasmInterpreter::WasmInterpreter(Isolate* isolate, const WasmModule* module,
                                 const ModuleWireBytes& wire_bytes,
                                 Handle<WasmInstanceObject> instance_object)
    : zone_(isolate->allocator(), ZONE_NAME),
      internals_(new (&zone_) WasmInterpreterInternals(
          &zone_, module, wire_bytes, MakeWeak(isolate, instance_object))) {}

WasmInterpreter::~WasmInterpreter() { internals_->~WasmInterpreterInternals(); }

void WasmInterpreter::Run() { internals_->threads_[0].Run(); }

void WasmInterpreter::Pause() { internals_->threads_[0].Pause(); }

bool WasmInterpreter::SetBreakpoint(const WasmFunction* function, pc_t pc,
                                    bool enabled) {
  InterpreterCode* code = internals_->codemap_.GetCode(function);
  size_t size = static_cast<size_t>(code->end - code->start);
  // Check bounds for {pc}.
  if (pc < code->locals.encoded_size || pc >= size) return false;
  // Make a copy of the code before enabling a breakpoint.
  if (enabled && code->orig_start == code->start) {
    code->start = reinterpret_cast<byte*>(zone_.New(size));
    memcpy(code->start, code->orig_start, size);
    code->end = code->start + size;
  }
  bool prev = code->start[pc] == kInternalBreakpoint;
  if (enabled) {
    code->start[pc] = kInternalBreakpoint;
  } else {
    code->start[pc] = code->orig_start[pc];
  }
  return prev;
}

bool WasmInterpreter::GetBreakpoint(const WasmFunction* function, pc_t pc) {
  InterpreterCode* code = internals_->codemap_.GetCode(function);
  size_t size = static_cast<size_t>(code->end - code->start);
  // Check bounds for {pc}.
  if (pc < code->locals.encoded_size || pc >= size) return false;
  // Check if a breakpoint is present at that place in the code.
  return code->start[pc] == kInternalBreakpoint;
}

bool WasmInterpreter::SetTracing(const WasmFunction* function, bool enabled) {
  UNIMPLEMENTED();
  return false;
}

int WasmInterpreter::GetThreadCount() {
  return 1;  // only one thread for now.
}

WasmInterpreter::Thread* WasmInterpreter::GetThread(int id) {
  CHECK_EQ(0, id);  // only one thread for now.
  return ToThread(&internals_->threads_[id]);
}

void WasmInterpreter::AddFunctionForTesting(const WasmFunction* function) {
  internals_->codemap_.AddFunction(function, nullptr, nullptr);
}

void WasmInterpreter::SetFunctionCodeForTesting(const WasmFunction* function,
                                                const byte* start,
                                                const byte* end) {
  internals_->codemap_.SetFunctionCode(function, start, end);
}

ControlTransferMap WasmInterpreter::ComputeControlTransfersForTesting(
    Zone* zone, const WasmModule* module, const byte* start, const byte* end) {
  // Create some dummy structures, to avoid special-casing the implementation
  // just for testing.
  FunctionSig sig(0, 0, nullptr);
  WasmFunction function{&sig, 0, 0, {0, 0}, false, false};
  InterpreterCode code{
      &function, BodyLocalDecls(zone), start, end, nullptr, nullptr, nullptr};

  // Now compute and return the control transfers.
  SideTable side_table(zone, module, &code);
  return side_table.map_;
}

//============================================================================
// Implementation of the frame inspection interface.
//============================================================================
const WasmFunction* InterpretedFrame::function() const {
  return ToImpl(this)->function();
}
int InterpretedFrame::pc() const { return ToImpl(this)->pc(); }
int InterpretedFrame::GetParameterCount() const {
  return ToImpl(this)->GetParameterCount();
}
int InterpretedFrame::GetLocalCount() const {
  return ToImpl(this)->GetLocalCount();
}
int InterpretedFrame::GetStackHeight() const {
  return ToImpl(this)->GetStackHeight();
}
WasmValue InterpretedFrame::GetLocalValue(int index) const {
  return ToImpl(this)->GetLocalValue(index);
}
WasmValue InterpretedFrame::GetStackValue(int index) const {
  return ToImpl(this)->GetStackValue(index);
}
void InterpretedFrameDeleter::operator()(InterpretedFrame* ptr) {
  delete ToImpl(ptr);
}

#undef TRACE
#undef LANE
#undef FOREACH_INTERNAL_OPCODE
#undef WASM_CTYPES
#undef FOREACH_SIMPLE_BINOP
#undef FOREACH_OTHER_BINOP
#undef FOREACH_I32CONV_FLOATOP
#undef FOREACH_OTHER_UNOP

}  // namespace wasm
}  // namespace internal
}  // namespace v8
