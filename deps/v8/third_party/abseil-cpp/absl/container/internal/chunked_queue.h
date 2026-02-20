// Copyright 2025 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ABSL_CONTAINER_INTERNAL_CHUNKED_QUEUE_H_
#define ABSL_CONTAINER_INTERNAL_CHUNKED_QUEUE_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/macros.h"
#include "absl/container/internal/layout.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace container_internal {

// ChunkedQueueBlock defines a node in a forward list of uninitialized storage
// of size T's. The user is responsible for constructing and destroying T's in
// said storage.
//
// ChunkedQueueBlock::New(size) returns said node, with at least size_hint T's
// of uninitialized storage.
template <typename T, typename Allocator>
class ChunkedQueueBlock {
 private:
  using ChunkedQueueBlockAllocator = typename std::allocator_traits<
      Allocator>::template rebind_alloc<ChunkedQueueBlock>;
  using ByteAllocator =
      typename std::allocator_traits<Allocator>::template rebind_alloc<char>;

 public:
  // NB, instances of this must not be created or destroyed directly, only via
  // the New() and Delete() methods.  (This notionally-private constructor is
  // public only to allow access from allocator types used by New().)
  explicit ChunkedQueueBlock(size_t size)
      : next_(nullptr), limit_(start() + size) {}

  // Must be deleted by ChunkedQueueBlock::Delete.
  static ChunkedQueueBlock* New(size_t size_hint, Allocator* alloc) {  // NOLINT
    ABSL_ASSERT(size_hint >= size_t{1});
    size_t allocation_bytes = AllocSize(size_hint);
    void* mem;
    std::tie(mem, allocation_bytes) = Allocate(allocation_bytes, alloc);
    const size_t element_count =
        (allocation_bytes - start_offset()) / sizeof(T);
    ChunkedQueueBlock* as_block = static_cast<ChunkedQueueBlock*>(mem);
    ChunkedQueueBlockAllocator block_alloc(*alloc);
    std::allocator_traits<ChunkedQueueBlockAllocator>::construct(
        block_alloc, as_block, element_count);
    return as_block;
  }

  static void Delete(ChunkedQueueBlock* ptr, Allocator* alloc) {
    const size_t allocation_bytes = AllocSize(ptr->size());
    ChunkedQueueBlockAllocator block_alloc(*alloc);
    std::allocator_traits<ChunkedQueueBlockAllocator>::destroy(block_alloc,
                                                               ptr);
    if constexpr (std::is_same_v<ByteAllocator, std::allocator<char>>) {
#ifdef __STDCPP_DEFAULT_NEW_ALIGNMENT__
      if (alignment() > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
        ::operator delete(ptr
#ifdef __cpp_sized_deallocation
                          ,
                          allocation_bytes
#endif
                          ,
                          std::align_val_t(alignment()));
        return;
      }
#endif
      ::operator delete(ptr);
    } else {
      void* mem = ptr;
      ByteAllocator byte_alloc(*alloc);
      std::allocator_traits<ByteAllocator>::deallocate(
          byte_alloc, static_cast<char*>(mem), allocation_bytes);
    }
  }

  ChunkedQueueBlock* next() const { return next_; }
  void set_next(ChunkedQueueBlock* next) { next_ = next; }
  T* start() {
    return reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(this) +
                                start_offset());
  }
  T* limit() { return limit_; }
  size_t size() { return limit() - start(); }

  static constexpr size_t block_size_from_bytes(size_t bytes) {
    return bytes <= static_cast<size_t>(start_offset())
               ? size_t{1}
               : elements_in_bytes(bytes - start_offset());
  }

 private:
  ChunkedQueueBlock(const ChunkedQueueBlock&) = delete;
  ChunkedQueueBlock& operator=(const ChunkedQueueBlock&) = delete;

  // The byte size to allocate to ensure space for `min_element_count` elements.
  static constexpr size_t AllocSize(size_t min_element_count) {
    return absl::container_internal::Layout<ChunkedQueueBlock, T>(
               1, min_element_count)
        .AllocSize();
  }

  static constexpr ptrdiff_t start_offset() {
    return absl::container_internal::Layout<ChunkedQueueBlock, T>(1, 1)
        .template Offset<1>();
  }

  static constexpr size_t alignment() {
    return absl::container_internal::Layout<ChunkedQueueBlock, T>(1, 1)
        .Alignment();
  }

  static constexpr size_t elements_in_bytes(size_t bytes) {
    return (bytes + sizeof(T) - 1) / sizeof(T);
  }

  static std::pair<void*, size_t> Allocate(size_t allocation_bytes,
                                           Allocator* alloc) {
    // If we're using the default allocator, then we can use new.
    void* mem;
    if constexpr (std::is_same_v<ByteAllocator, std::allocator<char>>) {
      // Older GCC versions have an unused variable warning on `alloc` inside
      // this constexpr branch.
      static_cast<void>(alloc);
#ifdef __STDCPP_DEFAULT_NEW_ALIGNMENT__
      if (alignment() > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
        // Align the allocation to respect alignof(T).
        mem = ::operator new(allocation_bytes, std::align_val_t(alignment()));
        return {mem, allocation_bytes};
      }
#endif
      mem = ::operator new(allocation_bytes);
    } else {
      ByteAllocator byte_alloc(*alloc);
      mem = std::allocator_traits<ByteAllocator>::allocate(byte_alloc,
                                                           allocation_bytes);
    }
    return {mem, allocation_bytes};
  }

  ChunkedQueueBlock* next_;
  T* limit_;
};

}  // namespace container_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_CONTAINER_INTERNAL_CHUNKED_QUEUE_H_
