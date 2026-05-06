#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS && \
    defined(HAVE_FFI_FASTCALL)

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace v8 { class Isolate; }

namespace node::ffi::fastcall {

// Handle returned from EmitStub (and from the lower-level Allocate path).
struct EmittedStub {
  void* entry;        // CFunction.address
  size_t alloc_size;
};

class JitMemory {
 public:
  // Returns the singleton. On first call, initialises internal state (page
  // size detection). The isolate parameter is retained for API symmetry with
  // future callers; the implementation uses direct mmap/VirtualAlloc rather
  // than the v8::PageAllocator interface to ensure MAP_JIT on Apple Silicon.
  // Subsequent calls may pass nullptr.
  static JitMemory* Get(v8::Isolate* isolate);

  // Allocate `size` bytes (rounded up to slot alignment, currently 64).
  // Returns nullptr on failure (zero size, allocator unavailable, OOM).
  // The returned pointer is in RW state — write the stub bytes, then call
  // MakeExecutable.
  void* Allocate(size_t size, size_t* out_alloc_size);

  // Transition the page containing `ptr` (extent `alloc_size`) from RW to
  // RX, flushing icache as needed. Returns true on success. After this
  // returns, the page is full as far as Allocate is concerned — subsequent
  // allocations go to a fresh page.
  bool MakeExecutable(void* ptr, size_t alloc_size);

  // Decrement the live-byte counter. Memory is not returned to the OS.
  void Free(void* ptr, size_t alloc_size);

  // Convenience overload for callers that hold an EmittedStub value.
  // Documents the pairing with EmitStub at the type level.
  void Free(const EmittedStub& stub) {
    Free(stub.entry, stub.alloc_size);
  }

  // Total bytes currently allocated to live stubs.
  size_t TotalLiveBytes() const;

  // All-in-one: allocate, write code, transition to RX, return handle.
  // Holds the mutex across the entire operation; safe under multi-isolate
  // concurrent emit. Returns std::nullopt on any failure (OOM, MakeExecutable).
  std::optional<EmittedStub> EmitStub(const uint8_t* code, size_t size);

  // For testing only: free all pages back to the OS. Asserts no live bytes.
  void ResetForTesting();

  // One-time process-wide self-test: allocate, write platform-native
  // "return 42", make RX, call. Cached. On failure, sets a flag so future
  // Allocate calls return nullptr; callers fall back to libffi.
  bool SelfTest(v8::Isolate* isolate);

 private:
  JitMemory();

  struct Page {
    void* base;
    size_t size;
    size_t bump;
    bool executable;
  };

  size_t page_size_;  // 0 means not yet initialised
  size_t slot_align_;
  std::vector<Page> pages_;
  size_t live_bytes_;
  mutable std::mutex mu_;
};

}  // namespace node::ffi::fastcall

#endif  // NODE_WANT_INTERNALS && HAVE_FFI_FASTCALL
