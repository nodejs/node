// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_VALUE_TYPE_H_
#define V8_WASM_VALUE_TYPE_H_

#include "src/machine-type.h"
#include "src/wasm/wasm-constants.h"

namespace v8 {
namespace internal {
namespace wasm {

enum ValueType : uint8_t {
  kWasmStmt,
  kWasmI32,
  kWasmI64,
  kWasmF32,
  kWasmF64,
  kWasmS128,
  kWasmAnyRef,
  kWasmAnyFunc,
  kWasmExceptRef,
  kWasmVar,
};

inline size_t hash_value(ValueType type) { return static_cast<size_t>(type); }

// TODO(clemensh): Compute memtype and size from ValueType once we have c++14
// constexpr support.
#define FOREACH_LOAD_TYPE(V) \
  V(I32, , Int32, 2)         \
  V(I32, 8S, Int8, 0)        \
  V(I32, 8U, Uint8, 0)       \
  V(I32, 16S, Int16, 1)      \
  V(I32, 16U, Uint16, 1)     \
  V(I64, , Int64, 3)         \
  V(I64, 8S, Int8, 0)        \
  V(I64, 8U, Uint8, 0)       \
  V(I64, 16S, Int16, 1)      \
  V(I64, 16U, Uint16, 1)     \
  V(I64, 32S, Int32, 2)      \
  V(I64, 32U, Uint32, 2)     \
  V(F32, , Float32, 2)       \
  V(F64, , Float64, 3)       \
  V(S128, , Simd128, 4)

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
      default:
        UNREACHABLE();
    }
  }

 private:
  const LoadTypeValue val_;

  static constexpr uint8_t kLoadSizeLog2[] = {
#define LOAD_SIZE(_, __, ___, size) size,
      FOREACH_LOAD_TYPE(LOAD_SIZE)
#undef LOAD_SIZE
  };

  static constexpr ValueType kValueType[] = {
#define VALUE_TYPE(type, ...) kWasm##type,
      FOREACH_LOAD_TYPE(VALUE_TYPE)
#undef VALUE_TYPE
  };

  static constexpr MachineType kMemType[] = {
#define MEMTYPE(_, __, memtype, ___) MachineType::memtype(),
      FOREACH_LOAD_TYPE(MEMTYPE)
#undef MEMTYPE
  };
};

#define FOREACH_STORE_TYPE(V) \
  V(I32, , Word32, 2)         \
  V(I32, 8, Word8, 0)         \
  V(I32, 16, Word16, 1)       \
  V(I64, , Word64, 3)         \
  V(I64, 8, Word8, 0)         \
  V(I64, 16, Word16, 1)       \
  V(I64, 32, Word32, 2)       \
  V(F32, , Float32, 2)        \
  V(F64, , Float64, 3)        \
  V(S128, , Simd128, 4)

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
      default:
        UNREACHABLE();
    }
  }

 private:
  const StoreTypeValue val_;

  static constexpr uint8_t kStoreSizeLog2[] = {
#define STORE_SIZE(_, __, ___, size) size,
      FOREACH_STORE_TYPE(STORE_SIZE)
#undef STORE_SIZE
  };

  static constexpr ValueType kValueType[] = {
#define VALUE_TYPE(type, ...) kWasm##type,
      FOREACH_STORE_TYPE(VALUE_TYPE)
#undef VALUE_TYPE
  };

  static constexpr MachineRepresentation kMemRep[] = {
#define MEMREP(_, __, memrep, ___) MachineRepresentation::k##memrep,
      FOREACH_STORE_TYPE(MEMREP)
#undef MEMREP
  };
};

// A collection of ValueType-related static methods.
class V8_EXPORT_PRIVATE ValueTypes {
 public:
  static byte MemSize(MachineType type) {
    return 1 << i::ElementSizeLog2Of(type.representation());
  }

  static int ElementSizeInBytes(ValueType type) {
    switch (type) {
      case kWasmI32:
      case kWasmF32:
        return 4;
      case kWasmI64:
      case kWasmF64:
        return 8;
      case kWasmS128:
        return 16;
      default:
        UNREACHABLE();
    }
  }

  static int ElementSizeLog2Of(ValueType type) {
    switch (type) {
      case kWasmI32:
      case kWasmF32:
        return 2;
      case kWasmI64:
      case kWasmF64:
        return 3;
      case kWasmS128:
        return 4;
      default:
        UNREACHABLE();
    }
  }

  static byte MemSize(ValueType type) { return 1 << ElementSizeLog2Of(type); }

  static ValueTypeCode ValueTypeCodeFor(ValueType type) {
    switch (type) {
      case kWasmI32:
        return kLocalI32;
      case kWasmI64:
        return kLocalI64;
      case kWasmF32:
        return kLocalF32;
      case kWasmF64:
        return kLocalF64;
      case kWasmS128:
        return kLocalS128;
      case kWasmAnyRef:
        return kLocalAnyRef;
      case kWasmExceptRef:
        return kLocalExceptRef;
      case kWasmStmt:
        return kLocalVoid;
      default:
        UNREACHABLE();
    }
  }

  static MachineType MachineTypeFor(ValueType type) {
    switch (type) {
      case kWasmI32:
        return MachineType::Int32();
      case kWasmI64:
        return MachineType::Int64();
      case kWasmF32:
        return MachineType::Float32();
      case kWasmF64:
        return MachineType::Float64();
      case kWasmAnyFunc:
      case kWasmAnyRef:
        return MachineType::TaggedPointer();
      case kWasmS128:
        return MachineType::Simd128();
      case kWasmStmt:
        return MachineType::None();
      default:
        UNREACHABLE();
    }
  }

  static MachineRepresentation MachineRepresentationFor(ValueType type) {
    switch (type) {
      case kWasmI32:
        return MachineRepresentation::kWord32;
      case kWasmI64:
        return MachineRepresentation::kWord64;
      case kWasmF32:
        return MachineRepresentation::kFloat32;
      case kWasmF64:
        return MachineRepresentation::kFloat64;
      case kWasmAnyRef:
        return MachineRepresentation::kTaggedPointer;
      case kWasmS128:
        return MachineRepresentation::kSimd128;
      case kWasmStmt:
        return MachineRepresentation::kNone;
      default:
        UNREACHABLE();
    }
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

  static char ShortNameOf(ValueType type) {
    switch (type) {
      case kWasmI32:
        return 'i';
      case kWasmI64:
        return 'l';
      case kWasmF32:
        return 'f';
      case kWasmF64:
        return 'd';
      case kWasmAnyRef:
        return 'r';
      case kWasmS128:
        return 's';
      case kWasmStmt:
        return 'v';
      case kWasmVar:
        return '*';
      default:
        return '?';
    }
  }

  static const char* TypeName(ValueType type) {
    switch (type) {
      case kWasmI32:
        return "i32";
      case kWasmI64:
        return "i64";
      case kWasmF32:
        return "f32";
      case kWasmF64:
        return "f64";
      case kWasmAnyRef:
        return "ref";
      case kWasmS128:
        return "s128";
      case kWasmStmt:
        return "<stmt>";
      case kWasmVar:
        return "<var>";
      default:
        return "<unknown>";
    }
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ValueTypes);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_VALUE_TYPE_H_
