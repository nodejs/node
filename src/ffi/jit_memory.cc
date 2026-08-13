#if HAVE_FFI

#include "ffi/jit_memory.h"

#include <cstdint>
#include <cstring>
#include <mutex>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <libkern/OSCacheControl.h>
#endif

#endif  // defined(_WIN32)

namespace node::ffi {

namespace {

bool SelfTest() {
#if !defined(__aarch64__) && !defined(_M_ARM64) && !defined(__x86_64__) &&     \
    !defined(_M_X64) && !defined(__powerpc64__) && !defined(__ppc64__) &&      \
    !defined(__PPC64__) && !defined(__loongarch64) &&                          \
    !(defined(__riscv) && __riscv_xlen == 64) && !defined(__s390x__)
  // No stub emitter for this platform; nothing to test.
  return false;
#else
#if defined(__aarch64__) || defined(_M_ARM64)
  // AArch64 BR LR: 0xD65F03C0
  constexpr uint32_t kInstruction = 0xD65F03C0;
  constexpr size_t kInstructionSize = sizeof(uint32_t);
#elif defined(__powerpc64__) || defined(__ppc64__) || defined(__PPC64__)
  // PPC64 BLR: 0x4E800020
  constexpr uint32_t kInstruction = 0x4E800020;
  constexpr size_t kInstructionSize = sizeof(uint32_t);
#elif defined(__loongarch64)
  // LoongArch64 JIRL zero, ra, 0
  constexpr uint32_t kInstruction = 0x4C000020;
  constexpr size_t kInstructionSize = sizeof(uint32_t);
#elif defined(__riscv) && __riscv_xlen == 64
  // RISC-V JALR zero, ra, 0
  constexpr uint32_t kInstruction = 0x00008067;
  constexpr size_t kInstructionSize = sizeof(uint32_t);
#elif defined(__s390x__)
  // s390x BR r14
  constexpr uint16_t kInstruction = 0x07fe;
  constexpr size_t kInstructionSize = sizeof(uint16_t);
#else
  // x86_64 RET: 0xC3
  constexpr uint8_t kInstruction = 0xC3;
  constexpr size_t kInstructionSize = sizeof(uint8_t);
#endif

#if defined(_WIN32)
  void* page = VirtualAlloc(
      nullptr, kInstructionSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (page == nullptr) {
    return false;
  }

  uint8_t* code = static_cast<uint8_t*>(page);
#if defined(__aarch64__) || defined(_M_ARM64) || defined(__powerpc64__) ||     \
    defined(__ppc64__) || defined(__PPC64__) || defined(__loongarch64) ||      \
    (defined(__riscv) && __riscv_xlen == 64) || defined(__s390x__)
  std::memcpy(code, &kInstruction, kInstructionSize);
#else
  code[0] = kInstruction;
#endif

  FlushInstructionCache(GetCurrentProcess(), page, kInstructionSize);

  DWORD old_protect;
  const bool ok =
      VirtualProtect(page, kInstructionSize, PAGE_EXECUTE_READ, &old_protect) !=
      0;
  VirtualFree(page, 0, MEM_RELEASE);
  return ok;
#else
  const size_t page_size = static_cast<size_t>(getpagesize());
  void* page = mmap(nullptr,
                    page_size,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANON,
                    -1,
                    0);
  if (page == MAP_FAILED) {
    return false;
  }

  uint8_t* code = static_cast<uint8_t*>(page);
#if defined(__aarch64__) || defined(_M_ARM64) || defined(__powerpc64__) ||     \
    defined(__ppc64__) || defined(__PPC64__) || defined(__loongarch64) ||      \
    (defined(__riscv) && __riscv_xlen == 64) || defined(__s390x__)
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
#endif
#endif
}

}  // namespace

bool IsJitMemorySupported() {
  // Run the self-test exactly once and publish only the final result, so
  // concurrent callers never observe a provisional value.
  static std::once_flag once;
  static bool supported = false;
  std::call_once(once, [] { supported = SelfTest(); });
  return supported;
}

}  // namespace node::ffi

#endif  // HAVE_FFI
