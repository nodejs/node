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
//
// -----------------------------------------------------------------------------
// File: chunked_queue.h
// -----------------------------------------------------------------------------
//
// `std::deque` provides random access and fast push/pop back/front. It is
// implemented as an array of fixed blocks. It provides no control of block size
// and implementations differ; libstdc++ tries to allocate blocks of ~512 bytes
// and libc++ tries for blocks of ~4k bytes.
//
// `absl::chunked_queue` provides the same minus random access. It is
// implemented as a double-linked list of fixed or variable sized blocks.
//
// `absl::chunked_queue` is useful when memory usage is paramount as it provides
//  finegrained and configurable block sizing.
//
// The interface supported by this class is limited to:
//
//   empty()
//   size()
//   max_size()
//   shrink_to_fit()
//   resize()
//   assign()
//   push_back()
//   emplace_back()
//   pop_front()
//   front()
//   back()
//   swap()
//   clear()
//   begin(), end()
//   cbegin(), cend()
//
// === ADVANCED USAGE
//
// == clear()
//
// As an optimization clear() leaves the first block of the chunked_queue
// allocated (but empty). So clear will not delete all memory of the container.
// In order to do so, call shrink_to_fit() or swap the container with an empty
// one.
//
//   absl::chunked_queue<int64> q = {1, 2, 3};
//   q.clear();
//   q.shrink_to_fit();
//
// == block size customization
//
// chunked_queue allows customization of the block size for each block. By
// default the block size is set to 1 element and the size doubles for the next
// block until it reaches the default max block size, which is 128 elements.
//
// = fixed size
//
// When only the first block size parameter is specified, it sets a fixed block
// size for all blocks:
//
//   chunked_queue<T, 32>: 32 elements per block
//
// The smaller the block size, the less the memory usage for small queues at the
// cost of performance. Caveat: For large queues, a smaller block size will
// increase memory usage, and reduce performance.
//
// = variable size
//
// When both block size parameters are specified, they set the min and max block
// sizes for the blocks. Initially the queue starts with the min block size and
// as it grows, the size of each block grows until it reaches the max block
// size.
// New blocks are double the size of the tail block (so they at least
// double the size of the queue).
//
//   chunked_queue<T, 4, 64>: first block 4 elements, second block 8 elements,
//                            third block 16 elements, fourth block 32 elements,
//                            all other blocks 64 elements
//
// One can specify a min and max such that small queues will not waste memory
// and large queues will not have too many blocks.

#ifndef ABSL_CONTAINER_CHUNKED_QUEUE_H_
#define ABSL_CONTAINER_CHUNKED_QUEUE_H_

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
#include "absl/base/internal/iterator_traits.h"
#include "absl/base/macros.h"
#include "absl/container/internal/chunked_queue.h"
#include "absl/container/internal/layout.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

template <typename T, size_t BLo = 0, size_t BHi = BLo,
          typename Allocator = std::allocator<T>>
class chunked_queue {
 public:
  static constexpr size_t kBlockSizeMin = (BLo == 0 && BHi == 0) ? 1 : BLo;
  static constexpr size_t kBlockSizeMax = (BLo == 0 && BHi == 0) ? 128 : BHi;

 private:
  static_assert(kBlockSizeMin > 0, "Min block size cannot be zero");
  static_assert(kBlockSizeMin <= kBlockSizeMax, "Invalid block size bounds");

  using Block = container_internal::ChunkedQueueBlock<T, Allocator>;
  using AllocatorTraits = std::allocator_traits<Allocator>;

  class iterator_common {
   public:
    friend bool operator==(const iterator_common& a, const iterator_common& b) {
      return a.ptr == b.ptr;
    }

    friend bool operator!=(const iterator_common& a, const iterator_common& b) {
      return !(a == b);
    }

   protected:
    iterator_common() = default;
    explicit iterator_common(Block* b)
        : block(b), ptr(b->start()), limit(b->limit()) {}

    void Incr() {
      // If we do not have a next block, make ptr point one past the end of this
      // block. If we do have a next block, make ptr point to the first element
      // of the next block.
      ++ptr;
      if (ptr == limit && block->next()) *this = iterator_common(block->next());
    }

    void IncrBy(size_t n) {
      while (ptr + n > limit) {
        n -= limit - ptr;
        *this = iterator_common(block->next());
      }
      ptr += n;
    }

    Block* block = nullptr;
    T* ptr = nullptr;
    T* limit = nullptr;
  };

  // CT can be either T or const T.
  template <typename CT>
  class basic_iterator : public iterator_common {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = typename AllocatorTraits::value_type;
    using difference_type = typename AllocatorTraits::difference_type;
    using pointer =
        typename std::conditional<std::is_const<CT>::value,
                                  typename AllocatorTraits::const_pointer,
                                  typename AllocatorTraits::pointer>::type;
    using reference = CT&;

    basic_iterator() = default;

    // Copy ctor if CT is T.
    // Otherwise it's a conversion of iterator to const_iterator.
    basic_iterator(const basic_iterator<T>& it)  // NOLINT(runtime/explicit)
        : iterator_common(it) {}

    basic_iterator& operator=(const basic_iterator& other) = default;

    reference operator*() const { return *this->ptr; }
    pointer operator->() const { return this->ptr; }
    basic_iterator& operator++() {
      this->Incr();
      return *this;
    }
    basic_iterator operator++(int) {
      basic_iterator t = *this;
      ++*this;
      return t;
    }

   private:
    explicit basic_iterator(Block* b) : iterator_common(b) {}

    friend chunked_queue;
  };

 public:
  using allocator_type = typename AllocatorTraits::allocator_type;
  using value_type = typename AllocatorTraits::value_type;
  using size_type = typename AllocatorTraits::size_type;
  using difference_type = typename AllocatorTraits::difference_type;
  using reference = value_type&;
  using const_reference = const value_type&;
  using iterator = basic_iterator<T>;
  using const_iterator = basic_iterator<const T>;

  // Constructs an empty queue.
  chunked_queue() : chunked_queue(allocator_type()) {}

  // Constructs an empty queue with a custom allocator.
  explicit chunked_queue(const allocator_type& alloc)
      : alloc_and_size_(alloc) {}

  // Constructs a queue with `count` default-inserted elements.
  explicit chunked_queue(size_type count,
                         const allocator_type& alloc = allocator_type())
      : alloc_and_size_(alloc) {
    resize(count);
  }

  // Constructs a queue with `count` copies of `value`.
  chunked_queue(size_type count, const T& value,
                const allocator_type& alloc = allocator_type())
      : alloc_and_size_(alloc) {
    assign(count, value);
  }

  // Constructs a queue with the contents of the range [first, last).
  template <typename Iter,
            typename = std::enable_if_t<
                base_internal::IsAtLeastInputIterator<Iter>::value>>
  chunked_queue(Iter first, Iter last,
                const allocator_type& alloc = allocator_type())
      : alloc_and_size_(alloc) {
    using Tag = typename std::iterator_traits<Iter>::iterator_category;
    RangeInit(first, last, Tag());
  }

  // Constructs a queue with the contents of the initializer list `list`.
  chunked_queue(std::initializer_list<T> list,
                const allocator_type& alloc = allocator_type())
      : chunked_queue(list.begin(), list.end(), alloc) {}

  ~chunked_queue();

  // Copy constructor.
  chunked_queue(const chunked_queue& other)
      : chunked_queue(other,
                      AllocatorTraits::select_on_container_copy_construction(
                          other.alloc_and_size_.allocator())) {}

  // Copy constructor with specific allocator.
  chunked_queue(const chunked_queue& other, const allocator_type& alloc)
      : alloc_and_size_(alloc) {
    for (const_reference item : other) {
      push_back(item);
    }
  }

  // Move constructor.
  chunked_queue(chunked_queue&& other) noexcept
      : head_(other.head_),
        tail_(other.tail_),
        alloc_and_size_(std::move(other.alloc_and_size_)) {
    other.head_ = {};
    other.tail_ = {};
    other.alloc_and_size_.size = 0;
  }

  // Replaces contents with those from initializer list `il`.
  chunked_queue& operator=(std::initializer_list<T> il) {
    assign(il.begin(), il.end());
    return *this;
  }

  // Copy assignment operator.
  chunked_queue& operator=(const chunked_queue& other) {
    if (this == &other) {
      return *this;
    }
    if (AllocatorTraits::propagate_on_container_copy_assignment::value &&
        (alloc_and_size_.allocator() != other.alloc_and_size_.allocator())) {
      // Destroy all current elements and blocks with the current allocator,
      // before switching this to use the allocator propagated from "other".
      DestroyAndDeallocateAll();
      alloc_and_size_ = AllocatorAndSize(other.alloc_and_size_.allocator());
    }
    assign(other.begin(), other.end());
    return *this;
  }

  // Move assignment operator.
  chunked_queue& operator=(chunked_queue&& other) noexcept;

  // Returns true if the queue contains no elements.
  bool empty() const { return alloc_and_size_.size == 0; }

  // Returns the number of elements in the queue.
  size_t size() const { return alloc_and_size_.size; }

  // Returns the maximum number of elements the queue is able to hold.
  size_type max_size() const noexcept {
    return AllocatorTraits::max_size(alloc_and_size_.allocator());
  }

  // Resizes the container to contain `new_size` elements.
  // If `new_size > size()`, additional default-inserted elements are appended.
  // If `new_size < size()`, elements are removed from the end.
  void resize(size_t new_size);

  // Resizes the container to contain `new_size` elements.
  // If `new_size > size()`, additional copies of `value` are appended.
  // If `new_size < size()`, elements are removed from the end.
  void resize(size_type new_size, const T& value) {
    if (new_size > size()) {
      size_t to_add = new_size - size();
      for (size_t i = 0; i < to_add; ++i) {
        push_back(value);
      }
    } else {
      resize(new_size);
    }
  }

  // Requests the removal of unused capacity.
  void shrink_to_fit() {
    // As an optimization clear() leaves the first block of the chunked_queue
    // allocated (but empty). When empty, shrink_to_fit() deallocates the first
    // block by swapping it a newly constructed container that has no first
    // block.
    if (empty()) {
      chunked_queue(alloc_and_size_.allocator()).swap(*this);
    }
  }

  // Replaces the contents with copies of those in the range [first, last).
  template <typename Iter,
            typename = std::enable_if_t<
                base_internal::IsAtLeastInputIterator<Iter>::value>>
  void assign(Iter first, Iter last) {
    auto out = begin();
    Block* prev_block = nullptr;

    // Overwrite existing elements.
    for (; out != end() && first != last; ++first) {
      // Track the previous block so we can correctly update tail_ if we stop
      // exactly at a block boundary.
      if (out.ptr + 1 == out.block->limit()) {
        prev_block = out.block;
      }
      *out = *first;
      ++out;
    }

    // If we stopped exactly at the start of a block (meaning the previous block
    // was full), we must ensure tail_ points to the end of the previous block,
    // not the start of the current (now empty and to be deleted) block.
    // This maintains the invariant required by back() which assumes tail_
    // never points to the start of a block (unless it's the only block).
    if (!empty() && out.block != nullptr && out.ptr == out.block->start() &&
        prev_block != nullptr) {
      // Delete the current block and all subsequent blocks.
      //
      // NOTE: Calling EraseAllFrom on an iterator that points to the limit of
      // the previous block will not delete any element from the previous block.
      iterator prev_block_end(prev_block);
      prev_block_end.ptr = prev_block->limit();
      EraseAllFrom(prev_block_end);

      // Update tail_ to point to the end of the previous block.
      tail_ = prev_block_end;
      prev_block->set_next(nullptr);
    } else {
      // Standard erase from the current position to the end.
      EraseAllFrom(out);
    }

    // Append any remaining new elements.
    for (; first != last; ++first) {
      push_back(*first);
    }
  }

  // Replaces the contents with `count` copies of `value`.
  void assign(size_type count, const T& value) {
    clear();
    for (size_type i = 0; i < count; ++i) {
      push_back(value);
    }
  }

  // Replaces the contents with the elements from the initializer list `il`.
  void assign(std::initializer_list<T> il) { assign(il.begin(), il.end()); }

  // Appends the given element value to the end of the container.
  // Invalidates `end()` iterator. References to other elements remain valid.
  void push_back(const T& val) { emplace_back(val); }
  void push_back(T&& val) { emplace_back(std::move(val)); }

  // Appends a new element to the end of the container.
  // The element is constructed in-place with `args`.
  // Returns a reference to the new element.
  // Invalidates `end()` iterator. References to other elements remain valid.
  template <typename... A>
  T& emplace_back(A&&... args) {
    T* storage = AllocateBack();
    AllocatorTraits::construct(alloc_and_size_.allocator(), storage,
                               std::forward<A>(args)...);
    return *storage;
  }

  // Removes the first element of the container.
  // Invalidates iterators to the removed element.
  // REQUIRES: !empty()
  void pop_front();

  // Returns a reference to the first element in the container.
  // REQUIRES: !empty()
  T& front() {
    ABSL_HARDENING_ASSERT(!empty());
    return *head_;
  }
  const T& front() const {
    ABSL_HARDENING_ASSERT(!empty());
    return *head_;
  }

  // Returns a reference to the last element in the container.
  // REQUIRES: !empty()
  T& back() {
    ABSL_HARDENING_ASSERT(!empty());
    return *(&*tail_ - 1);
  }
  const T& back() const {
    ABSL_HARDENING_ASSERT(!empty());
    return *(&*tail_ - 1);
  }

  // Swaps the contents of this queue with `other`.
  void swap(chunked_queue& other) noexcept {
    using std::swap;
    swap(head_, other.head_);
    swap(tail_, other.tail_);
    if (AllocatorTraits::propagate_on_container_swap::value) {
      swap(alloc_and_size_, other.alloc_and_size_);
    } else {
      // Swap only the sizes; each object keeps its allocator.
      //
      // (It is undefined behavior to swap between two containers with unequal
      // allocators if propagate_on_container_swap is false, so we don't have to
      // handle that here like we do in the move-assignment operator.)
      ABSL_HARDENING_ASSERT(get_allocator() == other.get_allocator());
      swap(alloc_and_size_.size, other.alloc_and_size_.size);
    }
  }

  // Erases all elements from the container.
  // Note: Leaves one empty block allocated as an optimization.
  // To free all memory, call shrink_to_fit() after calling clear().
  void clear();

  iterator begin() { return head_; }
  iterator end() { return tail_; }

  const_iterator begin() const { return head_; }
  const_iterator end() const { return tail_; }

  const_iterator cbegin() const { return head_; }
  const_iterator cend() const { return tail_; }

  // Returns the allocator associated with the container.
  allocator_type get_allocator() const { return alloc_and_size_.allocator(); }

 private:
  // Empty base-class optimization: bundle storage for our allocator together
  // with a field we had to store anyway (size), via inheriting from the
  // allocator, so this allocator instance doesn't consume any storage
  // when its type has no data members.
  struct AllocatorAndSize : private allocator_type {
    explicit AllocatorAndSize(const allocator_type& alloc)
        : allocator_type(alloc) {}
    const allocator_type& allocator() const { return *this; }
    allocator_type& allocator() { return *this; }
    size_t size = 0;
  };

  template <typename Iter>
  void RangeInit(Iter first, Iter last, std::input_iterator_tag) {
    while (first != last) {
      AddTailBlock();
      for (; first != last && tail_.ptr != tail_.limit;
           ++alloc_and_size_.size, ++tail_.ptr, ++first) {
        AllocatorTraits::construct(alloc_and_size_.allocator(), tail_.ptr,
                                   *first);
      }
    }
  }

  void Construct(T* start, T* limit) {
    ABSL_ASSERT(start <= limit);
    for (; start != limit; ++start) {
      AllocatorTraits::construct(alloc_and_size_.allocator(), start);
    }
  }

  size_t Destroy(T* start, T* limit) {
    ABSL_ASSERT(start <= limit);
    const size_t n = limit - start;
    for (; start != limit; ++start) {
      AllocatorTraits::destroy(alloc_and_size_.allocator(), start);
    }
    return n;
  }

  T* block_begin(Block* b) const {
    return b == head_.block ? head_.ptr : b->start();
  }
  T* block_end(Block* b) const {
    // We have the choice of !b->next or b == tail_.block to determine if b is
    // the tail or not. !b->next is usually faster because the caller of
    // block_end() is most likely traversing the list of blocks so b->next is
    // already fetched into some register.
    return !b->next() ? tail_.ptr : b->limit();
  }

  void AddTailBlock();
  size_t NewBlockSize() {
    // Double the last block size and bound to [kBlockSizeMin, kBlockSizeMax].
    if (!tail_.block) return kBlockSizeMin;
    return (std::min)(kBlockSizeMax, 2 * tail_.block->size());
  }

  T* AllocateBack();
  void EraseAllFrom(iterator i);

  // Destroys any contained elements and destroys all allocated storage.
  // (Like clear(), except this doesn't leave any empty blocks behind.)
  void DestroyAndDeallocateAll();

  // The set of elements in the queue is the following:
  //
  // (1) When we have just one block:
  //      [head_.ptr .. tail_.ptr-1]
  // (2) When we have multiple blocks:
  //      [head_.ptr .. head_.limit-1]
  //      ... concatenation of all elements from interior blocks ...
  //      [tail_.ptr .. tail_.limit-1]
  //
  // Rep invariants:
  // When have just one block:
  //   head_.limit == tail_.limit == &head_.block->element[kBlockSize]
  // Always:
  //   head_.ptr <= head_.limit
  //   tail_.ptr <= tail_.limit

  iterator head_;
  iterator tail_;
  AllocatorAndSize alloc_and_size_;
};

template <typename T, size_t BLo, size_t BHi, typename Allocator>
constexpr size_t chunked_queue<T, BLo, BHi, Allocator>::kBlockSizeMin;

template <typename T, size_t BLo, size_t BHi, typename Allocator>
constexpr size_t chunked_queue<T, BLo, BHi, Allocator>::kBlockSizeMax;

template <typename T, size_t BLo, size_t BHi, typename Allocator>
inline void swap(chunked_queue<T, BLo, BHi, Allocator>& a,
                 chunked_queue<T, BLo, BHi, Allocator>& b) noexcept {
  a.swap(b);
}

template <typename T, size_t BLo, size_t BHi, typename Allocator>
chunked_queue<T, BLo, BHi, Allocator>&
chunked_queue<T, BLo, BHi, Allocator>::operator=(
    chunked_queue&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  DestroyAndDeallocateAll();

  if constexpr (AllocatorTraits::propagate_on_container_move_assignment::
                    value) {
    // Take over the storage of "other", along with its allocator.
    head_ = other.head_;
    tail_ = other.tail_;
    alloc_and_size_ = std::move(other.alloc_and_size_);
    other.head_ = {};
    other.tail_ = {};
    other.alloc_and_size_.size = 0;
  } else if (get_allocator() == other.get_allocator()) {
    // Take over the storage of "other", with which we share an allocator.
    head_ = other.head_;
    tail_ = other.tail_;
    alloc_and_size_.size = other.alloc_and_size_.size;
    other.head_ = {};
    other.tail_ = {};
    other.alloc_and_size_.size = 0;
  } else {
    // We cannot take over of the storage from "other", since it has a different
    // allocator; we're stuck move-assigning elements individually.
    for (auto& elem : other) {
      push_back(std::move(elem));
    }
  }
  return *this;
}

template <typename T, size_t BLo, size_t BHi, typename Allocator>
inline chunked_queue<T, BLo, BHi, Allocator>::~chunked_queue() {
  Block* b = head_.block;
  while (b) {
    Block* next = b->next();
    Destroy(block_begin(b), block_end(b));
    Block::Delete(b, &alloc_and_size_.allocator());
    b = next;
  }
}

template <typename T, size_t BLo, size_t BHi, typename Allocator>
void chunked_queue<T, BLo, BHi, Allocator>::resize(size_t new_size) {
  while (new_size > size()) {
    ptrdiff_t to_add = new_size - size();
    if (tail_.ptr == tail_.limit) {
      AddTailBlock();
    }
    T* start = tail_.ptr;
    T* limit = (std::min)(tail_.limit, start + to_add);
    Construct(start, limit);
    tail_.ptr = limit;
    alloc_and_size_.size += limit - start;
  }
  if (size() == new_size) {
    return;
  }
  ABSL_ASSERT(new_size < size());
  auto new_end = begin();
  new_end.IncrBy(new_size);
  ABSL_ASSERT(new_end != end());
  EraseAllFrom(new_end);
}

template <typename T, size_t BLo, size_t BHi, typename Allocator>
inline void chunked_queue<T, BLo, BHi, Allocator>::AddTailBlock() {
  ABSL_ASSERT(tail_.ptr == tail_.limit);
  auto* b = Block::New(NewBlockSize(), &alloc_and_size_.allocator());
  if (!head_.block) {
    ABSL_ASSERT(!tail_.block);
    head_ = iterator(b);
  } else {
    ABSL_ASSERT(tail_.block);
    tail_.block->set_next(b);
  }
  tail_ = iterator(b);
}

template <typename T, size_t BLo, size_t BHi, typename Allocator>
inline T* chunked_queue<T, BLo, BHi, Allocator>::AllocateBack() {
  if (tail_.ptr == tail_.limit) {
    AddTailBlock();
  }
  ++alloc_and_size_.size;
  return tail_.ptr++;
}

template <typename T, size_t BLo, size_t BHi, typename Allocator>
inline void chunked_queue<T, BLo, BHi, Allocator>::EraseAllFrom(iterator i) {
  if (!i.block) {
    return;
  }
  ABSL_ASSERT(i.ptr);
  ABSL_ASSERT(i.limit);
  alloc_and_size_.size -= Destroy(i.ptr, block_end(i.block));
  Block* b = i.block->next();
  while (b) {
    Block* next = b->next();
    alloc_and_size_.size -= Destroy(b->start(), block_end(b));
    Block::Delete(b, &alloc_and_size_.allocator());
    b = next;
  }
  tail_ = i;
  tail_.block->set_next(nullptr);
}

template <typename T, size_t BLo, size_t BHi, typename Allocator>
inline void chunked_queue<T, BLo, BHi, Allocator>::DestroyAndDeallocateAll() {
  Block* b = head_.block;
  while (b) {
    Block* next = b->next();
    Destroy(block_begin(b), block_end(b));
    Block::Delete(b, &alloc_and_size_.allocator());
    b = next;
  }
  head_ = iterator();
  tail_ = iterator();
  alloc_and_size_.size = 0;
}

template <typename T, size_t BLo, size_t BHi, typename Allocator>
inline void chunked_queue<T, BLo, BHi, Allocator>::pop_front() {
  ABSL_HARDENING_ASSERT(!empty());
  ABSL_ASSERT(head_.block);
  AllocatorTraits::destroy(alloc_and_size_.allocator(), head_.ptr);
  ++head_.ptr;
  --alloc_and_size_.size;
  if (empty()) {
    // Reset head and tail to the start of the (only) block.
    ABSL_ASSERT(head_.block == tail_.block);
    head_.ptr = tail_.ptr = head_.block->start();
    return;
  }
  if (head_.ptr == head_.limit) {
    Block* n = head_.block->next();
    Block::Delete(head_.block, &alloc_and_size_.allocator());
    head_ = iterator(n);
  }
}

template <typename T, size_t BLo, size_t BHi, typename Allocator>
void chunked_queue<T, BLo, BHi, Allocator>::clear() {
  // NOTE: As an optimization we leave one block allocated.
  Block* b = head_.block;
  if (!b) {
    ABSL_ASSERT(empty());
    return;
  }
  while (b) {
    Block* next = b->next();
    Destroy(block_begin(b), block_end(b));
    if (head_.block != b) {
      Block::Delete(b, &alloc_and_size_.allocator());
    }
    b = next;
  }
  b = head_.block;
  b->set_next(nullptr);
  head_ = tail_ = iterator(b);
  alloc_and_size_.size = 0;
}

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_CONTAINER_CHUNKED_QUEUE_H_
