// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_SANDBOX_H_
#define V8_SANDBOX_SANDBOX_H_

#include "include/v8-internal.h"
#include "include/v8-platform.h"
#include "include/v8config.h"
#include "src/common/globals.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // nogncheck

namespace v8 {

namespace internal {

#ifdef V8_SANDBOX_IS_AVAILABLE

/**
 * The V8 Sandbox.
 *
 * When enabled, V8 reserves a large region of virtual address space - the
 * sandbox - and places most of its objects inside of it. It is then assumed
 * that an attacker can, by exploiting a vulnerability in V8, corrupt memory
 * inside the sandbox arbitrarily and from different threads. The sandbox
 * attempts to stop an attacker from corrupting other memory in the process.
 *
 * The sandbox relies on a number of different mechanisms to achieve its goal.
 * For example, objects inside the sandbox can reference each other through
 * offsets from the start of the sandbox ("sandboxed pointers") instead of raw
 * pointers, and external objects can be referenced through indices into a
 * per-Isolate table of external pointers ("sandboxed external pointers").
 *
 * The pointer compression region, which contains most V8 objects, and inside
 * of which compressed (32-bit) pointers are used, is located at the start of
 * the sandbox. The remainder of the sandbox is mostly used for memory
 * buffers, in particular ArrayBuffer backing stores and WASM memory cages.
 *
 * As the embedder is responsible for providing ArrayBuffer allocators, V8
 * exposes the virtual address space backing the sandbox to the embedder.
 */
class V8_EXPORT_PRIVATE Sandbox {
 public:
  // +-  ~~~  -+----------------------------------------  ~~~  -+-  ~~~  -+
  // |  32 GB  |                 (Ideally) 1 TB                 |  32 GB  |
  // |         |                                                |         |
  // | Guard   |      4 GB      :  ArrayBuffer backing stores,  | Guard   |
  // | Region  |    V8 Heap     :  WASM memory buffers, and     | Region  |
  // | (front) |     Region     :  any other sandboxed objects. | (back)  |
  // +-  ~~~  -+----------------+-----------------------  ~~~  -+-  ~~~  -+
  //           ^                                                ^
  //           base                                             base + size

  Sandbox() = default;

  Sandbox(const Sandbox&) = delete;
  Sandbox& operator=(Sandbox&) = delete;

  bool Initialize(v8::VirtualAddressSpace* vas);
  void Disable() {
    CHECK(!initialized_);
    disabled_ = true;
  }

  void TearDown();

  bool is_initialized() const { return initialized_; }
  bool is_disabled() const { return disabled_; }
  bool is_enabled() const { return !disabled_; }
  bool is_partially_reserved() const { return is_partially_reserved_; }

  Address base() const { return base_; }
  Address end() const { return end_; }
  size_t size() const { return size_; }

  Address base_address() const { return reinterpret_cast<Address>(&base_); }
  Address end_address() const { return reinterpret_cast<Address>(&end_); }
  Address size_address() const { return reinterpret_cast<Address>(&size_); }

  v8::PageAllocator* page_allocator() const {
    return sandbox_page_allocator_.get();
  }

  v8::VirtualAddressSpace* address_space() const {
    return address_space_.get();
  }

  bool Contains(Address addr) const {
    return addr >= base_ && addr < base_ + size_;
  }

  bool Contains(void* ptr) const {
    return Contains(reinterpret_cast<Address>(ptr));
  }

#ifdef V8_SANDBOXED_POINTERS
  class SandboxedPointerConstants final {
   public:
    Address empty_backing_store_buffer() const {
      return empty_backing_store_buffer_;
    }
    Address empty_backing_store_buffer_address() const {
      return reinterpret_cast<Address>(&empty_backing_store_buffer_);
    }
    void set_empty_backing_store_buffer(Address value) {
      empty_backing_store_buffer_ = value;
    }

    void Reset() { empty_backing_store_buffer_ = 0; }

   private:
    Address empty_backing_store_buffer_ = 0;
  };
  const SandboxedPointerConstants& constants() const { return constants_; }
#endif

 private:
  // The SequentialUnmapperTest calls the private Initialize method to create a
  // sandbox without guard regions, which would consume too much memory.
  friend class SequentialUnmapperTest;

  // These tests call the private Initialize methods below.
  FRIEND_TEST(SandboxTest, InitializationWithSize);
  FRIEND_TEST(SandboxTest, PartiallyReservedSandboxInitialization);
  FRIEND_TEST(SandboxTest, PartiallyReservedSandboxPageAllocation);

  // We allow tests to disable the guard regions around the sandbox. This is
  // useful for example for tests like the SequentialUnmapperTest which track
  // page allocations and so would incur a large overhead from the guard
  // regions. The provided virtual address space must be able to allocate
  // subspaces. The size must be a multiple of the allocation granularity of the
  // virtual memory space.
  bool Initialize(v8::VirtualAddressSpace* vas, size_t size,
                  bool use_guard_regions);

  // Used when reserving virtual memory is too expensive. A partially reserved
  // sandbox does not reserve all of its virtual memory and so doesn't have the
  // desired security properties as unrelated mappings could end up inside of
  // it and be corrupted. The size and size_to_reserve parameters must be
  // multiples of the allocation granularity of the virtual address space.
  bool InitializeAsPartiallyReservedSandbox(v8::VirtualAddressSpace* vas,
                                            size_t size,
                                            size_t size_to_reserve);

  // Initialize the constant objects for this sandbox. Called by the Initialize
  // methods above.
  void InitializeConstants();

  Address base_ = kNullAddress;
  Address end_ = kNullAddress;
  size_t size_ = 0;

  // Base and size of the virtual memory reservation backing this sandbox.
  // These can be different from the sandbox base and size due to guard regions
  // or when a fake sandbox is used.
  Address reservation_base_ = kNullAddress;
  size_t reservation_size_ = 0;

  bool initialized_ = false;
  bool disabled_ = false;
  bool is_partially_reserved_ = false;

  // The virtual address subspace backing the sandbox.
  std::unique_ptr<v8::VirtualAddressSpace> address_space_;

  // The page allocator instance for this sandbox.
  std::unique_ptr<v8::PageAllocator> sandbox_page_allocator_;

#ifdef V8_SANDBOXED_POINTERS
  // Constant objects inside this sandbox.
  SandboxedPointerConstants constants_;
#endif
};

#endif  // V8_SANDBOX_IS_AVAILABLE

#ifdef V8_SANDBOX
// This function is only available when the sandbox is actually used.
V8_EXPORT_PRIVATE Sandbox* GetProcessWideSandbox();
#endif

V8_INLINE void* EmptyBackingStoreBuffer() {
#ifdef V8_SANDBOXED_POINTERS
  return reinterpret_cast<void*>(
      GetProcessWideSandbox()->constants().empty_backing_store_buffer());
#else
  return nullptr;
#endif
}

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_SANDBOX_H_
