// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_ZONE_SPLAY_TREE_H_
#define V8_ZONE_ZONE_SPLAY_TREE_H_

#include "src/splay-tree.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

// A zone splay tree.  The config type parameter encapsulates the
// different configurations of a concrete splay tree (see splay-tree.h).
// The tree itself and all its elements are allocated in the Zone.
template <typename Config>
class ZoneSplayTree final : public SplayTree<Config, ZoneAllocationPolicy> {
 public:
  explicit ZoneSplayTree(Zone* zone)
      : SplayTree<Config, ZoneAllocationPolicy>(ZoneAllocationPolicy(zone)) {}
  ~ZoneSplayTree() {
    // Reset the root to avoid unneeded iteration over all tree nodes
    // in the destructor.  For a zone-allocated tree, nodes will be
    // freed by the Zone.
    SplayTree<Config, ZoneAllocationPolicy>::ResetRoot();
  }

  void* operator new(size_t size, Zone* zone) { return zone->New(size); }

  void operator delete(void* pointer) { UNREACHABLE(); }
  void operator delete(void* pointer, Zone* zone) { UNREACHABLE(); }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_ZONE_SPLAY_TREE_H_
