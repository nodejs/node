// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LIFTOFF_REGISTER_H_
#define V8_WASM_BASELINE_LIFTOFF_REGISTER_H_

#include <iosfwd>
#include <memory>

// Clients of this interface shouldn't depend on lots of compiler internals.
// Do not include anything from src/compiler here!
#include "src/base/bits.h"
#include "src/wasm/baseline/liftoff-assembler-defs.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

enum RegClass { kNoReg, kGpReg, kFpReg };

// TODO(clemensh): Use a switch once we require C++14 support.
static inline constexpr RegClass reg_class_for(ValueType type) {
  return type == kWasmI32 || type == kWasmI64  // int types
             ? kGpReg
             : type == kWasmF32 || type == kWasmF64  // float types
                   ? kFpReg
                   : kNoReg;  // other (unsupported) types
}

// RegForClass<rc>: Register for rc==kGpReg, DoubleRegister for rc==kFpReg, void
// for all other values of rc.
template <RegClass rc>
using RegForClass = typename std::conditional<
    rc == kGpReg, Register,
    typename std::conditional<rc == kFpReg, DoubleRegister, void>::type>::type;

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
static constexpr int kAfterMaxLiftoffGpRegCode = kMaxGpRegCode + 1;
static constexpr int kAfterMaxLiftoffFpRegCode =
    kAfterMaxLiftoffGpRegCode + kMaxFpRegCode + 1;
static constexpr int kAfterMaxLiftoffRegCode = kAfterMaxLiftoffFpRegCode;
static_assert(kAfterMaxLiftoffRegCode < 256,
              "liftoff register codes can be stored in one uint8_t");

class LiftoffRegister {
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

  constexpr bool is_gp() const { return code_ < kAfterMaxLiftoffGpRegCode; }
  constexpr bool is_fp() const {
    return code_ >= kAfterMaxLiftoffGpRegCode &&
           code_ < kAfterMaxLiftoffFpRegCode;
  }

  Register gp() const {
    DCHECK(is_gp());
    return Register::from_code(code_);
  }

  DoubleRegister fp() const {
    DCHECK(is_fp());
    return DoubleRegister::from_code(code_ - kAfterMaxLiftoffGpRegCode);
  }

  int liftoff_code() const { return code_; }

  RegClass reg_class() const {
    DCHECK(is_gp() || is_fp());
    return is_gp() ? kGpReg : kFpReg;
  }

  bool operator==(const LiftoffRegister other) const {
    return code_ == other.code_;
  }
  bool operator!=(const LiftoffRegister other) const {
    return code_ != other.code_;
  }

 private:
  uint8_t code_;

  explicit constexpr LiftoffRegister(uint8_t code) : code_(code) {}
};
static_assert(IS_TRIVIALLY_COPYABLE(LiftoffRegister),
              "LiftoffRegister can efficiently be passed by value");

inline std::ostream& operator<<(std::ostream& os, LiftoffRegister reg) {
  return reg.is_gp() ? os << "gp" << reg.gp().code()
                     : os << "fp" << reg.fp().code();
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
    regs_ |= storage_t{1} << reg.liftoff_code();
    return reg;
  }

  LiftoffRegister clear(LiftoffRegister reg) {
    regs_ &= ~(storage_t{1} << reg.liftoff_code());
    return reg;
  }

  bool has(LiftoffRegister reg) const {
    return (regs_ & (storage_t{1} << reg.liftoff_code())) != 0;
  }

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
        LiftoffRegister(regs)...};
    LiftoffRegList list;
    for (LiftoffRegister reg : regs_arr) list.set(reg);
    return list;
  }

 private:
  storage_t regs_ = 0;

  // Unchecked constructor. Only use for valid bits.
  explicit constexpr LiftoffRegList(storage_t bits) : regs_(bits) {}
};
static_assert(IS_TRIVIALLY_COPYABLE(LiftoffRegList),
              "LiftoffRegList can be passed by value");

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
