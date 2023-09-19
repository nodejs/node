// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_ZONE_HASHMAP_H_
#define V8_ZONE_ZONE_HASHMAP_H_

#include "src/base/hashmap.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

using ZoneHashMap = base::PointerTemplateHashMapImpl<ZoneAllocationPolicy>;

using CustomMatcherZoneHashMap =
    base::CustomMatcherTemplateHashMapImpl<ZoneAllocationPolicy>;

}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_ZONE_HASHMAP_H_
