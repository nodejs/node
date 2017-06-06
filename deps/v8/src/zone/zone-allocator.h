// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_ZONE_ALLOCATOR_H_
#define V8_ZONE_ZONE_ALLOCATOR_H_
#include <limits>

#include "src/zone/zone.h"

namespace v8 {
namespace internal {

template <typename T>
class ZoneAllocator {
 public:
  typedef T* pointer;
  typedef const T* const_pointer;
  typedef T& reference;
  typedef const T& const_reference;
  typedef T value_type;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  template <class O>
  struct rebind {
    typedef ZoneAllocator<O> other;
  };

#ifdef V8_CC_MSVC
  // MSVS unfortunately requires the default constructor to be defined.
  ZoneAllocator() : ZoneAllocator(nullptr) { UNREACHABLE(); }
#endif
  explicit ZoneAllocator(Zone* zone) throw() : zone_(zone) {}
  explicit ZoneAllocator(const ZoneAllocator& other) throw()
      : ZoneAllocator<T>(other.zone_) {}
  template <typename U>
  ZoneAllocator(const ZoneAllocator<U>& other) throw()
      : ZoneAllocator<T>(other.zone_) {}
  template <typename U>
  friend class ZoneAllocator;

  T* address(T& x) const { return &x; }
  const T* address(const T& x) const { return &x; }

  T* allocate(size_t n, const void* hint = 0) {
    return static_cast<T*>(zone_->NewArray<T>(static_cast<int>(n)));
  }
  void deallocate(T* p, size_t) { /* noop for Zones */
  }

  size_t max_size() const throw() {
    return std::numeric_limits<int>::max() / sizeof(T);
  }
  template <typename U, typename... Args>
  void construct(U* p, Args&&... args) {
    void* v_p = const_cast<void*>(static_cast<const void*>(p));
    new (v_p) U(std::forward<Args>(args)...);
  }
  template <typename U>
  void destroy(U* p) {
    p->~U();
  }

  bool operator==(ZoneAllocator const& other) const {
    return zone_ == other.zone_;
  }
  bool operator!=(ZoneAllocator const& other) const {
    return zone_ != other.zone_;
  }

  Zone* zone() { return zone_; }

 private:
  Zone* zone_;
};

// A recycling zone allocator maintains a free list of deallocated chunks
// to reuse on subsiquent allocations. The free list management is purposely
// very simple and works best for data-structures which regularly allocate and
// free blocks of similar sized memory (such as std::deque).
template <typename T>
class RecyclingZoneAllocator : public ZoneAllocator<T> {
 public:
  template <class O>
  struct rebind {
    typedef RecyclingZoneAllocator<O> other;
  };

#ifdef V8_CC_MSVC
  // MSVS unfortunately requires the default constructor to be defined.
  RecyclingZoneAllocator()
      : ZoneAllocator(nullptr, nullptr), free_list_(nullptr) {
    UNREACHABLE();
  }
#endif
  explicit RecyclingZoneAllocator(Zone* zone) throw()
      : ZoneAllocator<T>(zone), free_list_(nullptr) {}
  explicit RecyclingZoneAllocator(const RecyclingZoneAllocator& other) throw()
      : ZoneAllocator<T>(other), free_list_(nullptr) {}
  template <typename U>
  RecyclingZoneAllocator(const RecyclingZoneAllocator<U>& other) throw()
      : ZoneAllocator<T>(other), free_list_(nullptr) {}
  template <typename U>
  friend class RecyclingZoneAllocator;

  T* allocate(size_t n, const void* hint = 0) {
    // Only check top block in free list, since this will be equal to or larger
    // than the other blocks in the free list.
    if (free_list_ && free_list_->size >= n) {
      T* return_val = reinterpret_cast<T*>(free_list_);
      free_list_ = free_list_->next;
      return return_val;
    } else {
      return ZoneAllocator<T>::allocate(n, hint);
    }
  }

  void deallocate(T* p, size_t n) {
    if ((sizeof(T) * n < sizeof(FreeBlock))) return;

    // Only add block to free_list if it is equal or larger than previous block
    // so that allocation stays O(1) only having to look at the top block.
    if (!free_list_ || free_list_->size <= n) {
      // Store the free-list within the block being deallocated.
      DCHECK((sizeof(T) * n >= sizeof(FreeBlock)));
      FreeBlock* new_free_block = reinterpret_cast<FreeBlock*>(p);

      new_free_block->size = n;
      new_free_block->next = free_list_;
      free_list_ = new_free_block;
    }
  }

 private:
  struct FreeBlock {
    FreeBlock* next;
    size_t size;
  };

  FreeBlock* free_list_;
};

typedef ZoneAllocator<bool> ZoneBoolAllocator;
typedef ZoneAllocator<int> ZoneIntAllocator;

}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_ZONE_ALLOCATOR_H_
