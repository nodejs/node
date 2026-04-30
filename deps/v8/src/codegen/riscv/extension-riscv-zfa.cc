// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/riscv/extension-riscv-zfa.h"

#include <iostream>
namespace v8 {
namespace internal {

// =============================================================================
// Single-Precision Zfa Instructions
// =============================================================================

void AssemblerRISCVZfa::fli_s(FPURegister rd, uint8_t imm5) {
  // fli.s: funct7=0b1111000, funct3=0, rs2=1, rs1=imm5
  DCHECK(is_uint5(imm5));
  GenInstrALUFP_rr(0b1111000, 0b000, rd, Register::from_code(imm5),
                   Register::from_code(1));
}

void AssemblerRISCVZfa::fminm_s(FPURegister rd, FPURegister rs1,
                                FPURegister rs2) {
  // fminm.s: funct7=0b0010100, funct3=2
  GenInstrALUFP_rr(0b0010100, 0b010, rd, rs1, rs2);
}

void AssemblerRISCVZfa::fmaxm_s(FPURegister rd, FPURegister rs1,
                                FPURegister rs2) {
  // fmaxm.s: funct7=0b0010100, funct3=3
  GenInstrALUFP_rr(0b0010100, 0b011, rd, rs1, rs2);
}

void AssemblerRISCVZfa::fround_s(FPURegister rd, FPURegister rs1,
                                 FPURoundingMode frm) {
  // fround.s: funct7=0b0100000, funct3=rm, rs2=4
  GenInstrALUFP_rr(0b0100000, frm, rd, rs1, Register::from_code(4));
}

void AssemblerRISCVZfa::froundnx_s(FPURegister rd, FPURegister rs1,
                                   FPURoundingMode frm) {
  // froundnx.s: funct7=0b0100000, funct3=rm, rs2=5
  GenInstrALUFP_rr(0b0100000, frm, rd, rs1, Register::from_code(5));
}

void AssemblerRISCVZfa::fleq_s(Register rd, FPURegister rs1, FPURegister rs2) {
  // fleq.s: funct7=0b1010000, funct3=4
  GenInstrALUFP_rr(0b1010000, 0b100, rd, rs1, rs2);
}

void AssemblerRISCVZfa::fltq_s(Register rd, FPURegister rs1, FPURegister rs2) {
  // fltq.s: funct7=0b1010000, funct3=5
  GenInstrALUFP_rr(0b1010000, 0b101, rd, rs1, rs2);
}

// =============================================================================
// Double-Precision Zfa Instructions
// =============================================================================

void AssemblerRISCVZfa::fli_d(FPURegister rd, uint8_t imm5) {
  // fli.d: funct7=0b1111001, funct3=0, rs2=1, rs1=imm5
  DCHECK(is_uint5(imm5));
  GenInstrALUFP_rr(0b1111001, 0b000, rd, Register::from_code(imm5),
                   Register::from_code(1));
}

void AssemblerRISCVZfa::fminm_d(FPURegister rd, FPURegister rs1,
                                FPURegister rs2) {
  // fminm.d: funct7=0b0010101, funct3=2
  GenInstrALUFP_rr(0b0010101, 0b010, rd, rs1, rs2);
}

void AssemblerRISCVZfa::fmaxm_d(FPURegister rd, FPURegister rs1,
                                FPURegister rs2) {
  // fmaxm.d: funct7=0b0010101, funct3=3
  GenInstrALUFP_rr(0b0010101, 0b011, rd, rs1, rs2);
}

void AssemblerRISCVZfa::fround_d(FPURegister rd, FPURegister rs1,
                                 FPURoundingMode frm) {
  // fround.d: funct7=0b0100001, funct3=rm, rs2=4
  GenInstrALUFP_rr(0b0100001, frm, rd, rs1, Register::from_code(4));
}

void AssemblerRISCVZfa::froundnx_d(FPURegister rd, FPURegister rs1,
                                   FPURoundingMode frm) {
  // froundnx.d: funct7=0b0100001, funct3=rm, rs2=5
  GenInstrALUFP_rr(0b0100001, frm, rd, rs1, Register::from_code(5));
}

void AssemblerRISCVZfa::fcvtmod_w_d(Register rd, FPURegister rs1) {
  // fcvtmod.w.d: funct7=0b1100001, funct3=rm, rs2=8
  GenInstrALUFP_rr(0b1100001, RTZ, rd, rs1, Register::from_code(8));
}

void AssemblerRISCVZfa::fleq_d(Register rd, FPURegister rs1, FPURegister rs2) {
  // fleq.d: funct7=0b1010001, funct3=4
  GenInstrALUFP_rr(0b1010001, 0b100, rd, rs1, rs2);
}

void AssemblerRISCVZfa::fltq_d(Register rd, FPURegister rs1, FPURegister rs2) {
  // fltq.d: funct7=0b1010001, funct3=5
  GenInstrALUFP_rr(0b1010001, 0b101, rd, rs1, rs2);
}

// =============================================================================
// FLI (Floating-Point Load Immediate) Constants
// =============================================================================
// The FLI instruction loads one of 32 predefined floating-point constants.
// The constant is selected by a 5-bit immediate (imm5) in the rs1 field.
//
// imm5 encoding:
//   0:  -1.0
//   1:  minimum positive normal (FLT_MIN / DBL_MIN)
//   2:  1.0 × 2⁻¹⁶
//   3:  1.0 × 2⁻¹⁵
//   ...
//   16: 1.0
//   ...
//   30: +∞
//   31: Canonical NaN

// FLI.S immediate values lookup table (entries 2-29).
// Each entry is a 32-bit IEEE 754 single-precision representation.
// Index 0 corresponds to imm5=2, index 27 corresponds to imm5=29.
// The table is sorted for binary search.
static constexpr uint32_t kLoadFP32ImmArr[] = {
    0x37800000,  // imm5=2:  1.0×2⁻¹⁶ = 1.52588e-05
    0x38000000,  // imm5=3:  1.0×2⁻¹⁵ = 3.05176e-05
    0x3B800000,  // imm5=4:  1.0×2⁻⁸
    0x3C000000,  // imm5=5:  1.0×2⁻⁷
    0x3D800000,  // imm5=6:  0.0625 (2⁻⁴)
    0x3E000000,  // imm5=7:  0.125 (2⁻³)
    0x3E800000,  // imm5=8:  0.25
    0x3EA00000,  // imm5=9:  0.3125
    0x3EC00000,  // imm5=10: 0.375
    0x3EE00000,  // imm5=11: 0.4375
    0x3F000000,  // imm5=12: 0.5
    0x3F200000,  // imm5=13: 0.625
    0x3F400000,  // imm5=14: 0.75
    0x3F600000,  // imm5=15: 0.875
    0x3F800000,  // imm5=16: 1.0
    0x3FA00000,  // imm5=17: 1.25
    0x3FC00000,  // imm5=18: 1.5
    0x3FE00000,  // imm5=19: 1.75
    0x40000000,  // imm5=20: 2.0
    0x40200000,  // imm5=21: 2.5
    0x40400000,  // imm5=22: 3.0
    0x40800000,  // imm5=23: 4.0
    0x41000000,  // imm5=24: 8.0
    0x41800000,  // imm5=25: 16.0
    0x43000000,  // imm5=26: 128.0 (2⁷)
    0x43800000,  // imm5=27: 256.0 (2⁸)
    0x47000000,  // imm5=28: 2¹⁵ = 32768.0
    0x47800000,  // imm5=29: 2¹⁶ = 65536.0
};

// Returns the single-precision floating-point value for the given FLI.S
// immediate encoding (imm5).
// Precondition: imm5 <= 31
float GetFLISValue(uint8_t imm5) {
  DCHECK(is_uint5(imm5));
  // imm5=0: -1.0 (the only negative value)
  if (imm5 == 0) {
    return -1.0f;
  }
  // imm5=1: minimum positive normal (FLT_MIN)
  if (imm5 == 1) {
    return 1.17549435e-38f;  // FLT_MIN = 2^(-126)
  }
  // imm5=30: +infinity
  if (imm5 == 30) {
    uint32_t bits = 0x7f800000;  // exp=255, mantissa=0
    return base::bit_cast<float>(bits);
  }
  // imm5=31: Canonical NaN
  if (imm5 == 31) {
    return base::bit_cast<float>(kFP32DefaultNaN);
  }
  // imm5=2..29: use lookup table
  return base::bit_cast<float>(kLoadFP32ImmArr[imm5 - 2]);
}

// Returns the double-precision floating-point value for the given FLI.D
// immediate encoding (imm5).
// Precondition: imm5 <= 31
//
// For imm5 != 1, the values are the same as single-precision (just represented
// in double format). For imm5=1, DBL_MIN differs from FLT_MIN.
double GetFLIDValue(uint8_t imm5) {
  DCHECK(is_uint5(imm5));
  if (imm5 == 1) {
    return base::bit_cast<double>(kFP64DblMin);
  }
  // For all other imm5 values, convert the single-precision value to double.
  // The numerical value is the same, just with different representation.
  return static_cast<double>(GetFLISValue(imm5));
}

// Returns the FLI.S immediate encoding (imm5) for the given single-precision
// floating-point value, or -1 if the value cannot be encoded by FLI.S.
// This is used by the compiler to determine if a constant can be loaded
// using the FLI.S instruction.
int GetImm5ForFLIS(float value) {
  // Handle special values first

  // Check for NaN (imm5=31: Canonical NaN)
  if (std::isnan(value)) {
    // Only canonical NaN is encodable
    uint32_t bits = base::bit_cast<uint32_t>(value);
    return (bits == kFP32DefaultNaN) ? 31 : -1;
  }

  // Check for infinity (imm5=30: +∞)
  if (std::isinf(value)) {
    return (value > 0.0f) ? 30 : -1;  // Only +∞ is encodable
  }

  // Check for minimum positive normal (imm5=1)
  // FLT_MIN = 2^(-126)
  if (base::bit_cast<uint32_t>(value) == kFP32FltMin) {
    return 1;
  }

  // Get the bit representation
  uint32_t bits = base::bit_cast<uint32_t>(value);

  // Extract sign
  uint32_t sign = (bits >> 31) & 1;

  // For negative values, check the absolute value (clear sign bit)
  uint32_t abs_bits = bits & 0x7FFFFFFF;

  // FLI constants only have mantissa bits in the top 2 positions
  // Check if lower 21 bits of mantissa are zero
  uint32_t mantissa = abs_bits & 0x7FFFFF;
  if ((mantissa & 0x1FFFFF) != 0) {
    return -1;
  }

  // Binary search in the lookup table (using absolute value)
  auto it = std::lower_bound(std::begin(kLoadFP32ImmArr),
                             std::end(kLoadFP32ImmArr), abs_bits);

  if (it == std::end(kLoadFP32ImmArr) || *it != abs_bits) {
    return -1;  // Not found in table
  }

  int entry = static_cast<int>(it - std::begin(kLoadFP32ImmArr)) + 2;

  // Handle negative values: only -1.0 (entry 16 with sign bit) is valid
  if (sign) {
    return (entry == 16) ? 0 : -1;
  }

  return entry;
}

// Returns the FLI.D immediate encoding (imm5) for the given double-precision
// floating-point value, or -1 if the value cannot be encoded by FLI.D.
//
int GetImm5ForFLID(double value) {
  if (std::isnan(value)) {
    uint64_t bits = base::bit_cast<uint64_t>(value);
    return (bits == kFP64DefaultNaN) ? 31 : -1;
  }
  if (base::bit_cast<uint64_t>(value) == kFP64DblMin) {  // DBL_MIN
    return 1;
  }

  // This check is always false for all values in the kLoadFP32ImmArr constant
  // table, since all FLI-defined values are exactly representable in both float
  // and double.
  float fvalue = static_cast<float>(value);
  if (static_cast<double>(fvalue) != value) {
    return -1;
  }
  int result = GetImm5ForFLIS(fvalue);
  // imm5=1 for FLI.D is DBL_MIN, already handled above.
  // If GetImm5ForFLIS returned 1 here, the value is FLT_MIN (2^-126)
  // which is NOT encodable by FLI.D.
  if (result == 1) {
    return -1;
  }
  return result;
}

}  // namespace internal
}  // namespace v8
