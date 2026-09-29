// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/trap-fuzzer.h"

#ifdef V8_SANDBOX_TRAP_FUZZER_AVAILABLE

#include <signal.h>
#include <sys/mman.h>
#undef MAP_TYPE
#include <sys/time.h>
#include <unistd.h>

#include <atomic>

#include "src/base/bit-field.h"
#include "src/base/debug/stack_trace.h"
#include "src/base/platform/memory-protection-key.h"
#include "src/base/utils/random-number-generator.h"
#include "src/flags/flags.h"
#include "src/sandbox/hardware-support.h"
#include "src/sandbox/testing.h"

namespace v8 {
namespace internal {

namespace {

static inline void __cpuid(unsigned int* eax, unsigned int* ebx,
                           unsigned int* ecx, unsigned int* edx) {
  /* ecx is often an input as well as an output. */
  asm volatile("cpuid;"
               : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
               : "0"(*eax), "2"(*ecx));
}

#define XSTATE_PKEY_BIT (9)
#define XSTATE_PKEY 0x200
#define X86_EFLAGS_TF 0x100

int pkey_reg_xstate_offset(void) {
  unsigned int eax;
  unsigned int ebx;
  unsigned int ecx;
  unsigned int edx;
  int xstate_offset = 0;
  int xstate_size = 0;
  unsigned int XSTATE_CPUID = 0xd;
  int leaf;

  /* assume that XSTATE_PKEY is set in XCR0 */
  leaf = XSTATE_PKEY_BIT;
  {
    eax = XSTATE_CPUID;
    ecx = leaf;
    __cpuid(&eax, &ebx, &ecx, &edx);

    if (leaf == XSTATE_PKEY_BIT) {
      xstate_offset = ebx;
      xstate_size = eax;
    }
  }

  if (xstate_size == 0) {
    printf("could not find size/offset of PKEY in xsave state\n");
    return 0;
  }

  return xstate_offset;
}

}  // namespace

void SandboxTrapFuzzer::Enable() {
  if (enabled_) return;
  enabled_ = true;
  CHECK(SandboxHardwareSupport::IsActive());
  CHECK_WITH_MSG(v8_flags.single_threaded,
                 "ERROR: Trap-based sandbox fuzzer requires --single-threaded. "
                 "See crbug.com/506943333.\n");

  if (!SandboxTesting::IsEnabled()) {
    printf(
        "WARNING: Sandbox testing mode is NOT enabled. This configuration "
        "typically only makes sense for unit tests.\n");
  }

  sandbox_pkey_ = SandboxHardwareSupport::SandboxPkey();
  CHECK_NE(sandbox_pkey_, 0);
  pkey_mask_ =
      base::MemoryProtectionKey::ComputeRegisterMaskForPermissionSwitch(
          sandbox_pkey_, base::MemoryProtectionKey::Permission::kDisableAccess);

  if (v8_flags.random_seed != 0) {
    fuzz_rng_.SetSeed(v8_flags.random_seed);
  }

  // Create the pipes needed for SafeRead and SafeWrite.
  CHECK_EQ(pipe(pipe_fds_), 0);

  // Install signal handlers for heap sandbox fuzzer
  struct sigaction s;
  s.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
  s.sa_sigaction = SigvtalrmHandler;
  sigemptyset(&s.sa_mask);
  sigaction(SIGVTALRM, &s, &old_vtalrm_action_);

  s.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
  s.sa_sigaction = SigtrapHandler;
  sigemptyset(&s.sa_mask);
  sigaddset(&s.sa_mask, SIGVTALRM);
  sigaction(SIGTRAP, &s, &old_trap_action_);

  s.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
  s.sa_sigaction = SigsegvHandler;
  sigemptyset(&s.sa_mask);
  sigaddset(&s.sa_mask, SIGVTALRM);
  sigaction(SIGSEGV, &s, &old_segv_action_);

  // Warm up the backtrace. See WarmUpBacktrace(). Unclear if really
  // necessary but also can't hurt.
  base::debug::StackTrace stack_trace;
  stack_trace.ToString();

  // Install the timer. Use the virtual timer to measure against user-mode
  // CPU time.
  // The interval is roughly based on the execution time of complex samples.
  constexpr int kMaxInterval = 50000;  // 50 ms
  uint64_t interval = fuzz_rng_.NextInt(kMaxInterval);
  interval += 1;  // Must not be zero
  struct itimerval timer;
  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = interval;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = interval;
  if (setitimer(ITIMER_VIRTUAL, &timer, NULL) != 0) {
    FATAL("Could not set timer: %s", strerror(errno));
  }

  printf("Trap-based sandbox fuzzer initialized. Interval: %.2fms\n",
         static_cast<double>(interval) / 1000);
  printf(
      "NOTE: Crashes will NOT be reproducible. Instead, any crash must "
      "manually be reconstructed from the generated logs.\n");
}

void SandboxTrapFuzzer::Disable() {
  if (!enabled_) return;
  enabled_ = false;

  // Stop the timer.
  struct itimerval timer;
  timer.it_value.tv_sec = 0;
  timer.it_value.tv_usec = 0;
  timer.it_interval.tv_sec = 0;
  timer.it_interval.tv_usec = 0;
  setitimer(ITIMER_VIRTUAL, &timer, NULL);

  // Restore signal handlers.
  sigaction(SIGVTALRM, &old_vtalrm_action_, nullptr);
  sigaction(SIGTRAP, &old_trap_action_, nullptr);
  sigaction(SIGSEGV, &old_segv_action_, nullptr);

  // Restore pkey permissions.
  base::MemoryProtectionKey::SetPermissionsForKey(
      sandbox_pkey_, base::MemoryProtectionKey::Permission::kNoRestrictions);

  if (pipe_fds_[0] != -1) {
    close(pipe_fds_[0]);
    close(pipe_fds_[1]);
    pipe_fds_[0] = -1;
    pipe_fds_[1] = -1;
  }
}

bool SandboxTrapFuzzer::IsEnabled() { return enabled_; }

void SandboxTrapFuzzer::RegisterMutationRequest(int accesses_before_mutation) {
  remaining_accesses_to_trap_ = accesses_before_mutation;
  base::MemoryProtectionKey::SetPermissionsForKey(
      sandbox_pkey_, base::MemoryProtectionKey::Permission::kDisableAccess);
}

void SandboxTrapFuzzer::RemoveSandboxPkeyReadAccess(ucontext_t* context) {
  // See tools/testing/selftests/vm/protection_keys.c in linux kernel
  char* fpregs = reinterpret_cast<char*>(context->uc_mcontext.fpregs);
  int pkey_reg_offset = pkey_reg_xstate_offset();
  unsigned char* pkey_reg_ptr = (unsigned char*)(&fpregs[pkey_reg_offset]);
  *pkey_reg_ptr |= pkey_mask_;
}

void SandboxTrapFuzzer::RestoreSandboxPkeyReadAccess(ucontext_t* context) {
  // See tools/testing/selftests/vm/protection_keys.c in linux kernel
  char* fpregs = reinterpret_cast<char*>(context->uc_mcontext.fpregs);
  int pkey_reg_offset = pkey_reg_xstate_offset();
  unsigned char* pkey_reg_ptr = (unsigned char*)(&fpregs[pkey_reg_offset]);
  *pkey_reg_ptr &= ~pkey_mask_;
}

void SandboxTrapFuzzer::EnableSingleStepping(ucontext_t* context) {
  single_stepping_active_ = true;
  context->uc_mcontext.gregs[REG_EFL] |= X86_EFLAGS_TF;
}

void SandboxTrapFuzzer::DisableSingleStepping(ucontext_t* context) {
  single_stepping_active_ = false;
  context->uc_mcontext.gregs[REG_EFL] &= ~X86_EFLAGS_TF;
}

bool SandboxTrapFuzzer::SafeRead(void* addr, void* dst, size_t size) {
  DCHECK_NE(pipe_fds_[1], -1);
  return write(pipe_fds_[1], addr, size) == static_cast<ssize_t>(size) &&
         read(pipe_fds_[0], dst, size) == static_cast<ssize_t>(size);
}

bool SandboxTrapFuzzer::SafeWrite(void* addr, void* src, size_t size) {
  DCHECK_NE(pipe_fds_[1], -1);
  return write(pipe_fds_[1], src, size) == static_cast<ssize_t>(size) &&
         read(pipe_fds_[0], addr, size) == static_cast<ssize_t>(size);
}

void SandboxTrapFuzzer::ForwardSignal(int signum, siginfo_t* si, void* vcontext,
                                      struct sigaction* old_action) {
  if (old_action->sa_flags & SA_SIGINFO) {
    if (old_action->sa_sigaction != nullptr) {
      old_action->sa_sigaction(signum, si, vcontext);
    }
  } else {
    if (old_action->sa_handler == SIG_DFL) {
      signal(signum, SIG_DFL);
      raise(signum);
    } else if (old_action->sa_handler != SIG_IGN &&
               old_action->sa_handler != nullptr) {
      old_action->sa_handler(signum);
    }
  }
}

void SandboxTrapFuzzer::SigvtalrmHandler(int signum, siginfo_t* si,
                                         void* vcontext) {
  // Remove read access for the sandbox PKEY to trap on the next
  // read from data inside the sandbox.
  ucontext_t* context = reinterpret_cast<ucontext_t*>(vcontext);
  RemoveSandboxPkeyReadAccess(context);
}

void SandboxTrapFuzzer::SigtrapHandler(int signum, siginfo_t* si,
                                       void* vcontext) {
  const bool kNeedsFullAccess = true;
  base::MemoryProtectionKey::SetDefaultPermissionsForAllKeysInSignalHandler(
      kNeedsFullAccess);

  if (single_stepping_active_) {
    // We need a dedicated variable to track if single stepping was explicitly
    // enabled and cannot rely on remaining_accesses_to_trap_ (or
    // have_pending_write_). The reason is that we might be running into an
    // int3 (e.g. due to a failed CHECK) which would also raise a SIGTRAP which
    // we would then assume to be due to single-stepping and (incorrectly)
    // decide to continue the process. Alternatively we could check if the
    // current instruction is an int3, but a variable is probably simpler.
    CHECK(remaining_accesses_to_trap_ > 0 || have_pending_write_);
    ucontext_t* context = reinterpret_cast<ucontext_t*>(vcontext);

    if (remaining_accesses_to_trap_ > 0) {
      // Remove pkey access to trap the next memory access,
      RemoveSandboxPkeyReadAccess(context);
    }
    if (have_pending_write_) {
      have_pending_write_ = false;
      SafeWrite(pending_write_address_, &pending_write_value_,
                sizeof(uint32_t));
    }

    DisableSingleStepping(context);
  } else {
    // Unexpected SIGTAP. Forward to the "real" signal handler.
    ForwardSignal(signum, si, vcontext, &old_trap_action_);
  }
}

void SandboxTrapFuzzer::SigsegvHandler(int signum, siginfo_t* si,
                                       void* vcontext) {
  // Here we require full access (not just read-only access) since we'll be
  // writing to pkey-protected memory.
  const bool kNeedsFullAccess = true;
  base::MemoryProtectionKey::SetDefaultPermissionsForAllKeysInSignalHandler(
      kNeedsFullAccess);

  // Check if this is an expected segfault.
  //
  // Note that we don't just check for SEGV_PKUERR but also for a crash on
  // the expected pkey (the pkey for sandbox memory) to not accidentally
  // swallow real crashes. This is particularly important as we might cause a
  // crash on out-of-sandbox memory from sandboxed code, which would also
  // crash with SEGV_PKUERR but would be a valid bug.
  if (si->si_code != SEGV_PKUERR || si->si_pkey != sandbox_pkey_) {
    // Not one of our segfault. Forward to the "real" signal handler.
    ForwardSignal(signum, si, vcontext, &old_segv_action_);
    return;
  }

  ucontext_t* context = reinterpret_cast<ucontext_t*>(vcontext);
  uint32_t* faultaddr = reinterpret_cast<uint32_t*>(si->si_addr);
  Address pc = context->uc_mcontext.gregs[REG_RIP];

  // Always restore pkey access so that the faulting instruction will succeed
  // when we return from this handler.
  RestoreSandboxPkeyReadAccess(context);

  // Always log the faulting address and PC, mostly so we can better
  // reconstruct what happened.
  SignalSafeLogger logger;
  logger.Print("PC: ");
  logger.PrintHex(pc);
  logger.Print("\nFault address: ");
  logger.PrintHex(faultaddr);
  logger.Print("\n");

  if (remaining_accesses_to_trap_ == 0) {
    // First access in a sequence. Draw a random number to decide which access
    // to mutate. This might be zero, in which case we'll mutate this access.
    remaining_accesses_to_trap_ = fuzz_rng_.NextInt(kMaxAccessesBeforeMutation);
    logger.Print("Decided to mutate after ");
    logger.PrintDec(remaining_accesses_to_trap_);
    logger.Print(" more accesse(s)\n");
  } else {
    CHECK_GT(remaining_accesses_to_trap_, 0);
    remaining_accesses_to_trap_ -= 1;
    logger.Print("Remaining accesses: ");
    logger.PrintDec(remaining_accesses_to_trap_);
    logger.Print("\n");
  }

  if (remaining_accesses_to_trap_ > 0) {
    // Nothing more to do. We're waiting for a later access to mutate memory.
    // Execute the current instruction, then remove pkey access to trap the
    // next memory access after it. This requires single stepping for the next
    // instruction.
    EnableSingleStepping(context);
    return;
  }

  // If we get here, then this is a memory access that we've decided to
  // mutate.
  uint32_t oldval;
  if (!SafeRead(faultaddr, &oldval, sizeof(oldval))) {
    logger.Print("Failed to read memory\n");
    // Reset the state. TODO(saelo): should we add a helper function for this?
    remaining_accesses_to_trap_ = 0;
    return;
  }
  uint32_t newval;
  uint32_t choice = fuzz_rng_.NextInt(4);
  if (choice == 0) {
    // Choose a fully random number.
    newval = fuzz_rng_.NextInt();
  } else if (choice == 1) {
    // Flip a random bit in the current value.
    uint32_t bit = fuzz_rng_.NextInt(32);
    newval = oldval ^ (0x1 << bit);
  } else if (choice == 2) {
    // Increment or decrement by a random amount (in [-25, 25]).
    int delta = fuzz_rng_.NextInt(51);
    delta -= 25;

    // We left-shift the delta to make it roughly the same order of magnitude
    // as the current value. This is for example interesting for various
    // in-sandbox pointer table handles as these are effectively left-shifted
    // indices. If we would only increment them by a small number, the actual
    // index wouldn't change. Instead, we need to also left-shift the delta.
    int shift = 0;
    if (oldval != 0) {
      shift = std::countr_zero(oldval);
    }
    // If the shift amount ends up being really large, it's a bit unclear what
    // to do, so we just don't shift in that case.
    if (shift > 30) shift = 0;

    newval = oldval + (delta << shift);
  } else if (choice == 3) {
    // Negate the value (e.g. useful for sign extension bugs?).
    newval = -oldval;
  } else {
    FATAL("Invalid choice");
  }
  if (!SafeWrite(faultaddr, &newval, sizeof(newval))) {
    logger.Print("Failed to write memory\n");
    remaining_accesses_to_trap_ = 0;
    return;
  }

  logger.Print("Old value: ");
  logger.PrintHex(oldval);
  logger.Print("\nNew value: ");
  logger.PrintHex(newval);
  logger.Print("\n");
  logger.Print("Backtrace:\n");
  base::debug::StackTrace stack_trace;
  size_t count = 0;
  const void* const* addresses = stack_trace.Addresses(&count);
  for (size_t i = 0; i < count; ++i) {
    logger.Print("    ");
    logger.PrintHex(addresses[i]);
    logger.Print("\n");
  }

  if (fuzz_rng_.NextDouble() < kProbabilityOneTimeMutation) {
    // Make this a one-time mutation and restore the old value after the read.
    // This can help detect double-fetches by having only the first read see a
    // mutated value whereas the second read would get the original value.
    have_pending_write_ = true;
    pending_write_address_ = faultaddr;
    pending_write_value_ = oldval;
    logger.Print("Restoring old value after current instruction\n");
    EnableSingleStepping(context);
  }
  if (fuzz_rng_.NextDouble() < kProbabilityChainedMutation) {
    remaining_accesses_to_trap_ = fuzz_rng_.NextInt(kMaxAccessesBeforeMutation);
    // The value must be > 0 in this case since we've already handled this
    // access.
    remaining_accesses_to_trap_ += 1;
    logger.Print("Decided to mutate again after ");
    logger.PrintDec(remaining_accesses_to_trap_);
    logger.Print(" more accesse(s)\n");
    EnableSingleStepping(context);
  }

  return;
}

SandboxTrapFuzzer::SignalSafeLogger::SignalSafeLogger() {
  Print("------------------------- BEGIN -------------------------\n");
}

SandboxTrapFuzzer::SignalSafeLogger::~SignalSafeLogger() {
  Print("-------------------------- END --------------------------\n");
  Flush();
}

void SandboxTrapFuzzer::SignalSafeLogger::Print(const char* output) {
  // NOTE: This code MUST be async-signal safe.
  // NO malloc or stdio is allowed here.
  size_t len = strlen(output);
  CHECK_LT(len, kBufferSize);  // Individual messages must fit.
  if (offset_ + len >= kBufferSize) {
    Flush();
  }
  memcpy(buffer_ + offset_, output, len);
  offset_ += len;
}

void SandboxTrapFuzzer::SignalSafeLogger::PrintNumber(uintptr_t value,
                                                      int base) {
  CHECK_GE(base, 2);
  CHECK_LE(base, 16);
  char buf[64];
  char* str = buf + sizeof(buf) - 1;
  *str = '\0';
  do {
    --str;
    *str = "0123456789abcdef"[value % base];
    value /= base;
  } while (value > 0);
  Print(str);
}

void SandboxTrapFuzzer::SignalSafeLogger::PrintHex(uintptr_t value) {
  Print("0x");
  PrintNumber(value, 16);
}

void SandboxTrapFuzzer::SignalSafeLogger::PrintHex(const void* ptr) {
  PrintHex(reinterpret_cast<uintptr_t>(ptr));
}

void SandboxTrapFuzzer::SignalSafeLogger::PrintDec(uintptr_t value) {
  PrintNumber(value, 10);
}

void SandboxTrapFuzzer::SignalSafeLogger::Flush() {
  if (offset_ > 0) {
    ssize_t return_val = write(STDERR_FILENO, buffer_, offset_);
    USE(return_val);
    offset_ = 0;
  }
}

bool SandboxTrapFuzzer::enabled_ = false;

uint32_t SandboxTrapFuzzer::sandbox_pkey_ = 0;
uint32_t SandboxTrapFuzzer::pkey_mask_ = 0;
int SandboxTrapFuzzer::pipe_fds_[2] = {-1, -1};

base::RandomNumberGenerator SandboxTrapFuzzer::fuzz_rng_;
struct sigaction SandboxTrapFuzzer::old_segv_action_;
struct sigaction SandboxTrapFuzzer::old_trap_action_;
struct sigaction SandboxTrapFuzzer::old_vtalrm_action_;

std::atomic<int> SandboxTrapFuzzer::remaining_accesses_to_trap_ = 0;
std::atomic<bool> SandboxTrapFuzzer::single_stepping_active_ = false;
std::atomic<bool> SandboxTrapFuzzer::have_pending_write_ = false;
uint32_t* SandboxTrapFuzzer::pending_write_address_ = nullptr;
uint32_t SandboxTrapFuzzer::pending_write_value_ = 0;

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_TRAP_FUZZER_AVAILABLE
