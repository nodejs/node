// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/d8/hardware-watchpoints.h"

#include "src/d8/memory-access-information.h"

#ifdef V8_ENABLE_HARDWARE_WATCHPOINT_SUPPORT

#include <sys/mman.h>
#undef MAP_TYPE  // Conflicts with MAP_TYPE in Torque-generated instance-types.h
#include <linux/elf.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <atomic>
#include <vector>

#include "src/api/api-inl.h"
#include "src/base/platform/yield-processor.h"
#include "src/diagnostics/disasm.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-layout-inl.h"
#include "src/objects/js-objects-inl.h"
#include "src/sandbox/testing.h"
#include "src/utils/utils.h"

namespace v8 {

namespace {

// The hardware provides four watchpoints, set via the DR0 to DR3 registers.
static constexpr int kNumWatchpoints = 4;

struct WatchpointInfo {
  uintptr_t address = 0;
  // TODO(clemensb): Add length of the field (and pass that to DR7).
  // TODO(clemensb): Add information about how to manipulate read values.
};

struct HardwareWatchpointSupport {
  // Controls whether tracing output should be printed.
  bool tracing_enabled = false;
  // The PID of the d8 process; only used in the debugger process.
  int d8_pid = -1;

  // Initialized based on `v8_flags.random_seed`, used for random manipulation
  // of read values.
  base::RandomNumberGenerator rng{0};

  // Keep objects alive as long as they have a watchpoint. Used by the `d8`
  // process.
  std::vector<v8::Global<v8::Value>> kept_alive;

  // This part is allocated in shared space and used to communicate between the
  // two processes ("d8" and "debugger").
  struct SharedPart {
    // Written by the "d8" process to update watchpoints; read by the "debugger"
    // process which also resets `has_changed_watchpoints`.
    std::atomic<bool> has_changed_watchpoints{false};
    WatchpointInfo watchpoints[kNumWatchpoints];

    // Written by the "d8" process to signal the "debugger" process that it
    // should ignore any hit watchpoints for a while (e.g. while the d8 process
    // executes "trusted" code such as `object->Size()`).
    std::atomic<bool> ignore_watchpoints{false};

    // For coordinating initial setup between the two processes.
    std::atomic<int> initialization_step{0};
  }* shared = nullptr;
} g_support;

// Print trace output if `--trace-in-sandbox-manipulation-via-watchpoints` was
// specified.
// Note that the processes cannot really be debugged via gdb because they use
// the same kernel API that gdb would use (ptrace). Hence we need a lot of
// tracing to debug non-working watchpoints.
#define TRACE(...) \
  if (g_support.tracing_enabled) i::PrintF(__VA_ARGS__);

// RAII class to signal the debugger process to ignore watchpoint hits.
// This is not reentrant.
class IgnoreWatchpointsScope {
 public:
  V8_NODISCARD IgnoreWatchpointsScope() {
    DCHECK(
        !g_support.shared->ignore_watchpoints.load(std::memory_order_relaxed));
    g_support.shared->ignore_watchpoints.store(true, std::memory_order_relaxed);
  }
  ~IgnoreWatchpointsScope() {
    DCHECK(
        g_support.shared->ignore_watchpoints.load(std::memory_order_relaxed));
    g_support.shared->ignore_watchpoints.store(false,
                                               std::memory_order_relaxed);
  }
};

void SetNewWatchpoints() {
  WatchpointInfo* watchpoints = g_support.shared->watchpoints;
  static_assert(kNumWatchpoints == 4);
  TRACE("[debugger] Updating watchpoints to 0x%lx / 0x%lx / 0x%lx / 0x%lx\n",
        watchpoints[0].address, watchpoints[1].address, watchpoints[2].address,
        watchpoints[3].address);

  for (int i = 0; i < kNumWatchpoints; ++i) {
    // Write DR0-DR3 (watchpoint registers).
    CHECK_EQ(0, ptrace(PTRACE_POKEUSER, g_support.d8_pid,
                       offsetof(struct user, u_debugreg[i]),
                       watchpoints[i].address));
  }

  // Write DR7 (control register).
  uint64_t dr7 = 0;
  for (int i = 0; i < kNumWatchpoints; ++i) {
    if (!watchpoints[i].address) continue;  // watchpoint unused / disabled.
    dr7 |= 0x1 << (2 * i) |                 // Li=1 (activate watchpoint in DRi)
           (0b11 << (16 + 4 * i)) |         // RWi=r/w (0x11)
           (0b11 << (18 + 4 * i));          // LENi=4 (0x11) (cover 4 bytes)
  }
  CHECK_EQ(0, ptrace(PTRACE_POKEUSER, g_support.d8_pid,
                     offsetof(struct user, u_debugreg[7]), dr7));
}

void MutateRegister(reg_value_type* reg_ptr,
                    const MemoryAccessInformation& access_info) {
  // TODO(clemensb): Add API for specifying how to manipulate the value.
  // TODO(clemensb): Look into strategies used by AFL:
  // https://lcamtuf.blogspot.com/2014/08/binary-fuzzing-strategies-what-works.html.
  // TODO(clemensb): Unify with strategies used by Samuel's trap-based fuzzer
  // (https://crbug.com/361277236).
  uint64_t read_value = static_cast<uint64_t>(*reg_ptr);
  uint64_t new_value = read_value;
  int bit_width = access_info.access_width * 8;
  switch (g_support.rng.NextInt(4)) {
    case 0:
      new_value += 10;
      break;
    case 1:
      new_value -= 10;
      break;
    case 2:
      // Random bit flip within the accessed width.
      new_value ^= uint64_t{1} << g_support.rng.NextInt(bit_width);
      break;
    case 3:
      new_value = g_support.rng.NextInt64();
      break;
  }
  uint64_t mask =
      (bit_width == 64) ? ~uint64_t{0} : ((uint64_t{1} << bit_width) - 1);
  uint64_t mutated_bits = new_value & mask;

  if (access_info.extension == MemoryAccessInformation::kSignExtend) {
    uint64_t sign_bit = uint64_t{1} << (bit_width - 1);
    if ((mutated_bits & sign_bit) != 0) {
      mutated_bits |= ~mask;
    }
    new_value = mutated_bits;
  } else if (access_info.extension == MemoryAccessInformation::kZeroExtend) {
    new_value = mutated_bits;
  } else {
    new_value = (read_value & ~mask) | mutated_bits;
  }

  TRACE("[debugger] Changing value in result register from %" PRIu64 "/%" PRIx64
        " to %" PRIu64 "/%" PRIx64 ".\n",
        read_value, read_value, new_value, new_value);
  *reg_ptr = static_cast<reg_value_type>(new_value);
}

void DisassemblePreviousInstruction(struct user_regs_struct& regs,
                                    base::Vector<char> buffer) {
  // Set up the disassembler, ignore unknown / illegal bytes (we might start
  // decoding in the middle of an instruction).
  disasm::NameConverter disasm_name_converter;
  disasm::Disassembler disasm{
      disasm_name_converter,
      disasm::Disassembler::kContinueOnUnimplementedOpcode};

  // Read bytes around `rip`. Include the range [-16, 15], but align to whole
  // words.
  uint8_t bytes_around_rip[40];             // Five whole words.
  size_t rip_offset = 16 + (regs.rip & 7);  // Somewhere in [16, 23].
  for (size_t offset = 0; offset < arraysize(bytes_around_rip);
       offset += i::kSystemPointerSize) {
    errno = 0;
    uint64_t data = ptrace(PTRACE_PEEKTEXT, g_support.d8_pid,
                           regs.rip - rip_offset + offset, 0);
    CHECK_EQ(0, errno);

    static_assert(sizeof(data) == i::kSystemPointerSize);
    memcpy(bytes_around_rip + offset, &data, i::kSystemPointerSize);
  }

  // Try disassembling at increasing offsets. Eventually this should synchronize
  // with the instruction stream and find the instruction ending at `rip`.
  for (size_t start_offset = 0; start_offset < rip_offset; ++start_offset) {
    disasm.clear_hit_unimplemented_opcode();
    size_t offset = start_offset;
    while (offset < rip_offset && !disasm.hit_unimplemented_opcode()) {
      offset += disasm.InstructionDecode(buffer, bytes_around_rip + offset);
    }
    if (offset > rip_offset || disasm.hit_unimplemented_opcode()) continue;
    // Found the instruction!
    // The format is of the form: "4d637607     REX.W movsxlq r14,[r14+0x7]"
    TRACE("[debugger] Executed instruction was: %s\n", buffer.data());
    return;
  }

  FATAL("Failed to disassemble instruction before 0x%" PRIx64,
        static_cast<uint64_t>(regs.rip));
}

char* SkipToInstruction(char* disas) {
  while (isxdigit(*disas)) ++disas;
  // Skip over spaces.
  while (*disas == ' ') ++disas;
  // Skip over prefixes.
  while (true) {
    if (memcmp(disas, "REX.W ", 6) == 0) {
      disas += 6;
    } else if (memcmp(disas, "lock ", 5) == 0) {
      disas += 5;
    } else {
      break;
    }
  }
  return disas;
}

MemoryAccessInformation GetMemoryAccessInformationFromPreviousInstruction(
    struct user_regs_struct& regs) {
  // Buffer for disassembled instruction.
  char buffer[64];
  DisassemblePreviousInstruction(regs, base::VectorOf(buffer));
  char* insn_pos = SkipToInstruction(buffer);
  return ParseMemoryAccessInformationFromInstruction(insn_pos, regs);
}

void MutateReadValue(struct user_regs_struct& regs,
                     const MemoryAccessInformation& access_info) {
  DCHECK_EQ(MemoryAccessInformation::kRead, access_info.kind);

  const bool is_xmm = access_info.xmm_reg_index >= 0;

  if (is_xmm) {
    // Floating point values are much less interesting to mutate, but for
    // completeness we support mutating them as well.
    // XMM registers are part of the extended state (AVX/YMM). We use the
    // newer PTRACE_GETREGSET API to support both.
    // The XSAVE area size can vary depending on the processor features.
    // 8KB is sufficient for most current processors (including Sapphire Rapids
    // with AMX). We check the required size programmatically via cpuid.
    alignas(64) uint8_t xsave_buffer[8192];
    struct iovec iov;
    iov.iov_base = xsave_buffer;
    iov.iov_len = sizeof(xsave_buffer);

    int cpu_info[4];
    // CPUID leaf 0xD, sub-leaf 0 returns the maximum size (in bytes) required
    // by the XSAVE state for all currently enabled features in the XCR0
    // register. The size is returned in the EBX register (cpu_info[1]).
    __asm__ volatile("cpuid \n\t"
                     : "=a"(cpu_info[0]), "=b"(cpu_info[1]), "=c"(cpu_info[2]),
                       "=d"(cpu_info[3])
                     : "a"(0xD), "c"(0));
    size_t required_size = static_cast<size_t>(cpu_info[1]);
    CHECK_GE(sizeof(xsave_buffer), required_size);

    CHECK_EQ(0, ptrace(PTRACE_GETREGSET, g_support.d8_pid,
                       reinterpret_cast<void*>(NT_X86_XSTATE), &iov));

    static constexpr size_t kXmmRegistersOffset =
        offsetof(struct user_fpregs_struct, xmm_space);
    size_t reg_offset = access_info.xmm_reg_index * i::kSimd128Size;
    // Note: We currently only mutate the lower 64 bits of the YMM/XMM register.
    // This is sufficient to trigger a value change that can be detected in
    // JavaScript, but does not provide full 256-bit mutation.
    // TODO(clemensb): Implement better mutation for XMM/YMM registers in the
    // future, e.g. by mutating all bits.
    uint64_t* target_val_ptr = reinterpret_cast<uint64_t*>(
        xsave_buffer + kXmmRegistersOffset + reg_offset);
    uint64_t read_value_u64 = *target_val_ptr;
    double read_value_double = base::bit_cast<double>(read_value_u64);

    // TODO(clemensb): Same TODOs as below apply here.
    // TODO(clemensb): In particular we could / should generate special values
    // like different NaNs, denormals, +/-0, +/- inf.
    uint64_t new_value;
    double new_value_double;
    switch (g_support.rng.NextInt(4)) {
      case 0:
        new_value_double = read_value_double + 10.;
        new_value = base::bit_cast<uint64_t>(new_value_double);
        break;
      case 1:
        new_value_double = read_value_double - 10.;
        new_value = base::bit_cast<uint64_t>(new_value_double);
        break;
      case 2:
        // Random bit flip.
        new_value = read_value_u64 ^ (uint64_t{1} << g_support.rng.NextInt(64));
        new_value_double = base::bit_cast<double>(new_value);
        break;
      case 3:
        new_value = g_support.rng.NextInt64();
        new_value_double = base::bit_cast<double>(new_value);
        break;
    }
    TRACE(
        "[debugger] Changing value in result register from "
        "%" PRIu64 "/%" PRIx64 "/%.16g to %" PRIu64 "/%" PRIx64 "/%.16g.\n",
        read_value_u64, read_value_u64, read_value_double, new_value, new_value,
        new_value_double);

    *target_val_ptr = new_value;

    CHECK_EQ(0, ptrace(PTRACE_SETREGSET, g_support.d8_pid,
                       reinterpret_cast<void*>(NT_X86_XSTATE), &iov));
    return;
  }

  CHECK_NOT_NULL(access_info.result_reg);
  MutateRegister(access_info.result_reg, access_info);

  CHECK_EQ(0, ptrace(PTRACE_SETREGS, g_support.d8_pid, /* unused addr = */ 0,
                     &regs));
}

void MutateFlagsAfterCmp(struct user_regs_struct& regs) {
  reg_value_type old_flags = regs.eflags;

  // Define the specific bitmasks for the EFLAGS register
  constexpr uint32_t CF = 1 << 0;
  constexpr uint32_t PF = 1 << 2;
  constexpr uint32_t ZF = 1 << 6;
  constexpr uint32_t SF = 1 << 7;
  constexpr uint32_t OF = 1 << 11;

  // The 13 possible flag combinations generated by a 'cmp' instruction.
  constexpr std::array<uint32_t, 13> kValidCmpStates = {
      // Equal; PF set (even parity) because all bits are 0.
      ZF | PF,

      // Unsigned >=, Signed >, No Overflow
      0,   // odd parity
      PF,  // even parity

      // Unsigned >, Signed <, No Overflow
      SF,       // odd parity
      SF | PF,  // even parity

      // Unsigned >, Signed <, Overflowed into Positive
      OF,       // odd parity
      OF | PF,  // even parity

      // Unsigned <, Signed >, No Overflow
      CF,       // odd parity
      CF | PF,  // even parity

      // Unsigned <, Signed <, No Overflow
      CF | SF,       // odd parity
      CF | SF | PF,  // even parity

      // Unsigned <, Signed >, Overflowed into Negative
      CF | OF | SF,      // odd parity
      CF | OF | SF | PF  // even parity
  };

  constexpr reg_value_type kClearFlagsMask = ~(CF | PF | ZF | SF | OF);
  reg_value_type new_flags =
      (old_flags & kClearFlagsMask) |
      kValidCmpStates[g_support.rng.NextInt(kValidCmpStates.size())];

  TRACE(
      "[debugger] Changing EFLAGS from CF=%d PF=%d, ZF=%d, SF=%d, OF=%d to "
      "CF=%d PF=%d, ZF=%d, SF=%d, OF=%d.\n",
      (old_flags & CF) ? 1 : 0, (old_flags & PF) ? 1 : 0,
      (old_flags & ZF) ? 1 : 0, (old_flags & SF) ? 1 : 0,
      (old_flags & OF) ? 1 : 0, (new_flags & CF) ? 1 : 0,
      (new_flags & PF) ? 1 : 0, (new_flags & ZF) ? 1 : 0,
      (new_flags & SF) ? 1 : 0, (new_flags & OF) ? 1 : 0);

  regs.eflags = new_flags;
  CHECK_EQ(0, ptrace(PTRACE_SETREGS, g_support.d8_pid, /* unused addr = */ 0,
                     &regs));
}

void MutateCmpxchgOutcome(struct user_regs_struct& regs,
                          const MemoryAccessInformation& access_info) {
  // `cmpxchg` is an atomic compare-and-swap instruction.
  // - If [mem] == rax: ZF = 1, [mem] = src.
  // - If [mem] != rax: ZF = 0, rax = [mem].

  // We can fuzz the outcome of this instruction by simulating that it had the
  // opposite result of what actually happened, or by mutating the returned
  // register on failure while keeping the failure outcome.

  constexpr uint32_t kZF = 1 << 6;
  bool success = (regs.eflags & kZF) != 0;

  // We randomly choose one of the following:
  // - Do nothing (treat as a write or an unmodified atomic operation).
  // - Simulate the opposite outcome.
  // - On failure (ZF=0), keep ZF=0 but mutate rax.
  int strategy = g_support.rng.NextInt(3);
  if (strategy == 0) {
    TRACE("[debugger] cmpxchg: No mutation (hardware reported %s).\n",
          success ? "success" : "failure");
    return;
  }

  if (success) {
    // Hardware reported success (ZF=1), so the memory was updated.
    // We simulate a failure by flipping ZF to 0 and mutating `rax`.
    // In a real failure, `rax` would have been loaded with the current memory
    // value. By mutating it, we simulate that a different value was read.
    TRACE("[debugger] cmpxchg: Simulating failure (was success).\n");
    regs.eflags &= ~kZF;
    MutateRegister(&regs.rax, access_info);
  } else {
    if (strategy == 1) {
      // Hardware reported failure (ZF=0), so `rax` was loaded with the current
      // memory value.
      // We simulate a success by flipping ZF to 1.
      // This simulates that the comparison succeeded even though the memory
      // contained a different value.
      // Note that the memory was not actually updated by the hardware, and the
      // original `rax` value (the one we compared against) is lost because the
      // hardware already overrode it with the value from memory.
      // This can lead to unexpected crashes or failures in the calling code.
      // If this ever happens we have to revisit this mutation strategy.
      TRACE("[debugger] cmpxchg: Simulating success (was failure).\n");
      regs.eflags |= kZF;
    } else {
      // Hardware reported failure (ZF=0), so `rax` was loaded with the current
      // memory value.
      // We keep ZF=0 (failure) but mutate `rax`.
      // This simulates that the comparison failed and loaded a
      // mutated/corrupted value into `rax` from memory.
      TRACE("[debugger] cmpxchg: Mutating rax on failure (keeping ZF=0).\n");
      MutateRegister(&regs.rax, access_info);
    }
  }

  CHECK_EQ(0, ptrace(PTRACE_SETREGS, g_support.d8_pid, /* unused addr = */ 0,
                     &regs));
}

// Executed in the "debugger" process: Check if a watchpoint was hit and handle
// it.
// Return true if a watchpoint was hit, false otherwise.
bool HandleWatchpoint(struct user_regs_struct& regs) {
  errno = 0;
  uint64_t dr6 = ptrace(PTRACE_PEEKUSER, g_support.d8_pid,
                        offsetof(struct user, u_debugreg[6]), 0);
  CHECK_EQ(0, errno);
  // Check if a watchpoint was hit. Return false if not.
  // Note that more than one watchpoint may be hit for instructions that access
  // more than 4 bytes of memory (e.g. `movaps`).
  if ((dr6 & 0xf) == 0) return false;

  // Reset first four bits (B0-B3) in DR6.
  CHECK_EQ(0, ptrace(PTRACE_POKEUSER, g_support.d8_pid,
                     offsetof(struct user, u_debugreg[6]), dr6 & ~0xf));

  if (g_support.shared->ignore_watchpoints.load(std::memory_order_relaxed)) {
    TRACE("[debugger] Ignoring watchpoint as ignore_watchpoints is set.\n");
    return true;
  }

  // Disassemble the instruction that read the value (the instruction which ends
  // at the current RIP).
  MemoryAccessInformation access_info =
      GetMemoryAccessInformationFromPreviousInstruction(regs);
  switch (access_info.kind) {
    case MemoryAccessInformation::kRead:
      MutateReadValue(regs, access_info);
      break;

    case MemoryAccessInformation::kWrite:
      TRACE("[debugger] Ignoring write.\n");
      break;

    case MemoryAccessInformation::kCmp:
      MutateFlagsAfterCmp(regs);
      break;

    case MemoryAccessInformation::kCmpxchg:
      MutateCmpxchgOutcome(regs, access_info);
      break;
  }

  return true;
}

// This implements the main logic of the debugger; it waits until the `d8`
// process stops, then reacts by
// - stopping itself in case `d8` exited regularly or via a crash,
// - setting a new hardware watchpoint if requested, or
// - reacting to a hit watchpoint.
// Returns what's expected to happen next.
struct HowToContinueAfterWatchpoint {
  bool stop_debugger = false;
  int signal = 0;
};
HowToContinueAfterWatchpoint WaitForD8ToStopThenReact() {
  TRACE("[debugger] waiting for d8 to stop.\n");
  int status;
  waitpid(g_support.d8_pid, &status, 0);

  // Exactly one of `WIFSIGNALED`, `WIFEXITED`, and `WIFSTOPPED` is true.

  if (WIFSIGNALED(status)) {
    // Process terminated by unhandled signal.
    TRACE("[debugger] d8 crashed/killed by signal %d (%s).\n", WTERMSIG(status),
          strsignal(WTERMSIG(status)));
    return {.stop_debugger = true};
  }

  if (WIFEXITED(status)) {
    // Process terminated normally.
    TRACE("[debugger] d8 exited with status %d.\n", WEXITSTATUS(status));
    return {.stop_debugger = true};
  }

  // Process paused.
  CHECK(WIFSTOPPED(status));
  int signal = WSTOPSIG(status);

  struct user_regs_struct regs;
  CHECK_EQ(0, ptrace(PTRACE_GETREGS, g_support.d8_pid, /* unused addr = */ 0,
                     &regs));
  TRACE("[debugger] d8 stopped at 0x%" PRIx64 " (sig %d, %s).\n",
        static_cast<uint64_t>(regs.rip), signal, strsignal(signal));

  switch (signal) {
    case SIGSTOP:
      // D8 requested an update on watchpoints -> execute them.
      CHECK(g_support.shared->has_changed_watchpoints.exchange(false));
      SetNewWatchpoints();
      return {};

    case SIGTRAP: {
      // Check if a watchpoint has been hit; if so, handle it and return.
      if (HandleWatchpoint(regs)) return {};

      // Check if the process killed itself via 'int3'.
      char buffer[64];
      DisassemblePreviousInstruction(regs, base::VectorOf(buffer));
      char* insn_pos = SkipToInstruction(buffer);
      if (memcmp(insn_pos, "int3", 4) == 0) {
        TRACE("[debugger] d8 executed an int3 instruction\n");
        return {.signal = signal};
      }
    }
      [[fallthrough]];

    case SIGSEGV:
    case SIGABRT:
    case SIGILL:
      TRACE("[debugger] d8 crashed (%s)\n", strsignal(signal));
      return {.signal = signal};

    // All other signals are just forwarded to d8 unchanged.
    default:
      return {.signal = signal};
  }
}

}  // namespace

void SetupForHardwareWatchpoints(bool enable_tracing) {
  static std::atomic<bool> already_setup = false;
  CHECK(already_setup.exchange(true, std::memory_order_relaxed) == false);

  g_support.tracing_enabled = enable_tracing;

  // Set up shared memory for communication between the two processes.
  void* shared_mem =
      mmap(nullptr, sizeof(HardwareWatchpointSupport), PROT_READ | PROT_WRITE,
           MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (shared_mem == MAP_FAILED) {
    FATAL("mmap failed: %s", strerror(errno));
  }
  g_support.shared = new (shared_mem) HardwareWatchpointSupport::SharedPart();

  // Initialize the global RNG based on `--random-seed` or a randomly chosen
  // seed.
  g_support.rng.SetSeed(i::v8_flags.random_seed
                            ? i::v8_flags.random_seed
                            : base::RandomNumberGenerator{}.initial_seed());

  // Now fork the process; the parent will be the regular d8 process, the child
  // is the "debugger" which attaches to the parent via ptrace.
  pid_t d8_pid = getpid();
  pid_t debugger_pid = fork();
  if (debugger_pid == -1) {
    FATAL("fork failed: %s", strerror(errno));
  }

  if (debugger_pid != 0) {
    TRACE("[d8] I am the parent (d8) process!\n");

    // Allow the child (debugger) to ptrace me.
    if (prctl(PR_SET_PTRACER, debugger_pid, 0, 0, 0) == -1) {
      FATAL("PR_SET_PTRACER failed: %s", strerror(errno));
    }

    TRACE("[d8] signalling to debugger that they have permission to ptrace.\n");
    g_support.shared->initialization_step.store(1);

    TRACE("[d8] waiting for debugger to attach.\n");
    while (g_support.shared->initialization_step.load() != 2) {
      YIELD_PROCESSOR;
    }
    TRACE("[d8] debugger is attached; continuing d8 execution.\n");
    // Continue standard d8 execution.
    return;
  }

  TRACE("[debugger] I am the child (debugger) process!\n");
  g_support.d8_pid = d8_pid;

  // Dump the RNG seed for reproduction.
  TRACE("[debugger] RNG seed: %" PRIi64 "\n", g_support.rng.initial_seed());

  TRACE("[debugger] waiting for d8 process to signal readyness for attach.\n");
  while (g_support.shared->initialization_step.load() != 1) {
    YIELD_PROCESSOR;
  }

  TRACE("[debugger] attaching to d8 process.\n");
  if (ptrace(PTRACE_ATTACH, d8_pid, 0, 0) == -1) {
    FATAL("PTRACE_ATTACH failed: %s", strerror(errno));
  }

  TRACE("[debugger] waiting for d8 process to stop.\n");

  int status;
  waitpid(d8_pid, &status, 0);

  TRACE("[debugger] d8 stopped with status %d.\n", status);
  CHECK(WIFSTOPPED(status));

  // Kill the tracee (d8) if the tracer (debugger) is killed.
  TRACE("[debugger] setting ptrace options.\n");
  if (ptrace(PTRACE_SETOPTIONS, d8_pid, 0, PTRACE_O_EXITKILL) == -1) {
    FATAL("PTRACE_SETOPTIONS failed: %s", strerror(errno));
  }
  // Signal to d8 that it can start executing the actual JS code.
  g_support.shared->initialization_step.store(2);

  // Continuing d8 is the initial action.
  HowToContinueAfterWatchpoint how_to_continue = {};
  while (!how_to_continue.stop_debugger) {
    if (how_to_continue.signal) {
      TRACE("[debugger] continuing d8 with signal %d (%s)\n",
            how_to_continue.signal, strsignal(how_to_continue.signal));
    } else {
      TRACE("[debugger] continuing d8\n");
    }

    if (ptrace(PTRACE_CONT, d8_pid, nullptr, how_to_continue.signal) == -1) {
      FATAL("PTRACE_CONT failed: %s", strerror(errno));
    }

    how_to_continue = WaitForD8ToStopThenReact();
  }

  base::OS::ExitProcess(0);
  UNREACHABLE();
}

// Called from d8 on exit to reset all watchpoints.
void ResetAllHardwareWatchpoints() {
  WatchpointInfo* watchpoints = g_support.shared->watchpoints;
  bool has_hardware_watchpoint = false;
  for (int i = 0; i < kNumWatchpoints; ++i) {
    has_hardware_watchpoint |= watchpoints[i].address != 0;
  }
  if (!has_hardware_watchpoint) {
    TRACE("[d8] no hardware watchpoints to reset.\n");
    return;
  }

  TRACE("[d8] resetting all hardware watchpoints.\n");
  for (int i = 0; i < kNumWatchpoints; ++i) watchpoints[i].address = 0;
  g_support.kept_alive.clear();
  CHECK(!g_support.shared->has_changed_watchpoints.exchange(true));
  // Signal the debugger process; this pauses execution until that process
  // continues us.
  raise(SIGSTOP);
  TRACE("[d8] continuing execution.\n");
}

namespace {
void ThrowTypeError(v8::Isolate* isolate, std::string_view message) {
  isolate->ThrowException(v8::Exception::TypeError(
      v8::String::NewFromUtf8(isolate, message.data(), NewStringType::kNormal,
                              static_cast<int>(message.size()))
          .ToLocalChecked()));
}
}  // namespace

void SetHardwareWatchpointCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  Isolate* isolate = info.GetIsolate();
  Local<v8::Context> context = isolate->GetCurrentContext();

  if (info.Length() < 2) {
    ThrowTypeError(isolate, "Pass two arguments: object and offset");
    return;
  }

  i::DirectHandle<i::HeapObject> object;
  if (!i::TryCast(Utils::OpenDirectHandle(*info[0]), &object)) {
    ThrowTypeError(isolate, "First argument must be an object");
    return;
  }

  int32_t offset;
  if (!info[1]->IsInt32() || !info[1]->Int32Value(context).To(&offset)) {
    v8::String::Utf8Value field_name(isolate, info[1]);
    if (!*field_name) {
      ThrowTypeError(isolate, "Second argument must be an integer or a string");
      return;
    }

    i::InstanceType instance_type = object->map()->instance_type();
    if (std::optional<int> offset_from_name = i::SandboxTesting::GetFieldOffset(
            isolate, instance_type, *field_name)) {
      offset = offset_from_name.value();
    } else {
      DCHECK(isolate->HasPendingException());
      return;
    }
  }

  int object_size;
  {
    IgnoreWatchpointsScope ignore_watchpoints;
    object_size = object->Size();
  }
  DCHECK_EQ(0, object_size % i::kTaggedSize);
  if (offset < 0 || offset >= object_size) {
    std::ostringstream error;
    error << "Second argument (offset=" << offset << ") is "
          << "out of bounds of the given object of size " << object_size;
    ThrowTypeError(isolate, error.view());
    return;
  }
  if ((offset % i::kTaggedSize) != 0) {
    std::ostringstream error;
    error << "Second argument (offset=" << offset << ") is "
          << "not tagged-size-aligned";
    ThrowTypeError(isolate, error.view());
    return;
  }

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  int num_gcs = 0;
  while (i::HeapLayout::InYoungGeneration(*object)) {
    CHECK_LE(++num_gcs, 2);  // Should be promoted in <= 2 GCs.
    TRACE("[d8] Running minor GC to promote object to old space\n");
    i_isolate->heap()->PreciseCollectAllGarbage(
        i::GCFlag::kForced, i::GarbageCollectionReason::kRuntime);
  }

  i::Address watch_addr = object->address() + offset;

  int watchpoint_idx = 0;
  while (uintptr_t address =
             g_support.shared->watchpoints[watchpoint_idx].address) {
    if (address == watch_addr) {
      TRACE("[d8] Ignoring duplicate watchpoint for addr 0x%lx\n", watch_addr);
      return;
    }
    if (++watchpoint_idx == kNumWatchpoints) {
      FATAL("Maximum number of watchpoints (%d) exceeded", kNumWatchpoints);
    }
  }
  g_support.shared->watchpoints[watchpoint_idx].address = watch_addr;

  // Keep the object alive, using the original `info[0]` Local handle.
  g_support.kept_alive.emplace_back(isolate, info[0]);

  uint32_t current_content = *reinterpret_cast<uint32_t*>(watch_addr);
  TRACE(
      "[d8] sending watch request (#%d/%d) for addr 0x%lx (current content: "
      "%u/0x%x)\n",
      watchpoint_idx, kNumWatchpoints, watch_addr, current_content,
      current_content);
  CHECK(!g_support.shared->has_changed_watchpoints.exchange(true));
  // Signal the debugger process; this pauses execution until that process
  // continues us.
  raise(SIGSTOP);
  TRACE("[d8] continuing execution.\n");
}

}  // namespace v8

#endif  // V8_ENABLE_HARDWARE_WATCHPOINT_SUPPORT
