// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_INL_H_
#define V8_ZONE_INL_H_

#include "src/zone.h"

#ifdef V8_USE_ADDRESS_SANITIZER
  #include <sanitizer/asan_interface.h>
#else
  #define ASAN_UNPOISON_MEMORY_REGION(start, size) ((void) 0)
#endif

#include "src/counters.h"
#include "src/isolate.h"
#include "src/utils.h"

namespace v8 {
namespace internal {


static const int kASanRedzoneBytes = 24;  // Must be a multiple of 8.


bool Zone::excess_allocation() {
  return segment_bytes_allocated_ > kExcessLimit;
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
  DCHECK(zone_);
  return zone_->New(static_cast<int>(size));
}


template <typename T>
void* ZoneList<T>::operator new(size_t size, Zone* zone) {
  return zone->New(static_cast<int>(size));
}


template <typename T>
void* ZoneSplayTree<T>::operator new(size_t size, Zone* zone) {
  return zone->New(static_cast<int>(size));
}


} }  // namespace v8::internal

#endif  // V8_ZONE_INL_H_
