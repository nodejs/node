// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LIFTOFF_REGISTER_H_
#define V8_WASM_BASELINE_LIFTOFF_REGISTER_H_

#include <iosfwd>
#include <memory>

#include "src/base/bits.h"
#include "src/wasm/baseline/liftoff-assembler-defs.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

static constexpr bool kNeedI64RegPair = kPointerSize == 4;

enum RegClass : uint8_t {
  kGpReg,
  kFpReg,
  // {kGpRegPair} equals {kNoReg} if {kNeedI64RegPair} is false.
  kGpRegPair,
  kNoReg = kGpRegPair + kNeedI64RegPair
};

enum RegPairHalf : uint8_t { kLowWord, kHighWord };

static inline constexpr bool needs_reg_pair(ValueType type) {
  return kNeedI64RegPair && type == kWasmI64;
}

// TODO(clemensh): Use a switch once we require C++14 support.
static inline constexpr RegClass reg_class_for(ValueType type) {
  return needs_reg_pair(type)  // i64 on 32 bit
             ? kGpRegPair
             : type == kWasmI32 || type == kWasmI64  // int types
                   ? kGpReg
                   : type == kWasmF32 || type == kWasmF64  // float types
                         ? kFpReg
                         : kNoReg;  // other (unsupported) types
}

// Maximum code of a gp cache register.
static constexpr int kMaxGpRegCode =
    8 * sizeof(kLiftoffAssemblerGpCacheRegs) -
    base::bits::CountLeadingZeros(kLiftoffAssemblerGpCacheRegs);
// Maximum code of an fp cache register.
static constexpr int kMaxFpRegCode =
    8 * sizeof(kLiftoffAssemblerFpCacheRegs) -
    base::bits::CountLeadingZeros(kLiftoffAssemblerFpCacheRegs);
// LiftoffRegister encodes both gp and fp in a unified index space.
// [0 .. kMaxGpRegCode] encodes gp registers,
// [kMaxGpRegCode+1 .. kMaxGpRegCode + kMaxFpRegCode] encodes fp registers.
// I64 values on 32 bit platforms are stored in two registers, both encoded in
// the same LiftoffRegister value.
static constexpr int kAfterMaxLiftoffGpRegCode = kMaxGpRegCode + 1;
static constexpr int kAfterMaxLiftoffFpRegCode =
    kAfterMaxLiftoffGpRegCode + kMaxFpRegCode + 1;
static constexpr int kAfterMaxLiftoffRegCode = kAfterMaxLiftoffFpRegCode;
static constexpr int kBitsPerLiftoffRegCode =
    32 - base::bits::CountLeadingZeros<uint32_t>(kAfterMaxLiftoffRegCode - 1);
static constexpr int kBitsPerGpRegCode =
    32 - base::bits::CountLeadingZeros<uint32_t>(kMaxGpRegCode);
static constexpr int kBitsPerGpRegPair = 1 + 2 * kBitsPerGpRegCode;

class LiftoffRegister {
  static constexpr int needed_bits =
      Max(kNeedI64RegPair ? kBitsPerGpRegPair : 0, kBitsPerLiftoffRegCode);
  using storage_t = std::conditional<
      needed_bits <= 8, uint8_t,
      std::conditional<needed_bits <= 16, uint16_t, uint32_t>::type>::type;
  static_assert(8 * sizeof(storage_t) >= needed_bits &&
                    8 * sizeof(storage_t) < 2 * needed_bits,
                "right type has been chosen");

 public:
  explicit LiftoffRegister(Register reg) : LiftoffRegister(reg.code()) {
    DCHECK_EQ(reg, gp());
  }
  explicit LiftoffRegister(DoubleRegister reg)
      : LiftoffRegister(kAfterMaxLiftoffGpRegCode + reg.code()) {
    DCHECK_EQ(reg, fp());
  }

  static LiftoffRegister from_liftoff_code(int code) {
    DCHECK_LE(0, code);
    DCHECK_GT(kAfterMaxLiftoffRegCode, code);
    DCHECK_EQ(code, static_cast<storage_t>(code));
    return LiftoffRegister(code);
  }

  static LiftoffRegister from_code(RegClass rc, int code) {
    switch (rc) {
      case kGpReg:
        return LiftoffRegister(Register::from_code(code));
      case kFpReg:
        return LiftoffRegister(DoubleRegister::from_code(code));
      default:
        UNREACHABLE();
    }
  }

  static LiftoffRegister ForPair(Register low, Register high) {
    DCHECK(kNeedI64RegPair);
    DCHECK_NE(low, high);
    storage_t combined_code = low.code() | high.code() << kBitsPerGpRegCode |
                              1 << (2 * kBitsPerGpRegCode);
    return LiftoffRegister(combined_code);
  }

  constexpr bool is_pair() const {
    return kNeedI64RegPair && (code_ & (1 << (2 * kBitsPerGpRegCode))) != 0;
  }
  constexpr bool is_gp() const { return code_ < kAfterMaxLiftoffGpRegCode; }
  constexpr bool is_fp() const {
    return code_ >= kAfterMaxLiftoffGpRegCode &&
           code_ < kAfterMaxLiftoffFpRegCode;
  }

  LiftoffRegister low() const { return LiftoffRegister(low_gp()); }

  LiftoffRegister high() const { return LiftoffRegister(high_gp()); }

  Register low_gp() const {
    DCHECK(is_pair());
    static constexpr storage_t kCodeMask = (1 << kBitsPerGpRegCode) - 1;
    return Register::from_code(code_ & kCodeMask);
  }

  Register high_gp() const {
    DCHECK(is_pair());
    static constexpr storage_t kCodeMask = (1 << kBitsPerGpRegCode) - 1;
    return Register::from_code((code_ >> kBitsPerGpRegCode) & kCodeMask);
  }

  Register gp() const {
    DCHECK(is_gp());
    return Register::from_code(code_);
  }

  DoubleRegister fp() const {
    DCHECK(is_fp());
    return DoubleRegister::from_code(code_ - kAfterMaxLiftoffGpRegCode);
  }

  uint32_t liftoff_code() const {
    DCHECK(is_gp() || is_fp());
    return code_;
  }

  RegClass reg_class() const {
    return is_pair() ? kGpRegPair : is_gp() ? kGpReg : kFpReg;
  }

  bool operator==(const LiftoffRegister other) const {
    DCHECK_EQ(is_pair(), other.is_pair());
    return code_ == other.code_;
  }
  bool operator!=(const LiftoffRegister other) const {
    DCHECK_EQ(is_pair(), other.is_pair());
    return code_ != other.code_;
  }
  bool overlaps(const LiftoffRegister other) const {
    if (is_pair()) return low().overlaps(other) || high().overlaps(other);
    if (other.is_pair()) return *this == other.low() || *this == other.high();
    return *this == other;
  }

 private:
  storage_t code_;

  explicit constexpr LiftoffRegister(storage_t code) : code_(code) {}
};
ASSERT_TRIVIALLY_COPYABLE(LiftoffRegister);

inline std::ostream& operator<<(std::ostream& os, LiftoffRegister reg) {
  if (reg.is_pair()) {
    return os << "<gp" << reg.low_gp().code() << "+" << reg.high_gp().code()
              << ">";
  } else if (reg.is_gp()) {
    return os << "gp" << reg.gp().code();
  } else {
    return os << "fp" << reg.fp().code();
  }
}

class LiftoffRegList {
 public:
  static constexpr bool use_u16 = kAfterMaxLiftoffRegCode <= 16;
  static constexpr bool use_u32 = !use_u16 && kAfterMaxLiftoffRegCode <= 32;
  using storage_t = std::conditional<
      use_u16, uint16_t,
      std::conditional<use_u32, uint32_t, uint64_t>::type>::type;

  static constexpr storage_t kGpMask = storage_t{kLiftoffAssemblerGpCacheRegs};
  static constexpr storage_t kFpMask = storage_t{kLiftoffAssemblerFpCacheRegs}
                                       << kAfterMaxLiftoffGpRegCode;

  constexpr LiftoffRegList() = default;

  Register set(Register reg) { return set(LiftoffRegister(reg)).gp(); }
  DoubleRegister set(DoubleRegister reg) {
    return set(LiftoffRegister(reg)).fp();
  }

  LiftoffRegister set(LiftoffRegister reg) {
    if (reg.is_pair()) {
      regs_ |= storage_t{1} << reg.low().liftoff_code();
      regs_ |= storage_t{1} << reg.high().liftoff_code();
    } else {
      regs_ |= storage_t{1} << reg.liftoff_code();
    }
    return reg;
  }

  LiftoffRegister clear(LiftoffRegister reg) {
    if (reg.is_pair()) {
      regs_ &= ~(storage_t{1} << reg.low().liftoff_code());
      regs_ &= ~(storage_t{1} << reg.high().liftoff_code());
    } else {
      regs_ &= ~(storage_t{1} << reg.liftoff_code());
    }
    return reg;
  }

  bool has(LiftoffRegister reg) const {
    if (reg.is_pair()) {
      DCHECK_EQ(has(reg.low()), has(reg.high()));
      reg = reg.low();
    }
    return (regs_ & (storage_t{1} << reg.liftoff_code())) != 0;
  }
  bool has(Register reg) const { return has(LiftoffRegister(reg)); }
  bool has(DoubleRegister reg) const { return has(LiftoffRegister(reg)); }

  constexpr bool is_empty() const { return regs_ == 0; }

  constexpr unsigned GetNumRegsSet() const {
    return base::bits::CountPopulation(regs_);
  }

  constexpr LiftoffRegList operator&(const LiftoffRegList other) const {
    return LiftoffRegList(regs_ & other.regs_);
  }

  constexpr LiftoffRegList operator~() const {
    return LiftoffRegList(~regs_ & (kGpMask | kFpMask));
  }

  constexpr bool operator==(const LiftoffRegList other) const {
    return regs_ == other.regs_;
  }
  constexpr bool operator!=(const LiftoffRegList other) const {
    return regs_ != other.regs_;
  }

  LiftoffRegister GetFirstRegSet() const {
    DCHECK(!is_empty());
    unsigned first_code = base::bits::CountTrailingZeros(regs_);
    return LiftoffRegister::from_liftoff_code(first_code);
  }

  LiftoffRegister GetLastRegSet() const {
    DCHECK(!is_empty());
    unsigned last_code =
        8 * sizeof(regs_) - 1 - base::bits::CountLeadingZeros(regs_);
    return LiftoffRegister::from_liftoff_code(last_code);
  }

  LiftoffRegList MaskOut(const LiftoffRegList mask) const {
    // Masking out is guaranteed to return a correct reg list, hence no checks
    // needed.
    return FromBits(regs_ & ~mask.regs_);
  }

  static LiftoffRegList FromBits(storage_t bits) {
    DCHECK_EQ(bits, bits & (kGpMask | kFpMask));
    return LiftoffRegList(bits);
  }

  template <storage_t bits>
  static constexpr LiftoffRegList FromBits() {
    static_assert(bits == (bits & (kGpMask | kFpMask)), "illegal reg list");
    return LiftoffRegList(bits);
  }

  template <typename... Regs>
  static LiftoffRegList ForRegs(Regs... regs) {
    std::array<LiftoffRegister, sizeof...(regs)> regs_arr{
        {LiftoffRegister(regs)...}};
    LiftoffRegList list;
    for (LiftoffRegister reg : regs_arr) list.set(reg);
    return list;
  }

 private:
  storage_t regs_ = 0;

  // Unchecked constructor. Only use for valid bits.
  explicit constexpr LiftoffRegList(storage_t bits) : regs_(bits) {}
};
ASSERT_TRIVIALLY_COPYABLE(LiftoffRegList);

static constexpr LiftoffRegList kGpCacheRegList =
    LiftoffRegList::FromBits<LiftoffRegList::kGpMask>();
static constexpr LiftoffRegList kFpCacheRegList =
    LiftoffRegList::FromBits<LiftoffRegList::kFpMask>();

static constexpr LiftoffRegList GetCacheRegList(RegClass rc) {
  return rc == kGpReg ? kGpCacheRegList : kFpCacheRegList;
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_BASELINE_LIFTOFF_REGISTER_H_
