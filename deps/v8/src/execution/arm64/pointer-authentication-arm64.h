// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_ARM64_POINTER_AUTHENTICATION_ARM64_H_
#define V8_EXECUTION_ARM64_POINTER_AUTHENTICATION_ARM64_H_

#include "src/execution/pointer-authentication.h"

#include "src/common/globals.h"
#include "src/execution/arm64/simulator-arm64.h"

namespace v8 {
namespace internal {

// The following functions execute on the host and therefore need a different
// path based on whether we are simulating arm64 or not.

// clang-format fails to detect this file as C++, turn it off.
// clang-format off

// Authenticate the address stored in {pc_address}. {offset_from_sp} is the
// offset between {pc_address} and the pointer used as a context for signing.
V8_INLINE Address PointerAuthentication::AuthenticatePC(
    Address* pc_address, unsigned offset_from_sp) {
  uint64_t sp = reinterpret_cast<uint64_t>(pc_address) + offset_from_sp;
  uint64_t pc = reinterpret_cast<uint64_t>(*pc_address);
#ifdef USE_SIMULATOR
  pc = Simulator::AuthPAC(pc, sp, Simulator::kPACKeyIB,
                          Simulator::kInstructionPointer);
#else
  asm volatile(
      "  mov x17, %[pc]\n"
      "  mov x16, %[stack_ptr]\n"
      "  autib1716\n"
      "  ldr xzr, [x17]\n"
      "  mov %[pc], x17\n"
      : [pc] "+r"(pc)
      : [stack_ptr] "r"(sp)
      : "x16", "x17");
#endif
  return pc;
}

// Strip Pointer Authentication Code (PAC) from {pc} and return the raw value.
V8_INLINE Address PointerAuthentication::StripPAC(Address pc) {
#ifdef USE_SIMULATOR
  return Simulator::StripPAC(pc, Simulator::kInstructionPointer);
#else
  asm volatile(
      "  mov x16, lr\n"
      "  mov lr, %[pc]\n"
      "  xpaclri\n"
      "  mov %[pc], lr\n"
      "  mov lr, x16\n"
      : [pc] "+r"(pc)
      :
      : "x16", "lr");
  return pc;
#endif
}

// Authenticate the address stored in {pc_address} and replace it with
// {new_pc}, after signing it. {offset_from_sp} is the offset between
// {pc_address} and the pointer used as a context for signing.
V8_INLINE void PointerAuthentication::ReplacePC(Address* pc_address,
                                                Address new_pc,
                                                int offset_from_sp) {
  uint64_t sp = reinterpret_cast<uint64_t>(pc_address) + offset_from_sp;
  uint64_t old_pc = reinterpret_cast<uint64_t>(*pc_address);
#ifdef USE_SIMULATOR
  uint64_t auth_old_pc = Simulator::AuthPAC(old_pc, sp, Simulator::kPACKeyIB,
                                            Simulator::kInstructionPointer);
  uint64_t raw_old_pc =
      Simulator::StripPAC(old_pc, Simulator::kInstructionPointer);
  // Verify that the old address is authenticated.
  CHECK_EQ(auth_old_pc, raw_old_pc);
  new_pc = Simulator::AddPAC(new_pc, sp, Simulator::kPACKeyIB,
                             Simulator::kInstructionPointer);
#else
  // Only store newly signed address after we have verified that the old
  // address is authenticated.
  asm volatile(
      "  mov x17, %[new_pc]\n"
      "  mov x16, %[sp]\n"
      "  pacib1716\n"
      "  mov %[new_pc], x17\n"
      "  mov x17, %[old_pc]\n"
      "  autib1716\n"
      "  ldr xzr, [x17]\n"
      : [new_pc] "+&r"(new_pc)
      : [sp] "r"(sp), [old_pc] "r"(old_pc)
      : "x16", "x17");
#endif
  *pc_address = new_pc;
}


// Sign {pc} using {sp}.
V8_INLINE Address PointerAuthentication::SignAndCheckPC(Address pc,
                                                          Address sp) {
#ifdef USE_SIMULATOR
  pc = Simulator::AddPAC(pc, sp, Simulator::kPACKeyIB,
                        Simulator::kInstructionPointer);
  CHECK(Deoptimizer::IsValidReturnAddress(PointerAuthentication::StripPAC(pc)));
  return pc;
#else
  asm volatile(
    "  mov x17, %[pc]\n"
    "  mov x16, %[sp]\n"
    "  pacib1716\n"
    "  mov %[pc], x17\n"
    : [pc] "+r"(pc)
    : [sp] "r"(sp)
    : "x16", "x17");
  CHECK(Deoptimizer::IsValidReturnAddress(PointerAuthentication::StripPAC(pc)));
  return pc;
#endif
}

// clang-format on

}  // namespace internal
}  // namespace v8
#endif  // V8_EXECUTION_ARM64_POINTER_AUTHENTICATION_ARM64_H_
