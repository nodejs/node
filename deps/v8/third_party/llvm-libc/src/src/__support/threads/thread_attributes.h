//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Common thread attributes.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_THREAD_ATTRIBUTES_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_THREAD_ATTRIBUTES_H

#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/atomic.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/properties/architectures.h"

namespace LIBC_NAMESPACE_DECL {

using ThreadRunnerPosix = void *(void *);
using ThreadRunnerStdc = int(void *);

union ThreadRunner {
  ThreadRunnerPosix *posix_runner;
  ThreadRunnerStdc *stdc_runner;
};

union ThreadReturnValue {
  void *posix_retval;
  int stdc_retval;
  LIBC_INLINE constexpr ThreadReturnValue() : posix_retval(nullptr) {}
  LIBC_INLINE constexpr ThreadReturnValue(int r) : stdc_retval(r) {}
  LIBC_INLINE constexpr ThreadReturnValue(void *r) : posix_retval(r) {}
};

#if (defined(LIBC_TARGET_ARCH_IS_AARCH64) ||                                   \
     defined(LIBC_TARGET_ARCH_IS_X86_64) ||                                    \
     defined(LIBC_TARGET_ARCH_IS_ANY_RISCV))
constexpr unsigned int STACK_ALIGNMENT = 16;
#elif defined(LIBC_TARGET_ARCH_IS_ARM)
// See Section 6.2.1.2 Stack constraints at a public interface of AAPCS32.
constexpr unsigned int STACK_ALIGNMENT = 8;
#endif
// TODO: Provide stack alignment requirements for other architectures.

enum class DetachState : uint32_t {
  JOINABLE = 0x11,
  EXITING = 0x22,
  DETACHED = 0x33
};

enum class ThreadStyle : uint8_t { POSIX = 0x1, STDC = 0x2 };

// Detach type is useful in testing the detach operation.
enum class DetachType : int {
  // Indicates that the detach operation just set the detach state to DETACHED
  // and returned.
  SIMPLE = 1,

  // Indicates that the detach operation performed thread cleanup.
  CLEANUP = 2
};

class ThreadAtExitCallbackMgr;

// A data type to hold common thread attributes which have to be stored as
// thread state. Note that this is different from public attribute types like
// pthread_attr_t which might contain information which need not be saved as
// part of a thread's state. For example, the stack guard size.
//
// Thread attributes are typically stored on the stack. So, we align as required
// for the target architecture.
struct alignas(STACK_ALIGNMENT) ThreadAttributes {
  // We want the "detach_state" attribute to be an atomic value as it could be
  // updated by one thread while the self thread is reading it. It is a tristate
  // variable with the following state transitions:
  // 1. If a thread is created in a detached state, then user code should never
  //    call a detach or join function. Calling either of them can lead to
  //    undefined behavior.
  //    The value of |detach_state| is expected to be DetachState::DETACHED for
  //    its lifetime.
  // 2. If a thread is created in a joinable state, |detach_state| will start
  //    with the value DetachState::JOINABLE. Another thread can detach this
  //    thread before it exits. The state transitions will as follows:
  //      (a) If the detach method sees the state as JOINABLE, then it will
  //          compare exchange to a state of DETACHED. The thread will clean
  //          itself up after it finishes.
  //      (b) If the detach method does not see JOINABLE in (a), then it will
  //          conclude that the thread is EXITING and will wait until the thread
  //          exits. It will clean up the thread resources once the thread
  //          exits.
  cpp::Atomic<uint32_t> detach_state;
  void *stack;               // Pointer to the thread stack
  size_t stacksize;          // Size of the stack
  size_t guardsize;          // Guard size on stack
  uintptr_t tls;             // Address to the thread TLS memory
  uintptr_t tls_size;        // The size of area pointed to by |tls|.
  unsigned char owned_stack; // Indicates if the thread owns this stack memory
  int tid;
  ThreadStyle style;
  ThreadReturnValue retval;
  ThreadAtExitCallbackMgr *atexit_callback_mgr;
  void *platform_data;
  cpp::Atomic<ThreadAttributes *> joiner;

  LIBC_INLINE constexpr ThreadAttributes()
      : detach_state(uint32_t(DetachState::DETACHED)), stack(nullptr),
        stacksize(0), guardsize(0), tls(0), tls_size(0), owned_stack(false),
        tid(-1), style(ThreadStyle::POSIX), retval(),
        atexit_callback_mgr(nullptr), platform_data(nullptr), joiner(nullptr) {}
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_THREAD_ATTRIBUTES_H
