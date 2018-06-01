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
// values. The Flags<T, S> class is a template class, where T is an enum type,
// and S is the underlying storage type (usually int).
//
// The traditional C++ approach for storing OR-combinations of enum values is to
// use an int or unsigned int variable. The inconvenience with this approach is
// that there's no type checking at all; any enum value can be OR'd with any
// other enum value and passed on to a function that takes an int or unsigned
// int.
template <typename T, typename S = int>
class Flags final {
 public:
  typedef T flag_type;
  typedef S mask_type;

  Flags() : mask_(0) {}
  Flags(flag_type flag)  // NOLINT(runtime/explicit)
      : mask_(static_cast<S>(flag)) {}
  explicit Flags(mask_type mask) : mask_(static_cast<S>(mask)) {}

  bool operator==(flag_type flag) const {
    return mask_ == static_cast<S>(flag);
  }
  bool operator!=(flag_type flag) const {
    return mask_ != static_cast<S>(flag);
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

  Flags operator&(const Flags& flags) const { return Flags(*this) &= flags; }
  Flags operator|(const Flags& flags) const { return Flags(*this) |= flags; }
  Flags operator^(const Flags& flags) const { return Flags(*this) ^= flags; }

  Flags& operator&=(flag_type flag) { return operator&=(Flags(flag)); }
  Flags& operator|=(flag_type flag) { return operator|=(Flags(flag)); }
  Flags& operator^=(flag_type flag) { return operator^=(Flags(flag)); }

  Flags operator&(flag_type flag) const { return operator&(Flags(flag)); }
  Flags operator|(flag_type flag) const { return operator|(Flags(flag)); }
  Flags operator^(flag_type flag) const { return operator^(Flags(flag)); }

  Flags operator~() const { return Flags(~mask_); }

  operator mask_type() const { return mask_; }
  bool operator!() const { return !mask_; }

  friend size_t hash_value(const Flags& flags) { return flags.mask_; }

 private:
  mask_type mask_;
};

#define DEFINE_OPERATORS_FOR_FLAGS(Type)                             \
  inline Type operator&(                                             \
      Type::flag_type lhs,                                           \
      Type::flag_type rhs)ALLOW_UNUSED_TYPE V8_WARN_UNUSED_RESULT;   \
  inline Type operator&(Type::flag_type lhs, Type::flag_type rhs) {  \
    return Type(lhs) & rhs;                                          \
  }                                                                  \
  inline Type operator&(                                             \
      Type::flag_type lhs,                                           \
      const Type& rhs)ALLOW_UNUSED_TYPE V8_WARN_UNUSED_RESULT;       \
  inline Type operator&(Type::flag_type lhs, const Type& rhs) {      \
    return rhs & lhs;                                                \
  }                                                                  \
  inline void operator&(Type::flag_type lhs,                         \
                        Type::mask_type rhs)ALLOW_UNUSED_TYPE;       \
  inline void operator&(Type::flag_type lhs, Type::mask_type rhs) {} \
  inline Type operator|(Type::flag_type lhs, Type::flag_type rhs)    \
      ALLOW_UNUSED_TYPE V8_WARN_UNUSED_RESULT;                       \
  inline Type operator|(Type::flag_type lhs, Type::flag_type rhs) {  \
    return Type(lhs) | rhs;                                          \
  }                                                                  \
  inline Type operator|(Type::flag_type lhs, const Type& rhs)        \
      ALLOW_UNUSED_TYPE V8_WARN_UNUSED_RESULT;                       \
  inline Type operator|(Type::flag_type lhs, const Type& rhs) {      \
    return rhs | lhs;                                                \
  }                                                                  \
  inline void operator|(Type::flag_type lhs, Type::mask_type rhs)    \
      ALLOW_UNUSED_TYPE;                                             \
  inline void operator|(Type::flag_type lhs, Type::mask_type rhs) {} \
  inline Type operator^(Type::flag_type lhs, Type::flag_type rhs)    \
      ALLOW_UNUSED_TYPE V8_WARN_UNUSED_RESULT;                       \
  inline Type operator^(Type::flag_type lhs, Type::flag_type rhs) {  \
    return Type(lhs) ^ rhs;                                          \
  }                                                                  \
  inline Type operator^(Type::flag_type lhs, const Type& rhs)        \
      ALLOW_UNUSED_TYPE V8_WARN_UNUSED_RESULT;                       \
  inline Type operator^(Type::flag_type lhs, const Type& rhs) {      \
    return rhs ^ lhs;                                                \
  }                                                                  \
  inline void operator^(Type::flag_type lhs, Type::mask_type rhs)    \
      ALLOW_UNUSED_TYPE;                                             \
  inline void operator^(Type::flag_type lhs, Type::mask_type rhs) {} \
  inline Type operator~(Type::flag_type val)ALLOW_UNUSED_TYPE;       \
  inline Type operator~(Type::flag_type val) { return ~Type(val); }

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_FLAGS_H_
