// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_MACHINE_OPERATOR_H_
#define V8_COMPILER_MACHINE_OPERATOR_H_

#include "src/base/compiler-specific.h"
#include "src/base/flags.h"
#include "src/globals.h"
#include "src/machine-type.h"
#include "src/utils.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
struct MachineOperatorGlobalCache;
class Operator;


// For operators that are not supported on all platforms.
class OptionalOperator final {
 public:
  OptionalOperator(bool supported, const Operator* op)
      : supported_(supported), op_(op) {}

  bool IsSupported() const { return supported_; }
  // Gets the operator only if it is supported.
  const Operator* op() const {
    DCHECK(supported_);
    return op_;
  }
  // Always gets the operator, even for unsupported operators. This is useful to
  // use the operator as a placeholder in a graph, for instance.
  const Operator* placeholder() const { return op_; }

 private:
  bool supported_;
  const Operator* const op_;
};


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

V8_EXPORT_PRIVATE bool operator==(StoreRepresentation, StoreRepresentation);
bool operator!=(StoreRepresentation, StoreRepresentation);

size_t hash_value(StoreRepresentation);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&, StoreRepresentation);

StoreRepresentation const& StoreRepresentationOf(Operator const*);

typedef MachineType UnalignedLoadRepresentation;

UnalignedLoadRepresentation UnalignedLoadRepresentationOf(Operator const*);

// An UnalignedStore needs a MachineType.
typedef MachineRepresentation UnalignedStoreRepresentation;

UnalignedStoreRepresentation const& UnalignedStoreRepresentationOf(
    Operator const*);

// A CheckedLoad needs a MachineType.
typedef MachineType CheckedLoadRepresentation;

CheckedLoadRepresentation CheckedLoadRepresentationOf(Operator const*);


// A CheckedStore needs a MachineType.
typedef MachineRepresentation CheckedStoreRepresentation;

CheckedStoreRepresentation CheckedStoreRepresentationOf(Operator const*);

class StackSlotRepresentation final {
 public:
  StackSlotRepresentation(int size, int alignment)
      : size_(size), alignment_(alignment) {}

  int size() const { return size_; }
  int alignment() const { return alignment_; }

 private:
  int size_;
  int alignment_;
};

V8_EXPORT_PRIVATE bool operator==(StackSlotRepresentation,
                                  StackSlotRepresentation);
bool operator!=(StackSlotRepresentation, StackSlotRepresentation);

size_t hash_value(StackSlotRepresentation);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&,
                                           StackSlotRepresentation);

StackSlotRepresentation const& StackSlotRepresentationOf(Operator const* op);

MachineRepresentation AtomicStoreRepresentationOf(Operator const* op);

MachineType AtomicOpRepresentationOf(Operator const* op);

// Interface for building machine-level operators. These operators are
// machine-level but machine-independent and thus define a language suitable
// for generating code to run on architectures such as ia32, x64, arm, etc.
class V8_EXPORT_PRIVATE MachineOperatorBuilder final
    : public NON_EXPORTED_BASE(ZoneObject) {
 public:
  // Flags that specify which operations are available. This is useful
  // for operations that are unsupported by some back-ends.
  enum Flag : unsigned {
    kNoFlags = 0u,
    kFloat32RoundDown = 1u << 0,
    kFloat64RoundDown = 1u << 1,
    kFloat32RoundUp = 1u << 2,
    kFloat64RoundUp = 1u << 3,
    kFloat32RoundTruncate = 1u << 4,
    kFloat64RoundTruncate = 1u << 5,
    kFloat32RoundTiesEven = 1u << 6,
    kFloat64RoundTiesEven = 1u << 7,
    kFloat64RoundTiesAway = 1u << 8,
    kInt32DivIsSafe = 1u << 9,
    kUint32DivIsSafe = 1u << 10,
    kWord32ShiftIsSafe = 1u << 11,
    kWord32Ctz = 1u << 12,
    kWord64Ctz = 1u << 13,
    kWord32Popcnt = 1u << 14,
    kWord64Popcnt = 1u << 15,
    kWord32ReverseBits = 1u << 16,
    kWord64ReverseBits = 1u << 17,
    kWord32ReverseBytes = 1u << 18,
    kWord64ReverseBytes = 1u << 19,
    kInt32AbsWithOverflow = 1u << 20,
    kInt64AbsWithOverflow = 1u << 21,
    kAllOptionalOps =
        kFloat32RoundDown | kFloat64RoundDown | kFloat32RoundUp |
        kFloat64RoundUp | kFloat32RoundTruncate | kFloat64RoundTruncate |
        kFloat64RoundTiesAway | kFloat32RoundTiesEven | kFloat64RoundTiesEven |
        kWord32Ctz | kWord64Ctz | kWord32Popcnt | kWord64Popcnt |
        kWord32ReverseBits | kWord64ReverseBits | kWord32ReverseBytes |
        kWord64ReverseBytes | kInt32AbsWithOverflow | kInt64AbsWithOverflow
  };
  typedef base::Flags<Flag, unsigned> Flags;

  class AlignmentRequirements {
   public:
    enum UnalignedAccessSupport { kNoSupport, kSomeSupport, kFullSupport };

    bool IsUnalignedLoadSupported(MachineRepresentation rep) const {
      return IsUnalignedSupported(unalignedLoadUnsupportedTypes_, rep);
    }

    bool IsUnalignedStoreSupported(MachineRepresentation rep) const {
      return IsUnalignedSupported(unalignedStoreUnsupportedTypes_, rep);
    }

    static AlignmentRequirements FullUnalignedAccessSupport() {
      return AlignmentRequirements(kFullSupport);
    }
    static AlignmentRequirements NoUnalignedAccessSupport() {
      return AlignmentRequirements(kNoSupport);
    }
    static AlignmentRequirements SomeUnalignedAccessUnsupported(
        EnumSet<MachineRepresentation> unalignedLoadUnsupportedTypes,
        EnumSet<MachineRepresentation> unalignedStoreUnsupportedTypes) {
      return AlignmentRequirements(kSomeSupport, unalignedLoadUnsupportedTypes,
                                   unalignedStoreUnsupportedTypes);
    }

   private:
    explicit AlignmentRequirements(
        AlignmentRequirements::UnalignedAccessSupport unalignedAccessSupport,
        EnumSet<MachineRepresentation> unalignedLoadUnsupportedTypes =
            EnumSet<MachineRepresentation>(),
        EnumSet<MachineRepresentation> unalignedStoreUnsupportedTypes =
            EnumSet<MachineRepresentation>())
        : unalignedSupport_(unalignedAccessSupport),
          unalignedLoadUnsupportedTypes_(unalignedLoadUnsupportedTypes),
          unalignedStoreUnsupportedTypes_(unalignedStoreUnsupportedTypes) {}

    bool IsUnalignedSupported(EnumSet<MachineRepresentation> unsupported,
                              MachineRepresentation rep) const {
      // All accesses of bytes in memory are aligned.
      DCHECK_NE(MachineRepresentation::kWord8, rep);
      switch (unalignedSupport_) {
        case kFullSupport:
          return true;
        case kNoSupport:
          return false;
        case kSomeSupport:
          return !unsupported.Contains(rep);
      }
      UNREACHABLE();
    }

    const AlignmentRequirements::UnalignedAccessSupport unalignedSupport_;
    const EnumSet<MachineRepresentation> unalignedLoadUnsupportedTypes_;
    const EnumSet<MachineRepresentation> unalignedStoreUnsupportedTypes_;
  };

  explicit MachineOperatorBuilder(
      Zone* zone,
      MachineRepresentation word = MachineType::PointerRepresentation(),
      Flags supportedOperators = kNoFlags,
      AlignmentRequirements alignmentRequirements =
          AlignmentRequirements::FullUnalignedAccessSupport());

  const Operator* Comment(const char* msg);
  const Operator* DebugAbort();
  const Operator* DebugBreak();
  const Operator* UnsafePointerAdd();

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
  const OptionalOperator Word32ReverseBits();
  const OptionalOperator Word64ReverseBits();
  const OptionalOperator Word32ReverseBytes();
  const OptionalOperator Word64ReverseBytes();
  const OptionalOperator Int32AbsWithOverflow();
  const OptionalOperator Int64AbsWithOverflow();

  // Return true if the target's Word32 shift implementation is directly
  // compatible with JavaScript's specification. Otherwise, we have to manually
  // generate a mask with 0x1f on the amount ahead of generating the shift.
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

  const Operator* Int32PairAdd();
  const Operator* Int32PairSub();
  const Operator* Int32PairMul();
  const Operator* Word32PairShl();
  const Operator* Word32PairShr();
  const Operator* Word32PairSar();

  const Operator* Int32Add();
  const Operator* Int32AddWithOverflow();
  const Operator* Int32Sub();
  const Operator* Int32SubWithOverflow();
  const Operator* Int32Mul();
  const Operator* Int32MulWithOverflow();
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

  // This operator reinterprets the bits of a tagged pointer as word.
  const Operator* BitcastTaggedToWord();

  // This operator reinterprets the bits of a word as tagged pointer.
  const Operator* BitcastWordToTagged();

  // This operator reinterprets the bits of a word as a Smi.
  const Operator* BitcastWordToTaggedSigned();

  // JavaScript float64 to int32/uint32 truncation.
  const Operator* TruncateFloat64ToWord32();

  // These operators change the representation of numbers while preserving the
  // value of the number. Narrowing operators assume the input is representable
  // in the target type and are *not* defined for other inputs.
  // Use narrowing change operators only when there is a static guarantee that
  // the input value is representable in the target value.
  const Operator* ChangeFloat32ToFloat64();
  const Operator* ChangeFloat64ToInt32();   // narrowing
  const Operator* ChangeFloat64ToUint32();  // narrowing
  const Operator* ChangeFloat64ToUint64();
  const Operator* TruncateFloat64ToUint32();
  const Operator* TruncateFloat32ToInt32();
  const Operator* TruncateFloat32ToUint32();
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
  const Operator* TruncateInt64ToInt32();
  const Operator* RoundFloat64ToInt32();
  const Operator* RoundInt32ToFloat32();
  const Operator* RoundInt64ToFloat32();
  const Operator* RoundInt64ToFloat64();
  const Operator* RoundUint32ToFloat32();
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

  // Floating point min/max complying to EcmaScript 6 (double-precision).
  const Operator* Float64Max();
  const Operator* Float64Min();
  // Floating point min/max complying to WebAssembly (single-precision).
  const Operator* Float32Max();
  const Operator* Float32Min();

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

  // Floating point neg.
  const Operator* Float32Neg();
  const Operator* Float64Neg();

  // Floating point trigonometric functions (double-precision).
  const Operator* Float64Acos();
  const Operator* Float64Acosh();
  const Operator* Float64Asin();
  const Operator* Float64Asinh();
  const Operator* Float64Atan();
  const Operator* Float64Atan2();
  const Operator* Float64Atanh();
  const Operator* Float64Cos();
  const Operator* Float64Cosh();
  const Operator* Float64Sin();
  const Operator* Float64Sinh();
  const Operator* Float64Tan();
  const Operator* Float64Tanh();

  // Floating point exponential functions (double-precision).
  const Operator* Float64Exp();
  const Operator* Float64Expm1();
  const Operator* Float64Pow();

  // Floating point logarithm (double-precision).
  const Operator* Float64Log();
  const Operator* Float64Log1p();
  const Operator* Float64Log2();
  const Operator* Float64Log10();

  // Floating point cube root (double-precision).
  const Operator* Float64Cbrt();

  // Floating point bit representation.
  const Operator* Float64ExtractLowWord32();
  const Operator* Float64ExtractHighWord32();
  const Operator* Float64InsertLowWord32();
  const Operator* Float64InsertHighWord32();

  // Change signalling NaN to quiet NaN.
  // Identity for any input that is not signalling NaN.
  const Operator* Float64SilenceNaN();

  // SIMD operators.
  const Operator* F32x4Splat();
  const Operator* F32x4ExtractLane(int32_t);
  const Operator* F32x4ReplaceLane(int32_t);
  const Operator* F32x4SConvertI32x4();
  const Operator* F32x4UConvertI32x4();
  const Operator* F32x4Abs();
  const Operator* F32x4Neg();
  const Operator* F32x4RecipApprox();
  const Operator* F32x4RecipSqrtApprox();
  const Operator* F32x4Add();
  const Operator* F32x4AddHoriz();
  const Operator* F32x4Sub();
  const Operator* F32x4Mul();
  const Operator* F32x4Div();
  const Operator* F32x4Min();
  const Operator* F32x4Max();
  const Operator* F32x4Eq();
  const Operator* F32x4Ne();
  const Operator* F32x4Lt();
  const Operator* F32x4Le();

  const Operator* I32x4Splat();
  const Operator* I32x4ExtractLane(int32_t);
  const Operator* I32x4ReplaceLane(int32_t);
  const Operator* I32x4SConvertF32x4();
  const Operator* I32x4SConvertI16x8Low();
  const Operator* I32x4SConvertI16x8High();
  const Operator* I32x4Neg();
  const Operator* I32x4Shl(int32_t);
  const Operator* I32x4ShrS(int32_t);
  const Operator* I32x4Add();
  const Operator* I32x4AddHoriz();
  const Operator* I32x4Sub();
  const Operator* I32x4Mul();
  const Operator* I32x4MinS();
  const Operator* I32x4MaxS();
  const Operator* I32x4Eq();
  const Operator* I32x4Ne();
  const Operator* I32x4GtS();
  const Operator* I32x4GeS();

  const Operator* I32x4UConvertF32x4();
  const Operator* I32x4UConvertI16x8Low();
  const Operator* I32x4UConvertI16x8High();
  const Operator* I32x4ShrU(int32_t);
  const Operator* I32x4MinU();
  const Operator* I32x4MaxU();
  const Operator* I32x4GtU();
  const Operator* I32x4GeU();

  const Operator* I16x8Splat();
  const Operator* I16x8ExtractLane(int32_t);
  const Operator* I16x8ReplaceLane(int32_t);
  const Operator* I16x8SConvertI8x16Low();
  const Operator* I16x8SConvertI8x16High();
  const Operator* I16x8Neg();
  const Operator* I16x8Shl(int32_t);
  const Operator* I16x8ShrS(int32_t);
  const Operator* I16x8SConvertI32x4();
  const Operator* I16x8Add();
  const Operator* I16x8AddSaturateS();
  const Operator* I16x8AddHoriz();
  const Operator* I16x8Sub();
  const Operator* I16x8SubSaturateS();
  const Operator* I16x8Mul();
  const Operator* I16x8MinS();
  const Operator* I16x8MaxS();
  const Operator* I16x8Eq();
  const Operator* I16x8Ne();
  const Operator* I16x8GtS();
  const Operator* I16x8GeS();

  const Operator* I16x8UConvertI8x16Low();
  const Operator* I16x8UConvertI8x16High();
  const Operator* I16x8ShrU(int32_t);
  const Operator* I16x8UConvertI32x4();
  const Operator* I16x8AddSaturateU();
  const Operator* I16x8SubSaturateU();
  const Operator* I16x8MinU();
  const Operator* I16x8MaxU();
  const Operator* I16x8GtU();
  const Operator* I16x8GeU();

  const Operator* I8x16Splat();
  const Operator* I8x16ExtractLane(int32_t);
  const Operator* I8x16ReplaceLane(int32_t);
  const Operator* I8x16Neg();
  const Operator* I8x16Shl(int32_t);
  const Operator* I8x16ShrS(int32_t);
  const Operator* I8x16SConvertI16x8();
  const Operator* I8x16Add();
  const Operator* I8x16AddSaturateS();
  const Operator* I8x16Sub();
  const Operator* I8x16SubSaturateS();
  const Operator* I8x16Mul();
  const Operator* I8x16MinS();
  const Operator* I8x16MaxS();
  const Operator* I8x16Eq();
  const Operator* I8x16Ne();
  const Operator* I8x16GtS();
  const Operator* I8x16GeS();

  const Operator* I8x16ShrU(int32_t);
  const Operator* I8x16UConvertI16x8();
  const Operator* I8x16AddSaturateU();
  const Operator* I8x16SubSaturateU();
  const Operator* I8x16MinU();
  const Operator* I8x16MaxU();
  const Operator* I8x16GtU();
  const Operator* I8x16GeU();

  const Operator* S128Load();
  const Operator* S128Store();

  const Operator* S128Zero();
  const Operator* S128And();
  const Operator* S128Or();
  const Operator* S128Xor();
  const Operator* S128Not();
  const Operator* S128Select();

  const Operator* S8x16Shuffle(const uint8_t shuffle[16]);

  const Operator* S1x4AnyTrue();
  const Operator* S1x4AllTrue();
  const Operator* S1x8AnyTrue();
  const Operator* S1x8AllTrue();
  const Operator* S1x16AnyTrue();
  const Operator* S1x16AllTrue();

  // load [base + index]
  const Operator* Load(LoadRepresentation rep);
  const Operator* ProtectedLoad(LoadRepresentation rep);

  // store [base + index], value
  const Operator* Store(StoreRepresentation rep);
  const Operator* ProtectedStore(MachineRepresentation rep);

  // unaligned load [base + index]
  const Operator* UnalignedLoad(UnalignedLoadRepresentation rep);

  // unaligned store [base + index], value
  const Operator* UnalignedStore(UnalignedStoreRepresentation rep);

  const Operator* StackSlot(int size, int alignment = 0);
  const Operator* StackSlot(MachineRepresentation rep, int alignment = 0);

  // Access to the machine stack.
  const Operator* LoadStackPointer();
  const Operator* LoadFramePointer();
  const Operator* LoadParentFramePointer();

  // checked-load heap, index, length
  const Operator* CheckedLoad(CheckedLoadRepresentation);
  // checked-store heap, index, length, value
  const Operator* CheckedStore(CheckedStoreRepresentation);

  // atomic-load [base + index]
  const Operator* AtomicLoad(LoadRepresentation rep);
  // atomic-store [base + index], value
  const Operator* AtomicStore(MachineRepresentation rep);
  // atomic-exchange [base + index], value
  const Operator* AtomicExchange(MachineType rep);
  // atomic-compare-exchange [base + index], old_value, new_value
  const Operator* AtomicCompareExchange(MachineType rep);
  // atomic-add [base + index], value
  const Operator* AtomicAdd(MachineType rep);
  // atomic-sub [base + index], value
  const Operator* AtomicSub(MachineType rep);
  // atomic-and [base + index], value
  const Operator* AtomicAnd(MachineType rep);
  // atomic-or [base + index], value
  const Operator* AtomicOr(MachineType rep);
  // atomic-xor [base + index], value
  const Operator* AtomicXor(MachineType rep);

  // Target machine word-size assumed by this builder.
  bool Is32() const { return word() == MachineRepresentation::kWord32; }
  bool Is64() const { return word() == MachineRepresentation::kWord64; }
  MachineRepresentation word() const { return word_; }

  bool UnalignedLoadSupported(MachineRepresentation rep) {
    return alignment_requirements_.IsUnalignedLoadSupported(rep);
  }

  bool UnalignedStoreSupported(MachineRepresentation rep) {
    return alignment_requirements_.IsUnalignedStoreSupported(rep);
  }

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
  V(Word, Clz)            \
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
  Zone* zone_;
  MachineOperatorGlobalCache const& cache_;
  MachineRepresentation const word_;
  Flags const flags_;
  AlignmentRequirements const alignment_requirements_;

  DISALLOW_COPY_AND_ASSIGN(MachineOperatorBuilder);
};


DEFINE_OPERATORS_FOR_FLAGS(MachineOperatorBuilder::Flags)

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_MACHINE_OPERATOR_H_
