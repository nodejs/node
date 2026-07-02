// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TURBOFAN_TYPES_H_
#define V8_OBJECTS_TURBOFAN_TYPES_H_

#include "src/base/bit-field.h"
#include "src/common/globals.h"
#include "src/objects/heap-object.h"
#include "src/objects/objects.h"
#include "src/objects/tagged.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class TurbofanTypeLowBits {
 public:
  using UnusedPaddingField1Bit = base::BitField<bool, 0, 1, uint32_t>;
  using OtherUnsigned31Bit = UnusedPaddingField1Bit::Next<bool, 1>;
  using OtherUnsigned32Bit = OtherUnsigned31Bit::Next<bool, 1>;
  using OtherSigned32Bit = OtherUnsigned32Bit::Next<bool, 1>;
  using OtherNumberBit = OtherSigned32Bit::Next<bool, 1>;
  using OtherStringBit = OtherNumberBit::Next<bool, 1>;
  using Negative31Bit = OtherStringBit::Next<bool, 1>;
  using NullBit = Negative31Bit::Next<bool, 1>;
  using UndefinedBit = NullBit::Next<bool, 1>;
  using BooleanBit = UndefinedBit::Next<bool, 1>;
  using Unsigned30Bit = BooleanBit::Next<bool, 1>;
  using MinusZeroBit = Unsigned30Bit::Next<bool, 1>;
  using NaNBit = MinusZeroBit::Next<bool, 1>;
  using SymbolBit = NaNBit::Next<bool, 1>;
  using InternalizedStringBit = SymbolBit::Next<bool, 1>;
  using OtherCallableBit = InternalizedStringBit::Next<bool, 1>;
  using OtherObjectBit = OtherCallableBit::Next<bool, 1>;
  using OtherUndetectableBit = OtherObjectBit::Next<bool, 1>;
  using CallableProxyBit = OtherUndetectableBit::Next<bool, 1>;
  using OtherProxyBit = CallableProxyBit::Next<bool, 1>;
  using CallableFunctionBit = OtherProxyBit::Next<bool, 1>;
  using ClassConstructorBit = CallableFunctionBit::Next<bool, 1>;
  using BoundFunctionBit = ClassConstructorBit::Next<bool, 1>;
  using OtherInternalBit = BoundFunctionBit::Next<bool, 1>;
  using ExternalPointerBit = OtherInternalBit::Next<bool, 1>;
  using ArrayBit = ExternalPointerBit::Next<bool, 1>;
  using UnsignedBigInt63Bit = ArrayBit::Next<bool, 1>;
  using OtherUnsignedBigInt64Bit = UnsignedBigInt63Bit::Next<bool, 1>;
  using NegativeBigInt63Bit = OtherUnsignedBigInt64Bit::Next<bool, 1>;
  using OtherBigIntBit = NegativeBigInt63Bit::Next<bool, 1>;
  using WasmObjectBit = OtherBigIntBit::Next<bool, 1>;
  using SandboxedPointerBit = WasmObjectBit::Next<bool, 1>;
  enum Flag : uint32_t {
    kNone = 0,
    kUnusedPaddingField1 = UnusedPaddingField1Bit::kMask,
    kOtherUnsigned31 = OtherUnsigned31Bit::kMask,
    kOtherUnsigned32 = OtherUnsigned32Bit::kMask,
    kOtherSigned32 = OtherSigned32Bit::kMask,
    kOtherNumber = OtherNumberBit::kMask,
    kOtherString = OtherStringBit::kMask,
    kNegative31 = Negative31Bit::kMask,
    kNull = NullBit::kMask,
    kUndefined = UndefinedBit::kMask,
    kBoolean = BooleanBit::kMask,
    kUnsigned30 = Unsigned30Bit::kMask,
    kMinusZero = MinusZeroBit::kMask,
    kNaN = NaNBit::kMask,
    kSymbol = SymbolBit::kMask,
    kInternalizedString = InternalizedStringBit::kMask,
    kOtherCallable = OtherCallableBit::kMask,
    kOtherObject = OtherObjectBit::kMask,
    kOtherUndetectable = OtherUndetectableBit::kMask,
    kCallableProxy = CallableProxyBit::kMask,
    kOtherProxy = OtherProxyBit::kMask,
    kCallableFunction = CallableFunctionBit::kMask,
    kClassConstructor = ClassConstructorBit::kMask,
    kBoundFunction = BoundFunctionBit::kMask,
    kOtherInternal = OtherInternalBit::kMask,
    kExternalPointer = ExternalPointerBit::kMask,
    kArray = ArrayBit::kMask,
    kUnsignedBigInt63 = UnsignedBigInt63Bit::kMask,
    kOtherUnsignedBigInt64 = OtherUnsignedBigInt64Bit::kMask,
    kNegativeBigInt63 = NegativeBigInt63Bit::kMask,
    kOtherBigInt = OtherBigIntBit::kMask,
    kWasmObject = WasmObjectBit::kMask,
    kSandboxedPointer = SandboxedPointerBit::kMask,
  };
  using Flags = base::Flags<Flag>;
  static constexpr int kFlagCount = 32;
};

class TurbofanTypeHighBits {
 public:
  using MachineBit = base::BitField<bool, 0, 1, uint32_t>;
  using HoleBit = MachineBit::Next<bool, 1>;
  using StringWrapperBit = HoleBit::Next<bool, 1>;
  using TypedArrayBit = StringWrapperBit::Next<bool, 1>;
  enum Flag : uint32_t {
    kNone = 0,
    kMachine = MachineBit::kMask,
    kHole = HoleBit::kMask,
    kStringWrapper = StringWrapperBit::kMask,
    kTypedArray = TypedArrayBit::kMask,
  };
  using Flags = base::Flags<Flag>;
  static constexpr int kFlagCount = 4;
};

V8_OBJECT class TurbofanType : public HeapObject {
} V8_OBJECT_END;

V8_OBJECT class TurbofanBitsetType : public TurbofanType {
 public:
  inline uint32_t bitset_low() const;
  inline void set_bitset_low(uint32_t value);
  inline uint32_t bitset_high() const;
  inline void set_bitset_high(uint32_t value);

  static constexpr int SizeFor() { return sizeof(TurbofanBitsetType); }

  class BodyDescriptor;

  DECL_PRINTER(TurbofanBitsetType)
  DECL_VERIFIER(TurbofanBitsetType)

 private:
  friend class TorqueGeneratedTurbofanBitsetTypeAsserts;

  uint32_t bitset_low_;
  uint32_t bitset_high_;
} V8_OBJECT_END;

V8_OBJECT class TurbofanUnionType : public TurbofanType {
 public:
  inline Tagged<TurbofanType> type1() const;
  inline void set_type1(Tagged<TurbofanType> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline Tagged<TurbofanType> type2() const;
  inline void set_type2(Tagged<TurbofanType> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  static constexpr int SizeFor() { return sizeof(TurbofanUnionType); }

  class BodyDescriptor;

  DECL_PRINTER(TurbofanUnionType)
  DECL_VERIFIER(TurbofanUnionType)

 private:
  friend class TorqueGeneratedTurbofanUnionTypeAsserts;

  TaggedMember<TurbofanType> type1_;
  TaggedMember<TurbofanType> type2_;
} V8_OBJECT_END;

V8_OBJECT class TurbofanRangeType : public TurbofanType {
 public:
  inline double min() const;
  inline void set_min(double value);
  inline double max() const;
  inline void set_max(double value);

  static constexpr int SizeFor() { return sizeof(TurbofanRangeType); }

  class BodyDescriptor;

  DECL_PRINTER(TurbofanRangeType)
  DECL_VERIFIER(TurbofanRangeType)

 private:
  friend class TorqueGeneratedTurbofanRangeTypeAsserts;

  UnalignedDoubleMember min_;
  UnalignedDoubleMember max_;
} V8_OBJECT_END;

V8_OBJECT class TurbofanHeapConstantType : public TurbofanType {
 public:
  inline Tagged<HeapObject> constant() const;
  inline void set_constant(Tagged<HeapObject> value,
                           WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  static constexpr int SizeFor() { return sizeof(TurbofanHeapConstantType); }

  class BodyDescriptor;

  DECL_PRINTER(TurbofanHeapConstantType)
  DECL_VERIFIER(TurbofanHeapConstantType)

 private:
  friend class TorqueGeneratedTurbofanHeapConstantTypeAsserts;

  TaggedMember<Object> constant_;
} V8_OBJECT_END;

V8_OBJECT class TurbofanOtherNumberConstantType : public TurbofanType {
 public:
  inline double constant() const;
  inline void set_constant(double value);

  static constexpr int SizeFor() {
    return sizeof(TurbofanOtherNumberConstantType);
  }

  class BodyDescriptor;

  DECL_PRINTER(TurbofanOtherNumberConstantType)
  DECL_VERIFIER(TurbofanOtherNumberConstantType)

 private:
  friend class TorqueGeneratedTurbofanOtherNumberConstantTypeAsserts;

  UnalignedDoubleMember constant_;
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TURBOFAN_TYPES_H_
