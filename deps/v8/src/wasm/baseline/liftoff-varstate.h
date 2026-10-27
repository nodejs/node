// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_BASELINE_LIFTOFF_VARSTATE_H_
#define V8_WASM_BASELINE_LIFTOFF_VARSTATE_H_

#include "src/wasm/baseline/liftoff-register.h"
#include "src/wasm/wasm-value.h"

namespace v8::internal::wasm {

class LiftoffVarState {
 public:
  enum Location : uint8_t { kStack, kRegister, kIntConst };

  LiftoffVarState(ValueKind kind, int offset)
      : loc_(kStack), kind_(kind), spill_offset_(offset) {
    DCHECK_LE(0, offset);
  }
  LiftoffVarState(ValueKind kind, LiftoffRegister r, int offset)
      : loc_(kRegister), kind_(kind), reg_(r), spill_offset_(offset) {
    DCHECK_EQ(r.reg_class(), reg_class_for(kind));
    DCHECK_LE(0, offset);
  }
  LiftoffVarState(ValueKind kind, int32_t i32_const, int offset)
      : loc_(kIntConst),
        kind_(kind),
        i32_const_(i32_const),
        spill_offset_(offset) {
    DCHECK(kind_ == kI32 || kind_ == kI64 || kind_ == kF32 || kind_ == kF64);
    DCHECK_LE(0, offset);
  }

  bool is_stack() const { return loc_ == kStack; }
  bool is_gp_reg() const { return loc_ == kRegister && reg_.is_gp(); }
  bool is_fp_reg() const { return loc_ == kRegister && reg_.is_fp(); }
  bool is_simd128_reg() const { return loc_ == kRegister && reg_.is_simd128(); }
  bool is_gp_reg_pair() const { return loc_ == kRegister && reg_.is_gp_pair(); }
  bool is_fp_reg_pair() const { return loc_ == kRegister && reg_.is_fp_pair(); }
  bool is_reg() const { return loc_ == kRegister; }
  bool is_const() const { return loc_ == kIntConst; }

  ValueKind kind() const { return kind_; }

  Location loc() const { return loc_; }

  // The constant as 32-bit value, to be sign-extended if {kind()} is 64-bit.
  int32_t i32_const() const {
    DCHECK_EQ(loc_, kIntConst);
    return i32_const_;
  }
  WasmValue constant() const {
    DCHECK_EQ(loc_, kIntConst);
    switch (kind_) {
      case kI32:
        return WasmValue(i32_const_);
      case kI64:
        return WasmValue(int64_t{i32_const_});
      case kF32:
        return WasmValue(Float32::FromBits(static_cast<uint32_t>(i32_const_)));
      case kF64:
        return WasmValue(
            Float64::FromBits(static_cast<uint64_t>(int64_t{i32_const_})));
      default:
        UNREACHABLE();
    }
  }

  int offset() const {
    V8_ASSUME(spill_offset_ >= 0);
    return spill_offset_;
  }
  void set_offset(int offset) {
    DCHECK_LE(0, spill_offset_);
    spill_offset_ = offset;
  }

  Register gp_reg() const { return reg().gp(); }
  DoubleRegister fp_reg() const { return reg().fp(); }
  LiftoffRegister reg() const {
    DCHECK_EQ(loc_, kRegister);
    return reg_;
  }
  RegClass reg_class() const { return reg().reg_class(); }

  void MakeStack() { loc_ = kStack; }

  void MakeRegister(LiftoffRegister r) {
    loc_ = kRegister;
    reg_ = r;
  }

  void MakeConstant(int32_t i32_const) {
    DCHECK(kind_ == kI32 || kind_ == kI64 || kind_ == kF32 || kind_ == kF64);
    loc_ = kIntConst;
    i32_const_ = i32_const;
  }

  // Copy src to this, except for offset, since src and this could have been
  // from different stack states.
  void Copy(LiftoffVarState src) {
    loc_ = src.loc();
    kind_ = src.kind();
    if (loc_ == kRegister) {
      reg_ = src.reg();
    } else if (loc_ == kIntConst) {
      i32_const_ = src.i32_const();
    }
  }

 private:
  Location loc_;
  // TODO(wasm): This is redundant, the decoder already knows the type of each
  // stack value. Try to collapse.
  ValueKind kind_;

  union {
    LiftoffRegister reg_;  // used if loc_ == kRegister
    int32_t i32_const_;    // used if loc_ == kIntConst
  };
  int spill_offset_;
};

std::ostream& operator<<(std::ostream& os, LiftoffVarState);

ASSERT_TRIVIALLY_COPYABLE(LiftoffVarState);
}  // namespace v8::internal::wasm

#endif  // V8_WASM_BASELINE_LIFTOFF_VARSTATE_H_
