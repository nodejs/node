// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_FLAGS_H_
#define V8_BASE_FLAGS_H_

#include <cstddef>

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
  constexpr explicit Flags(mask_type mask)
      : mask_(static_cast<mask_type>(mask)) {}

  constexpr bool operator==(flag_type flag) const {
    return mask_ == static_cast<mask_type>(flag);
  }
  constexpr bool operator!=(flag_type flag) const {
    return mask_ != static_cast<mask_type>(flag);
  }

  Flags& operator&=(const Flags& flags) {
    mask_ &= flags.mask_;
    return *this;
  }
  Flags& operator|=(const Flags& flags) {
    mask_ |= flags.mask_;
    return *this;
  }
  Flags& operator^=(const Flags& flags) {
    mask_ ^= flags.mask_;
    return *this;
  }

  constexpr Flags operator&(const Flags& flags) const {
    return Flags(mask_ & flags.mask_);
  }
  constexpr Flags operator|(const Flags& flags) const {
    return Flags(mask_ | flags.mask_);
  }
  constexpr Flags operator^(const Flags& flags) const {
    return Flags(mask_ ^ flags.mask_);
  }

  Flags& operator&=(flag_type flag) { return operator&=(Flags(flag)); }
  Flags& operator|=(flag_type flag) { return operator|=(Flags(flag)); }
  Flags& operator^=(flag_type flag) { return operator^=(Flags(flag)); }

  // Sets or clears given flag.
  Flags& set(flag_type flag, bool value) {
    if (value) return operator|=(Flags(flag));
    return operator&=(~Flags(flag));
  }

  constexpr Flags operator&(flag_type flag) const {
    return operator&(Flags(flag));
  }
  constexpr Flags operator|(flag_type flag) const {
    return operator|(Flags(flag));
  }
  constexpr Flags operator^(flag_type flag) const {
    return operator^(Flags(flag));
  }

  constexpr Flags operator~() const { return Flags(~mask_); }

  constexpr operator mask_type() const { return mask_; }
  constexpr bool operator!() const { return !mask_; }

  Flags without(flag_type flag) const { return *this & (~Flags(flag)); }

  friend size_t hash_value(const Flags& flags) { return flags.mask_; }

 private:
  BitfieldStorageT mask_;
};

#define DEFINE_OPERATORS_FOR_FLAGS(Type)                                 \
  V8_ALLOW_UNUSED V8_WARN_UNUSED_RESULT inline constexpr Type operator&( \
      Type::flag_type lhs, Type::flag_type rhs) {                        \
    return Type(lhs) & rhs;                                              \
  }                                                                      \
  V8_ALLOW_UNUSED V8_WARN_UNUSED_RESULT inline constexpr Type operator&( \
      Type::flag_type lhs, const Type& rhs) {                            \
    return rhs & lhs;                                                    \
  }                                                                      \
  V8_ALLOW_UNUSED inline void operator&(Type::flag_type lhs,             \
                                        Type::mask_type rhs) {}          \
  V8_ALLOW_UNUSED V8_WARN_UNUSED_RESULT inline constexpr Type operator|( \
      Type::flag_type lhs, Type::flag_type rhs) {                        \
    return Type(lhs) | rhs;                                              \
  }                                                                      \
  V8_ALLOW_UNUSED V8_WARN_UNUSED_RESULT inline constexpr Type operator|( \
      Type::flag_type lhs, const Type& rhs) {                            \
    return rhs | lhs;                                                    \
  }                                                                      \
  V8_ALLOW_UNUSED inline void operator|(Type::flag_type lhs,             \
                                        Type::mask_type rhs) {}          \
  V8_ALLOW_UNUSED V8_WARN_UNUSED_RESULT inline constexpr Type operator^( \
      Type::flag_type lhs, Type::flag_type rhs) {                        \
    return Type(lhs) ^ rhs;                                              \
  }                                                                      \
  V8_ALLOW_UNUSED V8_WARN_UNUSED_RESULT inline constexpr Type operator^( \
      Type::flag_type lhs, const Type& rhs) {                            \
    return rhs ^ lhs;                                                    \
  }                                                                      \
  V8_ALLOW_UNUSED inline void operator^(Type::flag_type lhs,             \
                                        Type::mask_type rhs) {}          \
  V8_ALLOW_UNUSED inline constexpr Type operator~(Type::flag_type val) { \
    return ~Type(val);                                                   \
  }

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_FLAGS_H_
