// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_TRAP_FUZZER_H_
#define V8_SANDBOX_TRAP_FUZZER_H_

#include "src/common/globals.h"

#if defined(V8_ENABLE_SANDBOX_HARDWARE_SUPPORT) && V8_OS_LINUX && \
    V8_HOST_ARCH_X64
#define V8_SANDBOX_TRAP_FUZZER_AVAILABLE 1
#endif

#ifdef V8_SANDBOX_TRAP_FUZZER_AVAILABLE
#include <signal.h>

#include <atomic>

#include "include/v8-isolate.h"
#include "src/base/utils/random-number-generator.h"

namespace v8 {
namespace internal {

// A fuzzer that traps in-sandbox reads through PKEYS to find memory to mutate.
//
// This fuzzer builds on top of the sandbox hardware support, which assigns a
// PKEY to all in-sandbox memory. In the basic mode of operation, this fuzzer
// installs a timer with a random interval and, whenever the timer goes off,
// removes access to the sandbox PKEY for the current thread. As such, the next
// access to in-sandbox memory will crash with a SEGV_PKUERR. Then, this
// fuzzer's SIGSEGV signal handler will catch the signal and can now mutate the
// memory that is being accessed before restoring PKEY access and continuing
// the program. This way, we have a very precise and efficient fuzzer as only
// memory that is actually being used is mutated.
//
// The downside is that this fuzzer does *not* produce reproducible samples.
// Instead, it essentially creates a log of entries that essentially say "at
// this point in time the value at that address was mutated in this way". From
// this log, the bug then has to be recovered manually.
//
// On top of the basic fuzzing algorithm, this fuzzer can also:
//
// 1. Decide to mutate a subsequent access instead of the initial one. This can
//    for example be useful if multiple independent reads are "close together",
//    making it otherwise less likely to trap on one of the later reads. In
//    this case, using a uniformly-distributed N and mutating that read instead
//    effectively smoothes the probabilities of mutating specific accesses. For
//    this, the fuzzer will draw a random number to use as a counter on the
//    first trapped access. It will then restore PKEY access, single-step over
//    the current instruction, then again remove PKEY access to trap again on
//    the next access, at which point the counter will be decremented. Once the
//    counter reaches zero, the then-accessed memory is mutated.
//
// 2. Perform one-time mutations. This can be useful to detect double-fetches.
//    For this, the fuzzer mutates the memory, but then single-steps over the
//    current instruction (the memory load) and afterwards restores the previous
//    value. This effectively emulates another thread flipping the in-sandbox
//    memory between two values.
//
// 3. Perform multiple mutations in a row ("chaining mutations"). This can be
//    useful to detect bugs that require multiple values to be corrupted. For
//    this, the fuzzer reuses the machinery from (1) and simply sets the counter
//    to a non-zero value after mutating memory. Then, the n-th access from that
//    point on will again be mutated.
//
class SandboxTrapFuzzer : public AllStatic {
 public:
  static void Enable();
  static void Disable();
  static bool IsEnabled();

  // Requests a mutation after the given number of memory accesses.
  //
  // This is currently only used in tests, but in the future we could expose
  // this to JavaScript (and disable the timer) in an attempt to get
  // reproducible test cases from this fuzzer.
  static void RegisterMutationRequest(int accesses_before_mutation);

 private:
  // The maximum N when deciding to mutate the N-th memory access.
  static constexpr int kMaxAccessesBeforeMutation = 10;
  // The probability of performing chained mutations.
  static constexpr double kProbabilityChainedMutation = 0.75;
  // The probability of performing one-time mutations.
  static constexpr double kProbabilityOneTimeMutation = 0.25;

  static void RemoveSandboxPkeyReadAccess(ucontext_t* context);
  static void RestoreSandboxPkeyReadAccess(ucontext_t* context);
  static void EnableSingleStepping(ucontext_t* context);
  static void DisableSingleStepping(ucontext_t* context);

  static bool SafeRead(void* addr, void* dst, size_t size);
  static bool SafeWrite(void* addr, void* src, size_t size);
  static void ForwardSignal(int signum, siginfo_t* si, void* vcontext,
                            struct sigaction* old_action);

  static void SigvtalrmHandler(int signum, siginfo_t* si, void* vcontext);
  static void SigtrapHandler(int signum, siginfo_t* si, void* vcontext);
  static void SigsegvHandler(int signum, siginfo_t* si, void* vcontext);

  class SignalSafeLogger {
   public:
    SignalSafeLogger();
    ~SignalSafeLogger();

    void Print(const char* output);
    void PrintNumber(uintptr_t value, int base);
    void PrintHex(uintptr_t value);
    void PrintHex(const void* ptr);
    void PrintDec(uintptr_t value);

   private:
    void Flush();

    static constexpr int kBufferSize = 1024;
    char buffer_[kBufferSize];
    int offset_ = 0;
  };

  static bool enabled_;

  static uint32_t sandbox_pkey_;
  static uint32_t pkey_mask_;
  static int pipe_fds_[2];

  static base::RandomNumberGenerator fuzz_rng_;
  static struct sigaction old_segv_action_;
  static struct sigaction old_trap_action_;
  static struct sigaction old_vtalrm_action_;

  static std::atomic<int> remaining_accesses_to_trap_;
  static std::atomic<bool> single_stepping_active_;
  static std::atomic<bool> have_pending_write_;
  static uint32_t* pending_write_address_;
  static uint32_t pending_write_value_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_TRAP_FUZZER_AVAILABLE

#endif  // V8_SANDBOX_TRAP_FUZZER_H_
