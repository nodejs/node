// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_ALLOCATOR_H_
#define V8_ZONE_ALLOCATOR_H_

#include <limits>

#include "zone.h"

namespace v8 {
namespace internal {

template<typename T>
class zone_allocator {
 public:
  typedef T* pointer;
  typedef const T* const_pointer;
  typedef T& reference;
  typedef const T& const_reference;
  typedef T value_type;
  typedef size_t size_type;
  typedef ptrdiff_t difference_type;
  template<class O> struct rebind {
    typedef zone_allocator<O> other;
  };

  explicit zone_allocator(Zone* zone) throw() : zone_(zone) {}
  explicit zone_allocator(const zone_allocator& other) throw()
      : zone_(other.zone_) {}
  template<typename U> zone_allocator(const zone_allocator<U>& other) throw()
      : zone_(other.zone_) {}
  template<typename U> friend class zone_allocator;

  pointer address(reference x) const {return &x;}
  const_pointer address(const_reference x) const {return &x;}

  pointer allocate(size_type n, const void* hint = 0) {
    return static_cast<pointer>(zone_->NewArray<value_type>(
            static_cast<int>(n)));
  }
  void deallocate(pointer p, size_type) { /* noop for Zones */ }

  size_type max_size() const throw() {
    return std::numeric_limits<int>::max() / sizeof(value_type);
  }
  void construct(pointer p, const T& val) {
    new(static_cast<void*>(p)) T(val);
  }
  void destroy(pointer p) { p->~T(); }

  bool operator==(zone_allocator const& other) {
    return zone_ == other.zone_;
  }
  bool operator!=(zone_allocator const& other) {
    return zone_ != other.zone_;
  }

 private:
  zone_allocator();
  Zone* zone_;
};

} }  // namespace v8::internal

#endif  // V8_ZONE_ALLOCATOR_H_
