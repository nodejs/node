// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_CONTAINERS_H_
#define V8_ZONE_CONTAINERS_H_

#include <vector>
#include <set>

#include "zone.h"

namespace v8 {
namespace internal {

typedef zone_allocator<int> ZoneIntAllocator;
typedef std::vector<int, ZoneIntAllocator> IntVector;
typedef IntVector::iterator IntVectorIter;
typedef IntVector::reverse_iterator IntVectorRIter;

} }  // namespace v8::internal

#endif  // V8_ZONE_CONTAINERS_H_
