// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_TURBOSHAFT_TYPES_H_
#define V8_OBJECTS_TURBOSHAFT_TYPES_H_

#include "src/common/globals.h"
#include "src/objects/heap-object.h"
#include "torque-generated/bit-fields.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8::internal {

#include "torque-generated/src/objects/turboshaft-types-tq.inc"

class TurboshaftFloatSpecialValues {
 public:
  DEFINE_TORQUE_GENERATED_TURBOSHAFT_FLOAT_SPECIAL_VALUES()
};

V8_OBJECT class TurboshaftType : public HeapObject {
} V8_OBJECT_END;

V8_OBJECT class TurboshaftWord32Type : public TurboshaftType {
 public:
  DECL_VERIFIER(TurboshaftWord32Type)
} V8_OBJECT_END;

V8_OBJECT class TurboshaftWord32RangeType : public TurboshaftWord32Type {
 public:
  inline uint32_t from() const;
  inline void set_from(uint32_t value);
  inline uint32_t to() const;
  inline void set_to(uint32_t value);

  static constexpr int SizeFor() { return sizeof(TurboshaftWord32RangeType); }

  class BodyDescriptor;

  DECL_PRINTER(TurboshaftWord32RangeType)
  DECL_VERIFIER(TurboshaftWord32RangeType)

 private:
  friend class TorqueGeneratedTurboshaftWord32RangeTypeAsserts;

  uint32_t from_;
  uint32_t to_;
} V8_OBJECT_END;

V8_OBJECT class TurboshaftWord32SetType : public TurboshaftWord32Type {
 public:
  inline uint32_t set_size() const;
  inline void set_set_size(uint32_t value);
  inline uint32_t elements(int i) const;
  inline void set_elements(int i, uint32_t value);

  static constexpr int SizeFor(int set_size);

  class BodyDescriptor;

  DECL_PRINTER(TurboshaftWord32SetType)
  DECL_VERIFIER(TurboshaftWord32SetType)

 private:
  friend class TorqueGeneratedTurboshaftWord32SetTypeAsserts;

  uint32_t set_size_;
  FLEXIBLE_ARRAY_MEMBER(uint32_t, elements);
} V8_OBJECT_END;

constexpr int TurboshaftWord32SetType::SizeFor(int set_size) {
  return OBJECT_POINTER_ALIGN(OFFSET_OF_DATA_START(TurboshaftWord32SetType) +
                              set_size * sizeof(uint32_t));
}

V8_OBJECT class TurboshaftWord64Type : public TurboshaftType {
 public:
  DECL_VERIFIER(TurboshaftWord64Type)
} V8_OBJECT_END;

V8_OBJECT class TurboshaftWord64RangeType : public TurboshaftWord64Type {
 public:
  inline uint32_t from_high() const;
  inline void set_from_high(uint32_t value);
  inline uint32_t from_low() const;
  inline void set_from_low(uint32_t value);
  inline uint32_t to_high() const;
  inline void set_to_high(uint32_t value);
  inline uint32_t to_low() const;
  inline void set_to_low(uint32_t value);

  static constexpr int SizeFor() { return sizeof(TurboshaftWord64RangeType); }

  class BodyDescriptor;

  DECL_PRINTER(TurboshaftWord64RangeType)
  DECL_VERIFIER(TurboshaftWord64RangeType)

 private:
  friend class TorqueGeneratedTurboshaftWord64RangeTypeAsserts;

  uint32_t from_high_;
  uint32_t from_low_;
  uint32_t to_high_;
  uint32_t to_low_;
} V8_OBJECT_END;

V8_OBJECT class TurboshaftWord64SetType : public TurboshaftWord64Type {
 public:
  inline uint32_t set_size() const;
  inline void set_set_size(uint32_t value);
  inline uint32_t elements_high(int i) const;
  inline void set_elements_high(int i, uint32_t value);
  inline uint32_t elements_low(int i) const;
  inline void set_elements_low(int i, uint32_t value);

  static constexpr int SizeFor(int set_size);

  class BodyDescriptor;

  DECL_PRINTER(TurboshaftWord64SetType)
  DECL_VERIFIER(TurboshaftWord64SetType)

 private:
  friend class TorqueGeneratedTurboshaftWord64SetTypeAsserts;

  uint32_t set_size_;
  // Layout: elements_[0..set_size) is the high-32-bit array; the following
  // set_size slots hold the low-32-bit array.
  FLEXIBLE_ARRAY_MEMBER(uint32_t, elements);
} V8_OBJECT_END;

constexpr int TurboshaftWord64SetType::SizeFor(int set_size) {
  return OBJECT_POINTER_ALIGN(OFFSET_OF_DATA_START(TurboshaftWord64SetType) +
                              2 * set_size * sizeof(uint32_t));
}

V8_OBJECT class TurboshaftFloat64Type : public TurboshaftType {
 public:
  inline uint32_t special_values() const;
  inline void set_special_values(uint32_t value);

  DECL_VERIFIER(TurboshaftFloat64Type)

 private:
  friend class TorqueGeneratedTurboshaftFloat64TypeAsserts;

  uint32_t special_values_;
} V8_OBJECT_END;

V8_OBJECT class TurboshaftFloat64RangeType : public TurboshaftFloat64Type {
 public:
  inline double min() const;
  inline void set_min(double value);
  inline double max() const;
  inline void set_max(double value);

  static constexpr int SizeFor() { return sizeof(TurboshaftFloat64RangeType); }

  class BodyDescriptor;

  DECL_PRINTER(TurboshaftFloat64RangeType)
  DECL_VERIFIER(TurboshaftFloat64RangeType)

 private:
  friend class TorqueGeneratedTurboshaftFloat64RangeTypeAsserts;

  // Explicit padding to align `min_` / `max_` to an 8-byte boundary,
  // matching the previous Torque-generated layout. The leading underscore
  // mirrors the `_padding` field name in the .tq source, which is what
  // the Torque-generated offset assertion compares against.
  uint32_t _padding_;
  UnalignedDoubleMember min_;
  UnalignedDoubleMember max_;
} V8_OBJECT_END;

V8_OBJECT class TurboshaftFloat64SetType : public TurboshaftFloat64Type {
 public:
  inline uint32_t set_size() const;
  inline void set_set_size(uint32_t value);
  inline double elements(int i) const;
  inline void set_elements(int i, double value);

  static constexpr int SizeFor(int set_size);

  class BodyDescriptor;

  DECL_PRINTER(TurboshaftFloat64SetType)
  DECL_VERIFIER(TurboshaftFloat64SetType)

 private:
  friend class TorqueGeneratedTurboshaftFloat64SetTypeAsserts;

  uint32_t set_size_;
  FLEXIBLE_ARRAY_MEMBER(UnalignedDoubleMember, elements);
} V8_OBJECT_END;

constexpr int TurboshaftFloat64SetType::SizeFor(int set_size) {
  return OFFSET_OF_DATA_START(TurboshaftFloat64SetType) +
         set_size * sizeof(double);
}

}  // namespace v8::internal

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_TURBOSHAFT_TYPES_H_
