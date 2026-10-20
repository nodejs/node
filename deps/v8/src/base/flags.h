// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_FLAGS_H_
#define V8_BASE_FLAGS_H_

#include <cstddef>
#include <initializer_list>

#include "src/base/compiler-specific.h"

namespace v8 {
namespace base {

// The Flags class provides a type-safe way of storing OR-combinations of enum
// values.
//
// The traditional C++ approach for storing OR-combinations of enum values is to
// use an int or unsigned int variable. The inconvenience with this approach is
// that there's no type checking at all; any enum value can be OR'd with any
// other enum value and passed on to a function that takes an int or unsigned
// int.
template <typename EnumT, typename BitfieldT = int,
          typename BitfieldStorageT = BitfieldT>
class Flags final {
 public:
  static_assert(sizeof(BitfieldStorageT) >= sizeof(BitfieldT));
  using flag_type = EnumT;
  using mask_type = BitfieldT;

  constexpr Flags() : mask_(0) {}
  constexpr Flags(flag_type flag)  // NOLINT(runtime/explicit)
      : mask_(static_cast<mask_type>(flag)) {}
  // NOLINTNEXTLINE
  constexpr Flags(std::initializer_list<flag_type> flags) : Flags() {
    for (flag_type flag : flags) {
      mask_ |= static_cast<mask_type>(flag);
    }
  }
  constexpr explicit Flags(mask_type mask)
      : mask_(static_cast<mask_type>(mask)) {}

  constexpr bool operator==(const Flags& flags) const = default;
  constexpr bool operator!=(const Flags& flags) const = default;
  constexpr bool operator==(flag_type flag) const {
    return mask_ == static_cast<mask_type>(flag);
  }

  // Sets or clears given flag.
  Flags& set(flag_type flag, bool value) { return set(Flags(flag), value); }
  // Sets or clears given flags.
  Flags& set(const Flags& flags, bool value) {
    if (value) return *this |= flags;
    return *this &= ~flags;
  }

  constexpr bool contains(Flags flags) const {
    return (mask_ & flags.mask_) == flags.mask_;
  }

  Flags with(Flags flags) const { return *this | flags; }
  Flags without(Flags flags) const { return *this & ~flags; }

  friend size_t hash_value(Flags flags) { return flags.mask_; }

#define DEFINE_BITWISE_OPERATOR(OP)                                        \
  friend constexpr Flags operator OP(const Flags& flags1,                  \
                                     const Flags& flags2) {                \
    return Flags(flags1.mask_ OP flags2.mask_);                            \
  }                                                                        \
  friend constexpr Flags operator OP(const Flags& flags, flag_type flag) { \
    return flags OP Flags(flag);                                           \
  }                                                                        \
  friend constexpr Flags operator OP(flag_type flag, const Flags& flags) { \
    return Flags(flag) OP flags;                                           \
  }                                                                        \
  Flags& operator OP##=(const Flags & flags) {                             \
    mask_ OP## = flags.mask_;                                              \
    return *this;                                                          \
  }                                                                        \
  Flags& operator OP##=(flag_type flag) { return *this OP## = Flags(flag); }

  DEFINE_BITWISE_OPERATOR(&)
  DEFINE_BITWISE_OPERATOR(|)
  DEFINE_BITWISE_OPERATOR(^)
#undef DEFINE_BITWISE_OPERATOR

  constexpr Flags operator~() const { return Flags(~mask_); }

  // NOLINTNEXTLINE
  constexpr operator mask_type() const { return mask_; }
  constexpr bool operator!() const { return !mask_; }

 private:
  BitfieldStorageT mask_;
};

#define DEFINE_OPERATORS_FOR_FLAGS(Type)                                 \
  V8_ALLOW_UNUSED V8_WARN_UNUSED_RESULT inline constexpr Type operator&( \
      Type::flag_type lhs, Type::flag_type rhs) {                        \
    return Type(lhs) & rhs;                                              \
  }                                                                      \
  V8_ALLOW_UNUSED inline void operator&(Type::flag_type lhs,             \
                                        Type::mask_type rhs) {}          \
  V8_ALLOW_UNUSED V8_WARN_UNUSED_RESULT inline constexpr Type operator|( \
      Type::flag_type lhs, Type::flag_type rhs) {                        \
    return Type(lhs) | rhs;                                              \
  }                                                                      \
  V8_ALLOW_UNUSED inline void operator|(Type::flag_type lhs,             \
                                        Type::mask_type rhs) {}          \
  V8_ALLOW_UNUSED V8_WARN_UNUSED_RESULT inline constexpr Type operator^( \
      Type::flag_type lhs, Type::flag_type rhs) {                        \
    return Type(lhs) ^ rhs;                                              \
  }                                                                      \
  V8_ALLOW_UNUSED inline void operator^(Type::flag_type lhs,             \
                                        Type::mask_type rhs) {}          \
  V8_ALLOW_UNUSED inline constexpr Type operator~(Type::flag_type val) { \
    return ~Type(val);                                                   \
  }

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_FLAGS_H_
