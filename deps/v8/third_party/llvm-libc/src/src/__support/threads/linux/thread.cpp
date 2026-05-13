//===--- Implementation of a Linux thread class -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/threads/thread.h"
#include "config/app.h"
#include "src/__support/CPP/atomic.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/stringstream.h"
#include "src/__support/OSUtil/syscall.h" // For syscall functions.
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/libc_errno.h" // For error macros
#include "src/__support/macros/config.h"
#include "src/__support/threads/linux/futex_utils.h" // For FutexWordType

#ifdef LIBC_TARGET_ARCH_IS_AARCH64
#include <arm_acle.h>
#endif

#include "hdr/errno_macros.h"
#include "hdr/fcntl_macros.h"
#include "hdr/stdint_proxy.h"
#include <linux/param.h> // For EXEC_PAGESIZE.
#include <linux/prctl.h> // For PR_SET_NAME
#include <linux/sched.h> // For CLONE_* flags.
#include <sys/mman.h>    // For PROT_* and MAP_* definitions.
#include <sys/syscall.h> // For syscall numbers.

namespace LIBC_NAMESPACE_DECL {

#ifdef SYS_mmap2
static constexpr long MMAP_SYSCALL_NUMBER = SYS_mmap2;
#elif defined(SYS_mmap)
static constexpr long MMAP_SYSCALL_NUMBER = SYS_mmap;
#else
#error "mmap or mmap2 syscalls not available."
#endif

static constexpr size_t NAME_SIZE_MAX = 16; // Includes the null terminator
static constexpr uint32_t CLEAR_TID_VALUE = 0xABCD1234;
static constexpr unsigned CLONE_SYSCALL_FLAGS =
    CLONE_VM        // Share the memory space with the parent.
    | CLONE_FS      // Share the file system with the parent.
    | CLONE_FILES   // Share the files with the parent.
    | CLONE_SIGHAND // Share the signal handlers with the parent.
    | CLONE_THREAD  // Same thread group as the parent.
    | CLONE_SYSVSEM // Share a single list of System V semaphore adjustment
                    // values
    | CLONE_PARENT_SETTID  // Set child thread ID in |ptid| of the parent.
    | CLONE_CHILD_CLEARTID // Let the kernel clear the tid address
                           // wake the joining thread.
    | CLONE_SETTLS;        // Setup the thread pointer of the new thread.

#ifdef LIBC_TARGET_ARCH_IS_AARCH64
#define CLONE_RESULT_REGISTER "x0"
#elif defined(LIBC_TARGET_ARCH_IS_ANY_RISCV)
#define CLONE_RESULT_REGISTER "t0"
#elif defined(LIBC_TARGET_ARCH_IS_X86_64)
#define CLONE_RESULT_REGISTER "rax"
#else
#error "CLONE_RESULT_REGISTER not defined for your target architecture"
#endif

static constexpr ErrorOr<size_t> add_no_overflow(size_t lhs, size_t rhs) {
  if (lhs > SIZE_MAX - rhs)
    return Error{EINVAL};
  if (rhs > SIZE_MAX - lhs)
    return Error{EINVAL};
  return lhs + rhs;
}

static constexpr ErrorOr<size_t> round_to_page(size_t v) {
  auto vp_or_err = add_no_overflow(v, EXEC_PAGESIZE - 1);
  if (!vp_or_err)
    return vp_or_err;

  return vp_or_err.value() & -EXEC_PAGESIZE;
}

LIBC_INLINE ErrorOr<void *> alloc_stack(size_t stacksize, size_t guardsize) {

  // Guard needs to be mapped with PROT_NONE
  int prot = guardsize ? PROT_NONE : PROT_READ | PROT_WRITE;
  auto size_or_err = add_no_overflow(stacksize, guardsize);
  if (!size_or_err)
    return Error{int(size_or_err.error())};
  size_t size = size_or_err.value();

  // TODO: Maybe add MAP_STACK? Currently unimplemented on linux but helps
  // future-proof.
  long mmap_result = LIBC_NAMESPACE::syscall_impl<long>(
      MMAP_SYSCALL_NUMBER,
      0, // No special address
      size, prot,
      MAP_ANONYMOUS | MAP_PRIVATE, // Process private.
      -1,                          // Not backed by any file
      0                            // No offset
  );
  if (!linux_utils::is_valid_mmap(mmap_result))
    return Error{int(-mmap_result)};

  if (guardsize) {
    // Give read/write permissions to actual stack.
    // TODO: We are assuming stack growsdown here.
    long result = LIBC_NAMESPACE::syscall_impl<long>(
        SYS_mprotect, mmap_result + guardsize, stacksize,
        PROT_READ | PROT_WRITE);

    if (result != 0)
      return Error{int(-result)};
  }
  mmap_result += guardsize;
  return reinterpret_cast<void *>(mmap_result);
}

// This must always be inlined as we may be freeing the calling threads stack in
// which case a normal return from the top the stack would cause an invalid
// memory read.
[[gnu::always_inline]] LIBC_INLINE void
free_stack(void *stack, size_t stacksize, size_t guardsize) {
  uintptr_t stackaddr = reinterpret_cast<uintptr_t>(stack);
  stackaddr -= guardsize;
  stack = reinterpret_cast<void *>(stackaddr);
  LIBC_NAMESPACE::syscall_impl<long>(SYS_munmap, stack, stacksize + guardsize);
}

struct Thread;

// We align the start args to 16-byte boundary as we adjust the allocated
// stack memory with its size. We want the adjusted address to be at a
// 16-byte boundary to satisfy the x86_64 and aarch64 ABI requirements.
// If different architecture in future requires higher alignment, then we
// can add a platform specific alignment spec.
struct alignas(STACK_ALIGNMENT) StartArgs {
  ThreadAttributes *thread_attrib;
  ThreadRunner runner;
  void *arg;
};

// This must always be inlined as we may be freeing the calling threads stack in
// which case a normal return from the top the stack would cause an invalid
// memory read.
[[gnu::always_inline]] LIBC_INLINE void
cleanup_thread_resources(ThreadAttributes *attrib) {
  // Cleanup the TLS before the stack as the TLS information is stored on
  // the stack.
  cleanup_tls(attrib->tls, attrib->tls_size);
  if (attrib->owned_stack)
    free_stack(attrib->stack, attrib->stacksize, attrib->guardsize);
}

[[gnu::always_inline]] LIBC_INLINE uintptr_t get_start_args_addr() {
// NOTE: For __builtin_frame_address to work reliably across compilers,
// architectures and various optimization levels, the TU including this file
// should be compiled with -fno-omit-frame-pointer.
#ifdef LIBC_TARGET_ARCH_IS_X86_64
  return reinterpret_cast<uintptr_t>(__builtin_frame_address(0))
         // The x86_64 call instruction pushes resume address on to the stack.
         // Next, The x86_64 SysV ABI requires that the frame pointer be pushed
         // on to the stack. So, we have to step past two 64-bit values to get
         // to the start args.
         + sizeof(uintptr_t) * 2;
#elif defined(LIBC_TARGET_ARCH_IS_AARCH64)
  // The frame pointer after cloning the new thread in the Thread::run method
  // is set to the stack pointer where start args are stored. So, we fetch
  // from there.
  return reinterpret_cast<uintptr_t>(__builtin_frame_address(1));
#elif defined(LIBC_TARGET_ARCH_IS_ANY_RISCV)
  // The current frame pointer is the previous stack pointer where the start
  // args are stored.
  return reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
#endif
}

[[gnu::noinline]] void start_thread() {
  auto *start_args = reinterpret_cast<StartArgs *>(get_start_args_addr());
  auto *attrib = start_args->thread_attrib;
  self.attrib = attrib;
  self.attrib->atexit_callback_mgr = internal::get_thread_atexit_callback_mgr();

  if (attrib->style == ThreadStyle::POSIX) {
    attrib->retval.posix_retval =
        start_args->runner.posix_runner(start_args->arg);
    thread_exit(ThreadReturnValue(attrib->retval.posix_retval),
                ThreadStyle::POSIX);
  } else {
    attrib->retval.stdc_retval =
        start_args->runner.stdc_runner(start_args->arg);
    thread_exit(ThreadReturnValue(attrib->retval.stdc_retval),
                ThreadStyle::STDC);
  }
}

int Thread::run(ThreadStyle style, ThreadRunner runner, void *arg, void *stack,
                size_t stacksize, size_t guardsize, bool detached) {
  bool owned_stack = false;
  if (stack == nullptr) {
    // TODO: Should we return EINVAL here? Should we have a generic concept of a
    //       minimum stacksize (like 16384 for pthread).
    if (stacksize == 0)
      stacksize = DEFAULT_STACKSIZE;
    // Roundup stacksize/guardsize to page size.
    // TODO: Should be also add sizeof(ThreadAttribute) and other internal
    //       meta data?
    auto round_or_err = round_to_page(guardsize);
    if (!round_or_err)
      return round_or_err.error();
    guardsize = round_or_err.value();

    round_or_err = round_to_page(stacksize);
    if (!round_or_err)
      return round_or_err.error();

    stacksize = round_or_err.value();
    auto alloc = alloc_stack(stacksize, guardsize);
    if (!alloc)
      return alloc.error();
    else
      stack = alloc.value();
    owned_stack = true;
  }

  // Validate that stack/stacksize are validly aligned.
  uintptr_t stackaddr = reinterpret_cast<uintptr_t>(stack);
  if ((stackaddr % STACK_ALIGNMENT != 0) ||
      ((stackaddr + stacksize) % STACK_ALIGNMENT != 0)) {
    if (owned_stack)
      free_stack(stack, stacksize, guardsize);
    return EINVAL;
  }

  TLSDescriptor tls;
  init_tls(tls);

  // When the new thread is spawned by the kernel, the new thread gets the
  // stack we pass to the clone syscall. However, this stack is empty and does
  // not have any local vars present in this function. Hence, one cannot
  // pass arguments to the thread start function, or use any local vars from
  // here. So, we pack them into the new stack from where the thread can sniff
  // them out.
  //
  // Likewise, the actual thread state information is also stored on the
  // stack memory.

  static constexpr size_t INTERNAL_STACK_DATA_SIZE =
      sizeof(StartArgs) + sizeof(ThreadAttributes) + sizeof(Futex);

  // This is pretty arbitrary, but at the moment we don't adjust user provided
  // stacksize (or default) to account for this data as its assumed minimal. If
  // this assert starts failing we probably should. Likewise if we can't bound
  // this we may overflow when we subtract it from the top of the stack.
  static_assert(INTERNAL_STACK_DATA_SIZE < EXEC_PAGESIZE);

  // TODO: We are assuming stack growsdown here.
  auto adjusted_stack_or_err =
      add_no_overflow(reinterpret_cast<uintptr_t>(stack), stacksize);
  if (!adjusted_stack_or_err) {
    cleanup_tls(tls.addr, tls.size);
    if (owned_stack)
      free_stack(stack, stacksize, guardsize);
    return adjusted_stack_or_err.error();
  }

  uintptr_t adjusted_stack =
      adjusted_stack_or_err.value() - INTERNAL_STACK_DATA_SIZE;
  adjusted_stack &= ~(uintptr_t(STACK_ALIGNMENT) - 1);

  auto *start_args = reinterpret_cast<StartArgs *>(adjusted_stack);

  attrib =
      reinterpret_cast<ThreadAttributes *>(adjusted_stack + sizeof(StartArgs));
  attrib->style = style;
  attrib->detach_state =
      uint32_t(detached ? DetachState::DETACHED : DetachState::JOINABLE);
  attrib->stack = stack;
  attrib->stacksize = stacksize;
  attrib->guardsize = guardsize;
  attrib->owned_stack = owned_stack;
  attrib->tls = tls.addr;
  attrib->tls_size = tls.size;
  attrib->joiner = nullptr;

  start_args->thread_attrib = attrib;
  start_args->runner = runner;
  start_args->arg = arg;

  auto clear_tid = reinterpret_cast<Futex *>(
      adjusted_stack + sizeof(StartArgs) + sizeof(ThreadAttributes));
  clear_tid->set(CLEAR_TID_VALUE);
  attrib->platform_data = clear_tid;

  // The clone syscall takes arguments in an architecture specific order.
  // Also, we want the result of the syscall to be in a register as the child
  // thread gets a completely different stack after it is created. The stack
  // variables from this function will not be availalbe to the child thread.
#if defined(LIBC_TARGET_ARCH_IS_X86_64)
  long register clone_result asm(CLONE_RESULT_REGISTER);
  clone_result = LIBC_NAMESPACE::syscall_impl<long>(
      SYS_clone, CLONE_SYSCALL_FLAGS, adjusted_stack,
      &attrib->tid,    // The address where the child tid is written
      &clear_tid->val, // The futex where the child thread status is signalled
      tls.tp           // The thread pointer value for the new thread.
  );
#elif defined(LIBC_TARGET_ARCH_IS_AARCH64) ||                                  \
    defined(LIBC_TARGET_ARCH_IS_ANY_RISCV)
  long register clone_result asm(CLONE_RESULT_REGISTER);
  clone_result = LIBC_NAMESPACE::syscall_impl<long>(
      SYS_clone, CLONE_SYSCALL_FLAGS, adjusted_stack,
      &attrib->tid,   // The address where the child tid is written
      tls.tp,         // The thread pointer value for the new thread.
      &clear_tid->val // The futex where the child thread status is signalled
  );
#else
#error "Unsupported architecture for the clone syscall."
#endif

  if (clone_result == 0) {
#ifdef LIBC_TARGET_ARCH_IS_AARCH64
    // We set the frame pointer to be the same as the "sp" so that start args
    // can be sniffed out from start_thread.
#ifdef __clang__
    // GCC does not currently implement __arm_wsr64/__arm_rsr64.
    __arm_wsr64("x29", __arm_rsr64("sp"));
#else
    asm volatile("mov x29, sp");
#endif
#elif defined(LIBC_TARGET_ARCH_IS_ANY_RISCV)
    asm volatile("mv fp, sp");
#endif
    start_thread();
  } else if (clone_result < 0) {
    cleanup_thread_resources(attrib);
    return static_cast<int>(-clone_result);
  }

  return 0;
}

int Thread::join(ThreadReturnValue &retval) {
  if (self.attrib) {
    // Reject self join.
    if (self.attrib == attrib)
      return EDEADLK;

    // Do a best-effort check of concurrent/repeated join.
    // This cmpxchg establishes exclusive joiner role by setting the joiner
    // field iff there is no previous joiner
    ThreadAttributes *expected = nullptr;
    if (!attrib->joiner.compare_exchange_strong(expected, self.attrib,
                                                cpp::MemoryOrder::ACQ_REL))
      return EINVAL;

    // Reject mutual join.
    if (self.attrib->joiner.load(cpp::MemoryOrder::ACQUIRE) == attrib) {
      attrib->joiner.store(nullptr, cpp::MemoryOrder::RELEASE);
      return EDEADLK;
    }
  }

  wait();

  if (attrib->style == ThreadStyle::POSIX)
    retval.posix_retval = attrib->retval.posix_retval;
  else
    retval.stdc_retval = attrib->retval.stdc_retval;

  cleanup_thread_resources(attrib);

  return 0;
}

int Thread::detach() {
  uint32_t joinable_state = uint32_t(DetachState::JOINABLE);
  if (attrib->detach_state.compare_exchange_strong(
          joinable_state, uint32_t(DetachState::DETACHED))) {
    return int(DetachType::SIMPLE);
  }

  // If the thread was already detached, then the detach method should not
  // be called at all. If the thread is exiting, then we wait for it to exit
  // and free up resources.
  wait();

  cleanup_thread_resources(attrib);

  return int(DetachType::CLEANUP);
}

void Thread::wait() {
  // The kernel should set the value at the clear tid address to zero.
  // If not, it is a spurious wake and we should continue to wait on
  // the futex.
  auto *clear_tid = reinterpret_cast<Futex *>(attrib->platform_data);
  // We cannot do a FUTEX_WAIT_PRIVATE here as the kernel does a
  // FUTEX_WAKE and not a FUTEX_WAKE_PRIVATE.
  while (clear_tid->load() != 0)
    clear_tid->wait(CLEAR_TID_VALUE, cpp::nullopt, true);
}

bool Thread::operator==(const Thread &thread) const {
  return attrib->tid == thread.attrib->tid;
}

static constexpr cpp::string_view THREAD_NAME_PATH_PREFIX("/proc/self/task/");
static constexpr size_t THREAD_NAME_PATH_SIZE =
    THREAD_NAME_PATH_PREFIX.size() +
    IntegerToString<int>::buffer_size() + // Size of tid
    1 +                                   // For '/' character
    5; // For the file name "comm" and the nullterminator.

static void construct_thread_name_file_path(cpp::StringStream &stream,
                                            int tid) {
  stream << THREAD_NAME_PATH_PREFIX << tid << '/' << cpp::string_view("comm")
         << cpp::StringStream::ENDS;
}

int Thread::set_name(const cpp::string_view &name) {
  if (name.size() >= NAME_SIZE_MAX)
    return ERANGE;

  if (*this == self) {
    // If we are setting the name of the current thread, then we can
    // use the syscall to set the name.
    int retval =
        LIBC_NAMESPACE::syscall_impl<int>(SYS_prctl, PR_SET_NAME, name.data());
    if (retval < 0)
      return -retval;
    else
      return 0;
  }

  char path_name_buffer[THREAD_NAME_PATH_SIZE];
  cpp::StringStream path_stream(path_name_buffer);
  construct_thread_name_file_path(path_stream, attrib->tid);
#ifdef SYS_open
  int fd =
      LIBC_NAMESPACE::syscall_impl<int>(SYS_open, path_name_buffer, O_RDWR);
#else
  int fd = LIBC_NAMESPACE::syscall_impl<int>(SYS_openat, AT_FDCWD,
                                             path_name_buffer, O_RDWR);
#endif
  if (fd < 0)
    return -fd;

  int retval = LIBC_NAMESPACE::syscall_impl<int>(SYS_write, fd, name.data(),
                                                 name.size());
  LIBC_NAMESPACE::syscall_impl<long>(SYS_close, fd);

  if (retval < 0)
    return -retval;
  else if (retval != int(name.size()))
    return EIO;
  else
    return 0;
}

int Thread::get_name(cpp::StringStream &name) const {
  if (name.bufsize() < NAME_SIZE_MAX)
    return ERANGE;

  char name_buffer[NAME_SIZE_MAX];

  if (*this == self) {
    // If we are getting the name of the current thread, then we can
    // use the syscall to get the name.
    int retval =
        LIBC_NAMESPACE::syscall_impl<int>(SYS_prctl, PR_GET_NAME, name_buffer);
    if (retval < 0)
      return -retval;
    name << name_buffer << cpp::StringStream::ENDS;
    return 0;
  }

  char path_name_buffer[THREAD_NAME_PATH_SIZE];
  cpp::StringStream path_stream(path_name_buffer);
  construct_thread_name_file_path(path_stream, attrib->tid);
#ifdef SYS_open
  int fd =
      LIBC_NAMESPACE::syscall_impl<int>(SYS_open, path_name_buffer, O_RDONLY);
#else
  int fd = LIBC_NAMESPACE::syscall_impl<int>(SYS_openat, AT_FDCWD,
                                             path_name_buffer, O_RDONLY);
#endif
  if (fd < 0)
    return -fd;

  int retval = LIBC_NAMESPACE::syscall_impl<int>(SYS_read, fd, name_buffer,
                                                 NAME_SIZE_MAX);
  LIBC_NAMESPACE::syscall_impl<long>(SYS_close, fd);
  if (retval < 0)
    return -retval;
  if (retval == NAME_SIZE_MAX)
    return ERANGE;
  if (name_buffer[retval - 1] == '\n')
    name_buffer[retval - 1] = '\0';
  else
    name_buffer[retval] = '\0';
  name << name_buffer << cpp::StringStream::ENDS;
  return 0;
}

void thread_exit(ThreadReturnValue retval, ThreadStyle style) {
  auto attrib = self.attrib;

  // The very first thing we do is to call the thread's atexit callbacks.
  // These callbacks could be the ones registered by the language runtimes,
  // for example, the destructors of thread local objects. They can also
  // be destructors of the TSS objects set using API like pthread_setspecific.
  // NOTE: We cannot call the atexit callbacks as part of the
  // cleanup_thread_resources function as that function can be called from a
  // different thread. The destructors of thread local and TSS objects should
  // be called by the thread which owns them.
  internal::call_atexit_callbacks(attrib);

  uint32_t joinable_state = uint32_t(DetachState::JOINABLE);
  if (!attrib->detach_state.compare_exchange_strong(
          joinable_state, uint32_t(DetachState::EXITING))) {
    // Thread is detached so cleanup the resources.
    cleanup_thread_resources(attrib);

    // Set the CLEAR_TID address to nullptr to prevent the kernel
    // from signalling at a non-existent futex location.
    LIBC_NAMESPACE::syscall_impl<long>(SYS_set_tid_address, 0);
    // Return value for detached thread should be unused. We need to avoid
    // referencing `style` or `retval.*` because they may be stored on the stack
    // and we have deallocated our stack!
    LIBC_NAMESPACE::syscall_impl<long>(SYS_exit, 0);
    __builtin_unreachable();
  }

  if (style == ThreadStyle::POSIX)
    LIBC_NAMESPACE::syscall_impl<long>(SYS_exit, retval.posix_retval);
  else
    LIBC_NAMESPACE::syscall_impl<long>(SYS_exit, retval.stdc_retval);
  __builtin_unreachable();
}

} // namespace LIBC_NAMESPACE_DECL
