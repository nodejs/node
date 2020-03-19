// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_VALUE_TYPE_H_
#define V8_WASM_VALUE_TYPE_H_

#include "src/codegen/machine-type.h"
#include "src/wasm/wasm-constants.h"

namespace v8 {
namespace internal {

template <typename T>
class Signature;

namespace wasm {

// Type for holding simd values, defined in wasm-value.h.
class Simd128;

// Type lattice: For any two types connected by a line, the type at the bottom
// is a subtype of the other type.
//
//                       AnyRef
//                       /    \
//                 FuncRef    ExnRef
//                       \    /
// I32  I64  F32  F64    NullRef
//   \    \    \    \    /
//   ------------   Bottom
#define FOREACH_VALUE_TYPE(V)                                                  \
  V(Stmt, -1, -1, Void, None, 'v', "<stmt>")                                   \
  V(I32, 4, 2, I32, Int32, 'i', "i32")                                         \
  V(I64, 8, 3, I64, Int64, 'l', "i64")                                         \
  V(F32, 4, 2, F32, Float32, 'f', "f32")                                       \
  V(F64, 8, 3, F64, Float64, 'd', "f64")                                       \
  V(S128, 16, 4, S128, Simd128, 's', "s128")                                   \
  V(AnyRef, kSystemPointerSize, kSystemPointerSizeLog2, AnyRef, TaggedPointer, \
    'r', "anyref")                                                             \
  V(FuncRef, kSystemPointerSize, kSystemPointerSizeLog2, FuncRef,              \
    TaggedPointer, 'a', "funcref")                                             \
  V(NullRef, kSystemPointerSize, kSystemPointerSizeLog2, NullRef,              \
    TaggedPointer, 'n', "nullref")                                             \
  V(ExnRef, kSystemPointerSize, kSystemPointerSizeLog2, ExnRef, TaggedPointer, \
    'e', "exn")                                                                \
  V(Bottom, -1, -1, Void, None, '*', "<bot>")

enum ValueType : uint8_t {
#define DEF_ENUM(type, ...) kWasm##type,
  FOREACH_VALUE_TYPE(DEF_ENUM)
#undef DEF_ENUM
};

#define FOREACH_WASMVALUE_CTYPES(V) \
  V(kWasmI32, int32_t)              \
  V(kWasmI64, int64_t)              \
  V(kWasmF32, float)                \
  V(kWasmF64, double)               \
  V(kWasmS128, Simd128)

using FunctionSig = Signature<ValueType>;

inline size_t hash_value(ValueType type) { return static_cast<size_t>(type); }

#define FOREACH_LOAD_TYPE(V) \
  V(I32, , Int32)            \
  V(I32, 8S, Int8)           \
  V(I32, 8U, Uint8)          \
  V(I32, 16S, Int16)         \
  V(I32, 16U, Uint16)        \
  V(I64, , Int64)            \
  V(I64, 8S, Int8)           \
  V(I64, 8U, Uint8)          \
  V(I64, 16S, Int16)         \
  V(I64, 16U, Uint16)        \
  V(I64, 32S, Int32)         \
  V(I64, 32U, Uint32)        \
  V(F32, , Float32)          \
  V(F64, , Float64)          \
  V(S128, , Simd128)

class LoadType {
 public:
  enum LoadTypeValue : uint8_t {
#define DEF_ENUM(type, suffix, ...) k##type##Load##suffix,
    FOREACH_LOAD_TYPE(DEF_ENUM)
#undef DEF_ENUM
  };

  // Allow implicit convertion of the enum value to this wrapper.
  constexpr LoadType(LoadTypeValue val)  // NOLINT(runtime/explicit)
      : val_(val) {}

  constexpr LoadTypeValue value() const { return val_; }
  constexpr unsigned size_log_2() const { return kLoadSizeLog2[val_]; }
  constexpr unsigned size() const { return 1 << size_log_2(); }
  constexpr ValueType value_type() const { return kValueType[val_]; }
  constexpr MachineType mem_type() const { return kMemType[val_]; }

  static LoadType ForValueType(ValueType type) {
    switch (type) {
      case kWasmI32:
        return kI32Load;
      case kWasmI64:
        return kI64Load;
      case kWasmF32:
        return kF32Load;
      case kWasmF64:
        return kF64Load;
      case kWasmS128:
        return kS128Load;
      default:
        UNREACHABLE();
    }
  }

 private:
  const LoadTypeValue val_;

  static constexpr uint8_t kLoadSizeLog2[] = {
  // MSVC wants a static_cast here.
#define LOAD_SIZE(_, __, memtype) \
  static_cast<uint8_t>(           \
      ElementSizeLog2Of(MachineType::memtype().representation())),
      FOREACH_LOAD_TYPE(LOAD_SIZE)
#undef LOAD_SIZE
  };

  static constexpr ValueType kValueType[] = {
#define VALUE_TYPE(type, ...) kWasm##type,
      FOREACH_LOAD_TYPE(VALUE_TYPE)
#undef VALUE_TYPE
  };

  static constexpr MachineType kMemType[] = {
#define MEMTYPE(_, __, memtype) MachineType::memtype(),
      FOREACH_LOAD_TYPE(MEMTYPE)
#undef MEMTYPE
  };
};

#define FOREACH_STORE_TYPE(V) \
  V(I32, , Word32)            \
  V(I32, 8, Word8)            \
  V(I32, 16, Word16)          \
  V(I64, , Word64)            \
  V(I64, 8, Word8)            \
  V(I64, 16, Word16)          \
  V(I64, 32, Word32)          \
  V(F32, , Float32)           \
  V(F64, , Float64)           \
  V(S128, , Simd128)

class StoreType {
 public:
  enum StoreTypeValue : uint8_t {
#define DEF_ENUM(type, suffix, ...) k##type##Store##suffix,
    FOREACH_STORE_TYPE(DEF_ENUM)
#undef DEF_ENUM
  };

  // Allow implicit convertion of the enum value to this wrapper.
  constexpr StoreType(StoreTypeValue val)  // NOLINT(runtime/explicit)
      : val_(val) {}

  constexpr StoreTypeValue value() const { return val_; }
  constexpr unsigned size_log_2() const { return kStoreSizeLog2[val_]; }
  constexpr unsigned size() const { return 1 << size_log_2(); }
  constexpr ValueType value_type() const { return kValueType[val_]; }
  constexpr MachineRepresentation mem_rep() const { return kMemRep[val_]; }

  static StoreType ForValueType(ValueType type) {
    switch (type) {
      case kWasmI32:
        return kI32Store;
      case kWasmI64:
        return kI64Store;
      case kWasmF32:
        return kF32Store;
      case kWasmF64:
        return kF64Store;
      case kWasmS128:
        return kS128Store;
      default:
        UNREACHABLE();
    }
  }

 private:
  const StoreTypeValue val_;

  static constexpr uint8_t kStoreSizeLog2[] = {
  // MSVC wants a static_cast here.
#define STORE_SIZE(_, __, memrep) \
  static_cast<uint8_t>(ElementSizeLog2Of(MachineRepresentation::k##memrep)),
      FOREACH_STORE_TYPE(STORE_SIZE)
#undef STORE_SIZE
  };

  static constexpr ValueType kValueType[] = {
#define VALUE_TYPE(type, ...) kWasm##type,
      FOREACH_STORE_TYPE(VALUE_TYPE)
#undef VALUE_TYPE
  };

  static constexpr MachineRepresentation kMemRep[] = {
#define MEMREP(_, __, memrep) MachineRepresentation::k##memrep,
      FOREACH_STORE_TYPE(MEMREP)
#undef MEMREP
  };
};

// A collection of ValueType-related static methods.
class V8_EXPORT_PRIVATE ValueTypes {
 public:
  static inline bool IsSubType(ValueType actual, ValueType expected) {
    return (expected == actual) ||
           (expected == kWasmAnyRef && actual == kWasmNullRef) ||
           (expected == kWasmAnyRef && actual == kWasmFuncRef) ||
           (expected == kWasmAnyRef && actual == kWasmExnRef) ||
           (expected == kWasmFuncRef && actual == kWasmNullRef) ||
           (expected == kWasmExnRef && actual == kWasmNullRef);
  }

  static inline bool IsReferenceType(ValueType type) {
    return type == kWasmAnyRef || type == kWasmFuncRef ||
           type == kWasmNullRef || type == kWasmExnRef;
  }

  static inline ValueType CommonSubType(ValueType a, ValueType b) {
    if (a == b) return a;
    // The only sub type of any value type is {bot}.
    if (!IsReferenceType(a) || !IsReferenceType(b)) return kWasmBottom;
    if (IsSubType(a, b)) return a;
    if (IsSubType(b, a)) return b;
    // {a} and {b} are not each other's subtype. The biggest sub-type of all
    // reference types is {kWasmNullRef}.
    return kWasmNullRef;
  }

  static byte MemSize(MachineType type) {
    return 1 << i::ElementSizeLog2Of(type.representation());
  }

  static int ElementSizeInBytes(ValueType type) {
    DCHECK_NE(kWasmStmt, type);
    DCHECK_NE(kWasmBottom, type);

    constexpr int kElementSizeInBytes[] = {
#define ELEM_SIZE(type, size, ...) size,
        FOREACH_VALUE_TYPE(ELEM_SIZE)
#undef ELEM_SIZE
    };

    return kElementSizeInBytes[type];
  }

  static int ElementSizeLog2Of(ValueType type) {
    DCHECK_NE(kWasmStmt, type);
    DCHECK_NE(kWasmBottom, type);

    constexpr int kElementSizeLog2[] = {
#define ELEM_SIZE_LOG2(type, size, log2Size, ...) log2Size,
        FOREACH_VALUE_TYPE(ELEM_SIZE_LOG2)
#undef ELEM_SIZE_LOG2
    };

    return kElementSizeLog2[type];
  }

  static byte MemSize(ValueType type) { return 1 << ElementSizeLog2Of(type); }

  static ValueTypeCode ValueTypeCodeFor(ValueType type) {
    DCHECK_NE(kWasmBottom, type);

    constexpr ValueTypeCode kValueTypeCode[] = {
#define TYPE_CODE(type, size, log2Size, code, ...) kLocal##code,
        FOREACH_VALUE_TYPE(TYPE_CODE)
#undef TYPE_CODE
    };

    return kValueTypeCode[type];
  }

  static MachineType MachineTypeFor(ValueType type) {
    DCHECK_NE(kWasmBottom, type);

    constexpr MachineType kMachineType[] = {
#define MACH_TYPE(type, size, log2Size, code, machineType, ...) \
  MachineType::machineType(),
        FOREACH_VALUE_TYPE(MACH_TYPE)
#undef MACH_TYPE
    };

    return kMachineType[type];
  }

  static MachineRepresentation MachineRepresentationFor(ValueType type) {
    return MachineTypeFor(type).representation();
  }

  static ValueType ValueTypeFor(MachineType type) {
    switch (type.representation()) {
      case MachineRepresentation::kWord8:
      case MachineRepresentation::kWord16:
      case MachineRepresentation::kWord32:
        return kWasmI32;
      case MachineRepresentation::kWord64:
        return kWasmI64;
      case MachineRepresentation::kFloat32:
        return kWasmF32;
      case MachineRepresentation::kFloat64:
        return kWasmF64;
      case MachineRepresentation::kTaggedPointer:
        return kWasmAnyRef;
      case MachineRepresentation::kSimd128:
        return kWasmS128;
      default:
        UNREACHABLE();
    }
  }

  static constexpr char ShortNameOf(ValueType type) {
    constexpr char kShortName[] = {
#define SHORT_NAME(type, size, log2Size, code, machineType, shortName, ...) \
  shortName,
        FOREACH_VALUE_TYPE(SHORT_NAME)
#undef SHORT_NAME
    };

    return kShortName[type];
  }

  static constexpr const char* TypeName(ValueType type) {
    constexpr const char* kTypeName[] = {
#define TYPE_NAME(type, size, log2Size, code, machineType, shortName, \
                  typeName, ...)                                      \
  typeName,
        FOREACH_VALUE_TYPE(TYPE_NAME)
#undef TYPE_NAME
    };

    return kTypeName[type];
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ValueTypes);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_VALUE_TYPE_H_
