/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/profiling/perf/regs_parsing.h"

#include <inttypes.h>
#include <linux/perf_event.h>
#include <stdint.h>
#include <unistd.h>
#include <memory>

#include <unwindstack/Elf.h>
#include <unwindstack/MachineArm.h>
#include <unwindstack/MachineArm64.h>
#include <unwindstack/Regs.h>
#include <unwindstack/RegsArm.h>
#include <unwindstack/RegsArm64.h>
#include <unwindstack/RegsX86.h>
#include <unwindstack/RegsX86_64.h>
#include <unwindstack/UserArm.h>
#include <unwindstack/UserArm64.h>
#include <unwindstack/UserX86.h>
#include <unwindstack/UserX86_64.h>

// kernel uapi headers
#include <uapi/asm-arm/asm/perf_regs.h>
#include <uapi/asm-x86/asm/perf_regs.h>
#define perf_event_arm_regs perf_event_arm64_regs
#include <uapi/asm-arm64/asm/perf_regs.h>
#undef perf_event_arm_regs

namespace perfetto {
namespace profiling {

namespace {

constexpr size_t constexpr_max(size_t x, size_t y) {
  return x > y ? x : y;
}

template <typename T>
const char* ReadValue(T* value_out, const char* ptr) {
  memcpy(value_out, reinterpret_cast<const void*>(ptr), sizeof(T));
  return ptr + sizeof(T);
}

// Supported configurations:
// * 32 bit daemon, 32 bit userspace
// * 64 bit daemon, mixed bitness userspace
// Therefore give the kernel the mask corresponding to our build architecture.
// Register parsing handles the mixed userspace ABI cases.
// For simplicity, we ask for as many registers as we can, even if not all of
// them will be used during unwinding.
// TODO(rsavitski): cleanly detect 32 bit builds being side-loaded onto a system
// with 64 bit userspace processes.
uint64_t PerfUserRegsMask(unwindstack::ArchEnum arch) {
  switch (static_cast<uint8_t>(arch)) {  // cast to please -Wswitch-enum
    case unwindstack::ARCH_ARM64:
      return (1ULL << PERF_REG_ARM64_MAX) - 1;
    case unwindstack::ARCH_ARM:
      return ((1ULL << PERF_REG_ARM_MAX) - 1);
    // perf on x86_64 doesn't allow sampling ds/es/fs/gs registers. See
    // arch/x86/kernel/perf_regs.c in the kernel.
    case unwindstack::ARCH_X86_64:
      return (((1ULL << PERF_REG_X86_64_MAX) - 1) & ~(1ULL << PERF_REG_X86_DS) &
              ~(1ULL << PERF_REG_X86_ES) & ~(1ULL << PERF_REG_X86_FS) &
              ~(1ULL << PERF_REG_X86_GS));
    // Note: excluding these segment registers might not be necessary on x86,
    // but they won't be used anyway (so follow x64).
    case unwindstack::ARCH_X86:
      return ((1ULL << PERF_REG_X86_32_MAX) - 1) & ~(1ULL << PERF_REG_X86_DS) &
             ~(1ULL << PERF_REG_X86_ES) & ~(1ULL << PERF_REG_X86_FS) &
             ~(1ULL << PERF_REG_X86_GS);
    default:
      PERFETTO_FATAL("Unsupported architecture");
  }
}

// Adjusts the given architecture enum based on the ABI (as recorded in the perf
// sample). Note: we do not support 64 bit samples on a 32 bit daemon build, so
// this only converts from 64 bit to 32 bit architectures.
unwindstack::ArchEnum ArchForAbi(unwindstack::ArchEnum arch, uint64_t abi) {
  if (arch == unwindstack::ARCH_ARM64 && abi == PERF_SAMPLE_REGS_ABI_32) {
    return unwindstack::ARCH_ARM;
  }
  if (arch == unwindstack::ARCH_X86_64 && abi == PERF_SAMPLE_REGS_ABI_32) {
    return unwindstack::ARCH_X86;
  }
  return arch;
}

// Register values as an array, indexed using the kernel uapi perf_events.h enum
// values. Unsampled values will be left as zeroes.
struct RawRegisterData {
  static constexpr uint64_t kMaxSize =
      constexpr_max(PERF_REG_ARM64_MAX,
                    constexpr_max(PERF_REG_ARM_MAX, PERF_REG_X86_64_MAX));
  uint64_t regs[kMaxSize] = {};
};

// First converts the |RawRegisterData| array to libunwindstack's "user"
// register structs (which match the ptrace/coredump format, also available at
// <sys/user.h>), then constructs the relevant unwindstack::Regs subclass out
// of the latter.
std::unique_ptr<unwindstack::Regs> ToLibUnwindstackRegs(
    const RawRegisterData& raw_regs,
    unwindstack::ArchEnum arch) {
  if (arch == unwindstack::ARCH_ARM64) {
    static_assert(static_cast<int>(unwindstack::ARM64_REG_R0) ==
                          static_cast<int>(PERF_REG_ARM64_X0) &&
                      static_cast<int>(unwindstack::ARM64_REG_R0) == 0,
                  "register layout mismatch");
    static_assert(static_cast<int>(unwindstack::ARM64_REG_R30) ==
                      static_cast<int>(PERF_REG_ARM64_LR),
                  "register layout mismatch");
    // Both the perf_event register order and the "user" format are derived from
    // "struct pt_regs", so we can directly memcpy the first 31 regs (up to and
    // including LR).
    unwindstack::arm64_user_regs arm64_user_regs = {};
    memcpy(&arm64_user_regs.regs[0], &raw_regs.regs[0],
           sizeof(uint64_t) * (PERF_REG_ARM64_LR + 1));
    arm64_user_regs.sp = raw_regs.regs[PERF_REG_ARM64_SP];
    arm64_user_regs.pc = raw_regs.regs[PERF_REG_ARM64_PC];
    return std::unique_ptr<unwindstack::Regs>(
        unwindstack::RegsArm64::Read(&arm64_user_regs));
  }

  if (arch == unwindstack::ARCH_ARM) {
    static_assert(static_cast<int>(unwindstack::ARM_REG_R0) ==
                          static_cast<int>(PERF_REG_ARM_R0) &&
                      static_cast<int>(unwindstack::ARM_REG_R0) == 0,
                  "register layout mismatch");
    static_assert(static_cast<int>(unwindstack::ARM_REG_LAST) ==
                      static_cast<int>(PERF_REG_ARM_MAX),
                  "register layout mismatch");
    // As with arm64, the layouts match, but we need to downcast to u32.
    unwindstack::arm_user_regs arm_user_regs = {};
    for (size_t i = 0; i < unwindstack::ARM_REG_LAST; i++) {
      arm_user_regs.regs[i] = static_cast<uint32_t>(raw_regs.regs[i]);
    }
    return std::unique_ptr<unwindstack::Regs>(
        unwindstack::RegsArm::Read(&arm_user_regs));
  }

  if (arch == unwindstack::ARCH_X86_64) {
    // We've sampled more registers than what libunwindstack will use. Don't
    // copy over cs/ss/flags.
    unwindstack::x86_64_user_regs x86_64_user_regs = {};
    x86_64_user_regs.rax = raw_regs.regs[PERF_REG_X86_AX];
    x86_64_user_regs.rbx = raw_regs.regs[PERF_REG_X86_BX];
    x86_64_user_regs.rcx = raw_regs.regs[PERF_REG_X86_CX];
    x86_64_user_regs.rdx = raw_regs.regs[PERF_REG_X86_DX];
    x86_64_user_regs.r8 = raw_regs.regs[PERF_REG_X86_R8];
    x86_64_user_regs.r9 = raw_regs.regs[PERF_REG_X86_R9];
    x86_64_user_regs.r10 = raw_regs.regs[PERF_REG_X86_R10];
    x86_64_user_regs.r11 = raw_regs.regs[PERF_REG_X86_R11];
    x86_64_user_regs.r12 = raw_regs.regs[PERF_REG_X86_R12];
    x86_64_user_regs.r13 = raw_regs.regs[PERF_REG_X86_R13];
    x86_64_user_regs.r14 = raw_regs.regs[PERF_REG_X86_R14];
    x86_64_user_regs.r15 = raw_regs.regs[PERF_REG_X86_R15];
    x86_64_user_regs.rdi = raw_regs.regs[PERF_REG_X86_DI];
    x86_64_user_regs.rsi = raw_regs.regs[PERF_REG_X86_SI];
    x86_64_user_regs.rbp = raw_regs.regs[PERF_REG_X86_BP];
    x86_64_user_regs.rsp = raw_regs.regs[PERF_REG_X86_SP];
    x86_64_user_regs.rip = raw_regs.regs[PERF_REG_X86_IP];
    return std::unique_ptr<unwindstack::Regs>(
        unwindstack::RegsX86_64::Read(&x86_64_user_regs));
  }

  if (arch == unwindstack::ARCH_X86) {
    // We've sampled more registers than what libunwindstack will use. Don't
    // copy over cs/ss/flags.
    unwindstack::x86_user_regs x86_user_regs = {};
    x86_user_regs.eax = static_cast<uint32_t>(raw_regs.regs[PERF_REG_X86_AX]);
    x86_user_regs.ebx = static_cast<uint32_t>(raw_regs.regs[PERF_REG_X86_BX]);
    x86_user_regs.ecx = static_cast<uint32_t>(raw_regs.regs[PERF_REG_X86_CX]);
    x86_user_regs.edx = static_cast<uint32_t>(raw_regs.regs[PERF_REG_X86_DX]);
    x86_user_regs.ebp = static_cast<uint32_t>(raw_regs.regs[PERF_REG_X86_BP]);
    x86_user_regs.edi = static_cast<uint32_t>(raw_regs.regs[PERF_REG_X86_DI]);
    x86_user_regs.esi = static_cast<uint32_t>(raw_regs.regs[PERF_REG_X86_SI]);
    x86_user_regs.esp = static_cast<uint32_t>(raw_regs.regs[PERF_REG_X86_SP]);
    x86_user_regs.eip = static_cast<uint32_t>(raw_regs.regs[PERF_REG_X86_IP]);
    return std::unique_ptr<unwindstack::Regs>(
        unwindstack::RegsX86::Read(&x86_user_regs));
  }

  PERFETTO_FATAL("Unsupported architecture");
}

}  // namespace

uint64_t PerfUserRegsMaskForArch(unwindstack::ArchEnum arch) {
  return PerfUserRegsMask(arch);
}

// Assumes that the sampling was configured with
// |PerfUserRegsMaskForArch(unwindstack::Regs::CurrentArch())|.
std::unique_ptr<unwindstack::Regs> ReadPerfUserRegsData(const char** data) {
  unwindstack::ArchEnum requested_arch = unwindstack::Regs::CurrentArch();

  // Layout, assuming a sparse bitmask requesting r1 and r15:
  // userspace thread: [u64 abi] [u64 r1] [u64 r15]
  // kernel thread:    [u64 abi]
  const char* parse_pos = *data;
  uint64_t sampled_abi;
  parse_pos = ReadValue(&sampled_abi, parse_pos);

  // ABI_NONE means there were no registers, as we've sampled a kernel thread,
  // which doesn't have userspace registers.
  if (sampled_abi == PERF_SAMPLE_REGS_ABI_NONE) {
    *data = parse_pos;  // adjust caller's parsing position
    return nullptr;
  }

  // Unpack the densely-packed register values into |RawRegisterData|, which has
  // a value for every register (unsampled registers will be left at zero).
  RawRegisterData raw_regs{};
  uint64_t regs_mask = PerfUserRegsMaskForArch(requested_arch);
  for (size_t i = 0; regs_mask && (i < RawRegisterData::kMaxSize); i++) {
    if (regs_mask & (1ULL << i)) {
      parse_pos = ReadValue(&raw_regs.regs[i], parse_pos);
    }
  }

  // Special case: we've requested arm64 registers from a 64 bit kernel, but
  // ended up sampling a 32 bit arm userspace process. The 32 bit execution
  // state of the target process was saved by the exception entry in an
  // ISA-specific way. The userspace R0-R14 end up saved as arm64 W0-W14, but
  // the program counter (R15 on arm32) is still in PERF_REG_ARM64_PC (the 33rd
  // register). So we can take the kernel-dumped 64 bit register state, reassign
  // the PC into the R15 slot, and treat the resulting RawRegisterData as an
  // arm32 register bank. See "Fundamentals of ARMv8-A" (ARM DOC
  // 100878_0100_en), page 28.
  // x86-64 doesn't need any such fixups.
  if (requested_arch == unwindstack::ARCH_ARM64 &&
      sampled_abi == PERF_SAMPLE_REGS_ABI_32) {
    raw_regs.regs[PERF_REG_ARM_PC] = raw_regs.regs[PERF_REG_ARM64_PC];
  }

  *data = parse_pos;  // adjust caller's parsing position

  unwindstack::ArchEnum sampled_arch = ArchForAbi(requested_arch, sampled_abi);
  return ToLibUnwindstackRegs(raw_regs, sampled_arch);
}

}  // namespace profiling
}  // namespace perfetto
