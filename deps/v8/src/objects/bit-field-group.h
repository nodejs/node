// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_BIT_FIELD_GROUP_H_
#define V8_OBJECTS_BIT_FIELD_GROUP_H_

#include <type_traits>

#include "src/base/bit-field.h"
#include "src/base/macros.h"

namespace v8::internal {

// Base class for a struct that holds a base::BitField chain and is used as the
// type of the object member storing those bits:
//
//   class Map {
//     struct Bits1 : BitFieldGroup<Bits1, uint8_t> {
//       using BitFieldGroup::BitFieldGroup;
//       using IsCallableBit = base::BitField<bool, 0, 1, uint8_t>;
//       using IsUndetectableBit = IsCallableBit::Next<bool, 1>;
//       BIT_FIELD_GETTER(is_callable, IsCallableBit)
//       BIT_FIELD_GETTER(is_undetectable, IsUndetectableBit)
//     };
//     std::atomic<Bits1> bit_field_;
//   };
//
// Metagen recognizes members of such types and records the chain in the
// layout JSON, from which Torque derives (or verifies) the corresponding
// bitfield struct. Naming and layout requirements:
//  - Every type alias in the struct must be a base::BitField; they are read in
//    declaration order, which must be bit order without gaps.
//  - The Torque field name is the alias name without its Bit/Bits suffix, in
//    snake_case (IsCallableBit -> is_callable). The getter names follow the
//    same convention.
//  - The Torque struct name is the C++ name with the enclosing class names
//    prepended (Map::Bits1 -> MapBits1).
//
// Implicit conversions to and from U support Bit::decode(bit_field()) and
// set_bit_field(Bit::update(bit_field(), v)). get<Bit>() reads a field;
// with<Bit>(v) returns a copy with that field updated. The caller must store
// the updated copy back to the member.
template <typename Derived, typename U>
class BitFieldGroup {
 public:
  static_assert(std::is_unsigned_v<U>);

  using UnderlyingType = U;

  constexpr BitFieldGroup() = default;
  // NOLINTNEXTLINE(runtime/explicit)
  constexpr BitFieldGroup(U value) : value_(value) {}
  constexpr operator U() const { return value_; }

  template <typename Bit>
  constexpr typename Bit::FieldType get() const {
    static_assert(std::is_same_v<typename Bit::BaseType, U>);
    return Bit::decode(value_);
  }

  template <typename Bit>
  V8_NODISCARD constexpr Derived with(typename Bit::FieldType value) const {
    static_assert(std::is_same_v<typename Bit::BaseType, U>);
    return Derived(Bit::update(value_, value));
  }

 private:
  U value_ = 0;
};

}  // namespace v8::internal

#endif  // V8_OBJECTS_BIT_FIELD_GROUP_H_
