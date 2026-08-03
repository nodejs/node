// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GRAPH_ZONE_TRAITS_H_
#define V8_COMPILER_GRAPH_ZONE_TRAITS_H_

#include "src/zone/zone-type-traits.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class Node;

// GraphZoneTraits provides typedefs for pointer in zones.
using GraphZoneTraits = ZoneTypeTraits;

// ZoneNodePtr is a pointer to a Node allocated in a zone memory.
using ZoneNodePtr = GraphZoneTraits::Ptr<Node>;

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_GRAPH_ZONE_TRAITS_H_
