// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/modules.h"

#include "src/ast-value-factory.h"

namespace v8 {
namespace internal {

// ---------------------------------------------------------------------------
// Addition.

void ModuleDescriptor::Add(const AstRawString* name, Zone* zone, bool* ok) {
  void* key = const_cast<AstRawString*>(name);

  ZoneHashMap** map = &exports_;
  ZoneAllocationPolicy allocator(zone);

  if (*map == nullptr) {
    *map = new (zone->New(sizeof(ZoneHashMap)))
        ZoneHashMap(ZoneHashMap::PointersMatch,
                    ZoneHashMap::kDefaultHashMapCapacity, allocator);
  }

  ZoneHashMap::Entry* p =
      (*map)->Lookup(key, name->hash(), !IsFrozen(), allocator);
  if (p == nullptr || p->value != nullptr) {
    *ok = false;
  }

  p->value = key;
}
}
}  // namespace v8::internal
