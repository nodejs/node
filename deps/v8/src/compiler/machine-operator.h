// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_MACHINE_OPERATOR_H_
#define V8_COMPILER_MACHINE_OPERATOR_H_

#include "src/base/flags.h"
#include "src/machine-type.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
struct MachineOperatorGlobalCache;
class Operator;


// For operators that are not supported on all platforms.
class OptionalOperator final {
 public:
  explicit OptionalOperator(const Operator* op) : op_(op) {}

  bool IsSupported() const { return op_ != nullptr; }
  const Operator* op() const {
    DCHECK_NOT_NULL(op_);
    return op_;
  }

 private:
  const Operator* const op_;
};


// Supported float64 to int32 truncation modes.
enum class TruncationMode : uint8_t {
  kJavaScript,  // ES6 section 7.1.5
  kRoundToZero  // Round towards zero. Implementation defined for NaN and ovf.
};

V8_INLINE size_t hash_value(TruncationMode mode) {
  return static_cast<uint8_t>(mode);
}

std::ostream& operator<<(std::ostream&, TruncationMode);

TruncationMode TruncationModeOf(Operator const*);


// Supported write barrier modes.
enum WriteBarrierKind {
  kNoWriteBarrier,
  kMapWriteBarrier,
  kPointerWriteBarrier,
  kFullWriteBarrier
};

std::ostream& operator<<(std::ostream& os, WriteBarrierKind);


// A Load needs a MachineType.
typedef MachineType LoadRepresentation;

LoadRepresentation LoadRepresentationOf(Operator const*);

// A Store needs a MachineType and a WriteBarrierKind in order to emit the
// correct write barrier.
class StoreRepresentation final {
 public:
  StoreRepresentation(MachineRepresentation representation,
                      WriteBarrierKind write_barrier_kind)
      : representation_(representation),
        write_barrier_kind_(write_barrier_kind) {}

  MachineRepresentation representation() const { return representation_; }
  WriteBarrierKind write_barrier_kind() const { return write_barrier_kind_; }

 private:
  MachineRepresentation representation_;
  WriteBarrierKind write_barrier_kind_;
};

bool operator==(StoreRepresentation, StoreRepresentation);
bool operator!=(StoreRepresentation, StoreRepresentation);

size_t hash_value(StoreRepresentation);

std::ostream& operator<<(std::ostream&, StoreRepresentation);

StoreRepresentation const& StoreRepresentationOf(Operator const*);


// A CheckedLoad needs a MachineType.
typedef MachineType CheckedLoadRepresentation;

CheckedLoadRepresentation CheckedLoadRepresentationOf(Operator const*);


// A CheckedStore needs a MachineType.
typedef MachineRepresentation CheckedStoreRepresentation;

CheckedStoreRepresentation CheckedStoreRepresentationOf(Operator const*);


// Interface for building machine-level operators. These operators are
// machine-level but machine-independent and thus define a language suitable
// for generating code to run on architectures such as ia32, x64, arm, etc.
class MachineOperatorBuilder final : public ZoneObject {
 public:
  // Flags that specify which operations are available. This is useful
  // for operations that are unsupported by some back-ends.
  enum Flag {
    kNoFlags = 0u,
    // Note that Float*Max behaves like `(b < a) ? a : b`, not like Math.max().
    // Note that Float*Min behaves like `(a < b) ? a : b`, not like Math.min().
    kFloat32Max = 1u << 0,
    kFloat32Min = 1u << 1,
    kFloat64Max = 1u << 2,
    kFloat64Min = 1u << 3,
    kFloat32RoundDown = 1u << 4,
    kFloat64RoundDown = 1u << 5,
    kFloat32RoundUp = 1u << 6,
    kFloat64RoundUp = 1u << 7,
    kFloat32RoundTruncate = 1u << 8,
    kFloat64RoundTruncate = 1u << 9,
    kFloat32RoundTiesEven = 1u << 10,
    kFloat64RoundTiesEven = 1u << 11,
    kFloat64RoundTiesAway = 1u << 12,
    kInt32DivIsSafe = 1u << 13,
    kUint32DivIsSafe = 1u << 14,
    kWord32ShiftIsSafe = 1u << 15,
    kWord32Ctz = 1u << 16,
    kWord64Ctz = 1u << 17,
    kWord32Popcnt = 1u << 18,
    kWord64Popcnt = 1u << 19,
    kAllOptionalOps = kFloat32Max | kFloat32Min | kFloat64Max | kFloat64Min |
                      kFloat32RoundDown | kFloat64RoundDown | kFloat32RoundUp |
                      kFloat64RoundUp | kFloat32RoundTruncate |
                      kFloat64RoundTruncate | kFloat64RoundTiesAway |
                      kFloat32RoundTiesEven | kFloat64RoundTiesEven |
                      kWord32Ctz | kWord64Ctz | kWord32Popcnt | kWord64Popcnt
  };
  typedef base::Flags<Flag, unsigned> Flags;

  explicit MachineOperatorBuilder(
      Zone* zone,
      MachineRepresentation word = MachineType::PointerRepresentation(),
      Flags supportedOperators = kNoFlags);

  const Operator* Word32And();
  const Operator* Word32Or();
  const Operator* Word32Xor();
  const Operator* Word32Shl();
  const Operator* Word32Shr();
  const Operator* Word32Sar();
  const Operator* Word32Ror();
  const Operator* Word32Equal();
  const Operator* Word32Clz();
  const OptionalOperator Word32Ctz();
  const OptionalOperator Word32Popcnt();
  const OptionalOperator Word64Popcnt();
  bool Word32ShiftIsSafe() const { return flags_ & kWord32ShiftIsSafe; }

  const Operator* Word64And();
  const Operator* Word64Or();
  const Operator* Word64Xor();
  const Operator* Word64Shl();
  const Operator* Word64Shr();
  const Operator* Word64Sar();
  const Operator* Word64Ror();
  const Operator* Word64Clz();
  const OptionalOperator Word64Ctz();
  const Operator* Word64Equal();

  const Operator* Int32Add();
  const Operator* Int32AddWithOverflow();
  const Operator* Int32Sub();
  const Operator* Int32SubWithOverflow();
  const Operator* Int32Mul();
  const Operator* Int32MulHigh();
  const Operator* Int32Div();
  const Operator* Int32Mod();
  const Operator* Int32LessThan();
  const Operator* Int32LessThanOrEqual();
  const Operator* Uint32Div();
  const Operator* Uint32LessThan();
  const Operator* Uint32LessThanOrEqual();
  const Operator* Uint32Mod();
  const Operator* Uint32MulHigh();
  bool Int32DivIsSafe() const { return flags_ & kInt32DivIsSafe; }
  bool Uint32DivIsSafe() const { return flags_ & kUint32DivIsSafe; }

  const Operator* Int64Add();
  const Operator* Int64AddWithOverflow();
  const Operator* Int64Sub();
  const Operator* Int64SubWithOverflow();
  const Operator* Int64Mul();
  const Operator* Int64Div();
  const Operator* Int64Mod();
  const Operator* Int64LessThan();
  const Operator* Int64LessThanOrEqual();
  const Operator* Uint64Div();
  const Operator* Uint64LessThan();
  const Operator* Uint64LessThanOrEqual();
  const Operator* Uint64Mod();

  // These operators change the representation of numbers while preserving the
  // value of the number. Narrowing operators assume the input is representable
  // in the target type and are *not* defined for other inputs.
  // Use narrowing change operators only when there is a static guarantee that
  // the input value is representable in the target value.
  const Operator* ChangeFloat32ToFloat64();
  const Operator* ChangeFloat64ToInt32();   // narrowing
  const Operator* ChangeFloat64ToUint32();  // narrowing
  const Operator* TryTruncateFloat32ToInt64();
  const Operator* TryTruncateFloat64ToInt64();
  const Operator* TryTruncateFloat32ToUint64();
  const Operator* TryTruncateFloat64ToUint64();
  const Operator* ChangeInt32ToFloat64();
  const Operator* ChangeInt32ToInt64();
  const Operator* ChangeUint32ToFloat64();
  const Operator* ChangeUint32ToUint64();

  // These operators truncate or round numbers, both changing the representation
  // of the number and mapping multiple input values onto the same output value.
  const Operator* TruncateFloat64ToFloat32();
  const Operator* TruncateFloat64ToInt32(TruncationMode);
  const Operator* TruncateInt64ToInt32();
  const Operator* RoundInt64ToFloat32();
  const Operator* RoundInt64ToFloat64();
  const Operator* RoundUint64ToFloat32();
  const Operator* RoundUint64ToFloat64();

  // These operators reinterpret the bits of a floating point number as an
  // integer and vice versa.
  const Operator* BitcastFloat32ToInt32();
  const Operator* BitcastFloat64ToInt64();
  const Operator* BitcastInt32ToFloat32();
  const Operator* BitcastInt64ToFloat64();

  // Floating point operators always operate with IEEE 754 round-to-nearest
  // (single-precision).
  const Operator* Float32Add();
  const Operator* Float32Sub();
  const Operator* Float32Mul();
  const Operator* Float32Div();
  const Operator* Float32Sqrt();

  // Floating point operators always operate with IEEE 754 round-to-nearest
  // (double-precision).
  const Operator* Float64Add();
  const Operator* Float64Sub();
  const Operator* Float64Mul();
  const Operator* Float64Div();
  const Operator* Float64Mod();
  const Operator* Float64Sqrt();

  // Floating point comparisons complying to IEEE 754 (single-precision).
  const Operator* Float32Equal();
  const Operator* Float32LessThan();
  const Operator* Float32LessThanOrEqual();

  // Floating point comparisons complying to IEEE 754 (double-precision).
  const Operator* Float64Equal();
  const Operator* Float64LessThan();
  const Operator* Float64LessThanOrEqual();

  // Floating point min/max complying to IEEE 754 (single-precision).
  const OptionalOperator Float32Max();
  const OptionalOperator Float32Min();

  // Floating point min/max complying to IEEE 754 (double-precision).
  const OptionalOperator Float64Max();
  const OptionalOperator Float64Min();

  // Floating point abs complying to IEEE 754 (single-precision).
  const Operator* Float32Abs();

  // Floating point abs complying to IEEE 754 (double-precision).
  const Operator* Float64Abs();

  // Floating point rounding.
  const OptionalOperator Float32RoundDown();
  const OptionalOperator Float64RoundDown();
  const OptionalOperator Float32RoundUp();
  const OptionalOperator Float64RoundUp();
  const OptionalOperator Float32RoundTruncate();
  const OptionalOperator Float64RoundTruncate();
  const OptionalOperator Float64RoundTiesAway();
  const OptionalOperator Float32RoundTiesEven();
  const OptionalOperator Float64RoundTiesEven();

  // Floating point bit representation.
  const Operator* Float64ExtractLowWord32();
  const Operator* Float64ExtractHighWord32();
  const Operator* Float64InsertLowWord32();
  const Operator* Float64InsertHighWord32();

  // load [base + index]
  const Operator* Load(LoadRepresentation rep);

  // store [base + index], value
  const Operator* Store(StoreRepresentation rep);

  // Access to the machine stack.
  const Operator* LoadStackPointer();
  const Operator* LoadFramePointer();

  // checked-load heap, index, length
  const Operator* CheckedLoad(CheckedLoadRepresentation);
  // checked-store heap, index, length, value
  const Operator* CheckedStore(CheckedStoreRepresentation);

  // Target machine word-size assumed by this builder.
  bool Is32() const { return word() == MachineRepresentation::kWord32; }
  bool Is64() const { return word() == MachineRepresentation::kWord64; }
  MachineRepresentation word() const { return word_; }

// Pseudo operators that translate to 32/64-bit operators depending on the
// word-size of the target machine assumed by this builder.
#define PSEUDO_OP_LIST(V) \
  V(Word, And)            \
  V(Word, Or)             \
  V(Word, Xor)            \
  V(Word, Shl)            \
  V(Word, Shr)            \
  V(Word, Sar)            \
  V(Word, Ror)            \
  V(Word, Equal)          \
  V(Int, Add)             \
  V(Int, Sub)             \
  V(Int, Mul)             \
  V(Int, Div)             \
  V(Int, Mod)             \
  V(Int, LessThan)        \
  V(Int, LessThanOrEqual) \
  V(Uint, Div)            \
  V(Uint, LessThan)       \
  V(Uint, Mod)
#define PSEUDO_OP(Prefix, Suffix)                                \
  const Operator* Prefix##Suffix() {                             \
    return Is32() ? Prefix##32##Suffix() : Prefix##64##Suffix(); \
  }
  PSEUDO_OP_LIST(PSEUDO_OP)
#undef PSEUDO_OP
#undef PSEUDO_OP_LIST

 private:
  MachineOperatorGlobalCache const& cache_;
  MachineRepresentation const word_;
  Flags const flags_;

  DISALLOW_COPY_AND_ASSIGN(MachineOperatorBuilder);
};


DEFINE_OPERATORS_FOR_FLAGS(MachineOperatorBuilder::Flags)

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_MACHINE_OPERATOR_H_
