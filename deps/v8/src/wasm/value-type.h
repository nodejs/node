// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_VALUE_TYPE_H_
#define V8_WASM_VALUE_TYPE_H_

#include "src/base/bit-field.h"
#include "src/codegen/machine-type.h"
#include "src/wasm/wasm-constants.h"

namespace v8 {
namespace internal {

template <typename T>
class Signature;

namespace wasm {

// Type for holding simd values, defined in wasm-value.h.
class Simd128;

// Format: kind, log2Size, code, machineType, shortName, typeName
//
// Some of these types are from proposals that are not standardized yet:
// - "ref"/"optref" (a.k.a. "ref null") per
//   https://github.com/WebAssembly/function-references
// - "rtt" per https://github.com/WebAssembly/gc
#define FOREACH_VALUE_TYPE(V)                                               \
  V(Stmt, -1, Void, None, 'v', "<stmt>")                                    \
  V(I32, 2, I32, Int32, 'i', "i32")                                         \
  V(I64, 3, I64, Int64, 'l', "i64")                                         \
  V(F32, 2, F32, Float32, 'f', "f32")                                       \
  V(F64, 3, F64, Float64, 'd', "f64")                                       \
  V(S128, 4, S128, Simd128, 's', "s128")                                    \
  V(I8, 0, I8, Int8, 'b', "i8")                                             \
  V(I16, 1, I16, Int16, 'h', "i16")                                         \
  V(Rtt, kSystemPointerSizeLog2, Rtt, TaggedPointer, 't', "rtt")            \
  V(Ref, kSystemPointerSizeLog2, Ref, TaggedPointer, 'r', "ref")            \
  V(OptRef, kSystemPointerSizeLog2, OptRef, TaggedPointer, 'n', "ref null") \
  V(Bottom, -1, Void, None, '*', "<bot>")

enum HeapType : uint32_t {
  kHeapFunc = kV8MaxWasmTypes,  // shorthand: c
  kHeapExtern,                  // shorthand: e
  kHeapEq,                      // shorthand: q
  kHeapExn                      // shorthand: x
};
enum Nullability : bool { kNonNullable, kNullable };

V8_INLINE constexpr bool is_generic_heap_type(HeapType ht) {
  STATIC_ASSERT(kHeapExtern >= kHeapFunc && kHeapEq >= kHeapFunc &&
                kHeapExn >= kHeapFunc);
  return ht >= kHeapFunc;
}

class ValueType {
 public:
  enum Kind : uint8_t {
#define DEF_ENUM(kind, ...) k##kind,
    FOREACH_VALUE_TYPE(DEF_ENUM)
#undef DEF_ENUM
  };

  constexpr bool is_reference_type() const {
    return kind() == kRef || kind() == kOptRef || kind() == kRtt;
  }

  constexpr bool is_packed() const { return kind() == kI8 || kind() == kI16; }

  constexpr bool is_nullable() const { return kind() == kOptRef; }

  constexpr bool is_reference_to(HeapType htype) const {
    return (kind() == kRef || kind() == kOptRef) && heap_type() == htype;
  }

  constexpr ValueType Unpacked() const {
    return is_packed() ? Primitive(kI32) : *this;
  }

  constexpr bool has_index() const {
    return is_reference_type() && !is_generic_heap_type(heap_type());
  }
  constexpr bool has_depth() const { return kind() == kRtt; }

  constexpr ValueType() : bit_field_(KindField::encode(kStmt)) {}
  static constexpr ValueType Primitive(Kind kind) {
    CONSTEXPR_DCHECK(kind == kBottom || kind <= kI16);
    return ValueType(KindField::encode(kind));
  }
  static constexpr ValueType Ref(HeapType heap_type, Nullability nullability) {
    return ValueType(
        KindField::encode(nullability == kNullable ? kOptRef : kRef) |
        HeapTypeField::encode(heap_type));
  }

  static constexpr ValueType Rtt(HeapType heap_type,
                                 uint8_t inheritance_depth) {
    return ValueType(KindField::encode(kRtt) |
                     HeapTypeField::encode(heap_type) |
                     DepthField::encode(inheritance_depth));
  }

  static constexpr ValueType FromRawBitField(uint32_t bit_field) {
    return ValueType(bit_field);
  }

  constexpr Kind kind() const { return KindField::decode(bit_field_); }
  constexpr HeapType heap_type() const {
    CONSTEXPR_DCHECK(is_reference_type());
    return HeapTypeField::decode(bit_field_);
  }
  constexpr uint8_t depth() const {
    CONSTEXPR_DCHECK(has_depth());
    return DepthField::decode(bit_field_);
  }
  constexpr uint32_t ref_index() const {
    CONSTEXPR_DCHECK(has_index());
    return static_cast<uint32_t>(heap_type());
  }

  constexpr uint32_t raw_bit_field() const { return bit_field_; }

  constexpr int element_size_log2() const {
    CONSTEXPR_DCHECK(kStmt != kind());
    CONSTEXPR_DCHECK(kBottom != kind());

    constexpr int kElementSizeLog2[] = {
#define ELEM_SIZE_LOG2(kind, log2Size, ...) log2Size,
        FOREACH_VALUE_TYPE(ELEM_SIZE_LOG2)
#undef ELEM_SIZE_LOG2
    };

    return kElementSizeLog2[kind()];
  }

  constexpr int element_size_bytes() const { return 1 << element_size_log2(); }

  constexpr bool operator==(ValueType other) const {
    return bit_field_ == other.bit_field_;
  }
  constexpr bool operator!=(ValueType other) const {
    return bit_field_ != other.bit_field_;
  }

  constexpr MachineType machine_type() const {
    CONSTEXPR_DCHECK(kBottom != kind());

    constexpr MachineType kMachineType[] = {
#define MACH_TYPE(kind, log2Size, code, machineType, ...) \
  MachineType::machineType(),
        FOREACH_VALUE_TYPE(MACH_TYPE)
#undef MACH_TYPE
    };

    return kMachineType[kind()];
  }

  constexpr MachineRepresentation machine_representation() const {
    return machine_type().representation();
  }

  constexpr ValueTypeCode value_type_code() const {
    CONSTEXPR_DCHECK(kind() != kBottom);
    switch (kind()) {
      case kOptRef:
        switch (heap_type()) {
          case kHeapFunc:
            return kLocalFuncRef;
          case kHeapExtern:
            return kLocalExternRef;
          case kHeapEq:
            return kLocalEqRef;
          case kHeapExn:
            return kLocalExnRef;
          default:
            return kLocalOptRef;
        }
      case kRef:
        return kLocalRef;
      case kStmt:
        return kLocalVoid;
      case kRtt:
        return kLocalRtt;
      default:
        return static_cast<ValueTypeCode>(kLocalI32 - (kind() - kI32));
    }
  }

  constexpr bool encoding_needs_heap_type() const {
    return kind() == kRef || kind() == kRtt ||
           (kind() == kOptRef && !is_generic_heap_type(heap_type()));
  }

  constexpr uint32_t heap_type_code() const {
    CONSTEXPR_DCHECK(encoding_needs_heap_type());
    switch (heap_type()) {
      case kHeapFunc:
        return kLocalFuncRef;
      case kHeapExn:
        return kLocalExnRef;
      case kHeapExtern:
        return kLocalExternRef;
      case kHeapEq:
        return kLocalEqRef;
      default:
        return static_cast<uint32_t>(heap_type());
    }
  }

  static ValueType For(MachineType type) {
    switch (type.representation()) {
      case MachineRepresentation::kWord8:
      case MachineRepresentation::kWord16:
      case MachineRepresentation::kWord32:
        return Primitive(kI32);
      case MachineRepresentation::kWord64:
        return Primitive(kI64);
      case MachineRepresentation::kFloat32:
        return Primitive(kF32);
      case MachineRepresentation::kFloat64:
        return Primitive(kF64);
      case MachineRepresentation::kTaggedPointer:
        return Ref(kHeapExtern, kNullable);
      case MachineRepresentation::kSimd128:
        return Primitive(kS128);
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

    return kShortName[kind()];
  }

  const std::string type_name() const {
    std::ostringstream buf;
    switch (kind()) {
      case kRef:
        buf << "(ref " << heap_name() << ")";
        break;
      case kOptRef:
        if (is_generic_heap_type(heap_type())) {
          // We prefer the shorthand to be backwards-compatible with previous
          // proposals.
          buf << heap_name() << "ref";
        } else {
          buf << "(ref null " << heap_name() << ")";
        }
        break;
      case kRtt:
        buf << "(rtt " << depth() << " " << heap_name() + ")";
        break;
      default:
        buf << kind_name();
    }
    return buf.str();
  }

 private:
  using KindField = base::BitField<Kind, 0, 5>;
  using HeapTypeField = base::BitField<HeapType, 5, 20>;
  using DepthField = base::BitField<uint8_t, 25, 7>;

  constexpr explicit ValueType(uint32_t bit_field) : bit_field_(bit_field) {}

  constexpr const char* kind_name() const {
    constexpr const char* kTypeName[] = {
#define KIND_NAME(kind, log2Size, code, machineType, shortName, typeName, ...) \
  typeName,
        FOREACH_VALUE_TYPE(KIND_NAME)
#undef TYPE_NAME
    };

    return kTypeName[kind()];
  }

  const std::string heap_name() const {
    switch (heap_type()) {
      case kHeapFunc:
        return std::string("func");
      case kHeapExtern:
        return std::string("extern");
      case kHeapEq:
        return std::string("eq");
      case kHeapExn:
        return std::string("exn");
      default:
        return std::to_string(static_cast<uint32_t>(heap_type()));
    }
  }

  uint32_t bit_field_;
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

constexpr ValueType kWasmI32 = ValueType::Primitive(ValueType::kI32);
constexpr ValueType kWasmI64 = ValueType::Primitive(ValueType::kI64);
constexpr ValueType kWasmF32 = ValueType::Primitive(ValueType::kF32);
constexpr ValueType kWasmF64 = ValueType::Primitive(ValueType::kF64);
constexpr ValueType kWasmS128 = ValueType::Primitive(ValueType::kS128);
constexpr ValueType kWasmI8 = ValueType::Primitive(ValueType::kI8);
constexpr ValueType kWasmI16 = ValueType::Primitive(ValueType::kI16);
constexpr ValueType kWasmStmt = ValueType::Primitive(ValueType::kStmt);
constexpr ValueType kWasmBottom = ValueType::Primitive(ValueType::kBottom);
// Established wasm shorthands:
constexpr ValueType kWasmFuncRef = ValueType::Ref(kHeapFunc, kNullable);
constexpr ValueType kWasmExnRef = ValueType::Ref(kHeapExn, kNullable);
constexpr ValueType kWasmExternRef = ValueType::Ref(kHeapExtern, kNullable);
constexpr ValueType kWasmEqRef = ValueType::Ref(kHeapEq, kNullable);

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
#define VALUE_TYPE(type, ...) ValueType::Primitive(ValueType::k##type),
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
#define VALUE_TYPE(type, ...) ValueType::Primitive(ValueType::k##type),
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
