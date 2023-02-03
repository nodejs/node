// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_ARM64_REGLIST_ARM64_H_
#define V8_CODEGEN_ARM64_REGLIST_ARM64_H_

#include "src/codegen/arm64/utils-arm64.h"
#include "src/codegen/register-arch.h"
#include "src/codegen/reglist-base.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

using RegList = RegListBase<Register>;
using DoubleRegList = RegListBase<DoubleRegister>;
ASSERT_TRIVIALLY_COPYABLE(RegList);
ASSERT_TRIVIALLY_COPYABLE(DoubleRegList);

constexpr int kRegListSizeInBits = sizeof(RegList) * kBitsPerByte;

// -----------------------------------------------------------------------------
// Lists of registers.
class V8_EXPORT_PRIVATE CPURegList {
 public:
  template <typename... CPURegisters>
  explicit CPURegList(CPURegister reg0, CPURegisters... regs)
      : list_(((uint64_t{1} << reg0.code()) | ... |
               (regs.is_valid() ? uint64_t{1} << regs.code() : 0))),
        size_(reg0.SizeInBits()),
        type_(reg0.type()) {
    DCHECK(AreSameSizeAndType(reg0, regs...));
    DCHECK(is_valid());
  }

  CPURegList(int size, RegList list)
      : list_(list.bits()), size_(size), type_(CPURegister::kRegister) {
    DCHECK(is_valid());
  }

  CPURegList(int size, DoubleRegList list)
      : list_(list.bits()), size_(size), type_(CPURegister::kVRegister) {
    DCHECK(is_valid());
  }

  CPURegList(CPURegister::RegisterType type, int size, int first_reg,
             int last_reg)
      : size_(size), type_(type) {
    DCHECK(
        ((type == CPURegister::kRegister) && (last_reg < kNumberOfRegisters)) ||
        ((type == CPURegister::kVRegister) &&
         (last_reg < kNumberOfVRegisters)));
    DCHECK(last_reg >= first_reg);
    list_ = (1ULL << (last_reg + 1)) - 1;
    list_ &= ~((1ULL << first_reg) - 1);
    DCHECK(is_valid());
  }

  CPURegister::RegisterType type() const { return type_; }

  uint64_t bits() const { return list_; }

  inline void set_bits(uint64_t new_bits) {
    list_ = new_bits;
    DCHECK(is_valid());
  }

  // Combine another CPURegList into this one. Registers that already exist in
  // this list are left unchanged. The type and size of the registers in the
  // 'other' list must match those in this list.
  void Combine(const CPURegList& other);

  // Remove every register in the other CPURegList from this one. Registers that
  // do not exist in this list are ignored. The type of the registers in the
  // 'other' list must match those in this list.
  void Remove(const CPURegList& other);

  // Variants of Combine and Remove which take CPURegisters.
  void Combine(const CPURegister& other);
  void Remove(const CPURegister& other1, const CPURegister& other2 = NoCPUReg,
              const CPURegister& other3 = NoCPUReg,
              const CPURegister& other4 = NoCPUReg);

  // Variants of Combine and Remove which take a single register by its code;
  // the type and size of the register is inferred from this list.
  void Combine(int code);
  void Remove(int code);

  // Align the list to 16 bytes.
  void Align();

  CPURegister PopLowestIndex();
  CPURegister PopHighestIndex();

  // AAPCS64 callee-saved registers.
  static CPURegList GetCalleeSaved(int size = kXRegSizeInBits);
  static CPURegList GetCalleeSavedV(int size = kDRegSizeInBits);

  // AAPCS64 caller-saved registers. Note that this includes lr.
  // TODO(all): Determine how we handle d8-d15 being callee-saved, but the top
  // 64-bits being caller-saved.
  static CPURegList GetCallerSaved(int size = kXRegSizeInBits);
  static CPURegList GetCallerSavedV(int size = kDRegSizeInBits);

  bool IsEmpty() const { return list_ == 0; }

  bool IncludesAliasOf(const CPURegister& other1,
                       const CPURegister& other2 = NoCPUReg,
                       const CPURegister& other3 = NoCPUReg,
                       const CPURegister& other4 = NoCPUReg) const {
    uint64_t list = 0;
    if (!other1.IsNone() && (other1.type() == type_)) {
      list |= (uint64_t{1} << other1.code());
    }
    if (!other2.IsNone() && (other2.type() == type_)) {
      list |= (uint64_t{1} << other2.code());
    }
    if (!other3.IsNone() && (other3.type() == type_)) {
      list |= (uint64_t{1} << other3.code());
    }
    if (!other4.IsNone() && (other4.type() == type_)) {
      list |= (uint64_t{1} << other4.code());
    }
    return (list_ & list) != 0;
  }

  int Count() const { return CountSetBits(list_, kRegListSizeInBits); }

  int RegisterSizeInBits() const { return size_; }

  int RegisterSizeInBytes() const {
    int size_in_bits = RegisterSizeInBits();
    DCHECK_EQ(size_in_bits % kBitsPerByte, 0);
    return size_in_bits / kBitsPerByte;
  }

  int TotalSizeInBytes() const { return RegisterSizeInBytes() * Count(); }

 private:
  uint64_t list_;
  int size_;
  CPURegister::RegisterType type_;

  bool is_valid() const {
    constexpr uint64_t kValidRegisters{0x8000000ffffffff};
    constexpr uint64_t kValidVRegisters{0x0000000ffffffff};
    switch (type_) {
      case CPURegister::kRegister:
        return (list_ & kValidRegisters) == list_;
      case CPURegister::kVRegister:
        return (list_ & kValidVRegisters) == list_;
      case CPURegister::kNoRegister:
        return list_ == 0;
      default:
        UNREACHABLE();
    }
  }
};

// AAPCS64 callee-saved registers.
#define kCalleeSaved CPURegList::GetCalleeSaved()
#define kCalleeSavedV CPURegList::GetCalleeSavedV()

// AAPCS64 caller-saved registers. Note that this includes lr.
#define kCallerSaved CPURegList::GetCallerSaved()
#define kCallerSavedV CPURegList::GetCallerSavedV()

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_ARM64_REGLIST_ARM64_H_
