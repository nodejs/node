// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SMI_H_
#define V8_OBJECTS_SMI_H_

#include "src/globals.h"
#include "src/objects/heap-object.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// Smi represents integer Numbers that can be stored in 31 bits.
// Smis are immediate which means they are NOT allocated in the heap.
// The ptr_ value has the following format: [31 bit signed int] 0
// For long smis it has the following format:
//     [32 bit signed int] [31 bits zero padding] 0
// Smi stands for small integer.
class Smi : public Object {
 public:
  // This replaces the OBJECT_CONSTRUCTORS macro, because Smis are special
  // in that we want them to be constexprs.
  constexpr Smi() : Object() {}
  explicit constexpr Smi(Address ptr) : Object(ptr) {
#if V8_CAN_HAVE_DCHECK_IN_CONSTEXPR
    DCHECK(HAS_SMI_TAG(ptr));
#endif
  }
  Smi* operator->() { return this; }
  const Smi* operator->() const { return this; }

  // Returns the integer value.
  inline int value() const { return Internals::SmiValue(ptr()); }
  inline Smi ToUint32Smi() {
    if (value() <= 0) return Smi::FromInt(0);
    return Smi::FromInt(static_cast<uint32_t>(value()));
  }

  // Convert a Smi object to an int.
  static inline int ToInt(const Object object);

  // Convert a value to a Smi object.
  static inline constexpr Smi FromInt(int value) {
#if V8_CAN_HAVE_DCHECK_IN_CONSTEXPR
    DCHECK(Smi::IsValid(value));
#endif
    return Smi(Internals::IntToSmi(value));
  }

  static inline Smi FromIntptr(intptr_t value) {
    DCHECK(Smi::IsValid(value));
    int smi_shift_bits = kSmiTagSize + kSmiShiftSize;
    return Smi((static_cast<Address>(value) << smi_shift_bits) | kSmiTag);
  }

  // Given {value} in [0, 2^31-1], force it into Smi range by changing at most
  // the MSB (leaving the lower 31 bit unchanged).
  static inline Smi From31BitPattern(int value) {
    return Smi::FromInt((value << (32 - kSmiValueSize)) >>
                        (32 - kSmiValueSize));
  }

  template <typename E,
            typename = typename std::enable_if<std::is_enum<E>::value>::type>
  static inline Smi FromEnum(E value) {
    STATIC_ASSERT(sizeof(E) <= sizeof(int));
    return FromInt(static_cast<int>(value));
  }

  // Returns whether value can be represented in a Smi.
  static inline bool constexpr IsValid(intptr_t value) {
#if V8_CAN_HAVE_DCHECK_IN_CONSTEXPR
    DCHECK(Internals::IsValidSmi(value) ==
           (value >= kMinValue && value <= kMaxValue));
#endif
    return Internals::IsValidSmi(value);
  }

  // Compare two Smis x, y as if they were converted to strings and then
  // compared lexicographically. Returns:
  // -1 if x < y.
  //  0 if x == y.
  //  1 if x > y.
  // Returns the result (a tagged Smi) as a raw Address for ExternalReference
  // usage.
  static Address LexicographicCompare(Isolate* isolate, Smi x, Smi y);

  DECL_CAST(Smi)

  // Dispatched behavior.
  V8_EXPORT_PRIVATE void SmiPrint(std::ostream& os) const;  // NOLINT
  DECL_VERIFIER(Smi)

  // C++ does not allow us to have an object of type Smi within class Smi,
  // so the kZero value has type Object. Consider it deprecated; new code
  // should use zero() instead.
  V8_EXPORT_PRIVATE static constexpr Object kZero = Object(0);
  // If you need something with type Smi, call zero() instead. Since it is
  // a constexpr, "calling" it is just as efficient as reading kZero.
  static inline constexpr Smi zero() { return Smi::FromInt(0); }
  static constexpr int kMinValue = kSmiMinValue;
  static constexpr int kMaxValue = kSmiMaxValue;
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SMI_H_
