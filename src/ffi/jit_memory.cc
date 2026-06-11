#if HAVE_FFI

#include "ffi/jit_memory.h"

#if !defined(_WIN32)

#include <sys/mman.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <mutex>

#if defined(__APPLE__)
#include <libkern/OSCacheControl.h>
#endif

#endif  // !defined(_WIN32)

namespace node::ffi {

namespace {

#if !defined(_WIN32)

bool SelfTest() {
#if defined(__aarch64__) || defined(_M_ARM64)
  // AArch64 BR LR: 0xD65F03C0
  constexpr uint32_t kInstruction = 0xD65F03C0;
  constexpr size_t kInstructionSize = sizeof(uint32_t);
#elif defined(__x86_64__)
  // x86_64 RET: 0xC3
  constexpr uint8_t kInstruction = 0xC3;
  constexpr size_t kInstructionSize = sizeof(uint8_t);
#else
  // No stub emitter for this platform; nothing to test.
  return false;
#endif  // __aarch64__ || _M_ARM64 || __x86_64__

  const size_t page_size = static_cast<size_t>(getpagesize());
  void* page = mmap(nullptr, page_size, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANON, -1, 0);
  if (page == MAP_FAILED) {
    return false;
  }

  uint8_t* code = static_cast<uint8_t*>(page);
#if defined(__aarch64__) || defined(_M_ARM64)
  std::memcpy(code, &kInstruction, kInstructionSize);
#elif defined(__x86_64__)
  code[0] = kInstruction;
#endif

#if defined(__APPLE__)
  sys_icache_invalidate(page, kInstructionSize);
#else
  __builtin___clear_cache(static_cast<char*>(page),
                          static_cast<char*>(page) + kInstructionSize);
#endif

  // Transition the page to RX. This is the operation that actually fails on
  // the restricted environments we care about (macOS hardened runtime without
  // MAP_JIT, SELinux execmem denial, other OS-level execmem policies), so a
  // successful mprotect is the support signal.
  //
  // We deliberately do NOT execute the page. This probe may run during normal
  // operation (e.g. when an FFI function is first created), and executing
  // freshly generated code from a capability check could SIGSEGV/SIGKILL the
  // whole process on systems that block it. The real trampoline emitter runs
  // the same mprotect at creation time and falls back to libffi when it is
  // rejected, so environments that deny PROT_EXEC outright stay guarded.
  //
  // This does NOT cover the rarer case where mprotect(PROT_EXEC) succeeds but
  // execution itself still faults (e.g. macOS hardened runtime without
  // MAP_JIT, which this code path does not request). We accept that residual
  // risk rather than crash the process from a capability check; builds that
  // need fast FFI ship with the appropriate JIT entitlements.
  const bool ok = mprotect(page, page_size, PROT_READ | PROT_EXEC) == 0;
  munmap(page, page_size);
  return ok;
}

#endif  // !defined(_WIN32)

}  // namespace

bool IsJitMemorySupported() {
#if defined(_WIN32)
  // Windows stub emitter and VirtualAlloc-based JIT memory support not yet
  // implemented. Return false so the fast-call path falls back to libffi.
  return false;
#else
  // Run the self-test exactly once and publish only the final result, so
  // concurrent callers never observe a provisional value.
  static std::once_flag once;
  static bool supported = false;
  std::call_once(once, [] { supported = SelfTest(); });
  return supported;
#endif
}

}  // namespace node::ffi

#endif  // HAVE_FFI
