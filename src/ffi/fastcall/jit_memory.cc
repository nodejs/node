#include "ffi/fastcall/jit_memory.h"

#ifdef HAVE_FFI_FASTCALL

#include <atomic>
#include <cstring>

#include "node_internals.h"
#include "util.h"

#if defined(__POSIX__)
#include <sys/mman.h>
#include <unistd.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#endif

namespace node::ffi::fastcall {

namespace {

size_t RoundUp(size_t v, size_t align) {
  return (v + align - 1) & ~(align - 1);
}

// Allocate a region that can later become executable. On macOS/arm64 the
// mapping must be created with MAP_JIT at allocation time; only then can
// mprotect(PROT_READ|PROT_EXEC) succeed after the Hardened Runtime check.
void* AllocJitPages(size_t size) {
#if defined(__APPLE__)
  int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_JIT;
  void* p = mmap(nullptr, size, PROT_READ | PROT_WRITE, flags, -1, 0);
  return (p == MAP_FAILED) ? nullptr : p;
#elif defined(__POSIX__)
  void* p = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  return (p == MAP_FAILED) ? nullptr : p;
#elif defined(_WIN32)
  return VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT,
                      PAGE_READWRITE);
#else
  return nullptr;
#endif
}

bool MakePageRX(void* addr, size_t size) {
#if defined(__POSIX__)
  if (mprotect(addr, size, PROT_READ | PROT_EXEC) != 0) return false;
  // Flush icache for the full range.
  __builtin___clear_cache(static_cast<char*>(addr),
                          static_cast<char*>(addr) + size);
  return true;
#elif defined(_WIN32)
  DWORD old;
  if (!VirtualProtect(addr, size, PAGE_EXECUTE_READ, &old)) return false;
  FlushInstructionCache(GetCurrentProcess(), addr, size);
  return true;
#else
  return false;
#endif
}

void FreeJitPages(void* addr, size_t size) {
#if defined(__POSIX__)
  munmap(addr, size);
#elif defined(_WIN32)
  VirtualFree(addr, 0, MEM_RELEASE);
#endif
}

size_t SystemPageSize() {
#if defined(__POSIX__)
  return static_cast<size_t>(sysconf(_SC_PAGESIZE));
#elif defined(_WIN32)
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return static_cast<size_t>(si.dwPageSize);
#else
  return 4096;
#endif
}

}  // namespace

JitMemory* JitMemory::Get(v8::Isolate* isolate) {
  static JitMemory instance;
  // The isolate parameter is retained for API symmetry; the implementation
  // uses direct mmap/VirtualAlloc so no per-platform lookup is needed.
  std::lock_guard<std::mutex> lock(instance.mu_);
  if (instance.page_size_ == 0) {
    instance.page_size_ = SystemPageSize();
  }
  return &instance;
}

JitMemory::JitMemory()
    : page_size_(0),
      slot_align_(64),
      live_bytes_(0) {}

void* JitMemory::Allocate(size_t size, size_t* out_alloc_size) {
  if (out_alloc_size) *out_alloc_size = 0;
  if (size == 0) return nullptr;

  std::lock_guard<std::mutex> lock(mu_);
  if (page_size_ == 0) return nullptr;  // Not initialised.

  size_t need = RoundUp(size, slot_align_);

  if (!pages_.empty()) {
    auto& page = pages_.back();
    if (!page.executable && page.bump + need <= page.size) {
      void* p = static_cast<uint8_t*>(page.base) + page.bump;
      page.bump += need;
      live_bytes_ += need;
      if (out_alloc_size) *out_alloc_size = need;
      return p;
    }
  }

  size_t page_bytes = RoundUp(std::max(page_size_, need), page_size_);
  void* base = AllocJitPages(page_bytes);
  if (base == nullptr) return nullptr;

  pages_.push_back(Page{base, page_bytes, need, false});
  live_bytes_ += need;
  if (out_alloc_size) *out_alloc_size = need;
  return base;
}

bool JitMemory::MakeExecutable(void* ptr, size_t alloc_size) {
  std::lock_guard<std::mutex> lock(mu_);
  if (page_size_ == 0) return false;
  if (ptr == nullptr || alloc_size == 0) return false;

  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t page_start = addr & ~(page_size_ - 1);
  uintptr_t page_end = RoundUp(addr + alloc_size, page_size_);

  if (!MakePageRX(reinterpret_cast<void*>(page_start),
                  page_end - page_start)) {
    return false;
  }

  for (auto& page : pages_) {
    uintptr_t base = reinterpret_cast<uintptr_t>(page.base);
    if (addr >= base && addr < base + page.size) {
      page.executable = true;
      break;
    }
  }
  return true;
}

std::optional<EmittedStub> JitMemory::EmitStub(const uint8_t* code,
                                               size_t size) {
  if (code == nullptr || size == 0) return std::nullopt;

  std::lock_guard<std::mutex> lock(mu_);
  if (page_size_ == 0) return std::nullopt;

  size_t need = RoundUp(size, slot_align_);

  // Find or allocate a writable page.
  void* dst = nullptr;
  size_t alloc_size = need;

  if (!pages_.empty()) {
    auto& page = pages_.back();
    if (!page.executable && page.bump + need <= page.size) {
      dst = static_cast<uint8_t*>(page.base) + page.bump;
      page.bump += need;
      live_bytes_ += need;
    }
  }

  if (dst == nullptr) {
    size_t page_bytes = RoundUp(std::max(page_size_, need), page_size_);
    void* base = AllocJitPages(page_bytes);
    if (base == nullptr) return std::nullopt;
    pages_.push_back(Page{base, page_bytes, need, false});
    live_bytes_ += need;
    dst = base;
  }

  std::memcpy(dst, code, size);

  // Make executable — inline the mprotect/flush without re-locking.
  uintptr_t addr = reinterpret_cast<uintptr_t>(dst);
  uintptr_t page_start = addr & ~(page_size_ - 1);
  uintptr_t page_end = RoundUp(addr + alloc_size, page_size_);
  if (!MakePageRX(reinterpret_cast<void*>(page_start),
                  page_end - page_start)) {
    // MakePageRX failed (e.g., mprotect rejected by hardened runtime).
    // Roll back the live-bytes counter so the leak doesn't show up in
    // MemoryTracker. We do NOT roll back page.bump — the failed slot is
    // permanently wasted within the page, but the page itself stays in
    // pages_ in RW state and the next EmitStub may successfully use the
    // rest of it. The mapping is not returned to the OS.
    live_bytes_ -= alloc_size;
    return std::nullopt;
  }

  for (auto& page : pages_) {
    uintptr_t base = reinterpret_cast<uintptr_t>(page.base);
    if (addr >= base && addr < base + page.size) {
      page.executable = true;
      break;
    }
  }

  return EmittedStub{dst, alloc_size};
}

void JitMemory::Free(void* ptr, size_t alloc_size) {
  if (ptr == nullptr || alloc_size == 0) return;
  std::lock_guard<std::mutex> lock(mu_);
  CHECK_GE(live_bytes_, alloc_size);
  live_bytes_ -= alloc_size;
}

size_t JitMemory::TotalLiveBytes() const {
  std::lock_guard<std::mutex> lock(mu_);
  return live_bytes_;
}

void JitMemory::ResetForTesting() {
  std::lock_guard<std::mutex> lock(mu_);
  CHECK_EQ(live_bytes_, 0u);
  for (auto& page : pages_) {
    FreeJitPages(page.base, page.size);
  }
  pages_.clear();
}

bool JitMemory::SelfTest(v8::Isolate* isolate) {
  static std::once_flag once;
  static std::atomic<int> result{-1};

  std::call_once(once, [this, isolate]() {
    Get(isolate);

    size_t alloc_size = 0;
    void* p = Allocate(16, &alloc_size);
    if (p == nullptr) {
      result.store(0, std::memory_order_release);
      return;
    }

    uint8_t* code = static_cast<uint8_t*>(p);
#if defined(__x86_64__) || defined(_M_X64)
    code[0] = 0xb8; code[1] = 0x2a; code[2] = 0x00;
    code[3] = 0x00; code[4] = 0x00;
    code[5] = 0xc3;
#elif defined(__aarch64__) || defined(_M_ARM64)
    uint32_t* w = reinterpret_cast<uint32_t*>(code);
    w[0] = 0x52800540;
    w[1] = 0xd65f03c0;
#elif defined(__arm__) || defined(_M_ARM)
    uint32_t* w = reinterpret_cast<uint32_t*>(code);
    w[0] = 0xe3a0002a;
    w[1] = 0xe12fff1e;
#else
    Free(p, alloc_size);
    result.store(1, std::memory_order_release);
    return;
#endif

    if (!MakeExecutable(p, alloc_size)) {
      Free(p, alloc_size);
      result.store(0, std::memory_order_release);
      return;
    }

    using FnPtr = int (*)();
    int got = reinterpret_cast<FnPtr>(p)();
    bool ok = (got == 42);
    // The page stays executable, but the slot's bytes are no longer
    // counted as live. Future allocations skip this page (it's marked
    // executable), so the slot just sits unused — that's fine.
    Free(p, alloc_size);
    result.store(ok ? 1 : 0, std::memory_order_release);
  });

  return result.load(std::memory_order_acquire) == 1;
}

}  // namespace node::ffi::fastcall

#endif  // HAVE_FFI_FASTCALL
