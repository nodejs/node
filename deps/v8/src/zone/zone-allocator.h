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
class zone_allocator {
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
    typedef zone_allocator<O> other;
  };

#ifdef V8_CC_MSVC
  // MSVS unfortunately requires the default constructor to be defined.
  zone_allocator() : zone_(nullptr) { UNREACHABLE(); }
#endif
  explicit zone_allocator(Zone* zone) throw() : zone_(zone) {}
  explicit zone_allocator(const zone_allocator& other) throw()
      : zone_(other.zone_) {}
  template <typename U>
  zone_allocator(const zone_allocator<U>& other) throw() : zone_(other.zone_) {}
  template <typename U>
  friend class zone_allocator;

  pointer address(reference x) const { return &x; }
  const_pointer address(const_reference x) const { return &x; }

  pointer allocate(size_type n, const void* hint = 0) {
    return static_cast<pointer>(
        zone_->NewArray<value_type>(static_cast<int>(n)));
  }
  void deallocate(pointer p, size_type) { /* noop for Zones */
  }

  size_type max_size() const throw() {
    return std::numeric_limits<int>::max() / sizeof(value_type);
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

  bool operator==(zone_allocator const& other) const {
    return zone_ == other.zone_;
  }
  bool operator!=(zone_allocator const& other) const {
    return zone_ != other.zone_;
  }

  Zone* zone() { return zone_; }

 private:
  Zone* zone_;
};

typedef zone_allocator<bool> ZoneBoolAllocator;
typedef zone_allocator<int> ZoneIntAllocator;
}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_ZONE_ALLOCATOR_H_
