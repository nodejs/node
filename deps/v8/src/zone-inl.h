// Copyright 2012 the V8 project authors. All rights reserved.
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

#ifndef V8_ZONE_INL_H_
#define V8_ZONE_INL_H_

#include "zone.h"

#include "counters.h"
#include "isolate.h"
#include "utils.h"
#include "v8-counters.h"

namespace v8 {
namespace internal {


inline void* Zone::New(int size) {
  ASSERT(scope_nesting_ > 0);
  // Round up the requested size to fit the alignment.
  size = RoundUp(size, kAlignment);

  // If the allocation size is divisible by 8 then we return an 8-byte aligned
  // address.
  if (kPointerSize == 4 && kAlignment == 4) {
    position_ += ((~size) & 4) & (reinterpret_cast<intptr_t>(position_) & 4);
  } else {
    ASSERT(kAlignment >= kPointerSize);
  }

  // Check if the requested size is available without expanding.
  Address result = position_;

  if (size > limit_ - position_) {
     result = NewExpand(size);
  } else {
     position_ += size;
  }

  // Check that the result has the proper alignment and return it.
  ASSERT(IsAddressAligned(result, kAlignment, 0));
  allocation_size_ += size;
  return reinterpret_cast<void*>(result);
}


template <typename T>
T* Zone::NewArray(int length) {
  return static_cast<T*>(New(length * sizeof(T)));
}


bool Zone::excess_allocation() {
  return segment_bytes_allocated_ > zone_excess_limit_;
}


void Zone::adjust_segment_bytes_allocated(int delta) {
  segment_bytes_allocated_ += delta;
  isolate_->counters()->zone_segment_bytes()->Set(segment_bytes_allocated_);
}


template <typename Config>
ZoneSplayTree<Config>::~ZoneSplayTree() {
  // Reset the root to avoid unneeded iteration over all tree nodes
  // in the destructor.  For a zone-allocated tree, nodes will be
  // freed by the Zone.
  SplayTree<Config, ZoneAllocationPolicy>::ResetRoot();
}


void* ZoneObject::operator new(size_t size, Zone* zone) {
  return zone->New(static_cast<int>(size));
}

inline void* ZoneAllocationPolicy::New(size_t size) {
  ASSERT(zone_);
  return zone_->New(static_cast<int>(size));
}


template <typename T>
void* ZoneList<T>::operator new(size_t size, Zone* zone) {
  return zone->New(static_cast<int>(size));
}


ZoneScope::ZoneScope(Zone* zone, ZoneScopeMode mode)
    : zone_(zone), mode_(mode) {
  zone_->scope_nesting_++;
}


bool ZoneScope::ShouldDeleteOnExit() {
  return zone_->scope_nesting_ == 1 && mode_ == DELETE_ON_EXIT;
}


} }  // namespace v8::internal

#endif  // V8_ZONE_INL_H_
