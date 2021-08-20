// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/arm64/simulator-arm64.h"

#if defined(USE_SIMULATOR)

namespace v8 {
namespace internal {

// Randomly generated example key for simulating only.
const Simulator::PACKey Simulator::kPACKeyIB = {0xeebb163b474e04c8,
                                                0x5267ac6fc280fb7c, 1};

namespace {

uint64_t GetNibble(uint64_t in_data, int position) {
  return (in_data >> position) & 0xf;
}

uint64_t PACCellShuffle(uint64_t in_data) {
  static int in_positions[16] = {52, 24, 44, 0,  28, 48, 4,  40,
                                 32, 12, 56, 20, 8,  36, 16, 60};
  uint64_t out_data = 0;
  for (int i = 0; i < 16; ++i) {
    out_data |= GetNibble(in_data, in_positions[i]) << (4 * i);
  }
  return out_data;
}

uint64_t PACCellInvShuffle(uint64_t in_data) {
  static int in_positions[16] = {12, 24, 48, 36, 56, 44, 4,  16,
                                 32, 52, 28, 8,  20, 0,  40, 60};
  uint64_t out_data = 0;
  for (int i = 0; i < 16; ++i) {
    out_data |= GetNibble(in_data, in_positions[i]) << (4 * i);
  }
  return out_data;
}

uint64_t RotCell(uint64_t in_cell, int amount) {
  DCHECK((amount >= 1) && (amount <= 3));

  in_cell &= 0xf;
  uint8_t temp = in_cell << 4 | in_cell;
  return static_cast<uint64_t>((temp >> (4 - amount)) & 0xf);
}

uint64_t PACMult(uint64_t s_input) {
  uint8_t t0;
  uint8_t t1;
  uint8_t t2;
  uint8_t t3;
  uint64_t s_output = 0;

  for (int i = 0; i < 4; ++i) {
    uint8_t s12 = (s_input >> (4 * (i + 12))) & 0xf;
    uint8_t s8 = (s_input >> (4 * (i + 8))) & 0xf;
    uint8_t s4 = (s_input >> (4 * (i + 4))) & 0xf;
    uint8_t s0 = (s_input >> (4 * (i + 0))) & 0xf;

    t0 = RotCell(s8, 1) ^ RotCell(s4, 2) ^ RotCell(s0, 1);
    t1 = RotCell(s12, 1) ^ RotCell(s4, 1) ^ RotCell(s0, 2);
    t2 = RotCell(s12, 2) ^ RotCell(s8, 1) ^ RotCell(s0, 1);
    t3 = RotCell(s12, 1) ^ RotCell(s8, 2) ^ RotCell(s4, 1);

    s_output |= static_cast<uint64_t>(t3) << (4 * (i + 0));
    s_output |= static_cast<uint64_t>(t2) << (4 * (i + 4));
    s_output |= static_cast<uint64_t>(t1) << (4 * (i + 8));
    s_output |= static_cast<uint64_t>(t0) << (4 * (i + 12));
  }
  return s_output;
}

uint64_t PACSub(uint64_t t_input) {
  uint64_t t_output = 0;
  uint8_t substitutions[16] = {0xb, 0x6, 0x8, 0xf, 0xc, 0x0, 0x9, 0xe,
                               0x3, 0x7, 0x4, 0x5, 0xd, 0x2, 0x1, 0xa};
  for (int i = 0; i < 16; ++i) {
    unsigned index = ((t_input >> (4 * i)) & 0xf);
    t_output |= static_cast<uint64_t>(substitutions[index]) << (4 * i);
  }
  return t_output;
}

uint64_t PACInvSub(uint64_t t_input) {
  uint64_t t_output = 0;
  uint8_t substitutions[16] = {0x5, 0xe, 0xd, 0x8, 0xa, 0xb, 0x1, 0x9,
                               0x2, 0x6, 0xf, 0x0, 0x4, 0xc, 0x7, 0x3};
  for (int i = 0; i < 16; ++i) {
    unsigned index = ((t_input >> (4 * i)) & 0xf);
    t_output |= static_cast<uint64_t>(substitutions[index]) << (4 * i);
  }
  return t_output;
}

uint64_t TweakCellInvRot(uint64_t in_cell) {
  uint64_t out_cell = 0;
  out_cell |= (in_cell & 0x7) << 1;
  out_cell |= (in_cell & 0x1) ^ ((in_cell >> 3) & 0x1);
  return out_cell;
}

uint64_t TweakInvShuffle(uint64_t in_data) {
  uint64_t out_data = 0;
  out_data |= TweakCellInvRot(in_data >> 48) << 0;
  out_data |= ((in_data >> 52) & 0xf) << 4;
  out_data |= ((in_data >> 20) & 0xff) << 8;
  out_data |= ((in_data >> 0) & 0xff) << 16;
  out_data |= TweakCellInvRot(in_data >> 8) << 24;
  out_data |= ((in_data >> 12) & 0xf) << 28;
  out_data |= TweakCellInvRot(in_data >> 28) << 32;
  out_data |= TweakCellInvRot(in_data >> 60) << 36;
  out_data |= TweakCellInvRot(in_data >> 56) << 40;
  out_data |= TweakCellInvRot(in_data >> 16) << 44;
  out_data |= ((in_data >> 32) & 0xfff) << 48;
  out_data |= TweakCellInvRot(in_data >> 44) << 60;
  return out_data;
}

uint64_t TweakCellRot(uint64_t in_cell) {
  uint64_t out_cell = 0;
  out_cell |= ((in_cell & 0x1) ^ ((in_cell >> 1) & 0x1)) << 3;
  out_cell |= (in_cell >> 0x1) & 0x7;
  return out_cell;
}

uint64_t TweakShuffle(uint64_t in_data) {
  uint64_t out_data = 0;
  out_data |= ((in_data >> 16) & 0xff) << 0;
  out_data |= TweakCellRot(in_data >> 24) << 8;
  out_data |= ((in_data >> 28) & 0xf) << 12;
  out_data |= TweakCellRot(in_data >> 44) << 16;
  out_data |= ((in_data >> 8) & 0xff) << 20;
  out_data |= TweakCellRot(in_data >> 32) << 28;
  out_data |= ((in_data >> 48) & 0xfff) << 32;
  out_data |= TweakCellRot(in_data >> 60) << 44;
  out_data |= TweakCellRot(in_data >> 0) << 48;
  out_data |= ((in_data >> 4) & 0xf) << 52;
  out_data |= TweakCellRot(in_data >> 40) << 56;
  out_data |= TweakCellRot(in_data >> 36) << 60;
  return out_data;
}

}  // namespace

// For a description of QARMA see:
// The QARMA Block Cipher Family, Roberto Avanzi, Qualcomm Product Security
// Initiative.
// The pseudocode is available in ARM DDI 0487D.b, J1-6946.
uint64_t Simulator::ComputePAC(uint64_t data, uint64_t context, PACKey key) {
  uint64_t key0 = key.high;
  uint64_t key1 = key.low;
  const uint64_t RC[5] = {0x0000000000000000, 0x13198a2e03707344,
                          0xa4093822299f31d0, 0x082efa98ec4e6c89,
                          0x452821e638d01377};
  const uint64_t Alpha = 0xc0ac29B7c97c50dd;

  uint64_t modk0 = ((key0 & 0x1) << 63) | ((key0 >> 2) << 1) |
                   ((key0 >> 63) ^ ((key0 >> 1) & 0x1));
  uint64_t running_mod = context;
  uint64_t working_val = data ^ key0;
  uint64_t round_key;
  for (int i = 0; i < 5; ++i) {
    round_key = key1 ^ running_mod;
    working_val ^= round_key;
    working_val ^= RC[i];
    if (i > 0) {
      working_val = PACCellShuffle(working_val);
      working_val = PACMult(working_val);
    }
    working_val = PACSub(working_val);
    running_mod = TweakShuffle(running_mod);
  }

  round_key = modk0 ^ running_mod;
  working_val ^= round_key;
  working_val = PACCellShuffle(working_val);
  working_val = PACMult(working_val);
  working_val = PACSub(working_val);
  working_val = PACCellShuffle(working_val);
  working_val = PACMult(working_val);
  working_val ^= key1;
  working_val = PACCellInvShuffle(working_val);
  working_val = PACInvSub(working_val);
  working_val = PACMult(working_val);
  working_val = PACCellInvShuffle(working_val);
  working_val ^= key0;
  working_val ^= running_mod;

  for (int i = 0; i < 5; ++i) {
    working_val = PACInvSub(working_val);
    if (i < 4) {
      working_val = PACMult(working_val);
      working_val = PACCellInvShuffle(working_val);
    }
    running_mod = TweakInvShuffle(running_mod);
    round_key = key1 ^ running_mod;
    working_val ^= RC[4 - i];
    working_val ^= round_key;
    working_val ^= Alpha;
  }

  return working_val ^ modk0;
}

// The TTBR is selected by bit 63 or 55 depending on TBI for pointers without
// codes, but is always 55 once a PAC code is added to a pointer. For this
// reason, it must be calculated at the call site.
uint64_t Simulator::CalculatePACMask(uint64_t ptr, PointerType type, int ttbr) {
  int bottom_pac_bit = GetBottomPACBit(ptr, ttbr);
  int top_pac_bit = GetTopPACBit(ptr, type);
  return unsigned_bitextract_64(top_pac_bit, bottom_pac_bit,
                                0xffffffffffffffff & ~kTTBRMask)
         << bottom_pac_bit;
}

uint64_t Simulator::AuthPAC(uint64_t ptr, uint64_t context, PACKey key,
                            PointerType type) {
  DCHECK((key.number == 0) || (key.number == 1));

  uint64_t pac_mask = CalculatePACMask(ptr, type, (ptr >> 55) & 1);
  uint64_t original_ptr =
      ((ptr & kTTBRMask) == 0) ? (ptr & ~pac_mask) : (ptr | pac_mask);

  uint64_t pac = ComputePAC(original_ptr, context, key);

  uint64_t error_code = UINT64_C(1) << key.number;
  if ((pac & pac_mask) == (ptr & pac_mask)) {
    return original_ptr;
  } else {
    int error_lsb = GetTopPACBit(ptr, type) - 2;
    uint64_t error_mask = UINT64_C(0x3) << error_lsb;
    if (FLAG_sim_abort_on_bad_auth) {
      FATAL("Pointer authentication failure.");
    }
    return (original_ptr & ~error_mask) | (error_code << error_lsb);
  }
}

uint64_t Simulator::AddPAC(uint64_t ptr, uint64_t context, PACKey key,
                           PointerType type) {
  int top_pac_bit = GetTopPACBit(ptr, type);

  DCHECK(HasTBI(ptr, type));
  int ttbr = (ptr >> 55) & 1;
  uint64_t pac_mask = CalculatePACMask(ptr, type, ttbr);
  uint64_t ext_ptr = (ttbr == 0) ? (ptr & ~pac_mask) : (ptr | pac_mask);

  uint64_t pac = ComputePAC(ext_ptr, context, key);

  // If the pointer isn't all zeroes or all ones in the PAC bitfield, corrupt
  // the resulting code.
  if (((ptr & (pac_mask | kTTBRMask)) != 0x0) &&
      ((~ptr & (pac_mask | kTTBRMask)) != 0x0)) {
    pac ^= UINT64_C(1) << (top_pac_bit - 1);
  }

  uint64_t ttbr_shifted = static_cast<uint64_t>(ttbr) << 55;
  return (pac & pac_mask) | ttbr_shifted | (ptr & ~pac_mask);
}

uint64_t Simulator::StripPAC(uint64_t ptr, PointerType type) {
  uint64_t pac_mask = CalculatePACMask(ptr, type, (ptr >> 55) & 1);
  return ((ptr & kTTBRMask) == 0) ? (ptr & ~pac_mask) : (ptr | pac_mask);
}

}  // namespace internal
}  // namespace v8

#endif  // USE_SIMULATOR
