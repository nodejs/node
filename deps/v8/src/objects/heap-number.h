// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HEAP_NUMBER_H_
#define V8_OBJECTS_HEAP_NUMBER_H_

#include "src/objects/primitive-heap-object.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/heap-number-tq.inc"

// The HeapNumber class describes heap allocated numbers that cannot be
// represented in a Smi (small integer).
class HeapNumber
    : public TorqueGeneratedHeapNumber<HeapNumber, PrimitiveHeapObject> {
 public:
  // Since the value is read from the compiler, and it can't be done
  // atomically, we signal both to TSAN and ourselves that this is a
  // relaxed load and store.
  inline uint64_t value_as_bits(RelaxedLoadTag) const;
  inline void set_value_as_bits(uint64_t bits, RelaxedStoreTag);

  inline int get_exponent();
  inline int get_sign();

  // Layout description.
  // IEEE doubles are two 32 bit words.  The first is just mantissa, the second
  // is a mixture of sign, exponent and mantissa. The offsets of two 32 bit
  // words within double numbers are endian dependent and they are set
  // accordingly.
#if defined(V8_TARGET_LITTLE_ENDIAN)
  static const int kMantissaOffset = kValueOffset;
  static const int kExponentOffset = kValueOffset + 4;
#elif defined(V8_TARGET_BIG_ENDIAN)
  static const int kMantissaOffset = kValueOffset + 4;
  static const int kExponentOffset = kValueOffset;
#else
#error Unknown byte ordering
#endif

  static const uint32_t kSignMask = 0x80000000u;
  static const uint32_t kExponentMask = 0x7ff00000u;
  static const uint32_t kMantissaMask = 0xfffffu;
  static const int kMantissaBits = 52;
  static const int kExponentBits = 11;
  static const int kExponentBias = 1023;
  static const int kExponentShift = 20;
  static const int kInfinityOrNanExponent =
      (kExponentMask >> kExponentShift) - kExponentBias;
  static const int kMantissaBitsInTopWord = 20;
  static const int kNonMantissaBitsInTopWord = 12;

  DECL_PRINTER(HeapNumber)
  V8_EXPORT_PRIVATE void HeapNumberShortPrint(std::ostream& os);

  TQ_OBJECT_CONSTRUCTORS(HeapNumber)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_HEAP_NUMBER_H_
