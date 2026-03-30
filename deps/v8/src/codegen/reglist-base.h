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

template <typename registered>
class RegListBase {
  using num_registers_sized_storage_t = std::conditional_t<
      registered::kNumRegisters <= 16, uint16_t,
      std::conditional_t<registered::kNumRegisters <= 32, uint32_t, uint64_t>>;
  static_assert(registered::kNumRegisters <= 64);

 public:
  class Iterator;
  class ReverseIterator;

#ifdef V8_TARGET_ARCH_ARM64
  // On ARM64 the sp register has the special value 63 (kSPRegInternalCode)
  using storage_t = typename std::conditional_t<
      std::is_same_v<registered, v8::internal::Register>, uint64_t,
      num_registers_sized_storage_t>;
#else
  using storage_t = num_registers_sized_storage_t;
#endif

  constexpr RegListBase() = default;
  constexpr RegListBase(std::initializer_list<registered> regs) {
    for (registered reg : regs) {
      set(reg);
    }
  }

  constexpr void set(registered reg) {
    if (!reg.is_valid()) return;
    regs_ |= storage_t{1} << reg.code();
  }

  constexpr void clear(registered reg) {
    if (!reg.is_valid()) return;
    regs_ &= ~(storage_t{1} << reg.code());
  }

  constexpr bool has(registered reg) const {
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

  constexpr RegListBase operator|(const registered reg) const {
    return *this | RegListBase{reg};
  }

  constexpr RegListBase operator-(const registered reg) const {
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

  constexpr registered first() const {
    CHECK(!is_empty());
    int first_code = base::bits::CountTrailingZerosNonZero(regs_);
    return registered::from_code(first_code);
  }

  constexpr registered last() const {
    CHECK(!is_empty());
    int last_code =
        8 * sizeof(regs_) - 1 - base::bits::CountLeadingZeros(regs_);
    return registered::from_code(last_code);
  }

  constexpr registered PopFirst() {
    registered reg = first();
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

template <typename registered>
class RegListBase<registered>::Iterator
    : public base::iterator<std::forward_iterator_tag, registered> {
 public:
  registered operator*() { return remaining_.first(); }
  Iterator& operator++() {
    remaining_.clear(remaining_.first());
    return *this;
  }
  bool operator==(Iterator other) { return remaining_ == other.remaining_; }
  bool operator!=(Iterator other) { return remaining_ != other.remaining_; }

 private:
  explicit Iterator(RegListBase<registered> remaining) : remaining_(remaining) {}
  friend class RegListBase;

  RegListBase<registered> remaining_;
};

template <typename registered>
class RegListBase<registered>::ReverseIterator
    : public base::iterator<std::forward_iterator_tag, registered> {
 public:
  registered operator*() { return remaining_.last(); }
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
  explicit ReverseIterator(RegListBase<registered> remaining)
      : remaining_(remaining) {}
  friend class RegListBase;

  RegListBase<registered> remaining_;
};

template <typename registered>
typename RegListBase<registered>::Iterator RegListBase<registered>::begin()
    const {
  return Iterator{*this};
}
template <typename registered>
typename RegListBase<registered>::Iterator RegListBase<registered>::end() const {
  return Iterator{RegListBase<registered>{}};
}

template <typename registered>
typename RegListBase<registered>::ReverseIterator
RegListBase<registered>::rbegin() const {
  return ReverseIterator{*this};
}
template <typename registered>
typename RegListBase<registered>::ReverseIterator RegListBase<registered>::rend()
    const {
  return ReverseIterator{RegListBase<registered>{}};
}

template <typename registered>
inline std::ostream& operator<<(std::ostream& os,
                                RegListBase<registered> reglist) {
  os << "{";
  for (bool first = true; !reglist.is_empty(); first = false) {
    registered reg = reglist.first();
    reglist.clear(reg);
    os << (first ? "" : ", ") << reg;
  }
  return os << "}";
}

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_REGLIST_BASE_H_
