// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TURBOFAN_TYPES_H_
#define V8_OBJECTS_TURBOFAN_TYPES_H_

#include "src/common/globals.h"
#include "src/objects/heap-object.h"
#include "src/objects/objects.h"
#include "src/objects/tagged.h"
#include "torque-generated/bit-fields.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/turbofan-types-tq.inc"

class TurbofanTypeLowBits {
 public:
  DEFINE_TORQUE_GENERATED_TURBOFAN_TYPE_LOW_BITS()
};

class TurbofanTypeHighBits {
 public:
  DEFINE_TORQUE_GENERATED_TURBOFAN_TYPE_HIGH_BITS()
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
