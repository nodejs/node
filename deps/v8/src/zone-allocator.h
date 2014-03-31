// Copyright 2014 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_ZONE_ALLOCATOR_H_
#define V8_ZONE_ALLOCATOR_H_

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

  pointer allocate(size_type count, const void* hint = 0) {
    size_t size = count * sizeof(value_type);
    size = RoundUp(size, kPointerSize);
    return static_cast<pointer>(zone_->New(size));
  }
  void deallocate(pointer p, size_type) { /* noop for Zones */ }

  size_type max_size() const throw() {
    size_type max = static_cast<size_type>(-1) / sizeof(T);
    return (max > 0 ? max : 1);
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
