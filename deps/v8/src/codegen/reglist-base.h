// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_REGLIST_BASE_H_
#define V8_CODEGEN_REGLIST_BASE_H_

#include <cstdint>
#include <initializer_list>

#include "src/base/bits.h"
#include "src/base/iterator.h"
#include "src/base/template-utils.h"

namespace v8 {
namespace internal {

class Register;

template <typename RegisterT>
class RegListBase {
  using num_registers_sized_storage_t = std::conditional_t<
      RegisterT::kNumRegisters <= 16, uint16_t,
      std::conditional_t<RegisterT::kNumRegisters <= 32, uint32_t, uint64_t>>;
  static_assert(RegisterT::kNumRegisters <= 64);

 public:
  class Iterator;
  class ReverseIterator;

#ifdef V8_TARGET_ARCH_ARM64
  // On ARM64 the sp register has the special value 63 (kSPRegInternalCode)
  using storage_t = typename std::conditional_t<
      std::is_same_v<RegisterT, v8::internal::Register>, uint64_t,
      num_registers_sized_storage_t>;
#else
  using storage_t = num_registers_sized_storage_t;
#endif

  constexpr RegListBase() = default;
  constexpr RegListBase(std::initializer_list<RegisterT> regs) {
    for (RegisterT reg : regs) {
      set(reg);
    }
  }

  constexpr void set(RegisterT reg) {
    if (!reg.is_valid()) return;
    regs_ |= storage_t{1} << reg.code();
  }

  constexpr void clear(RegisterT reg) {
    if (!reg.is_valid()) return;
    regs_ &= ~(storage_t{1} << reg.code());
  }

  constexpr bool has(RegisterT reg) const {
    if (!reg.is_valid()) return false;
    return (regs_ & (storage_t{1} << reg.code())) != 0;
  }

  constexpr void clear(RegListBase other) { regs_ &= ~other.regs_; }

  constexpr bool is_empty() const { return regs_ == 0; }

  constexpr unsigned Count() const {
    return base::bits::CountPopulation(regs_);
  }

  constexpr RegListBase operator&(const RegListBase other) const {
    return RegListBase(regs_ & other.regs_);
  }

  constexpr RegListBase operator|(const RegListBase other) const {
    return RegListBase(regs_ | other.regs_);
  }

  constexpr RegListBase operator^(const RegListBase other) const {
    return RegListBase(regs_ ^ other.regs_);
  }

  constexpr RegListBase operator-(const RegListBase other) const {
    return RegListBase(regs_ & ~other.regs_);
  }

  constexpr RegListBase operator|(const RegisterT reg) const {
    return *this | RegListBase{reg};
  }

  constexpr RegListBase operator-(const RegisterT reg) const {
    return *this - RegListBase{reg};
  }

  constexpr RegListBase& operator&=(const RegListBase other) {
    regs_ &= other.regs_;
    return *this;
  }

  constexpr RegListBase& operator|=(const RegListBase other) {
    regs_ |= other.regs_;
    return *this;
  }

  constexpr bool operator==(const RegListBase other) const {
    return regs_ == other.regs_;
  }
  constexpr bool operator!=(const RegListBase other) const {
    return regs_ != other.regs_;
  }

  constexpr RegisterT first() const {
    CHECK(!is_empty());
    int first_code = base::bits::CountTrailingZerosNonZero(regs_);
    return RegisterT::from_code(first_code);
  }

  constexpr RegisterT last() const {
    CHECK(!is_empty());
    int last_code =
        8 * sizeof(regs_) - 1 - base::bits::CountLeadingZeros(regs_);
    return RegisterT::from_code(last_code);
  }

  constexpr RegisterT PopFirst() {
    RegisterT reg = first();
    clear(reg);
    return reg;
  }

  constexpr storage_t bits() const { return regs_; }

  inline Iterator begin() const;
  inline Iterator end() const;

  inline ReverseIterator rbegin() const;
  inline ReverseIterator rend() const;

  static RegListBase FromBits(storage_t bits) { return RegListBase(bits); }

  template <storage_t bits>
  static constexpr RegListBase FromBits() {
    return RegListBase{bits};
  }

 private:
  // Unchecked constructor. Only use for valid bits.
  explicit constexpr RegListBase(storage_t bits) : regs_(bits) {}

  storage_t regs_ = 0;
};

template <typename RegisterT>
class RegListBase<RegisterT>::Iterator
    : public base::iterator<std::forward_iterator_tag, RegisterT> {
 public:
  RegisterT operator*() { return remaining_.first(); }
  Iterator& operator++() {
    remaining_.clear(remaining_.first());
    return *this;
  }
  bool operator==(Iterator other) { return remaining_ == other.remaining_; }
  bool operator!=(Iterator other) { return remaining_ != other.remaining_; }

 private:
  explicit Iterator(RegListBase<RegisterT> remaining) : remaining_(remaining) {}
  friend class RegListBase;

  RegListBase<RegisterT> remaining_;
};

template <typename RegisterT>
class RegListBase<RegisterT>::ReverseIterator
    : public base::iterator<std::forward_iterator_tag, RegisterT> {
 public:
  RegisterT operator*() { return remaining_.last(); }
  ReverseIterator& operator++() {
    remaining_.clear(remaining_.last());
    return *this;
  }
  bool operator==(ReverseIterator other) {
    return remaining_ == other.remaining_;
  }
  bool operator!=(ReverseIterator other) {
    return remaining_ != other.remaining_;
  }

 private:
  explicit ReverseIterator(RegListBase<RegisterT> remaining)
      : remaining_(remaining) {}
  friend class RegListBase;

  RegListBase<RegisterT> remaining_;
};

template <typename RegisterT>
typename RegListBase<RegisterT>::Iterator RegListBase<RegisterT>::begin()
    const {
  return Iterator{*this};
}
template <typename RegisterT>
typename RegListBase<RegisterT>::Iterator RegListBase<RegisterT>::end() const {
  return Iterator{RegListBase<RegisterT>{}};
}

template <typename RegisterT>
typename RegListBase<RegisterT>::ReverseIterator
RegListBase<RegisterT>::rbegin() const {
  return ReverseIterator{*this};
}
template <typename RegisterT>
typename RegListBase<RegisterT>::ReverseIterator RegListBase<RegisterT>::rend()
    const {
  return ReverseIterator{RegListBase<RegisterT>{}};
}

template <typename RegisterT>
inline std::ostream& operator<<(std::ostream& os,
                                RegListBase<RegisterT> reglist) {
  os << "{";
  for (bool first = true; !reglist.is_empty(); first = false) {
    RegisterT reg = reglist.first();
    reglist.clear(reg);
    os << (first ? "" : ", ") << reg;
  }
  return os << "}";
}

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_REGLIST_BASE_H_
