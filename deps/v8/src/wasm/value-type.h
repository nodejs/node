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
// Format: kind, log2Size, code, machineType, shortName, typeName
#define FOREACH_VALUE_TYPE(V)                                                \
  V(Stmt, -1, Void, None, 'v', "<stmt>")                                     \
  V(I32, 2, I32, Int32, 'i', "i32")                                          \
  V(I64, 3, I64, Int64, 'l', "i64")                                          \
  V(F32, 2, F32, Float32, 'f', "f32")                                        \
  V(F64, 3, F64, Float64, 'd', "f64")                                        \
  V(S128, 4, S128, Simd128, 's', "s128")                                     \
  V(AnyRef, kSystemPointerSizeLog2, AnyRef, TaggedPointer, 'r', "anyref")    \
  V(FuncRef, kSystemPointerSizeLog2, FuncRef, TaggedPointer, 'a', "funcref") \
  V(NullRef, kSystemPointerSizeLog2, NullRef, TaggedPointer, 'n', "nullref") \
  V(ExnRef, kSystemPointerSizeLog2, ExnRef, TaggedPointer, 'e', "exn")       \
  V(Bottom, -1, Void, None, '*', "<bot>")

class ValueType {
 public:
  enum Kind : uint8_t {
#define DEF_ENUM(kind, ...) k##kind,
    FOREACH_VALUE_TYPE(DEF_ENUM)
#undef DEF_ENUM
  };

  constexpr ValueType() : kind_(kStmt) {}
  explicit constexpr ValueType(Kind kind) : kind_(kind) {}

  constexpr Kind kind() const { return kind_; }

  constexpr int element_size_log2() const {
#if V8_HAS_CXX14_CONSTEXPR
    DCHECK_NE(kStmt, kind_);
    DCHECK_NE(kBottom, kind_);
#endif

    constexpr int kElementSizeLog2[] = {
#define ELEM_SIZE_LOG2(kind, log2Size, ...) log2Size,
        FOREACH_VALUE_TYPE(ELEM_SIZE_LOG2)
#undef ELEM_SIZE_LOG2
    };

    return kElementSizeLog2[kind_];
  }

  constexpr int element_size_bytes() const { return 1 << element_size_log2(); }

  constexpr bool operator==(ValueType other) const {
    return kind_ == other.kind_;
  }
  constexpr bool operator!=(ValueType other) const {
    return kind_ != other.kind_;
  }

  bool IsSubTypeOf(ValueType other) const {
    return (*this == other) || (kind_ == kNullRef && other.kind_ == kAnyRef) ||
           (kind_ == kFuncRef && other.kind_ == kAnyRef) ||
           (kind_ == kExnRef && other.kind_ == kAnyRef) ||
           (kind_ == kNullRef && other.kind_ == kFuncRef) ||
           (kind_ == kNullRef && other.kind_ == kExnRef);
  }

  bool IsReferenceType() const {
    return kind_ == kAnyRef || kind_ == kFuncRef || kind_ == kNullRef ||
           kind_ == kExnRef;
  }

  static ValueType CommonSubType(ValueType a, ValueType b) {
    if (a.kind() == b.kind()) return a;
    // The only sub type of any value type is {bot}.
    if (!a.IsReferenceType() || !b.IsReferenceType()) {
      return ValueType(kBottom);
    }
    if (a.IsSubTypeOf(b)) return a;
    if (b.IsSubTypeOf(a)) return b;
    // {a} and {b} are not each other's subtype. The biggest sub-type of all
    // reference types is {kWasmNullRef}.
    return ValueType(kNullRef);
  }

  ValueTypeCode value_type_code() const {
    DCHECK_NE(kBottom, kind_);

    constexpr ValueTypeCode kValueTypeCode[] = {
#define TYPE_CODE(kind, log2Size, code, ...) kLocal##code,
        FOREACH_VALUE_TYPE(TYPE_CODE)
#undef TYPE_CODE
    };

    return kValueTypeCode[kind_];
  }

  MachineType machine_type() const {
    DCHECK_NE(kBottom, kind_);

    constexpr MachineType kMachineType[] = {
#define MACH_TYPE(kind, log2Size, code, machineType, ...) \
  MachineType::machineType(),
        FOREACH_VALUE_TYPE(MACH_TYPE)
#undef MACH_TYPE
    };

    return kMachineType[kind_];
  }

  MachineRepresentation machine_representation() {
    return machine_type().representation();
  }

  static ValueType For(MachineType type) {
    switch (type.representation()) {
      case MachineRepresentation::kWord8:
      case MachineRepresentation::kWord16:
      case MachineRepresentation::kWord32:
        return ValueType(kI32);
      case MachineRepresentation::kWord64:
        return ValueType(kI64);
      case MachineRepresentation::kFloat32:
        return ValueType(kF32);
      case MachineRepresentation::kFloat64:
        return ValueType(kF64);
      case MachineRepresentation::kTaggedPointer:
        return ValueType(kAnyRef);
      case MachineRepresentation::kSimd128:
        return ValueType(kS128);
      default:
        UNREACHABLE();
    }
  }

  constexpr char short_name() const {
    constexpr char kShortName[] = {
#define SHORT_NAME(kind, log2Size, code, machineType, shortName, ...) shortName,
        FOREACH_VALUE_TYPE(SHORT_NAME)
#undef SHORT_NAME
    };

    return kShortName[kind_];
  }

  constexpr const char* type_name() const {
    constexpr const char* kTypeName[] = {
#define TYPE_NAME(kind, log2Size, code, machineType, shortName, typeName, ...) \
  typeName,
        FOREACH_VALUE_TYPE(TYPE_NAME)
#undef TYPE_NAME
    };

    return kTypeName[kind_];
  }

 private:
  Kind kind_ : 8;
  // TODO(jkummerow): Add and use the following for reference types:
  // uint32_t ref_index_ : 24;
};

static_assert(sizeof(ValueType) <= kUInt32Size,
              "ValueType is small and can be passed by value");

inline size_t hash_value(ValueType type) {
  return static_cast<size_t>(type.kind());
}

// Output operator, useful for DCHECKS and others.
inline std::ostream& operator<<(std::ostream& oss, ValueType type) {
  return oss << type.type_name();
}

constexpr ValueType kWasmI32 = ValueType(ValueType::kI32);
constexpr ValueType kWasmI64 = ValueType(ValueType::kI64);
constexpr ValueType kWasmF32 = ValueType(ValueType::kF32);
constexpr ValueType kWasmF64 = ValueType(ValueType::kF64);
constexpr ValueType kWasmAnyRef = ValueType(ValueType::kAnyRef);
constexpr ValueType kWasmExnRef = ValueType(ValueType::kExnRef);
constexpr ValueType kWasmFuncRef = ValueType(ValueType::kFuncRef);
constexpr ValueType kWasmNullRef = ValueType(ValueType::kNullRef);
constexpr ValueType kWasmS128 = ValueType(ValueType::kS128);
constexpr ValueType kWasmStmt = ValueType(ValueType::kStmt);
constexpr ValueType kWasmBottom = ValueType(ValueType::kBottom);

#define FOREACH_WASMVALUE_CTYPES(V) \
  V(kI32, int32_t)                  \
  V(kI64, int64_t)                  \
  V(kF32, float)                    \
  V(kF64, double)                   \
  V(kS128, Simd128)

using FunctionSig = Signature<ValueType>;

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

  // Allow implicit conversion of the enum value to this wrapper.
  constexpr LoadType(LoadTypeValue val)  // NOLINT(runtime/explicit)
      : val_(val) {}

  constexpr LoadTypeValue value() const { return val_; }
  constexpr unsigned size_log_2() const { return kLoadSizeLog2[val_]; }
  constexpr unsigned size() const { return 1 << size_log_2(); }
  constexpr ValueType value_type() const { return kValueType[val_]; }
  constexpr MachineType mem_type() const { return kMemType[val_]; }

  static LoadType ForValueType(ValueType type) {
    switch (type.kind()) {
      case ValueType::kI32:
        return kI32Load;
      case ValueType::kI64:
        return kI64Load;
      case ValueType::kF32:
        return kF32Load;
      case ValueType::kF64:
        return kF64Load;
      case ValueType::kS128:
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
#define VALUE_TYPE(type, ...) ValueType(ValueType::k##type),
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
    switch (type.kind()) {
      case ValueType::kI32:
        return kI32Store;
      case ValueType::kI64:
        return kI64Store;
      case ValueType::kF32:
        return kF32Store;
      case ValueType::kF64:
        return kF64Store;
      case ValueType::kS128:
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
#define VALUE_TYPE(type, ...) ValueType(ValueType::k##type),
      FOREACH_STORE_TYPE(VALUE_TYPE)
#undef VALUE_TYPE
  };

  static constexpr MachineRepresentation kMemRep[] = {
#define MEMREP(_, __, memrep) MachineRepresentation::k##memrep,
      FOREACH_STORE_TYPE(MEMREP)
#undef MEMREP
  };
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_VALUE_TYPE_H_
